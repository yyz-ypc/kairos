#pragma once
#include "core.h"
// E5：WebView2 Runtime 探测（detection 实现在 her/her_webview.cpp）。
//   WebViewRuntime { bool found; std::wstring version; bool ok; std::wstring err; }
//   WebViewCheckRuntime() / WebViewInstall()
#include "her/her_webview.h"

void WebUiInit(HWND hwnd);
void WebUiResize();
void WebUiPublish();
void WebUiClose();
bool WebUiActive();
bool WebUiFallback();
void WebUiPaintLoading(HDC dc, int width, int height);
// First-run bootstrap when the HTML runtime itself is missing.
void WebUiNativeTick();
bool WebUiNativeCommand(unsigned id);
bool WebUiNativeMessage(MSG* message);
HBRUSH WebUiNativeControlColor(HDC dc, HWND control);
bool WebUiTakeNativeBootstrap();
wstring WebUiState();
void WebUiCommand(const wstring& message);
// 最近一次界面降级/启动失败的原因（给人看的一句话）；正常启动时为空。
// 没装或过旧的 WebView2 走原生兜底时，这里就是给「安装 WebView2」入口用的提示。
wstring WebViewLastError();

void WebUiStartupMark(const char* phase);
