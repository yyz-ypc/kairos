#include "../brand.h"
// 潮信（Hermes）托管通道的实现。
// 线程约定：这里只碰网络、文件和本文件的缓存；绝不碰 g_*、GDI、WebView2。
// UI 线程通过 TideSnapshot() 读缓存，通过 TideAction()/TideRefreshAsync() 派后台活。
#include "her_tide.h"

#include <atomic>
#include <ctime>
#include <random>

namespace her {
namespace {

struct Cache {
    std::wstring token;
    bool managed = false;        // 控制通道验证通过过（token 有效）
    bool reachable = false;      // 端口通、接口有响应
    std::wstring error;
    std::wstring stateJson;      // /api/state 原文
    std::wstring logsJson;       // /api/logs 原文
    std::vector<std::pair<std::wstring, std::wstring>> messages;  // uin → 消息数组原文
    std::wstring opMsg;
    bool opOk = true;
    DWORD lastRefresh = 0;
    DWORD lastOk = 0;
};

std::mutex g_mu;
Cache g_c;
std::atomic<bool> g_refreshing{false};
std::atomic<bool> g_starting{false};
std::atomic<bool> g_acting{false};
std::atomic<DWORD> g_ownedPid{0};

// 端口可被环境变量覆盖：自动化测试要开一套独立的潮信，不和用户正在跑的那套抢 5109。
int TidePort() { return HermesPort(); }
std::wstring TidePortText() { return std::to_wstring(TidePort()); }

std::wstring Hex64() {
    std::random_device rd;
    std::uniform_int_distribution<int> d(0, 15);
    static const wchar_t* H = L"0123456789abcdef";
    std::wstring s;
    s.reserve(64);
    for (int i = 0; i < 64; ++i) s.push_back(H[d(rd)]);
    return s;
}

bool ValidUin(const std::wstring& uin) {
    if (uin.size() < 4 || uin.size() > 12) return false;
    for (wchar_t c : uin)
        if (c < L'0' || c > L'9') return false;
    return true;
}

void SetOp(bool ok, const std::wstring& msg) {
    std::lock_guard<std::mutex> lk(g_mu);
    g_c.opOk = ok;
    g_c.opMsg = msg;
}

// 把潮信返回的错误变成一句人话
std::wstring ReplyError(const TideReply& r, const wchar_t* what) {
    if (r.code == 0) return std::wstring(what) + L"：连不上潮信（127.0.0.1:" + TidePortText() + L"）。";
    bool parsed = false;
    J j = J::Parse(r.body, &parsed);
    if (parsed && j.Obj()) {
        std::wstring e = j.Str(L"error");
        if (!e.empty()) return e;
    }
    return std::wstring(what) + L"失败（HTTP " + std::to_wstring(r.code) + L"）。";
}

// 只给本机回环地址用：拒绝一切非 127.0.0.1 的地址，防止以后 path 被拼坏
std::wstring TideUrl(const std::wstring& path) { return L"http://127.0.0.1:" + std::to_wstring(TidePort()) + path; }

void RefreshOnce() {
    Cache fresh;
    {
        std::lock_guard<std::mutex> lk(g_mu);
        fresh.token = g_c.token;
    }
    bool portUp = Listening(TidePort());

    TideReply st = TideApi(L"GET", L"/api/state", L"", 3500);
    fresh.reachable = st.code != 0;
    if (st.code == 200) {
        fresh.managed = true;
        fresh.stateJson = st.body;
        bool okp = false;
        J j = J::Parse(st.body, &okp);
        if (okp && j.Obj()) {
            const J* acc = j.Get(L"accounts");
            if (acc && acc->Arr()) {
                for (auto& a : acc->a) {
                    std::wstring uin = a.Str(L"uin");
                    if (!ValidUin(uin)) continue;
                    TideReply m = TideApi(L"GET", L"/api/accounts/" + uin + L"/messages", L"", 3500);
                    if (m.code == 200) fresh.messages.emplace_back(uin, m.body);
                }
            }
        }
        TideReply logs = TideApi(L"GET", L"/api/logs", L"", 3500);
        if (logs.code == 200) fresh.logsJson = logs.body;
    } else if (st.code == 401 || st.code == 403) {
        // 端口通，但没有控制通道：潮信是用户自己（或别的壳）启动的
        fresh.managed = false;
        fresh.error = L"潮信不是由" BRAND_NAME L"启动的，管理页面拿不到控制通道。想在这里管它，点「重启潮信」。";
    } else if (!portUp) {
        fresh.error.clear();
    } else {
        fresh.error = L"潮信在运行，但接口没有响应（可能正在启动）。";
    }

    {
        std::lock_guard<std::mutex> lk(g_mu);
        DWORD keepRefresh = GetTickCount();
        std::wstring keepOp = g_c.opMsg;
        bool keepOpOk = g_c.opOk;
        DWORD keepOk = g_c.lastOk;
        g_c = std::move(fresh);
        g_c.lastRefresh = keepRefresh;
        g_c.opMsg = keepOp;
        g_c.opOk = keepOpOk;
        g_c.lastOk = st.code == 200 ? keepRefresh : keepOk;
    }
}

}  // namespace

const std::wstring& TideToken() {
    static std::wstring token;
    static std::once_flag once;
    std::call_once(once, [] { token = Hex64(); });
    return token;
}

bool TideManaged() {
    std::lock_guard<std::mutex> lk(g_mu);
    return !g_c.token.empty();
}

TideReply TideApi(const wchar_t* method, const std::wstring& path, const std::wstring& body, int timeoutMs) {
    TideReply r;
    if (path.empty() || path[0] != L'/') {
        r.err = L"内部错误：接口路径不合法。";
        return r;
    }
    std::wstring tok;
    {
        std::lock_guard<std::mutex> lk(g_mu);
        tok = g_c.token;
    }
    std::vector<std::pair<std::wstring, std::wstring>> h;
    h.push_back({L"Accept", L"application/json"});
    if (!tok.empty()) h.push_back({BRAND_HERMES_TOKEN, tok});
    Http http = HttpCall(method, TideUrl(path), body, h, timeoutMs);
    r.code = http.code;
    r.body = http.body;
    r.ok = http.code >= 200 && http.code < 300;
    if (!r.ok) r.err = ReplyError(r, L"潮信接口");
    return r;
}

// 随包潮信必须完整：少一个文件就当没装到，交给修复/重装，不拿残缺副本硬起。
static bool TideTreeOk(const std::wstring& dir) {
    if (dir.empty()) return false;
    static const wchar_t* need[] = {
        L"start.cmd", L"src\\main.mjs", L"vendor\\ws\\wrapper.mjs",
        L"vendor\\silk-wasm\\silk.wasm", L"native\\build\\hermes-qq-hook.dll",
        L"native\\build\\hermes-qq-launcher.exe"};
    for (const wchar_t* rel : need)
        if (!FileExists(JoinPath(dir, rel))) return false;
    return true;
}

static std::wstring TideBundle() { return JoinPath(SidecarDir(), BRAND_HERMES); }

// 路径规则必须和 tools/bootstrap.ps1 的 verify 探针一致：
// 1) 安装目录里的 sidecar\\hermes（随包）完整就用它——老 bots 副本不能遮住新版本；
// 2) 随包不完整、老 bots\\Hermes 完整，才走旧布局兜底；
// 3) 都不完整，返回随包路径，让报错明确指向随包文件。
static std::wstring TideRoot() {
    std::wstring side = TideBundle();
    if (TideTreeOk(side)) return side;
    std::wstring legacy = BotPath(BRAND_HERMES_DIR);
    if (TideTreeOk(legacy)) return legacy;
    return side;
}

// 潮信已经不再有访问密码：老装机把首密码明文写在 secrets\ 或 private\ 下，
// 那两个文件现在没有任何人读，留着只是把密码摊在磁盘上。拉起潮信时顺手收掉，
// 删不掉（只读、被占用、目录不存在）也不影响启动。
void TideForgetLegacySecret() {
    DeleteFileW(P(L"secrets\\hermes-webui.txt").c_str());
    DeleteFileW(P(L"private\\hermes-webui.txt").c_str());
}

DWORD TideLaunch(std::wstring* errOut) {
    std::wstring root = TideRoot();
    std::wstring cmd = JoinPath(root, L"start.cmd");
    if (!TideTreeOk(root)) {
        if (errOut) *errOut = L"随包的潮信文件不完整（sidecar\\hermes）。请点「修复」重试，或重装时序之子。";
        return 0;
    }
    if (Listening(TidePort())) { if (errOut) *errOut = L"端口已被占用，未接管其他程序；请使用独立端口。"; return 0; }
    TideForgetLegacySecret();
    std::wstring token = TideToken();
    {
        std::lock_guard<std::mutex> lk(g_mu);
        g_c.token = token;
        g_c.error.clear();
    }
    EnvList env;
    env.emplace_back(BRAND_ENV_HERMES_HOST_TOKEN, token);
    env.emplace_back(BRAND_ENV_HERMES_PORT, TidePortText());
    env.emplace_back(BRAND_ENV_HERMES_HOME, JoinPath(StateDir(), BRAND_HERMES));
    env.emplace_back(BRAND_ENV_KAIROS_SIDECAR, SidecarDir());
    // 潮信用随包的 Node（sidecar\\node），别让系统里更旧的 node.exe 抢先。
    std::wstring node = JoinPath(JoinPath(SidecarDir(), L"node"), L"node.exe");
    if (FileExists(node)) env.emplace_back(L"HERMES_NODE", node);
    DWORD pid = LaunchExe(L"cmd.exe", L"/d /c \"\"" + cmd + L"\"\"", root, env, true);
    if (!pid) {
        if (errOut) *errOut = L"潮信启动失败（CreateProcess 被拒）。";
        { std::lock_guard<std::mutex> lk(g_mu); g_c.token.clear(); }
        return 0;
    }
    g_ownedPid = pid;
    if (!WaitFor([] { return Listening(TidePort()); }, 25)) {
        if (errOut) *errOut = L"潮信起来了，但 25 秒内没监听 " + TidePortText() + L"（看 logs\\hermes）。";
        return 0;
    }
    TideRefreshAsync(true);
    return pid;
}

bool TideStop(std::wstring* errOut) {
    std::wstring root = TideRoot();
    if (root.empty() || !DirExists(root)) {
        if (errOut) *errOut = L"找不到潮信目录。";
        return false;
    }
    DWORD owned = g_ownedPid.exchange(0);
    int killed = owned && KillTree(owned) ? 1 : 0;
    {
        std::lock_guard<std::mutex> lk(g_mu);
        g_c.token.clear();
        g_c.managed = false;
        g_c.reachable = false;
        g_c.stateJson.clear();
        g_c.messages.clear();
        g_c.logsJson.clear();
    }
    if (!killed) {
        if (errOut) *errOut = L"潮信本来就没在跑。";
        return false;
    }
    return true;
}

void TideRefreshAsync(bool force) {
    DWORD now = GetTickCount();
    {
        std::lock_guard<std::mutex> lk(g_mu);
        if (!force && now - g_c.lastRefresh < 1500) return;
        g_c.lastRefresh = now;
    }
    if (g_refreshing.exchange(true)) return;
    std::thread([] {
        RefreshOnce();
        g_refreshing = false;
    }).detach();
}

bool TideBusy() { return g_refreshing.load() || g_acting.load(); }

bool TideInstalled() { return TideTreeOk(TideRoot()); }

// 三处界面（向导/设置/体检）共用这一个语义：已随包、正在初始化、已就绪。
// 判断只走本地文件和可写目录，不碰网络，也不代表 QQ 账号已登录。
std::wstring TideSetupState() {
    if (!TideTreeOk(TideRoot())) return L"missing";
    std::wstring cfg = JoinPath(JoinPath(StateDir(), BRAND_HERMES), L"config\\onebot.json");
    if (!FileExists(cfg)) return L"initializing";
    return L"ready";
}

// 开机就替用户把潮信拉起来：只在端口空着且脚本存在时动手。
// 已经有一套在跑（用户的、或上次留下的）绝不杀——那不是我们的进程，先连上去看能不能托管。
void TideAutoStart() {
    static std::atomic<bool> once{false};
    if (once.exchange(true)) return;
    std::thread([] {
        if (!TideInstalled() || Listening(TidePort())) {
            TideRefreshAsync(true);
            return;
        }
        TideAction(L"start", JObj());
    }).detach();
}

std::wstring TideSnapshot() {
    bool portUp = Listening(TidePort());
    std::lock_guard<std::mutex> lk(g_mu);
    J s = JObj();
    s.Set(L"port", HermesPort());
    s.Set(L"reachable", g_c.reachable && portUp);
    s.Set(L"managed", g_c.managed);
    s.Set(L"error", g_c.error);
    s.Set(L"opMsg", g_c.opMsg);
    s.Set(L"opOk", g_c.opOk);
    s.Set(L"busy", g_refreshing.load() || g_acting.load());
    s.Set(L"starting", g_starting.load());
    s.Set(L"installed", TideInstalled());
    s.Set(L"bundled", TideTreeOk(TideBundle()));
    s.Set(L"legacy", !TideTreeOk(TideBundle()) && TideTreeOk(BotPath(BRAND_HERMES_DIR)));
    s.Set(L"home", JoinPath(StateDir(), BRAND_HERMES));
    s.Set(L"setup", TideSetupState());
    if (!g_c.stateJson.empty()) {
        bool ok = false;
        J st = J::Parse(g_c.stateJson, &ok);
        if (ok) s.Set(L"state", st);
    }
    if (!g_c.logsJson.empty()) {
        bool ok = false;
        J logs = J::Parse(g_c.logsJson, &ok);
        if (ok) s.Set(L"logs", logs);
    }
    J msg = JObj();
    for (auto& kv : g_c.messages) {
        bool ok = false;
        J rows = J::Parse(kv.second, &ok);
        if (ok) msg.Set(kv.first, rows);
    }
    s.Set(L"messages", msg);
    return s.Dump();
}

bool TideKnownOp(const std::wstring& op) {
    static const wchar_t* ops[] = {L"refresh", L"start",    L"stop",     L"disconnect", L"send",
                                   L"settings", L"native_detect", L"native_start",
                                   L"native_stop", L"native_takeover", L"restart"};
    for (auto o : ops)
        if (op == o) return true;
    return false;
}

void TideAction(const std::wstring& op, const J& args) {
    if (!TideKnownOp(op)) {
        SetOp(false, L"不认识的动作：" + op);
        return;
    }
    if (g_acting.exchange(true)) {
        SetOp(false, L"上一项潮信操作还没结束。");
        return;
    }
    std::thread([op, args] {
        std::wstring msg;
        bool ok = true;
        auto fail = [&](const TideReply& r, const wchar_t* what) {
            ok = false;
            msg = ReplyError(r, what);
        };
        if (op == L"refresh") {
            ok = true;
        } else if (op == L"start" || op == L"restart") {
            std::wstring err;
            g_starting = true;
            if (op == L"restart") {   // 已经在跑、但控制通道不是我们的：先收掉再带 token 起
                TideStop(nullptr);
                for (int i = 0; i < 40 && Listening(TidePort()); ++i) ::Sleep(250);
            }
            DWORD pid = TideLaunch(&err);
            ok = pid != 0;
            msg = ok ? L"潮信已启动。" : err;
            g_starting = false;
        } else if (op == L"stop") {
            std::wstring err;
            ok = TideStop(&err);
            msg = ok ? L"潮信已停止。" : err;
        } else if (op == L"disconnect") {
            std::wstring uin = args.Str(L"uin");
            if (!ValidUin(uin)) { ok = false; msg = L"账号不正确。"; }
            else {
                TideReply r = TideApi(L"POST", L"/api/accounts/" + uin + L"/disconnect", L"{}");
                if (!r.ok) fail(r, L"断开账号");
                else msg = L"已断开 " + uin + L"。";
            }
        } else if (op == L"send") {
            std::wstring uin = args.Str(L"uin"), text = args.Str(L"text"), kind = args.Str(L"kind", L"private");
            std::wstring target = args.Str(L"target");
            if (!ValidUin(uin)) { ok = false; msg = L"账号不正确。"; }
            else if (text.empty()) { ok = false; msg = L"消息不能为空。"; }
            else if (kind != L"private" && kind != L"group") { ok = false; msg = L"只能发私聊或群聊。"; }
            else {
                J p = JObj();
                if (kind == L"group") {
                    if (!ValidUin(target)) { ok = false; msg = L"群号不正确。"; }
                    p.Set(L"group_id", J((long long)_wtoi64(target.c_str())));
                } else {
                    if (!ValidUin(target)) { ok = false; msg = L"对方 QQ 号不正确。"; }
                    p.Set(L"user_id", J((long long)_wtoi64(target.c_str())));
                }
                if (ok) {
                    p.Set(L"message", text);
                    J body = JObj();
                    body.Set(L"action", kind == L"group" ? L"send_group_msg" : L"send_private_msg");
                    body.Set(L"params", p);
                    TideReply r = TideApi(L"POST", L"/api/accounts/" + uin + L"/action", body.Dump(), 8000);
                    if (!r.ok) fail(r, L"发送消息");
                    else {
                        bool parsed = false;
                        J res = J::Parse(r.body, &parsed);
                        if (parsed && res.Obj() && res.Str(L"status") == L"failed") {
                            ok = false;
                            msg = res.Str(L"message", L"潮信拒绝了这条消息。");
                        } else {
                            msg = L"已发送。";
                        }
                    }
                }
            }
        } else if (op == L"settings") {
            J body = JObj();
            body.Set(L"theme", args.Str(L"theme", L"system"));
            body.Set(L"accent", args.Str(L"accent", L"#1f7a78"));
            body.Set(L"notes", args.Str(L"notes"));
            body.Set(L"logLevel", args.Str(L"logLevel", L"INFO"));
            body.Set(L"logRetainDays", (int)args.Int(L"logRetainDays", 14));
            body.Set(L"reducedMotion", args.Bool(L"reducedMotion", false));
            TideReply r = TideApi(L"PUT", L"/api/settings", body.Dump(), 8000);
            if (!r.ok) fail(r, L"保存设置");
            else msg = L"设置已保存。";
        } else if (op == L"native_detect") {
            TideReply r = TideApi(L"GET", L"/api/native/detect", L"", 8000);
            if (!r.ok) fail(r, L"探测 QQ");
            else { msg = L"探测完成。"; }
        } else if (op == L"native_start") {
            J body = JObj();
            body.Set(L"qqPath", args.Str(L"qqPath"));
            TideReply r = TideApi(L"POST", L"/api/native/start", body.Dump(), 12000);
            if (!r.ok) fail(r, L"启动原生 QQ");
            else msg = L"原生 QQ 启动命令已发出。";
        } else if (op == L"native_takeover") {
            J body = JObj();
            body.Set(L"qqPath", args.Str(L"qqPath"));
            // 会先请 QQ 正常退出再经 Hook 启动，给足 30 秒
            TideReply r = TideApi(L"POST", L"/api/native/takeover", body.Dump(), 30000);
            if (!r.ok) {
                fail(r, L"接管 QQ");
            } else {
                bool parsed = false;
                J res = J::Parse(r.body, &parsed);
                std::wstring pid = (parsed && res.Obj()) ? std::to_wstring(res.Int(L"pid", 0)) : std::wstring();
                msg = pid.empty() || pid == L"0" ? L"QQ 已由潮信启动，登录后账号会出现在这里。"
                                                 : (L"QQ 已由潮信接管（PID " + pid + L"），扫码登录后账号会出现在这里。");
            }
        } else if (op == L"native_stop") {
            J body = JObj();
            body.Set(L"id", args.Str(L"id"));
            TideReply r = TideApi(L"POST", L"/api/native/stop", body.Dump(), 8000);
            if (!r.ok) fail(r, L"停止原生 QQ");
            else msg = L"已停止。";
        }
        SetOp(ok, msg);
        g_acting = false;
        if (op != L"refresh") TideRefreshAsync(true);
    }).detach();
}

}  // namespace her
