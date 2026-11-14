#include "../brand.h"
// WebView2 Evergreen Runtime 探测（任务书 E5）—— 实现见 her_webview.h 顶部注释。
#include "her_webview.h"
#include <mutex>

#include <windows.h>
#include <objidl.h>

#include <cerrno>
#include <cwctype>
#include <string>
#include <vector>

#include "../../vendor/webview2/WebView2.h"
// MinGW 的 __uuidof 需要这些 __CRT_UUID_DECL（vendor 头里没有 IID 定义）。
#include "../webui_uuid.h"

namespace {

// ---------------------------------------------------------------- 小工具

std::wstring TrimSpace(const std::wstring& s) {
    size_t a = 0, b = s.size();
    while (a < b && iswspace(s[a])) ++a;
    while (b > a && iswspace(s[b - 1])) --b;
    return s.substr(a, b - a);
}

std::wstring Join(const std::wstring& a, const std::wstring& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    const wchar_t tail = a.back();
    if (tail == L'\\' || tail == L'/') return a + b;
    return a + L"\\" + b;
}

std::wstring EnvVar(const wchar_t* name) {
    wchar_t buf[1024];
    DWORD n = GetEnvironmentVariableW(name, buf, (DWORD)(sizeof(buf) / sizeof(buf[0])));
    if (n == 0 || n >= sizeof(buf) / sizeof(buf[0])) return std::wstring();
    return std::wstring(buf, n);
}

// 版本号比较：按 . 分段比数值，缺的段当 0（"109.0" == "109.0.0.0"）。
long NextPart(const std::wstring& s, size_t* pos) {
    size_t i = *pos;
    long v = 0;
    while (i < s.size() && s[i] >= L'0' && s[i] <= L'9') {
        if (v < 1000000) v = v * 10 + (s[i] - L'0');  // 防溢出，多出来的位数不影响比较
        ++i;
    }
    while (i < s.size() && s[i] != L'.') ++i;  // 跳过后缀，如 " (x64)"
    if (i < s.size()) ++i;                     // 跳过 '.'
    *pos = i;
    return v;
}

int CompareVersion(const std::wstring& a, const std::wstring& b) {
    size_t pa = 0, pb = 0;
    while (pa < a.size() || pb < b.size()) {
        long va = pa < a.size() ? NextPart(a, &pa) : 0;
        long vb = pb < b.size() ? NextPart(b, &pb) : 0;
        if (va != vb) return va < vb ? -1 : 1;
    }
    return 0;
}

// ---------------------------------------------------------------- 注册表

// EdgeUpdate 的客户端项 GUID 是固定的（WebView2 Evergreen Runtime）。
const wchar_t* kClientKey = L"SOFTWARE\\Microsoft\\EdgeUpdate\\Clients\\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}";

std::wstring RegPv(HKEY root, REGSAM view) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, kClientKey, 0, KEY_READ | view, &key) != ERROR_SUCCESS) return std::wstring();
    wchar_t buf[512] = {0};
    DWORD bytes = (DWORD)sizeof(buf);
    DWORD type = 0;
    LSTATUS st = RegQueryValueExW(key, L"pv", nullptr, &type, reinterpret_cast<LPBYTE>(buf), &bytes);
    RegCloseKey(key);
    if (st != ERROR_SUCCESS) return std::wstring();
    if (type != REG_SZ && type != REG_EXPAND_SZ) return std::wstring();
    if (bytes < sizeof(wchar_t)) return std::wstring();
    size_t chars = bytes / sizeof(wchar_t);
    while (chars > 0 && buf[chars - 1] == L'\0') --chars;  // 不保证带结尾 0
    return TrimSpace(std::wstring(buf, chars));
}

// 三条注册表路径，取第一个非空。
std::wstring RegVersion() {
    std::wstring v = RegPv(HKEY_LOCAL_MACHINE, KEY_WOW64_64KEY);   // 机器级，64 位视图
    if (!v.empty()) return v;
    v = RegPv(HKEY_LOCAL_MACHINE, KEY_WOW64_32KEY);                // 机器级，32 位视图（WOW6432Node）
    if (!v.empty()) return v;
    return RegPv(HKEY_CURRENT_USER, 0);                            // 用户级
}

// ---------------------------------------------------------------- 目录

// 扫一个 <base>\Microsoft\EdgeWebView\Application 目录，取版本号最高的子目录。
bool DirVersion(const std::wstring& base, std::wstring* version) {
    const std::wstring root = WebViewApplicationDir(base);
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW(Join(root, L"*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return false;
    bool any = false;
    std::wstring best;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        const std::wstring name = fd.cFileName;
        int major = 0;
        if (!WebViewParseMajor(name, &major)) continue;  // "SetupMetrics" 之类的目录直接跳过
        const std::wstring exe = WebViewExePath(base, name);
        if (GetFileAttributesW(exe.c_str()) == INVALID_FILE_ATTRIBUTES) continue;  // 没 exe 的壳目录不算
        if (!any || CompareVersion(name, best) > 0) {
            best = name;
            any = true;
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    if (!any) return false;
    if (version) *version = best;
    return true;
}

// ---------------------------------------------------------------- COM 兜底

// CreateCoreWebView2EnvironmentWithOptions 是异步的：等回调期间泵消息。
struct ProbeHandler final : ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler {
    ULONG refs = 1;
    bool done = false;
    HRESULT code = E_PENDING;
    std::wstring browser;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (IsEqualIID(id, IID_IUnknown) ||
            IsEqualIID(id, __uuidof(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler))) {
            *out = static_cast<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(reinterpret_cast<volatile LONG*>(&refs)); }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG left = InterlockedDecrement(reinterpret_cast<volatile LONG*>(&refs));
        if (!left) delete this;
        return left;
    }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT hr, ICoreWebView2Environment* env) override {
        code = hr;
        if (SUCCEEDED(hr) && env) {
            LPWSTR text = nullptr;
            if (SUCCEEDED(env->get_BrowserVersionString(&text)) && text) browser = text;
            if (text) CoTaskMemFree(text);
        }
        done = true;
        return S_OK;
    }
};

// 只有目录和注册表都没结果时才走到这里：真正试着建一次环境。
// 成功 → Runtime 在（可能是固定版本部署，拿不到安装目录也能用），版本取 get_BrowserVersionString。
// 失败 → 按没装处理（0x80070002 = 缺 Runtime，0x80040154 = 没注册）。
WebViewRuntime ProbeByLoader() {
    WebViewRuntime r;
    wchar_t exe[MAX_PATH] = {0};
    DWORD n = GetModuleFileNameW(nullptr, exe, (DWORD)(sizeof(exe) / sizeof(exe[0])));
    if (n == 0 || n >= sizeof(exe) / sizeof(exe[0])) return r;
    std::wstring dir = std::wstring(exe, n);
    const size_t slash = dir.find_last_of(L"\\/");
    if (slash != std::wstring::npos) dir.erase(slash);
    HMODULE loader = LoadLibraryExW(Join(dir, L"WebView2Loader.dll").c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!loader) return r;
    // loader 故意不 FreeLibrary：和 webui.cpp 一致，映射到进程退出，避免异步回调回来时代码段已经没了。
    using Create = HRESULT(STDAPICALLTYPE*)(PCWSTR, PCWSTR, ICoreWebView2EnvironmentOptions*,
                                           ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*);
    auto create = reinterpret_cast<Create>(GetProcAddress(loader, "CreateCoreWebView2EnvironmentWithOptions"));
    if (!create) return r;

    wchar_t tmp[MAX_PATH] = {0};
    DWORD t = GetTempPathW((DWORD)(sizeof(tmp) / sizeof(tmp[0])), tmp);
    const std::wstring userData = Join(t ? std::wstring(tmp, t) : dir, L"kairos-wv2-probe");

    ProbeHandler* handler = new ProbeHandler();
    handler->AddRef();
    HRESULT hr = create(nullptr, userData.c_str(), nullptr, handler);
    if (FAILED(hr)) {
        handler->Release();
        handler->Release();
        return r;
    }
    const DWORD start = GetTickCount();
    while (!handler->done && GetTickCount() - start < 2000) {
        DWORD wait = MsgWaitForMultipleObjects(0, nullptr, FALSE, 50, QS_ALLINPUT);
        if (wait == WAIT_OBJECT_0) {
            MSG msg;
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
    }
    if (handler->done) {
        r.found = SUCCEEDED(handler->code);
        if (r.found) r.version = handler->browser;
    }
    // 超时：不下结论（found=false），交给正常启动流程去报具体错误码，免得误判成「没装」。
    handler->Release();
    handler->Release();
    return r;
}

WebViewRuntime ProbeRuntime() {
    // 1) 目录优先：目录名就是实际安装的版本，最贴近真相。
    for (const wchar_t* base : {L"ProgramFiles(x86)", L"ProgramFiles", L"LOCALAPPDATA"}) {
        const std::wstring dir = EnvVar(base);
        if (dir.empty()) continue;
        std::wstring version;
        if (DirVersion(dir, &version)) return WebViewEvaluate(true, version);
    }
    // 2) 注册表：三条视图各试一次。
    const std::wstring reg = RegVersion();
    if (!reg.empty()) return WebViewEvaluate(true, reg);
    // 3) COM 兜底。
    WebViewRuntime probe = ProbeByLoader();
    if (probe.found) return WebViewEvaluate(true, probe.version);
    return WebViewEvaluate(false, std::wstring());
}

}  // namespace

// ---------------------------------------------------------------- 导出

int WebViewMinMajor() { return 109; }
const wchar_t* WebViewMinVersion() { return L"109.0.1518.0"; }

bool WebViewParseMajor(const std::wstring& text, int* major) {
    if (major) *major = 0;
    size_t i = 0;
    while (i < text.size() && iswspace(text[i])) ++i;
    if (i < text.size() && (text[i] == L'v' || text[i] == L'V')) ++i;  // "v109.0.1" 这种也认
    const size_t start = i;
    while (i < text.size() && text[i] >= L'0' && text[i] <= L'9') ++i;
    if (i == start) return false;
    // 数字后面只能是版本分隔符或空白；"109abc" 不算版本号。
    if (i < text.size() && text[i] != L'.' && !iswspace(text[i])) return false;
    // wcstol 而不是 stoi：不抛异常，溢出用 errno 判。
    errno = 0;
    const std::wstring digits = text.substr(start, i - start);
    wchar_t* end = nullptr;
    const long value = wcstol(digits.c_str(), &end, 10);
    if (end == digits.c_str() || (end && *end != L'\0')) return false;
    if (errno == ERANGE || value < 0 || value > 99999) return false;
    if (major) *major = static_cast<int>(value);
    return true;
}

bool WebViewVersionAtLeast(const std::wstring& version, int minMajor) {
    int major = 0;
    if (!WebViewParseMajor(version, &major)) return false;
    return major >= minMajor;
}

std::wstring WebViewApplicationDir(const std::wstring& baseDir) {
    if (baseDir.empty()) return std::wstring();
    return Join(baseDir, L"Microsoft\\EdgeWebView\\Application");
}

std::wstring WebViewExePath(const std::wstring& baseDir, const std::wstring& version) {
    if (baseDir.empty() || version.empty()) return std::wstring();
    return Join(Join(WebViewApplicationDir(baseDir), version), L"msedgewebview2.exe");
}

WebViewRuntime WebViewEvaluate(bool found, const std::wstring& version) {
    WebViewRuntime r;
    r.found = found;
    r.version = TrimSpace(version);
    if (!found) {
        r.ok = false;
        r.err = L"这台机器没有 WebView2 Runtime，界面用原生模式显示。点「安装 WebView2」装好后重启" BRAND_NAME L"。";
        return r;
    }
    int major = 0;
    if (!WebViewParseMajor(r.version, &major)) {
        // 找得到但版本格式认不出来（固定版本部署/第三方打包）：不拦，让真正创建环境那步说话。
        r.ok = true;
        return r;
    }
    if (major < WebViewMinMajor()) {
        r.ok = false;
        r.err = L"WebView2 版本太旧（" + r.version + L"），至少需要 " + std::to_wstring(WebViewMinMajor()) +
                L"。界面用原生模式显示。点「安装 WebView2」装好后重启" BRAND_NAME L"。";
        return r;
    }
    r.ok = true;
    return r;
}

WebViewRuntime WebViewCheckRuntime(bool refresh) {
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
    // 启动时算一次就够：装完 Runtime 本来也要重启工作台。
    static bool scanned = false;
    static WebViewRuntime cached;
    if (refresh || !scanned) {
        cached = ProbeRuntime();
        scanned = true;
    }
    return cached;
}

bool WebViewInstall() {
    // 微软官方 Evergreen Bootstrapper（与 WebView2 文档里给的一键安装链接一致）。
    HINSTANCE result = ShellExecuteW(nullptr, L"open", L"https://go.microsoft.com/fwlink/p/?LinkId=2124703",
                                     nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(result) > 32;  // ShellExecute 的约定：<=32 是错误码
}
