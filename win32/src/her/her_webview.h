// WebView2 Evergreen Runtime 探测（任务书 E5）
//
// 三条探测路径，按可靠性排序（目录优先，因为目录名就是实际安装的版本）：
//   1. 目录：%ProgramFiles(x86)%\Microsoft\EdgeWebView\Application\<版本>\msedgewebview2.exe
//      每用户安装再试 %LOCALAPPDATA%\Microsoft\EdgeWebView\Application\<版本>\...
//      按版本号目录名排序取最高。
//   2. 注册表：EdgeUpdate 客户端项 {F3017226-FE2A-4295-8BDF-00C3A9A7E4C5} 的 pv 值。
//      机器级 64 位视图（HKLM\SOFTWARE\Microsoft\...）→ 机器级 32 位视图
//      （HKLM\SOFTWARE\WOW6432Node\Microsoft\...）→ 用户级（HKCU\Software\...）。
//   3. 兜底：LoadLibrary(WebView2Loader.dll) + CreateCoreWebView2EnvironmentWithOptions，
//      成功就说明 Runtime 在；顺便用 get_BrowserVersionString 拿真实版本，
//      失败码（0x80070002 等）就当没装。只有前两条都查不到才走这条。
//
// 本文件只做探测和纯逻辑；界面侧的降级（原生兜底、错误文案）在 webui.cpp。
#pragma once
#include <string>

struct WebViewRuntime {
    bool found = false;    // 找到 Runtime（目录/注册表/COM 任意一条命中）
    std::wstring version;  // 探测到的版本原文；可能为空（COM 兜底成功但拿不到版本）
    bool ok = false;       // found && 主版本 >= 最低要求
    std::wstring err;      // !ok 时给人看的一句话；ok 时为空
};

// 最低要求的 WebView2 主版本（109.0.1518.0）。更老的 Runtime 缺这台机器用到的
// API，不如直接走原生界面。
int WebViewMinMajor();
const wchar_t* WebViewMinVersion();

// ---------------- 纯函数（tests\webview_runtime.test.cpp 直接测） ----------------

// "109.0.1518.78" / "109.0.1518.78 (x64)" / " v157.0.4341.42 " 都要能取出主版本。
// 用 wcstol 而不是 stoi：绝不允许抛异常；尾部带非版本字符（"(x64)"）也算成功，
// 但 "109abc" 这种数字后面直接粘字母的不认。
bool WebViewParseMajor(const std::wstring& text, int* major);

// 版本够不够最低主版本。解析不出来时返回 false（调用方 WebViewEvaluate 另有容错）。
bool WebViewVersionAtLeast(const std::wstring& version, int minMajor);

// 拼路径：<base>\Microsoft\EdgeWebView\Application
std::wstring WebViewApplicationDir(const std::wstring& baseDir);
// 拼路径：<base>\Microsoft\EdgeWebView\Application\<version>\msedgewebview2.exe
std::wstring WebViewExePath(const std::wstring& baseDir, const std::wstring& version);

// 由 found/version 判断 ok 并生成 err 文案（纯函数，不碰系统）。
WebViewRuntime WebViewEvaluate(bool found, const std::wstring& version);

// ---------------- 探测 / 安装 ----------------

// 读目录、读注册表、必要时用 COM 兜底；结果在进程内缓存一次。
WebViewRuntime WebViewCheckRuntime(bool refresh = false);

// 打开微软官方 Evergreen Bootstrapper 下载页；返回 ShellExecuteW 是否成功。
bool WebViewInstall();
