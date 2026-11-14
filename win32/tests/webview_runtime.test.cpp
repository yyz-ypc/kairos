// WebView2 Runtime 检测单元测试（任务书 E5）
//
// 验的是纯逻辑：版本号解析容错、最低版本阈值、安装路径拼装、以及由
// found/version 生成 ok/err 的判定（文案里要能看出是「没装」还是「太旧」）。
// 探测本身（注册表/目录/COM）要碰真实安装，所以放在 --probe 后面，不参与断言。
//
// 从仓库根目录编译运行：
//   cd win32
//   g++ -std=c++17 -O2 -Wall -DUNICODE -D_UNICODE -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 ^
//       -Isrc tests\webview_runtime.test.cpp src\her\her_webview.cpp -o build\webview_runtime.test.exe ^
//       -static -static-libgcc -static-libstdc++ ^
//       -lgdiplus -lole32 -loleaut32 -luuid -lshell32 -lshlwapi -lcomdlg32 -lcomctl32 ^
//       -lws2_32 -liphlpapi -lgdi32 -luser32 -ladvapi32
//   build\webview_runtime.test.exe
//
// 或者直接跑 win32\tests\run-core-tests.bat（第三步就是本测试）。
#include "../src/her/her_webview.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace {

int g_failed = 0;

void Check(bool ok, const char* what) {
    if (ok) return;
    ++g_failed;
    std::printf("FAIL: %s\n", what);
}

bool Has(const std::wstring& text, const wchar_t* needle) {
    return text.find(needle) != std::wstring::npos;
}

void TestParseMajor() {
    int major = -1;
    Check(WebViewParseMajor(L"109.0.1518.78", &major) && major == 109, "109.0.1518.78 -> 109");
    Check(WebViewParseMajor(L"109.0.1518.78 (x64)", &major) && major == 109, "109.0.1518.78 (x64)");
    Check(WebViewParseMajor(L" 157.0.4341.42 ", &major) && major == 157, "带空白的 157.0.4341.42");
    Check(WebViewParseMajor(L"v110.0.1", &major) && major == 110, "v 前缀");
    Check(WebViewParseMajor(L"109", &major) && major == 109, "只有主版本");
    Check(WebViewParseMajor(L"0109.0.1", &major) && major == 109, "前导 0");
    // 容错：坏输入必须返回 false，且不能抛（wcstol + errno，不用 stoi）
    Check(!WebViewParseMajor(L"", &major), "空串不认");
    Check(!WebViewParseMajor(L"   ", &major), "全是空白不认");
    Check(!WebViewParseMajor(L"abc", &major), "abc 不认");
    Check(!WebViewParseMajor(L"109abc", &major), "109abc 不认");
    Check(!WebViewParseMajor(L"-109.0.1", &major), "负数不认");
    Check(!WebViewParseMajor(L"999999999999999999999.1", &major), "溢出值不认");
    // 传 nullptr 也不能崩
    Check(!WebViewParseMajor(L"xyz", nullptr), "nullptr major 安全");
    Check(WebViewParseMajor(L"109.0.0.0", nullptr), "nullptr major 正常");
}

void TestThreshold() {
    Check(WebViewMinMajor() == 109, "最低主版本是 109");
    Check(std::wstring(WebViewMinVersion()) == L"109.0.1518.0", "最低版本字符串");
    Check(WebViewVersionAtLeast(L"109.0.1518.0", 109), "109 刚好够");
    Check(WebViewVersionAtLeast(L"109.0.1518.78", 109), "109.0.1518.78 够");
    Check(WebViewVersionAtLeast(L"157.0.4341.42 (x64)", 109), "157 够");
    Check(!WebViewVersionAtLeast(L"108.0.9999.0", 109), "108 不够");
    Check(!WebViewVersionAtLeast(L"109", 110), "自定义阈值 110");
    Check(!WebViewVersionAtLeast(L"garbage", 109), "认不出来时不放行");
}

void TestPaths() {
    const std::wstring base = L"C:\\Program Files (x86)";
    Check(WebViewApplicationDir(base) == L"C:\\Program Files (x86)\\Microsoft\\EdgeWebView\\Application",
          "Application 目录拼装");
    Check(WebViewApplicationDir(L"D:\\Apps\\") == L"D:\\Apps\\Microsoft\\EdgeWebView\\Application",
          "带结尾反斜杠不产生双斜杠");
    Check(WebViewExePath(base, L"157.0.4341.42") ==
              L"C:\\Program Files (x86)\\Microsoft\\EdgeWebView\\Application\\157.0.4341.42\\msedgewebview2.exe",
          "msedgewebview2.exe 路径拼装");
    Check(WebViewExePath(L"", L"157.0.1") .empty(), "空 base 返回空");
    Check(WebViewExePath(base, L"").empty(), "空版本返回空");
}

void TestEvaluate() {
    // 没装：一句话说清楚，且带「安装 WebView2」的指引
    WebViewRuntime missing = WebViewEvaluate(false, L"");
    Check(!missing.found && !missing.ok, "没装时 found/ok 都是 false");
    Check(Has(missing.err, L"没有 WebView2"), "没装时提示「没有 WebView2」");
    Check(Has(missing.err, L"原生模式"), "没装时说明走原生模式");
    Check(Has(missing.err, L"安装 WebView2"), "没装时给出安装入口");

    // 过旧：文案里要有探测到的版本和最低要求
    WebViewRuntime old = WebViewEvaluate(true, L"108.0.2000.1");
    Check(old.found && !old.ok, "108 判为过旧");
    Check(Has(old.err, L"太旧"), "过旧提示含「太旧」");
    Check(Has(old.err, L"108.0.2000.1"), "过旧提示含实际版本");
    Check(Has(old.err, L"109"), "过旧提示含最低要求 109");
    Check(Has(old.err, L"原生模式"), "过旧时说明走原生模式");

    // 够新
    WebViewRuntime fine = WebViewEvaluate(true, L"157.0.4341.42");
    Check(fine.found && fine.ok, "157 判为可用");
    Check(fine.version == L"157.0.4341.42", "版本原文保留");
    Check(fine.err.empty(), "可用时没有错误文案");

    WebViewRuntime edge = WebViewEvaluate(true, L"109.0.1518.0");
    Check(edge.ok, "109.0.1518.0 刚好够（边界）");

    // 认不出来的版本不拦（固定版本部署等），别把能用的机器误判成没装
    WebViewRuntime odd = WebViewEvaluate(true, L"custom-build");
    Check(odd.found && odd.ok, "版本格式认不出时不拦");

    // 前后空白要 trim
    WebViewRuntime padded = WebViewEvaluate(true, L"  157.0.4341.42  ");
    Check(padded.version == L"157.0.4341.42", "版本字符串 trim");
}

}  // namespace

int main(int argc, char** argv) {
    TestParseMajor();
    TestThreshold();
    TestPaths();
    TestEvaluate();

    if (g_failed) {
        std::printf("FAILED: webview_runtime（%d 处断言没过）\n", g_failed);
        return 1;
    }
    std::printf("PASS: webview_runtime（版本解析 + 阈值 + 路径拼装 + 降级文案）\n");

    // 人工自查用：本机真实探测结果（不参与断言，免得没装 Runtime 的机器上测试失败）。
    if (argc > 1 && std::strcmp(argv[1], "--probe") == 0) {
        WebViewRuntime real = WebViewCheckRuntime();
        std::printf("[probe] found=%d ok=%d version=%ls\n", real.found ? 1 : 0, real.ok ? 1 : 0,
                    real.version.c_str());
        if (!real.err.empty()) std::printf("[probe] err=%ls\n", real.err.c_str());
    }
    return 0;
}
