// 时序之子 · 结构化事件日志的实现（E4，无第三方库）
#include "her_event.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

namespace her {
namespace {

// 进程内串行：跨进程靠 CreateFile 的「独占写」句柄 + 短暂重试。
std::mutex g_eventMutex;

const unsigned long long kRotateBytes = 2ull * 1024 * 1024;  // 2MB
const int kKeepFiles = 7;                                    // events-*.jsonl.1 … .7
const size_t kMaxErrChars = 500;

// unix 秒 → 本地 SYSTEMTIME（不依赖 CRT 的 localtime，免得跟着时区环境变量乱跳）
void LocalSt(long long unixNow, SYSTEMTIME& st) {
    ULARGE_INTEGER u;
    u.QuadPart = ((unsigned long long)(unixNow + 11644473600LL)) * 10000000ULL;
    FILETIME ft{u.LowPart, u.HighPart};
    FILETIME lt{};
    if (!FileTimeToLocalFileTime(&ft, &lt)) lt = ft;
    if (!FileTimeToSystemTime(&lt, &st)) {
        memset(&st, 0, sizeof(st));
        st.wYear = 1970;
        st.wMonth = 1;
        st.wDay = 1;
    }
}

std::wstring DirOf(const std::wstring& p) {
    size_t i = p.find_last_of(L"\\/");
    return i == std::wstring::npos ? std::wstring() : p.substr(0, i);
}

// 只为自己建目录（paths.cpp 的 EnsureDirTree 是 static，这边不跨模块借）
void MkDirs(const std::wstring& dir) {
    if (dir.empty() || DirExists(dir)) return;
    const size_t pos = dir.find_last_of(L"\\/");
    if (pos != std::wstring::npos && pos > 2) MkDirs(dir.substr(0, pos));
    CreateDirectoryW(dir.c_str(), nullptr);
}

std::wstring Pad2(unsigned n) {
    wchar_t b[8];
    swprintf(b, 8, L"%02u", n);
    return b;
}

// 本地相对 UTC 的偏移（含夏令时），形如 +08:00
std::wstring TzOffset(long long unixNow) {
    ULARGE_INTEGER u;
    u.QuadPart = ((unsigned long long)(unixNow + 11644473600LL)) * 10000000ULL;
    FILETIME ft{u.LowPart, u.HighPart};
    FILETIME lt{};
    long long diffMin = 0;
    if (FileTimeToLocalFileTime(&ft, &lt)) {
        LARGE_INTEGER a, b;
        a.LowPart = lt.dwLowDateTime;
        a.HighPart = lt.dwHighDateTime;
        b.LowPart = ft.dwLowDateTime;
        b.HighPart = ft.dwHighDateTime;
        diffMin = (a.QuadPart - b.QuadPart) / 600000000LL;  // 100ns → 分钟
    }
    const bool neg = diffMin < 0;
    if (neg) diffMin = -diffMin;
    return (neg ? L"-" : L"+") + Pad2((unsigned)(diffMin / 60)) + L":" + Pad2((unsigned)(diffMin % 60));
}

// JSON 字符串转义（非 ASCII 原样留着，最后整体转 UTF-8，中文在日志里还能直接读）
std::wstring JsonEscape(const std::wstring& s) {
    std::wstring o;
    o.reserve(s.size() + 8);
    for (size_t i = 0; i < s.size(); i++) {
        const wchar_t c = s[i];
        switch (c) {
            case L'"': o += L"\\\""; break;
            case L'\\': o += L"\\\\"; break;
            case L'\n': o += L"\\n"; break;
            case L'\r': o += L"\\r"; break;
            case L'\t': o += L"\\t"; break;
            default:
                if (c < 0x20 && c >= 0) {
                    wchar_t b[8];
                    swprintf(b, 8, L"\\u%04x", (unsigned)c);
                    o += b;
                } else {
                    o += c;
                }
        }
    }
    return o;
}

std::string ToUtf8(const std::wstring& w) {
    if (w.empty()) return std::string();
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return std::string();
    std::string s((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

// token / 密码的字符集：base64url、base64、hex 都在里面
bool IsTokChar(wchar_t c) {
    return (c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || c == L'-' ||
           c == L'_' || c == L'+' || c == L'/' || c == L'=';
}

// 当前文件滚成 .1：先丢最老的，再依次后移
void Rotate(const std::wstring& path, int keep) {
    if (keep < 1) keep = 1;
    DeleteFileW((path + L"." + std::to_wstring(keep)).c_str());
    for (int n = keep - 1; n >= 1; n--) {
        MoveFileExW((path + L"." + std::to_wstring(n)).c_str(), (path + L"." + std::to_wstring(n + 1)).c_str(),
                    MOVEFILE_REPLACE_EXISTING);
    }
    MoveFileExW(path.c_str(), (path + L".1").c_str(), MOVEFILE_REPLACE_EXISTING);
}

}  // namespace

namespace event_internal {

long long Now() {
    FILETIME ft{};
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER u;
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    return (long long)(u.QuadPart / 10000000ULL) - 11644473600LL;
}

std::wstring LogsDir() { return JoinPath(StateDir(), L"logs"); }

std::wstring FileFor(long long unixNow) {
    SYSTEMTIME st{};
    LocalSt(unixNow, st);
    wchar_t name[32];
    swprintf(name, 32, L"events-%04u%02u%02u.jsonl", st.wYear, st.wMonth, st.wDay);
    return JoinPath(LogsDir(), name);
}

std::wstring IsoLocal(long long unixNow) {
    SYSTEMTIME st{};
    LocalSt(unixNow, st);
    wchar_t head[32];
    swprintf(head, 32, L"%04u-%02u-%02uT%02u:%02u:%02u", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute,
             st.wSecond);
    return std::wstring(head) + TzOffset(unixNow);
}

std::wstring Sanitize(const std::wstring& err) {
    // ① 换行压成空格（一条事件必须是一行）
    std::wstring t;
    t.reserve(err.size());
    for (wchar_t c : err) {
        if (c == L'\r' || c == L'\n' || c == L'\t') {
            if (!t.empty() && t.back() != L' ') t += L' ';
        } else {
            t += c;
        }
    }
    // ② 疑似 token / 密码：连续 24 个以上 base64url / hex 字符
    std::wstring masked;
    masked.reserve(t.size());
    size_t i = 0;
    while (i < t.size()) {
        if (!IsTokChar(t[i])) {
            masked += t[i++];
            continue;
        }
        size_t j = i;
        while (j < t.size() && IsTokChar(t[j])) j++;
        if (j - i >= 24) masked += L"***";
        else masked.append(t, i, j - i);
        i = j;
    }
    // ③ 截断到 500 字符（末尾留一个省略号，不把长度撑过 500）
    if (masked.size() > kMaxErrChars) {
        masked.resize(kMaxErrChars - 1);
        masked += L'…';
    }
    return masked;
}

std::string Line(const wchar_t* op, bool ok, long long ms, const std::wstring& err, long long unixNow) {
    std::wstring w;
    w.reserve(256);
    w += L"{\"ts\":\"";
    w += IsoLocal(unixNow);
    w += L"\",\"op\":\"";
    w += JsonEscape(op ? std::wstring(op) : std::wstring());
    w += L"\",\"ok\":";
    w += ok ? L"true" : L"false";
    w += L",\"ms\":";
    w += std::to_wstring(ms);
    w += L",\"err\":\"";
    w += JsonEscape(Sanitize(err));
    w += L"\"}";
    return ToUtf8(w);
}

bool Append(const std::wstring& path, const std::string& utf8Line, unsigned long long rotateBytes, int keep) {
    std::lock_guard<std::mutex> lk(g_eventMutex);  // 进程内串行
    const std::wstring dir = DirOf(path);
    if (!dir.empty() && !DirExists(dir)) MkDirs(dir);

    std::string data = utf8Line;
    if (data.empty() || data.back() != '\n') data += '\n';

    // 追加写；别的进程正占着（FILE_APPEND_DATA 独占写）就等一下，最多 ~80ms，再不行就放弃
    HANDLE h = INVALID_HANDLE_VALUE;
    for (int i = 0; i < 40; i++) {
        h = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h != INVALID_HANDLE_VALUE) break;
        const DWORD e = GetLastError();
        if (e != ERROR_SHARING_VIOLATION && e != ERROR_ACCESS_DENIED) return false;
        Sleep(2);
    }
    if (h == INVALID_HANDLE_VALUE) return false;

    DWORD wrote = 0;
    const BOOL ok = WriteFile(h, data.data(), (DWORD)data.size(), &wrote, nullptr);
    LARGE_INTEGER size{};
    const BOOL hasSize = GetFileSizeEx(h, &size);
    CloseHandle(h);
    if (!ok || wrote != data.size()) return false;
    if (hasSize && rotateBytes > 0 && (unsigned long long)size.QuadPart > rotateBytes) Rotate(path, keep);
    return true;
}

}  // namespace event_internal

void Event(const wchar_t* op, bool ok, long long ms, const std::wstring& err) {
    try {
        const long long now = event_internal::Now();
        const std::string line = event_internal::Line(op, ok, ms, err, now);
        event_internal::Append(event_internal::FileFor(now), line, kRotateBytes, kKeepFiles);
    } catch (...) {
        // 记日志失败绝不能影响主流程
    }
}

EventScope::EventScope(const wchar_t* op) : op_(op ? op : L""), t0_(GetTickCount64()) {}

EventScope::~EventScope() {
    try {
        const unsigned long long dt = GetTickCount64() - t0_;
        Event(op_, ok_, (long long)dt, err_);
    } catch (...) {
    }
}

}  // namespace her
