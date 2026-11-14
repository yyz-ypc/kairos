#include "../brand.h"
// 时序之子 · 配置：路径、answers.json、private\*.txt、astrbot 的几份 json、版本号
// 全部按「只读为主、要改就整份原子替换」来做，跟 python 那版的行为一致。
#include "her.h"

#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <cwctype>
#include <ctime>

namespace her {

namespace {

std::wstring Up(const std::wstring& d) {
    size_t i = d.find_last_of(L"\\/");
    return i == std::wstring::npos ? d : d.substr(0, i);
}

std::wstring Lower(std::wstring s) {
    for (auto& c : s)
        if (c >= L'A' && c <= L'Z') c = (wchar_t)(c - L'A' + L'a');
    return s;
}

// ---------------------------------------------------------------- 配置缓存
// 缓存的生命跟着「当前工作区」走：切工作区（paths.cpp 的 SetWorkspaceDir）或跑完自设定
// （her_setup.cpp）都要调 InvalidateConfigCache() 把这几份清掉，否则工作台还拿旧工作区的
// answers.json / Base 算路径（E1）。另加一道保险：answers.json 的落盘指纹（mtime+大小）
// 变了就自动重读——her_features.cpp、安装包、手改也会写这个文件，那些路径不经过
// InvalidateConfigCache()。
//
// 锁的规矩：只在读写缓存的一瞬间持锁，读盘（ReadJson / BaseDir）都放在锁外，磁盘慢的时候
// 不会把别的线程一起堵住；g_cfgGen 是失效代数，防「读盘读到一半被失效」把旧工作区的数据
// 又装回缓存。
std::mutex g_cfgMutex;
unsigned g_cfgGen = 0;  // 受 g_cfgMutex 保护
J g_answers;
bool g_answersOk = false;
std::wstring g_answersStamp;
std::wstring g_base;
bool g_baseOk = false;
std::wstring g_data;
bool g_dataOk = false;

// 文件的「最后修改时间 + 大小」；只用来比较，不读内容。
std::wstring FileStamp(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA d{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &d)) return L"";
    wchar_t buf[48];
    swprintf(buf, 48, L"%08lx%08lx:%08lx%08lx", (unsigned long)d.ftLastWriteTime.dwHighDateTime,
             (unsigned long)d.ftLastWriteTime.dwLowDateTime, (unsigned long)d.nFileSizeHigh,
             (unsigned long)d.nFileSizeLow);
    return buf;
}

// 读 answers.json：命中缓存就拷一份走；没命中也把指纹带回去，装缓存时用同一份指纹。
J AnswersJson() {
    std::wstring path = JoinPath(Base(), L"answers.json");
    std::wstring stamp = FileStamp(path);
    unsigned gen = 0;
    {
        std::lock_guard<std::mutex> lk(g_cfgMutex);
        if (g_answersOk && g_answersStamp == stamp) return g_answers;
        gen = g_cfgGen;
    }
    const J loaded = ReadJson(path);  // 锁外读盘
    std::lock_guard<std::mutex> lk(g_cfgMutex);
    if (g_cfgGen != gen) return loaded;  // 期间切过工作区：结果照用，但不往缓存里装
    g_answers = loaded;
    g_answersOk = true;
    g_answersStamp = stamp;
    return g_answers;
}

}  // namespace

// ---------------------------------------------------------------- 路径
// 机器人根：跟工作台设置里的 base 一致（旧版按 exe 往上两级猜，装到 %LOCALAPPDATA% 时就猜歪了）
std::wstring Base() {
    unsigned gen = 0;
    {
        std::lock_guard<std::mutex> lk(g_cfgMutex);
        if (g_baseOk) return g_base;
        gen = g_cfgGen;
    }
    const std::wstring b = BaseDir();  // 锁外：要读 settings.json，可能还要建目录
    std::lock_guard<std::mutex> lk(g_cfgMutex);
    if (g_cfgGen != gen) return b;  // 期间切过工作区：这次照用，不装回缓存
    if (!g_baseOk) {
        g_base = b;
        g_baseOk = true;
    }
    return g_base;
}

void InvalidateConfigCache() {
    std::lock_guard<std::mutex> lk(g_cfgMutex);
    g_cfgGen++;
    g_answers = J();
    g_answersOk = false;
    g_answersStamp.clear();
    g_base.clear();
    g_baseOk = false;
    g_data.clear();
    g_dataOk = false;
}

std::wstring Root() {
    // 安装版 tools/pack 跟 exe 同级；开发版 exe 在 bin（或 win32/build）里。
    // 旧快照可能只有 exe，最后才借用已配置工作区里的资源。
    // 判据是 tools\components.json，不只是名字叫 tools 的目录：开发树里的
    // win32\tools 放的是调试脚本，把它当安装目录会让组件清单读空。
    auto hasTools = [](const std::wstring& dir) {
        return DirExists(JoinPath(dir, L"tools")) && FileExists(JoinPath(JoinPath(dir, L"tools"), L"components.json"));
    };
    const std::wstring exe = ExeDir();
    if (hasTools(exe)) return exe;
    const std::wstring parent = Up(exe);
    const std::wstring leaf = Lower(exe.substr(parent.size() + 1));
    if (leaf == L"bin" && hasTools(parent)) return parent;
    if (leaf == L"build" && hasTools(Up(parent))) return Up(parent);
    for (const std::wstring& dir : {WorkspaceDir(), Base()})
        if (hasTools(dir)) return dir;
    return exe;  // 缺资源时也指向实际安装目录，不再猜安装目录的上一级。
}

std::wstring P(const wchar_t* rel) { return JoinPath(Base(), rel); }
// 组件目录：新布局 <工作区>\bots\<name> 优先，老布局 <Base>\<原名> 兜底。
// 都没有时返回新位置——报错、安装、打开都指着它，用户能看懂该往哪放。
std::wstring BotPath(const wchar_t* name) {
    std::wstring inBots = BotDir(name);
    if (DirExists(inBots)) return inBots;
    struct Alias {
        const wchar_t* want;
        const wchar_t* old1;
        const wchar_t* old2;
    };
    static const Alias kAlias[] = {
        {L"AstrBot", L"astrbot", L"AstrBot"},
        {BRAND_HERMES_DIR, BRAND_HERMES_DIR, BRAND_HERMES},
        {L"SnowLuma", L"SnowLuma", L"snowluma"},
        {L"NapCat", L"NapCat", L"napcat"},
        {L"Breeze", L"breeze-tts", L"Breeze"},
        {L"Ollama", L"Ollama", L"ollama"},
        {L"uv", L"uv", L""},
        {BRAND_RIFT_DIR, BRAND_RIFT_DIR, BRAND_RIFT},
    };
    for (auto& a : kAlias) {
        if (_wcsicmp(name, a.want) != 0) continue;
        std::wstring c1 = P(a.old1);
        if (DirExists(c1)) return c1;
        if (a.old2[0]) {
            std::wstring c2 = P(a.old2);
            if (DirExists(c2)) return c2;
        }
        break;
    }
    return inBots;
}

// 组件启动脚本：bots 布局看组件目录里的 start.cmd；老布局看 <Base>\start-xxx.cmd。
std::wstring ComponentStartCmd(const wchar_t* name, const wchar_t* baseCmd) {
    std::wstring dir = BotPath(name);
    std::wstring c = JoinPath(dir, L"start.cmd");
    if (FileExists(c)) return c;
    // 早期 bots 布局里本机语音的脚本叫 start-breeze.cmd
    c = JoinPath(dir, L"start-breeze.cmd");
    if (FileExists(c)) return c;
    if (baseCmd && baseCmd[0]) {
        c = JoinPath(Base(), baseCmd);
        if (FileExists(c)) return c;
    }
    return std::wstring();
}


namespace {
// 真正去探 data 目录（会扫盘）；缓存和锁在下面的 Data() 里。
std::wstring ComputeData() {
    std::wstring d;
    // 新布局：组件在 <工作区>\bots\AstrBot；老布局在 <Base>\astrbot。
    std::wstring ba = BotPath(L"AstrBot");
    std::wstring cands[] = {JoinPath(ba, L"data"), JoinPath(ba, L"core\\data"),
                            JoinPath(Base(), L"astrbot\\data")};
    for (auto& c : cands) {
        if (DirExists(c)) {
            d = c;
            return d;
        }
    }
    // 不是 CLI 安装的话，实例目录一般叫 AstrBot*，找一个带 data 的（老位置和 bots 下都看）
    for (const std::wstring& root : {Base(), BotRoot()}) {
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW(JoinPath(root, L"AstrBot*").c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) continue;
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
            std::wstring cand = JoinPath(JoinPath(root, fd.cFileName), L"core\\data");
            if (!DirExists(cand)) cand = JoinPath(JoinPath(root, fd.cFileName), L"data");
            if (DirExists(cand)) {
                d = cand;
                break;
            }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
        if (!d.empty()) return d;
    }
    d = cands[2];
    return d;
}

}  // namespace

std::wstring Data() {
    unsigned gen = 0;
    {
        std::lock_guard<std::mutex> lk(g_cfgMutex);
        if (g_dataOk) return g_data;
        gen = g_cfgGen;
    }
    const std::wstring d = ComputeData();  // 锁外扫盘
    std::lock_guard<std::mutex> lk(g_cfgMutex);
    if (g_cfgGen != gen) return d;
    if (!g_dataOk) {
        g_data = d;
        g_dataOk = true;
    }
    return g_data;
}

std::wstring Answers(const wchar_t* key, const wchar_t* def) {
    const J j = AnswersJson();  // 缓存是共享的，这里拿一份快照再取值
    const J* v = j.Get(key);
    if (!v) return def;
    if (v->t == J::STR) return v->s;
    if (v->t == J::NUM) return v->NumText();
    if (v->t == J::BOOL) return v->b ? L"1" : L"0";
    return def;
}

int Voice() { return _wtoi(Answers(L"voice", L"0").c_str()); }
int Memory() { return _wtoi(Answers(L"memory", L"0").c_str()); }
int Video() { return _wtoi(Answers(L"video", L"0").c_str()); }

// ---------------------------------------------------------------- 功能页的覆盖文件
// 这三个文件跟 answers.json 分开存：answers.json 是安装时答的题，用户后来在界面上的
// 改动不该把那份「安装记录」改掉，复查起来也对不上。
static std::wstring PrivateDir() { return JoinPath(Base(), L"private"); }

static void EnsurePrivateDir() {
    std::wstring d = PrivateDir();
    if (!DirExists(d)) CreateDirectoryW(d.c_str(), nullptr);
}

bool FeatureOn(const std::wstring& name, bool def) {
    std::wstring f = JoinPath(PrivateDir(), L"features.txt");
    for (auto& raw : SplitLines(ReadTextFile(f, 65536))) {
        std::wstring line = Trim(raw);
        if (!line.empty() && line[0] == 0xFEFF) line = Trim(line.substr(1));
        if (line.empty() || line[0] == L'#') continue;
        size_t c = line.find(L'=');
        if (c == std::wstring::npos) continue;
        if (Trim(line.substr(0, c)) != name) continue;
        std::wstring v = Trim(line.substr(c + 1));
        for (auto& ch : v) ch = (wchar_t)towlower(ch);
        return v == L"1" || v == L"true" || v == L"on" || v == L"开";
    }
    return def;
}

int ModeNum(const std::wstring& key, int def) {
    std::wstring f = JoinPath(PrivateDir(), key + L"-mode.txt");
    if (FileExists(f)) {
        std::wstring t = Trim(ReadTextFile(f, 64));
        if (!t.empty()) return _wtoi(t.c_str());
    }
    std::wstring a = Answers(key.c_str(), L"");
    if (!a.empty()) return _wtoi(a.c_str());
    return def;
}

// 一行一个 key=value；有就换、没有就加，别的行原样保留。
static void UpsertKvFile(const std::wstring& path, const std::wstring& key, const std::wstring& val) {
    std::wstring out;
    bool hit = false;
    for (auto& raw : SplitLines(ReadTextFile(path, 65536))) {
        std::wstring line = Trim(raw);
        size_t c = line.find(L'=');
        if (c != std::wstring::npos && Trim(line.substr(0, c)) == key) {
            out += key + L"=" + val + L"\r\n";
            hit = true;
        } else if (!line.empty()) {
            out += raw + L"\r\n";
        }
    }
    if (!hit) out += key + L"=" + val + L"\r\n";
    WriteTextFile(path, out);
}

ActionResult SetFeature(const std::wstring& arg) {
    std::vector<std::wstring> p;
    {
        std::wstring cur;
        for (wchar_t c : arg) {
            if (c == L'/') { p.push_back(cur); cur.clear(); }
            else cur.push_back(c);
        }
        p.push_back(cur);
    }
    if (p.empty()) return {false, L"没说要改什么"};
    EnsurePrivateDir();
    if (p[0] == L"protocol" && p.size() == 2) {
        if (p[1] != BRAND_HERMES && p[1] != L"snowluma" && p[1] != L"napcat") return {false, L"不认识的 QQ 接入端"};
        if (Listening(HermesPort()) || Listening(5099) || Listening(6099)) return {false, L"先停止当前 QQ 接入端，再切换，避免两边争用同一个账号。"};
        if (!WriteTextFile(JoinPath(PrivateDir(), L"protocol.txt"), p[1] + L"\r\n")) return {false, L"接入端选择没保存，请检查工作区是否可写。"};
        WriteTextFile(JoinPath(PrivateDir(), L"tide-default-migrated.txt"), L"1\r\n");
        return {true, L"接入端已保存。启动 QQ 程序后生效；已有启动项可点「修开机自启」更新。"};
    }
    if (p[0] == L"plugin" && p.size() >= 3) {
        const std::wstring& name = p[1];
        bool on = p[2] == L"1" || p[2] == L"on";
        UpsertKvFile(JoinPath(PrivateDir(), L"features.txt"), name, on ? L"1" : L"0");
        return {true, on ? L"开了 " + name + L"（跑完自设定就生效）" : L"关了 " + name + L"（跑完自设定就生效）"};
    }
    if (p[0] == L"memory" && p.size() >= 2) {
        WriteTextFile(JoinPath(PrivateDir(), L"memory-mode.txt"), p[1]);
        return {true, L"记忆改成 " + p[1] + L" 了（跑完自设定就生效）"};
    }
    if (p[0] == L"voice" && p.size() >= 2) {
        WriteTextFile(JoinPath(PrivateDir(), L"voice-mode.txt"), p[1]);
        // 选豆包/在线 Breeze 就等于要用那两个插件，顺手把开关也打开
        if (p[1] == L"3") UpsertKvFile(JoinPath(PrivateDir(), L"features.txt"), L"astrbot_plugin_seed_audio_tts", L"1");
        if (p[1] == L"5") UpsertKvFile(JoinPath(PrivateDir(), L"features.txt"), L"astrbot_plugin_breeze_online_tts", L"1");
        return {true, L"语音路线改成 " + p[1] + L" 了（跑完自设定就生效）"};
    }
    return {false, L"不认识的功能设置：" + arg};
}

bool IsCli() {
    return FileExists(JoinPath(Base(), L"start-astrbot.cmd")) ||
           FileExists(JoinPath(BotPath(L"AstrBot"), L"start.cmd"));
}

std::wstring Protocol() {
    std::wstring p = Lower(Trim(ReadTextFile(SecretFile(L"protocol.txt"), 4096)));
    // 未跑组件预配置的旧安装也按新默认走；新界面明确选择回退时会写迁移标记。
    // 这里只决定有效端，不在只读查询中改文件；安装预配置负责持久化一次性迁移。
    if (p == L"snowluma" && !FileExists(JoinPath(PrivateDir(), L"tide-default-migrated.txt")) &&
        (FileExists(JoinPath(BotPath(BRAND_HERMES_DIR), L"start.cmd")) || FileExists(JoinPath(Base(), L"start-hermes.cmd"))))
        return BRAND_HERMES;
    if (p == BRAND_HERMES || p == L"snowluma" || p == L"napcat") return p;
    // 潮信是现在的默认 QQ 接入端：老布局 <Base>\start-hermes.cmd，
    // bots 布局是 <组件目录>\start.cmd
    if (FileExists(JoinPath(Base(), L"start-hermes.cmd"))) return BRAND_HERMES;
    if (FileExists(JoinPath(BotPath(BRAND_HERMES_DIR), L"start.cmd"))) return BRAND_HERMES;
    if (FileExists(JoinPath(Base(), L"start-napcat.cmd"))) return L"napcat";
    if (FileExists(JoinPath(BotPath(L"NapCat"), L"start.cmd")) ||
        FileExists(JoinPath(BotPath(L"NapCat"), L"start-napcat.cmd")))
        return L"napcat";
    // 旧 SnowLuma 只留作回退：没装潮信、它还在的老机器按老样子认
    if (FileExists(JoinPath(Base(), L"start-snowluma.cmd"))) return L"snowluma";
    if (FileExists(JoinPath(BotPath(L"SnowLuma"), L"start.cmd")) ||
        FileExists(JoinPath(BotPath(L"SnowLuma"), L"start-snowluma.cmd")))
        return L"snowluma";
    return BRAND_HERMES;
}

int BackendPort() {
    std::wstring url = Answers(L"base_url", L"http://127.0.0.1:6185");
    size_t c = url.find_last_of(L':');
    if (c == std::wstring::npos) return 6185;
    int p = _wtoi(url.c_str() + c + 1);
    return p > 0 ? p : 6185;
}

// ---------------------------------------------------------------- 文本小工具
std::wstring ReadKv(const std::wstring& path, const std::wstring& key) {
    std::wstring txt = ReadTextFile(path, 65536);
    std::wstring want = Lower(key);
    for (auto& raw : SplitLines(txt)) {
        std::wstring line = Trim(raw);
        if (!line.empty() && line[0] == 0xFEFF) line = Trim(line.substr(1));  // 记事本存的 UTF-8 BOM
        if (line.empty() || line[0] == L'#') continue;
        size_t c = line.find_first_of(L":：");
        if (c == std::wstring::npos) continue;
        std::wstring k = Trim(line.substr(0, c));
        if (Lower(k) == want) return Trim(line.substr(c + 1));
    }
    return L"";
}

std::wstring MaskQQ(const std::wstring& qq) {
    std::wstring t = Trim(qq);
    if (t.size() < 5) return t;
    for (wchar_t c : t)
        if (c < L'0' || c > L'9') return t;
    return t.substr(0, 2) + L"****" + t.substr(t.size() - 2);
}

std::wstring PrettyDur(long long sec) {
    if (sec < 0) sec = 0;
    long long d = sec / 86400, h = (sec % 86400) / 3600, m = (sec % 3600) / 60;
    if (d > 0) return std::to_wstring(d) + L" 天 " + std::to_wstring(h) + L" 小时";
    if (h > 0) return std::to_wstring(h) + L" 小时 " + std::to_wstring(m) + L" 分";
    if (m > 0) return std::to_wstring(m) + L" 分钟";
    return std::to_wstring(sec) + L" 秒";
}

std::wstring NowLocal(const wchar_t* fmt) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    struct tm t {};
    t.tm_year = st.wYear - 1900;
    t.tm_mon = st.wMonth - 1;
    t.tm_mday = st.wDay;
    t.tm_hour = st.wHour;
    t.tm_min = st.wMinute;
    t.tm_sec = st.wSecond;
    wchar_t buf[128];
    if (!wcsftime(buf, 128, fmt, &t)) return std::wstring();
    return buf;
}

long long BootUnix() {
    long long now = (long long)time(nullptr);
    long long up = (long long)(GetTickCount64() / 1000);
    return now - up;
}

// ---------------------------------------------------------------- json 文件
J ReadJson(const std::wstring& path, bool* ok) {
    std::wstring txt = ReadTextFile(path);
    if (!txt.empty() && txt[0] == 0xFEFF) txt = txt.substr(1);  // AstrBot 的 json 常带 BOM
    if (txt.empty()) {
        if (ok) *ok = false;
        return J();
    }
    bool good = false;
    J j = J::Parse(txt, &good);
    if (ok) *ok = good;
    return j;
}

bool WriteJsonAtomic(const std::wstring& path, const J& v) {
    std::wstring tmp = path + L".tmp";
    if (!WriteTextFile(tmp, v.Dump(2))) return false;
    if (MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
    // 目标被占用（astrbot 正读着）就退一步：直接覆盖
    return WriteTextFile(path, v.Dump(2));
}

J CmdConfig() { return ReadJson(JoinPath(Data(), L"cmd_config.json")); }

bool CmdConfigSave(const J& cfg) { return WriteJsonAtomic(JoinPath(Data(), L"cmd_config.json"), cfg); }

bool WhitelistOn() {
    // 以前是一行 const J* v = CmdConfig().Path(...)：CmdConfig() 返回的是临时 J，
    // 那条语句一结束它就被析构了，v 立刻变悬垂——并发切页时会随机 0xc0000005。
    // 先把配置接住，再取指针。
    J cfg = CmdConfig();
    const J* v = cfg.Path(L"platform_settings.enable_id_white_list");
    return v && v->t == J::BOOL ? v->b : false;
}

// ---------------------------------------------------------------- DeepSeek 余额
static std::wstring DeepSeekKey() {
    std::wstring path = SecretFile(L"deepseek-key.txt");
    std::wstring k = ReadKv(path, L"key");
    if (k.empty()) k = ReadKv(path, L"deepseek_key");
    if (k.empty()) k = ReadKv(path, L"api_key");
    if (!k.empty()) return k;
    for (auto& raw : SplitLines(ReadTextFile(path, 4096))) {
        std::wstring t = Trim(raw);
        if (!t.empty() && t[0] != L'#') return t;
    }
    return L"";
}

Balance DeepSeekBalance(bool force) {
    Balance b;
    std::wstring key = DeepSeekKey();
    if (key.empty()) {
        b.state = L"nokey";
        return b;
    }
    b.has = true;
    std::wstring cachePath = JoinPath(JoinPath(Root(), L"logs"), L"cache.json");
    J cache = ReadJson(cachePath);
    const J* old = cache.Get(L"balance");
    long long now = (long long)time(nullptr);
    if (old && old->t == J::OBJ) {
        b.at = old->Int(L"at");
        b.state = old->Str(L"state", L"net");
        b.amount = old->Num(L"amount");
        b.currency = old->Str(L"currency", L"CNY");
        long long ttl = (b.state == L"ok") ? 600 : 120;
        if (!force && b.at > 0 && now - b.at < ttl) return b;
    }

    Http r = HttpGetJson(L"https://api.deepseek.com/user/balance", key, 8000);
    if (r.code == 200) {
        J j = J::Parse(r.body);
        const J* infos = j.Get(L"balance_infos");
        if (infos && infos->t == J::ARR && !infos->a.empty()) {
            const J& i0 = infos->a[0];
            b.state = L"ok";
            std::wstring amt = i0.Str(L"total_balance");
            if (amt.empty()) amt = i0.Get(L"total_balance") ? i0.Get(L"total_balance")->NumText() : L"0";
            b.amount = _wtof(amt.c_str());
            b.currency = i0.Str(L"currency", L"CNY");
            const J* av = j.Get(L"is_available");
            if (av && av->t == J::BOOL && !av->b) b.state = L"empty";
        } else {
            b.state = L"http";
        }
    } else if (r.code == 401 || r.code == 403) {
        b.state = L"auth";
    } else if (r.code == 0) {
        b.state = L"net";
    } else {
        b.state = L"http";
    }
    b.at = now;

    // 写回缓存：别把 updates 那部分冲掉
    J bal = JObj();
    bal.Set(L"state", JStr(b.state));
    bal.Set(L"amount", JNum(b.amount));
    bal.Set(L"currency", JStr(b.currency));
    bal.Set(L"at", JNum((double)b.at));
    cache.Set(L"balance", bal);
    J day = JObj();
    day.Set(L"date", JStr(NowLocal(L"%Y-%m-%d")));
    day.Set(L"amount", JNum(b.amount));
    day.Set(L"at", JNum((double)b.at));
    cache.Set(L"balance_day", day);
    WriteJsonAtomic(cachePath, cache);
    return b;
}

// ---------------------------------------------------------------- 长期记忆接线
Wiring MemoryWiring() {
    Wiring w;
    std::wstring pluginDir = JoinPath(Data(), L"plugins\\astrbot_plugin_livingmemory");
    w.installed = DirExists(pluginDir);

    // preferences 里 inactivated_plugins 记着被停用的插件
    w.on = w.installed;
    {
        Sqlite db;
        if (db.OpenRead(JoinPath(Data(), L"data_v4.db"))) {
            std::vector<std::vector<std::wstring>> rows;
            if (db.QueryTexts(L"SELECT value FROM preferences WHERE key='inactivated_plugins'", rows) && !rows.empty()) {
                J v = J::Parse(rows[0][0]);
                const J* val = v.Get(L"val");
                const J* arr = val ? val : &v;
                if (arr->t == J::ARR) {
                    for (auto& it : arr->a) {
                        if (it.t != J::STR) continue;
                        if (it.s.find(L"livingmemory") != std::wstring::npos) w.on = false;
                    }
                }
            }
        }
    }

    // 它用哪个 embedding
    J lm = ReadJson(JoinPath(JoinPath(Data(), L"config"), L"astrbot_plugin_livingmemory_config.json"));
    const J* ps = lm.Get(L"provider_settings");
    if (ps) w.provider = ps->Str(L"embedding_provider_id");
    if (w.provider.empty()) w.provider = lm.Str(L"embedding_provider_id", L"ollama_embedding");
    w.model = L"bge-m3";

    J cfg = CmdConfig();
    const J* provs = cfg.Get(L"provider");
    if (provs && provs->t == J::ARR) {
        for (auto& p : provs->a) {
            if (p.Str(L"id") != w.provider) continue;
            w.base = p.Str(L"api_base");
            if (w.base.empty()) w.base = p.Str(L"base_url");
            std::wstring model = p.Str(L"model");
            if (!model.empty()) w.model = model;
            w.key = p.Str(L"api_key");
            std::wstring type = Lower(p.Str(L"type"));
            std::wstring base = Lower(w.base);
            w.ollama = type.find(L"ollama") != std::wstring::npos || base.find(L":11434") != std::wstring::npos;
            w.online = !w.ollama && (base.find(L"127.0.0.1") == std::wstring::npos &&
                                     base.find(L"localhost") == std::wstring::npos);
            break;
        }
    }
    if (w.base.empty()) {
        w.ollama = w.provider.find(L"ollama") != std::wstring::npos;
        if (w.ollama) w.base = L"http://127.0.0.1:11434";
    }
    return w;
}

// ---------------------------------------------------------------- 语音 / 看视频
std::wstring AsrState() {
    std::wstring dir = Answers(L"asr_dir", L"");
    if (dir.empty()) dir = P(L"asr");
    std::wstring model = JoinPath(dir, L"model.int8.onnx");
    bool core = FileExists(JoinPath(dir, L"sherpa-onnx-offline.exe")) && FileExists(model) &&
                FileExists(JoinPath(dir, L"tokens.txt"));
    auto sizeOf = [](const std::wstring& p) -> long long {
        WIN32_FILE_ATTRIBUTE_DATA d{};
        if (!GetFileAttributesExW(p.c_str(), GetFileExInfoStandard, &d)) return 0;
        return ((long long)d.nFileSizeHigh << 32) | d.nFileSizeLow;
    };
    if (core && sizeOf(model) > 200LL * 1024 * 1024) {
        bool plus = FileExists(JoinPath(dir, L"funasr-nano\\llm.int8.onnx"));
        return plus ? L"plus" : L"local";
    }
    J vid = ReadJson(JoinPath(JoinPath(Data(), L"config"), L"astrbot_plugin_dafeiyu_video_config.json"));
    if (!vid.Str(L"asr_key").empty()) return L"online";
    if (FileExists(model)) return L"half";
    return L"none";
}

bool LauncherAutostart() {
    HKEY k = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &k) !=
        ERROR_SUCCESS)
        return false;
    bool has = false;
    if (RegQueryValueExW(k, L"AstrBot Launcher", nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) has = true;
    RegCloseKey(k);
    if (!has) return false;
    // StartupApproved\Run 的二进制第一位：1 表示启用
    HKEY a = nullptr;
    bool enabled = true;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run", 0, KEY_READ, &a) ==
        ERROR_SUCCESS) {
        BYTE buf[64];
        DWORD len = sizeof(buf), type = 0;
        if (RegQueryValueExW(a, L"AstrBot Launcher", nullptr, &type, buf, &len) == ERROR_SUCCESS && len > 0)
            enabled = (buf[0] & 1) != 0;
        RegCloseKey(a);
    }
    return enabled;
}

// ---------------------------------------------------------------- 表情包
static int CountImages(const std::wstring& dir, int depth = 0) {
    if (depth > 4) return 0;
    int n = 0;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        std::wstring name = fd.cFileName;
        if (name == L"." || name == L"..") continue;
        std::wstring full = dir + L"\\" + name;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            n += CountImages(full, depth + 1);
            continue;
        }
        std::wstring low = Lower(name);
        if (low.size() > 4) {
            std::wstring ext = low.substr(low.size() - 4);
            std::wstring ext3 = low.size() > 5 ? low.substr(low.size() - 5) : L"";
            if (ext == L".png" || ext == L".jpg" || ext == L".gif" || ext == L".bmp" || ext3 == L".webp")
                n++;
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return n;
}

MemeInfo MemeState() {
    MemeInfo m;
    J pack = ReadJson(P(L"dafeiyu-config-pack\\settings\\pack.json"));
    const J* packs = pack.Get(L"meme_packs");
    if (!packs) packs = pack.Get(L"memes");
    if (packs && packs->t == J::ARR) m.expected = (int)packs->a.size();

    J reg = ReadJson(JoinPath(JoinPath(Data(), L"plugin_data\\meme_manager"), L"registry.json"));
    const J* inst = reg.Get(L"installed_packs");
    if (inst && inst->t == J::ARR) {
        for (auto& p : inst->a) {
            if (p.t != J::OBJ) continue;
            if (!p.Bool(L"enabled", true)) continue;
            m.installed++;
            std::wstring id = p.Str(L"id");
            if (!id.empty())
                m.pictures += CountImages(JoinPath(JoinPath(Data(), L"plugin_data\\meme_manager\\packs"), id));
        }
    }
    std::wstring imp = ImportLogState(L"memes");
    if (imp == L"running") m.note = L"正在装新的表情包…";
    else if (!imp.empty()) m.note = imp;
    return m;
}

std::wstring ImportLogState(const wchar_t* block) {
    std::wstring path = JoinPath(JoinPath(Base(), L"logs"), std::wstring(L"import-") + block + L".log");
    if (!FileExists(path)) return L"";
    std::wstring txt = ReadTextFile(path, 400000);
    if (txt.empty()) return L"";
    std::wstring result;
    for (auto& line : SplitLines(txt)) {
        std::wstring t = Trim(line);
        if (!StartsWith(t, L"RESULT")) continue;
        result = t;
    }
    bool running = result.empty();
    if (running) {
        WIN32_FILE_ATTRIBUTE_DATA d{};
        if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &d)) {
            ULARGE_INTEGER u;
            u.HighPart = d.ftLastWriteTime.dwHighDateTime;
            u.LowPart = d.ftLastWriteTime.dwLowDateTime;
            long long at = (long long)(u.QuadPart / 10000000ULL) - 11644473600LL;
            running = (long long)time(nullptr) - at < 180;
        }
    }
    if (running) return L"running";
    // RESULT ok=… reason=… next=…
    std::vector<std::wstring> f;
    std::wstring cur;
    for (wchar_t c : result) {
        if (c == L' ' || c == L'\t') {
            if (!cur.empty()) {
                f.push_back(cur);
                cur.clear();
            }
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) f.push_back(cur);
    std::wstring state, reason, next;
    for (auto& kv : f) {
        if (StartsWith(kv, L"ok=")) state = kv.substr(3);
        else if (StartsWith(kv, L"reason=")) reason = kv.substr(7);
        else if (StartsWith(kv, L"next=")) next = kv.substr(5);
    }
    if (state == L"ok") return L"上次装好了";
    if (state.empty()) return L"";
    return L"上次没装成" + (reason.empty() ? std::wstring() : (L"（" + reason + L"）"));
}

// ---------------------------------------------------------------- 掉线提醒 / 打游戏
WatchInfo Watch() {
    WatchInfo w;
    w.on = !FileExists(P(L"private\\qq-watch-off.txt"));
    w.can = true;
    long long now = (long long)time(nullptr);
    {   // ① python 老链路（tools\\qq_watch.py）
        J j = ReadJson(P(L"logs\\qq-watch.json"));
        w.at = j.Int(L"at");
        w.state = j.Str(L"state");
        w.note = j.Str(L"note");
        if (w.at > 0 && now - w.at < 600) w.running = true;
    }
    {   // ② 工作台自己的心跳（首选，不需要 python）
        J j = ReadJson(P(L"logs\\workbench-watch.json"));
        long long at = (long long)j.Int(L"at");
        if (at > 0 && now - at < 900) {
            w.running = true;
            w.at = at;
        }
    }
    if (w.on && !w.running) w.note = L"开关开着，但没人在查";
    return w;
}

GameInfo Game() {
    GameInfo g;
    J j = ReadJson(P(L"logs\\game-mode.json"));
    if (!j.Obj()) return g;
    g.on = j.Bool(L"on") && j.Int(L"since") >= BootUnix();
    g.since = j.Int(L"since");
    g.tts = j.Bool(L"tts");
    g.mem = j.Bool(L"mem");
    return g;
}

// ---------------------------------------------------------------- 版本
static std::wstring DistVer(const std::wstring& dir, const std::wstring& prefix) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(JoinPath(dir, prefix + L"*").c_str(), &fd);
    std::wstring found;
    if (h != INVALID_HANDLE_VALUE) {
        do {
            std::wstring n = fd.cFileName;
            if (n.size() <= prefix.size()) continue;
            std::wstring rest = n.substr(prefix.size());
            bool dist = rest.size() > 10 && rest.substr(rest.size() - 10) == L".dist-info";
            if (dist) rest = rest.substr(0, rest.size() - 10);
            while (!rest.empty() && (rest[0] == L'-' || rest[0] == L'_')) rest.erase(0, 1);
            bool hasDigit = false;
            for (wchar_t c : rest)
                if (c >= L'0' && c <= L'9') hasDigit = true;
            // site-packages 里常有同名的包目录（astrbot），要挑带版本号的 dist-info
            if (!dist || !hasDigit) continue;
            found = rest;
            break;
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    return found;
}

static std::wstring VerDigits(const std::wstring& s) {
    std::wstring out;
    bool any = false;
    for (size_t i = 0; i < s.size(); i++) {
        wchar_t c = s[i];
        if (c >= L'0' && c <= L'9') {
            out.push_back(c);
            any = true;
        } else if (c == L'.' && any) {
            out.push_back(c);
        } else if (any) {
            break;
        }
    }
    while (!out.empty() && out.back() == L'.') out.pop_back();
    return any ? out : std::wstring();
}

// 在 root 下面有限深度找一个文件名（SnowLuma\release.json、astrbot-*.dist-info 这类）
static std::wstring FindUnder(const std::wstring& root, const std::wstring& name, int depth) {
    if (depth < 0) return std::wstring();
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(JoinPath(root, L"*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return std::wstring();
    std::vector<std::wstring> dirs;
    std::wstring hit;
    do {
        std::wstring n = fd.cFileName;
        if (n == L"." || n == L"..") continue;
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && !(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
            std::wstring sub = JoinPath(root, n);
            // dist-info 是目录，版本藏在里面的 METADATA
            if (Lower(n).find(name) != std::wstring::npos && FileExists(JoinPath(sub, L"METADATA"))) {
                hit = JoinPath(sub, L"METADATA");
                break;
            }
            dirs.push_back(sub);
        }
        else if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && Lower(n).find(name) != std::wstring::npos) {
            hit = JoinPath(root, n);
            break;
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    if (!hit.empty()) return hit;
    for (auto& d : dirs) {
        hit = FindUnder(d, name, depth - 1);
        if (!hit.empty()) return hit;
    }
    return std::wstring();
}

std::wstring MingBridgeVersion() {
    // 装好的看工作区；没装（或开发树里没跑过安装）就看随包的 sidecar，别把「没装」当成没有版本。
    std::wstring dir = BotPath(BRAND_HERMES_DIR);
    std::wstring v = ReadJson(JoinPath(dir, L"package.json")).Str(L"version");
    if (v.empty()) v = Trim(ReadTextFile(JoinPath(dir, L"version.txt"), 4096));
    if (v.empty()) {
        std::wstring side = JoinPath(SidecarDir(), BRAND_HERMES);
        v = ReadJson(JoinPath(side, L"package.json")).Str(L"version");
        if (v.empty()) v = Trim(ReadTextFile(JoinPath(side, L"version.txt"), 4096));
    }
    return v;
}

std::vector<std::pair<std::wstring, std::wstring>> Versions() {
    std::vector<std::pair<std::wstring, std::wstring>> v;
    v.push_back({L"工作台", L"2.0"});
    std::wstring astr = DistVer(JoinPath(BotPath(L"uv"), L"tools\\astrbot\\Lib\\site-packages"), L"astrbot");
    if (astr.empty()) {
        std::wstring hit = FindUnder(Base(), L"astrbot-", 5);
        if (!hit.empty()) astr = DistVer(Up(hit), L"astrbot");
    }
    if (!astr.empty()) v.push_back({L"AstrBot", astr});
    // 潮信：版本写在 package.json（官方包）或 version.txt（绿色包）里。它随工作台内置更新，
    // 不从 GitHub 单独拉取。
    {
        std::wstring mbv = MingBridgeVersion();
        if (!mbv.empty()) v.push_back({BRAND_HERMES_DIR, mbv});
    }
    // 旧 SnowLuma 只用来显示「机器上还留着旧版」，不作为升级来源
    J sl = ReadJson(JoinPath(BotPath(L"SnowLuma"), L"release.json"));
    std::wstring slv = sl.Str(L"version");
    if (slv.empty()) {
        std::wstring hit = FindUnder(Base(), L"release.json", 3);
        if (!hit.empty()) slv = ReadJson(hit).Str(L"version");
    }
    if (!slv.empty())
        v.push_back({Protocol() == L"snowluma" ? L"SnowLuma（旧版）" : L"旧版 SnowLuma（未使用）", slv});
    // 冥零的安装 Skill：先看 tools\skill-version.json，没有就顺着 tools\skill-home.txt 指的目录找
    {
        std::wstring tools = JoinPath(Base(), L"tools");
        std::wstring sv = ReadJson(JoinPath(tools, L"skill-version.json")).Str(L"version");
        if (sv.empty()) {
            std::wstring home = Trim(ReadTextFile(JoinPath(tools, L"skill-home.txt"), 2048));
            if (!home.empty() && home[0] == 0xFEFF) home = Trim(home.substr(1));
            if (!home.empty()) sv = ReadJson(JoinPath(home, L"version.json")).Str(L"version");
        }
        if (!sv.empty()) v.push_back({L"技能", sv});
    }
    // Breeze：bin 里那个带 -v0.1.3 的目录
    {
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW(JoinPath(BotPath(L"Breeze"), L"bin\\*").c_str(), &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                std::wstring n = fd.cFileName;
                size_t vp = n.find(L"-v");
                if (vp == std::wstring::npos) continue;
                std::wstring ver;
                for (size_t i = vp + 2; i < n.size(); i++) {
                    wchar_t c = n[i];
                    if (c >= L'0' && c <= L'9') ver.push_back(c);
                    else if (c == L'.' && !ver.empty()) ver.push_back(c);
                    else break;
                }
                while (!ver.empty() && ver.back() == L'.') ver.pop_back();
                if (!ver.empty()) {
                    v.push_back({L"Breeze", ver});
                    break;
                }
            } while (FindNextFileW(h, &fd));
            FindClose(h);
        }
    }
    // Ollama：记忆用本机的时候才显示
    if (Memory() == 1 && Listening(11434)) {
        Http r = HttpGetJson(L"http://127.0.0.1:11434/api/version", L"", 4000);
        if (r.ok) {
            bool ok = false;
            J j = J::Parse(r.body, &ok);
            std::wstring ov = ok ? j.Str(L"version") : L"";
            if (!ov.empty()) v.push_back({L"Ollama", ov});
        }
    }
    return v;
}

}  // namespace her
