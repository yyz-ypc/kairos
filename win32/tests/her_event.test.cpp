// 结构化事件日志单元测试（任务 E4）
//
// 验的是：一行合法 JSON、UTF-8 无 BOM、err 单行+脱敏+截断 500、超过阈值滚动且只留限定份数、
// 写不进去时静默不抛。
//
// 从仓库根目录编译运行（只用临时 state 目录，靠 KAIROS_STATE_DIR 重定向）：
//   cd win32
//   g++ -std=c++17 -O2 -Wall -DUNICODE -D_UNICODE -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 ^
//       -Isrc tests\her_event.test.cpp src\her\her_event.cpp src\her\her_json.cpp ^
//       src\her\her_config.cpp src\util.cpp src\paths.cpp -o build\her_event.test.exe ^
//       -static -static-libgcc -static-libstdc++ ^
//       -lgdiplus -lole32 -loleaut32 -luuid -lshell32 -lshlwapi -lcomdlg32 -lcomctl32 ^
//       -lws2_32 -liphlpapi -lgdi32 -luser32 -ladvapi32
//   build\her_event.test.exe
//
// 或者直接跑 win32\tests\run-core-tests.bat。
#include "../src/her/her.h"
#include "../src/her/her_event.h"

#include <cassert>
#include <cstdio>
#include <cstring>

// 事件日志只碰文件系统：网络/系统状态不在范围内，给最朴素的桩。
her::Http her::HttpGetJson(const std::wstring&, const std::wstring&, int) { return her::Http{}; }
bool her::Listening(int) { return false; }
her::ActionResult her::RunAction(const std::wstring&, const std::wstring&) { return {false, L"stub"}; }
bool her::Sqlite::OpenRead(const std::wstring&, std::wstring*) { return false; }
void her::Sqlite::Close() {}
bool her::Sqlite::QueryLongs(const std::wstring&, std::vector<long long>&, std::wstring*) { return false; }
bool her::Sqlite::QueryTexts(const std::wstring&, std::vector<std::vector<std::wstring>>&, std::wstring*) {
    return false;
}

namespace {

std::wstring Wide(const std::string& s) {
    if (s.empty()) return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}

std::string ReadRaw(const std::wstring& path) {
    std::string out;
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return out;
    char buf[4096];
    DWORD got = 0;
    while (ReadFile(h, buf, sizeof(buf), &got, nullptr) && got > 0) out.append(buf, got);
    CloseHandle(h);
    return out;
}

std::vector<std::string> Lines(const std::string& s) {
    std::vector<std::string> v;
    size_t i = 0;
    while (i < s.size()) {
        size_t j = s.find('\n', i);
        if (j == std::string::npos) {
            v.push_back(s.substr(i));
            break;
        }
        v.push_back(s.substr(i, j - i));
        i = j + 1;
    }
    return v;
}

// 每行都必须是完整 JSON，且字段齐
her::J MustParse(const std::string& line, const char* where) {
    bool ok = false;
    her::J j = her::J::Parse(Wide(line), &ok);
    if (!ok) {
        std::fprintf(stderr, "[FAIL] %s：这行不是合法 JSON：%s\n", where, line.c_str());
        assert(ok);
    }
    return j;
}

// 2026-11-11T21:03:04+08:00
void CheckTs(const std::wstring& ts, const char* where) {
    const bool shape = ts.size() == 25 && ts[4] == L'-' && ts[7] == L'-' && ts[10] == L'T' && ts[13] == L':' &&
                       ts[16] == L':' && (ts[19] == L'+' || ts[19] == L'-') && ts[22] == L':';
    if (!shape) std::fprintf(stderr, "[FAIL] %s：ts 不是 2026-11-11T21:03:04+08:00 这种形状\n", where);
    assert(shape);
    for (size_t i = 0; i < ts.size(); i++)
        if (i != 4 && i != 7 && i != 10 && i != 13 && i != 16 && i != 19 && i != 22) assert(ts[i] >= L'0' && ts[i] <= L'9');
}

std::wstring TempRoot() {
    wchar_t cur[2048] = {0};
    GetCurrentDirectoryW(2048, cur);
    const std::wstring dir = JoinPath(cur, L"build\\event-test-" + Num(GetCurrentProcessId()));
    CreateDirectoryW(JoinPath(cur, L"build").c_str(), nullptr);
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir;
}

void RmTree(const std::wstring& dir) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(JoinPath(dir, L"*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            const std::wstring n = fd.cFileName;
            if (n == L"." || n == L"..") continue;
            const std::wstring full = JoinPath(dir, n);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) RmTree(full);
            else DeleteFileW(full.c_str());
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryW(dir.c_str());
}

}  // namespace

int main() {
    const std::wstring root = TempRoot();
    const std::wstring state = JoinPath(root, L"state");
    CreateDirectoryW(state.c_str(), nullptr);
    SetEnvironmentVariableW(L"KAIROS_STATE_DIR", state.c_str());

    const long long now = her::event_internal::Now();
    const std::wstring file = her::event_internal::FileFor(now);
    // 路径必须是 <StateDir>\logs\events-YYYYMMDD.jsonl（命名靠 FileFor，别自己拼）
    assert(file.find(JoinPath(state, L"logs") + L"\\") == 0);
    assert(file.size() > 24 && file.compare(file.size() - 6, 6, L".jsonl") == 0);
    {
        const std::wstring name = file.substr(file.find_last_of(L"\\") + 1);
        assert(name.size() == 7 + 8 + 6);  // "events-" + 8 位日期 + ".jsonl"
        for (size_t i = 7; i < 15; i++) assert(name[i] >= L'0' && name[i] <= L'9');
    }

    // ① 正常一条：UTF-8 无 BOM、一行合法 JSON、字段全对
    her::Event(L"op:test", true, 12);
    {
        const std::string raw = ReadRaw(file);
        assert(!raw.empty());
        assert(raw[0] == '{');  // 没有 BOM（BOM 会以 EF BB BF 开头）
        assert(raw.find("\xEF\xBB\xBF") == std::string::npos);
        assert(raw.back() == '\n');
        const std::vector<std::string> rows = Lines(raw);
        assert(rows.size() == 1);
        const her::J j = MustParse(rows[0], "Event()");
        assert(j.Str(L"op") == L"op:test");
        assert(j.Bool(L"ok"));
        assert(j.Int(L"ms") == 12);
        assert(j.Str(L"err").empty());
        CheckTs(j.Str(L"ts"), "Event()");
        assert(j.Str(L"ts") == her::event_internal::IsoLocal(now));  // 同一秒内格式一致
    }

    // ② err：\r\n 压成空格、疑似 token 变 ***
    her::Event(L"op:fail", false, 5, L"上传失败 token=sk-" L"abcdefghijklmnopqrstuvwxyz0123\r\n服务器没连上");
    {
        const std::vector<std::string> rows = Lines(ReadRaw(file));
        assert(rows.size() == 2);
        const her::J j = MustParse(rows[1], "Event(err)");
        assert(!j.Bool(L"ok"));
        const std::wstring err = j.Str(L"err");
        assert(err.find(L"sk-" L"abcdefghijklmnopqrstuvwxyz0123") == std::wstring::npos);
        assert(err.find(L"***") != std::wstring::npos);
        assert(err.find(L'\n') == std::wstring::npos && err.find(L'\r') == std::wstring::npos);
        assert(err.find(L"服务器没连上") != std::wstring::npos);
    }

    // ③ 超长 err 截断到 500（脱敏在截断之前，所以要用不带长 token 的文字）
    {
        std::wstring longErr;
        for (int i = 0; i < 400; i++) longErr += L"错 ";
        assert(longErr.size() == 800);
        assert(her::event_internal::Sanitize(longErr).size() == 500);
        assert(her::event_internal::Sanitize(L"一二三四").size() == 4);
        her::Event(L"op:long", false, 1, longErr);
        const std::vector<std::string> rows = Lines(ReadRaw(file));
        const her::J j = MustParse(rows.back(), "Event(长 err)");
        const std::wstring err = j.Str(L"err");
        assert(err.size() == 500);
        assert(err.find(L'\n') == std::wstring::npos);
    }

    // ④ 滚动：阈值调到 200 字节、只留 3 份，连着写 20 条
    {
        const std::wstring roll = JoinPath(root, L"roll.jsonl");
        std::string one = her::event_internal::Line(L"op:roll", true, 1, L"", now);
        for (int i = 0; i < 20; i++) assert(her::event_internal::Append(roll, one, 200, 3));
        assert(FileExists(roll));
        assert(FileExists(roll + L".1") && FileExists(roll + L".2") && FileExists(roll + L".3"));
        assert(!FileExists(roll + L".4"));  // 只留 3 份
        for (const std::wstring& p : {roll, roll + L".1", roll + L".2", roll + L".3"})
            for (const std::string& line : Lines(ReadRaw(p))) MustParse(line, "滚动后的文件");
    }

    // ⑤ EventScope：析构时自动写一条，默认成功
    {
        {
            her::EventScope s(L"op:scope");
            Sleep(2);
        }
        const std::vector<std::string> rows = Lines(ReadRaw(file));
        const her::J j = MustParse(rows.back(), "EventScope");
        assert(j.Str(L"op") == L"op:scope");
        assert(j.Bool(L"ok"));
        assert(j.Int(L"ms") <= 60000);
    }

    // ⑥ 写不进去时静默：不抛、不崩、返回值照常
    {
        assert(!her::event_internal::Append(L"Z:\\没有这个盘\\logs\\x.jsonl", "{}", 0, 7));
        SetEnvironmentVariableW(L"KAIROS_STATE_DIR", L"Z:\\没有这个盘\\state");
        her::Event(L"op:nowhere", true, 1);  // 不抛就是过
        SetEnvironmentVariableW(L"KAIROS_STATE_DIR", state.c_str());
    }

    std::puts("PASS: her_event（JSONL 合法性 / UTF-8 无 BOM / err 脱敏截断 / 滚动 / 静默失败）");
    SetEnvironmentVariableW(L"KAIROS_STATE_DIR", nullptr);
    RmTree(root);
    return 0;
}
