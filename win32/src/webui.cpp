#include "brand.h"
// 本地内存资源的 HTML 界面；只有这个源能调到 C++ 这边。
#include "webui.h"
#include "art_resources.h"
#include "../vendor/webview2/WebView2.h"
#include "webui_uuid.h"
#include <shlwapi.h>
#include <functional>
#include <commctrl.h>
#include <cstdio>
#include "her/her.h"

void WebUiStartupMark(const char* phase) {
    wchar_t log[2048]{};if(!GetEnvironmentVariableW(L"KAIROS_BENCH_LOG",log,2048))return;
    HANDLE file=CreateFileW(log,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return;
    char line[256];const int n=snprintf(line,sizeof(line),"%llu %s\n",GetTickCount64(),phase);DWORD wrote;WriteFile(file,line,n,&wrote,nullptr);CloseHandle(file);
}
template<class Interface, class... Args> class Callback final : public Interface {
    ULONG refs = 1;
    std::function<HRESULT(Args...)> fn;
public:
    explicit Callback(std::function<HRESULT(Args...)> f) : fn(std::move(f)) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (IsEqualIID(id, IID_IUnknown) || IsEqualIID(id, __uuidof(Interface))) {
            *out = static_cast<Interface*>(this); AddRef(); return S_OK;
        }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override { ULONG n = --refs; if (!n) delete this; return n; }
    HRESULT STDMETHODCALLTYPE Invoke(Args... args) override { return fn(args...); }
};
template<class I, class... A, class F> static auto Handler(F fn) {
    return new Callback<I, A...>(fn);
}
static HWND host;
static HMODULE loader;
static ICoreWebView2Environment* environment;
static ICoreWebView2Controller* controller;
static ICoreWebView2* view;
static bool closing = false;
static bool fallback = false;
// 给人看的降级原因；正常启动时为空。见 webui.h 的 WebViewLastError()。
static wstring lastError;
static const wchar_t* origin = BRAND_ORIGIN;
static HWND nativePrimary, nativeWorkspace, nativeExit, nativeStatus, nativeProgress;
static HBRUSH nativeBackground;
static bool nativeRetryAttempted=false, nativeContinueSetup=false, nativeRestoringUi=false;
static DWORD nativeProgressAt=0;
static void ClearNativeSetup(){
    for(auto* control:{&nativePrimary,&nativeWorkspace,&nativeExit,&nativeStatus,&nativeProgress}){
        if(*control&&IsWindow(*control)){DestroyWindow(*control);}
        *control=nullptr;
    }
    if(nativeBackground){DeleteObject(nativeBackground);nativeBackground=nullptr;}
}
// 切到原生兜底：藏掉 WebView、补回标题栏（原生界面要靠它关窗/拖动），再重画。
static void EnterFallback() {
    fallback = true;
    if (controller) controller->put_IsVisible(FALSE);
    // The native fallback needs its own close/move controls when HTML cannot load.
    SetWindowLongPtrW(host, GWL_STYLE, GetWindowLongPtrW(host, GWL_STYLE) | WS_CAPTION);
    SetWindowPos(host, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    InvalidateRect(host, nullptr, FALSE);
}
// 真正坏掉（COM 报错）：保留原来的弹窗，错误码对排查有用。
static void Fail(HRESULT hr) {
    EnterFallback();
    wchar_t code[32]; swprintf(code, 32, L"0x%08lX", (unsigned long)hr);
    lastError = L"内嵌界面没能启动（" + wstring(code) + L"）。\n请安装 Microsoft Edge WebView2 Runtime，并确认 WebView2Loader.dll 与工作台在同一目录。";
    MessageBoxW(host, lastError.c_str(), L"" BRAND_NAME L" · 界面启动失败", MB_OK | MB_ICONERROR);
}
// 预期内的降级（没装 / 版本过旧）：只记一句话，不弹阻塞对话框。
static void FailQuiet(const wstring& message) {
    EnterFallback();
    lastError = message;
}
bool WebUiActive() { return view != nullptr; }
bool WebUiFallback() { return fallback; }
wstring WebViewLastError() { return lastError; }
static COLORREF ThemeColor(const wstring& key, COLORREF fallbackColor) {
    wstring value = SettingGet(key);
    if (value.size() != 7 || value[0] != L'#' || value.find_first_not_of(L"0123456789abcdefABCDEF", 1) != wstring::npos) return fallbackColor;
    unsigned long rgb = wcstoul(value.c_str() + 1, nullptr, 16);
    return RGB((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255);
}
static bool NativeLightTheme() {
    const auto theme=SettingGet(L"theme");
    return theme==L"daylight"||theme==L"suit";
}
static COLORREF ThemeBackground() {
    wstring theme = SettingGet(L"theme");
    if (NativeLightTheme()) return RGB(0xee, 0xf3, 0xfb);
    if (theme == L"custom") return ThemeColor(L"theme_bg", RGB(0x10, 0x18, 0x27));
    return RGB(0x07, 0x0d, 0x18);
}
void WebUiPaintLoading(HDC dc, int width, int height) {
    RECT bounds{0, 0, width, height};
    HBRUSH brush = CreateSolidBrush(ThemeBackground());
    FillRect(dc, &bounds, brush); DeleteObject(brush);
    if ((view && !fallback) || closing) return;
    if(fallback&&SettingGet(L"wizard_complete")!=L"1")return;
    COLORREF ink = NativeLightTheme() ? RGB(32,48,74) : RGB(233,239,252);
    if (SettingGet(L"theme") == L"custom") ink = ThemeColor(L"theme_ink", ink);
    int oldMode = SetBkMode(dc, TRANSPARENT);
    COLORREF oldInk = SetTextColor(dc, ink);
    HGDIOBJ oldFont = SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
    DrawTextW(dc, L"正在打开工作台…", -1, &bounds, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(dc, oldFont); SetTextColor(dc, oldInk); SetBkMode(dc, oldMode);
}
// 界面是深夜蓝，系统默认的白色标题栏会像在画上贴了块膏药，所以把边框也刷成同一个色。
static void PaintFrameTheme(HWND hwnd) {
    using DwmSet = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
    static DwmSet dwm = [] {
        HMODULE m = LoadLibraryW(L"dwmapi.dll");
        return m ? reinterpret_cast<DwmSet>(GetProcAddress(m, "DwmSetWindowAttribute")) : (DwmSet) nullptr;
    }();
    if (!dwm) return;
    wstring mode = SettingGet(L"theme"), base = SettingGet(L"theme_base");
    auto color = [](const wstring& key, COLORREF fallback) {
        wstring v = SettingGet(key);
        if (v.size() != 7 || v[0] != L'#' || v.find_first_not_of(L"0123456789abcdefABCDEF", 1) != wstring::npos) return fallback;
        unsigned long rgb = wcstoul(v.c_str() + 1, nullptr, 16);
        return RGB((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255);
    };
    BOOL on = !((mode == L"daylight" || mode == L"suit") || (mode == L"custom" && base == L"light"));
    // 20 是 Win10 20H1+ 的编号，19 是 1809 之前的，认哪个用哪个。
    if (FAILED(dwm(hwnd, 20, &on, sizeof(on)))) dwm(hwnd, 19, &on, sizeof(on));
    COLORREF caption = RGB(0x08, 0x0e, 0x1a), text = RGB(0xe9, 0xef, 0xfc), border = RGB(0x17, 0x24, 0x40);
    if ((mode == L"daylight" || mode == L"suit")) { caption = RGB(255,255,255); text = RGB(32,48,74); border = RGB(219,226,236); }
    if (mode == L"custom") { caption = color(L"theme_panel", caption); text = color(L"theme_ink", text); border = color(L"theme_bg", border); }
    // 下面三条是 Win11 才认；Win10 返回失败，标题栏还是靠上面那条变深。
    dwm(hwnd, 35, &caption, sizeof(caption));
    dwm(hwnd, 36, &text, sizeof(text));
    dwm(hwnd, 34, &border, sizeof(border));
}
void WebUiResize() {
    if (controller) { RECT rc; GetClientRect(host, &rc); controller->put_Bounds(rc); }
}
void WebUiPublish() {
    if (view && !closing) {
        static wstring lastTheme;
        wstring theme = SettingGet(L"theme") + SettingGet(L"theme_base") + SettingGet(L"theme_panel") + SettingGet(L"theme_ink") + SettingGet(L"theme_bg");
        if (theme != lastTheme) { PaintFrameTheme(host); lastTheme = theme; }
        view->PostWebMessageAsJson(WebUiState().c_str());
    }
}
void WebUiClose() {
    closing = true;
    ClearNativeSetup();
    if (controller) { controller->Close(); controller->Release(); controller = nullptr; }
    if (view) { view->Release(); view = nullptr; }
    if (environment) { environment->Release(); environment = nullptr; }
    // Keep the loader mapped until process exit: asynchronous COM callbacks may still unwind.
}
void WebUiInit(HWND hwnd) {
    WebUiStartupMark("webview-init");
    host = hwnd;
    closing = false;
    fallback = false;
    lastError.clear();
    // E5：先用目录/注册表把 Runtime 版本查清楚。缺失或过旧就直接走原生兜底——
    // CreateCoreWebView2EnvironmentWithOptions 失败只会给个天书错误码，还容易卡住启动。
    // 调试开关：KAIROS_UI_FALLBACK=1 时不启动 WebView2，直接看原生兜底页。
    // 只在启动时读一次，不落配置；正常用户不会设它。
    if (GetEnvironmentVariableW(BRAND_ENV_KAIROS_UI_FALLBACK, nullptr, 0) > 0) {
        FailQuiet(L"调试模式：这次没启动内嵌界面。");
        return;
    }
    WebViewRuntime runtime = WebViewCheckRuntime();
    if (!runtime.ok) { FailQuiet(runtime.err); return; }
    loader = LoadLibraryExW(JoinPath(ExeDir(), L"WebView2Loader.dll").c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!loader) { Fail(HRESULT_FROM_WIN32(GetLastError())); return; }
    using Create = HRESULT(STDAPICALLTYPE*)(PCWSTR, PCWSTR, ICoreWebView2EnvironmentOptions*, ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*);
    auto create = reinterpret_cast<Create>(GetProcAddress(loader, "CreateCoreWebView2EnvironmentWithOptions"));
    if (!create) { Fail(E_NOINTERFACE); return; }
    auto envHandler = Handler<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler, HRESULT, ICoreWebView2Environment*>(
        [](HRESULT hr, ICoreWebView2Environment* env) -> HRESULT {
            WebUiStartupMark("environment-ready");
            if (closing) return S_OK;
            if (FAILED(hr) || !env) { Fail(hr); return S_OK; }
            environment = env; env->AddRef();
            auto ctlHandler = Handler<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler, HRESULT, ICoreWebView2Controller*>(
                [](HRESULT result, ICoreWebView2Controller* ctl) -> HRESULT {
                    if (closing) return S_OK;
                    if (FAILED(result) || !ctl) { Fail(result); return S_OK; }
                    WebUiStartupMark("controller-ready");
                    controller = ctl; ctl->AddRef(); ctl->get_CoreWebView2(&view);
                    if(nativeRestoringUi){
                        nativeRestoringUi=false;
                        SetWindowLongPtrW(host,GWL_STYLE,GetWindowLongPtrW(host,GWL_STYLE)&~WS_CAPTION);
                        SetWindowPos(host,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);
                    }
                    PaintFrameTheme(host);
                    // 页面第一次绘制前先把底色压成深夜蓝，免得一闪白。
                    ICoreWebView2Controller2* ctl2 = nullptr;
                    if (SUCCEEDED(ctl->QueryInterface(IID_ICoreWebView2Controller2, (void**)&ctl2)) && ctl2) {
                        COLORREF color = ThemeBackground();
                        COREWEBVIEW2_COLOR bg{255, GetRValue(color), GetGValue(color), GetBValue(color)};
                        ctl2->put_DefaultBackgroundColor(bg);
                        ctl2->Release();
                    }
                    ICoreWebView2Settings* settings = nullptr;
                    if (SUCCEEDED(view->get_Settings(&settings))) {
                        settings->put_AreDefaultContextMenusEnabled(FALSE);
                        settings->put_AreDevToolsEnabled(FALSE);
                        settings->put_AreDefaultScriptDialogsEnabled(FALSE);
                        settings->put_IsStatusBarEnabled(FALSE);
                        settings->Release();
                    }
                    EventRegistrationToken token{};
                    auto resource = Handler<ICoreWebView2WebResourceRequestedEventHandler, ICoreWebView2*, ICoreWebView2WebResourceRequestedEventArgs*>(
                        [](ICoreWebView2*, ICoreWebView2WebResourceRequestedEventArgs* args) -> HRESULT {
                            ICoreWebView2WebResourceRequest* request = nullptr;
                            args->get_Request(&request); LPWSTR uri = nullptr; request->get_Uri(&uri); request->Release();
                            wstring url = uri ? uri : L""; CoTaskMemFree(uri);
                            int id = url == origin ? 200 : WebArtResource(url);
                            HRSRC res = id ? FindResourceW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(id), RT_RCDATA) : nullptr;
                            IStream* stream = nullptr;
                            if (res) stream = SHCreateMemStream((const BYTE*)LockResource(LoadResource(GetModuleHandleW(nullptr), res)), SizeofResource(GetModuleHandleW(nullptr), res));
                            ICoreWebView2WebResourceResponse* response = nullptr;
                            environment->CreateWebResourceResponse(stream, res ? 200 : 404, res ? L"OK" : L"Not Found",
                                id == 200 ? L"Content-Type: text/html; charset=utf-8\r\nCache-Control: no-store" : L"Content-Type: image/webp\r\nCache-Control: public, max-age=31536000, immutable", &response);
                            if (response) { args->put_Response(response); response->Release(); }
                            if (stream) stream->Release();
                            return S_OK;
                        });
                    view->AddWebResourceRequestedFilter(L"*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
                    view->add_WebResourceRequested(resource, &token); resource->Release();
                    auto navigation = Handler<ICoreWebView2NavigationStartingEventHandler, ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs*>(
                        [](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                            LPWSTR uri = nullptr; args->get_Uri(&uri);
                            if (!uri || wstring(uri) != origin) args->put_Cancel(TRUE);
                            CoTaskMemFree(uri); return S_OK;
                        });
                    view->add_NavigationStarting(navigation, &token); navigation->Release();
                    auto completed = Handler<ICoreWebView2NavigationCompletedEventHandler, ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*>(
                        [](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
                            if (closing) return S_OK;
                            BOOL success = FALSE; args->get_IsSuccess(&success);
                            if (!success) Fail(E_FAIL);
                            return S_OK;
                        });
                    view->add_NavigationCompleted(completed, &token); completed->Release();
                    auto popup = Handler<ICoreWebView2NewWindowRequestedEventHandler, ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs*>(
                        [](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT { args->put_Handled(TRUE); return S_OK; });
                    view->add_NewWindowRequested(popup, &token); popup->Release();
                    auto message = Handler<ICoreWebView2WebMessageReceivedEventHandler, ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs*>(
                        [](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                            LPWSTR source = nullptr; args->get_Source(&source);
                            bool trusted = source && wstring(source) == origin; CoTaskMemFree(source);
                            if (!trusted) return S_OK;
                            LPWSTR json = nullptr; args->get_WebMessageAsJson(&json);
                            if (json) { WebUiCommand(json); CoTaskMemFree(json); WebUiPublish(); }
                            return S_OK;
                        });
                    view->add_WebMessageReceived(message, &token); message->Release();
                    WebUiResize(); controller->put_IsVisible(TRUE);
                    WebUiStartupMark("navigate");
                    view->Navigate(origin);
                    return S_OK;
                });
            HRESULT result = env->CreateCoreWebView2Controller(host, ctlHandler); ctlHandler->Release();
            if (FAILED(result)) Fail(result);
            return S_OK;
        });
    HRESULT hr = create(nullptr, JoinPath(StateDir(), L"webview2").c_str(), nullptr, envHandler);
    envHandler->Release();
    if (FAILED(hr)) Fail(hr);
}


static bool NativeSetupVisible(){return host&&IsWindow(host)&&fallback&&!closing&&SettingGet(L"wizard_complete")!=L"1";}
bool WebUiTakeNativeBootstrap(){bool result=nativeContinueSetup;nativeContinueSetup=false;return result;}
HBRUSH WebUiNativeControlColor(HDC dc,HWND control){
    if(control!=nativeStatus||!NativeSetupVisible())return nullptr;
    COLORREF ink=NativeLightTheme()?RGB(32,48,74):RGB(233,239,252);
    if(SettingGet(L"theme")==L"custom")ink=ThemeColor(L"theme_ink",ink);
    SetTextColor(dc,ink);SetBkColor(dc,ThemeBackground());
    if(!nativeBackground){nativeBackground=CreateSolidBrush(ThemeBackground());}
    return nativeBackground;
}
void WebUiNativeTick(){
    if(fallback&&!closing&&!nativeRetryAttempted&&SettingGet(L"wizard_complete")==L"1"&&GetEnvironmentVariableW(BRAND_ENV_KAIROS_UI_FALLBACK,nullptr,0)==0){
        auto p=her::SetupSnapshot();const auto* items=p.Get(L"items");bool done=false;
        if(items)for(auto& item:items->a)if(item.Str(L"id")==L"webview2"&&item.Str(L"state")==L"done")done=true;
        if(done&&WebViewCheckRuntime(true).ok){nativeRetryAttempted=true;nativeRestoringUi=true;WebUiClose();WebUiInit(host);return;}
    }
    if(!NativeSetupVisible()){if(nativePrimary)ClearNativeSetup();return;}
    if(!nativePrimary){
        INITCOMMONCONTROLSEX common{sizeof(common),ICC_PROGRESS_CLASS|ICC_STANDARD_CLASSES};InitCommonControlsEx(&common);
        auto create=[&](const wchar_t* cls,const wchar_t* text,unsigned id,DWORD style){
            HWND control=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,0,0,1,1,host,(HMENU)(UINT_PTR)id,GetModuleHandleW(nullptr),nullptr);
            SendMessageW(control,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);return control;
        };
        nativePrimary=create(L"BUTTON",L"下一步",62001,WS_TABSTOP|WS_GROUP|BS_DEFPUSHBUTTON);
        nativeWorkspace=create(L"BUTTON",L"选择工作区",62002,WS_TABSTOP|BS_PUSHBUTTON);
        nativeExit=create(L"BUTTON",L"退出，下次继续",62003,WS_TABSTOP|BS_PUSHBUTTON);
        nativeStatus=create(L"STATIC",L"",62005,SS_CENTER|SS_NOPREFIX);
        nativeProgress=create(PROGRESS_CLASSW,L"",62004,PBS_SMOOTH);
        SetFocus(nativePrimary);
    }
    RECT rect{};GetClientRect(host,&rect);int middle=rect.right/2,top=rect.bottom/2;
    MoveWindow(nativeStatus,32,(std::max)(20,top-145),(std::max)(1,(int)rect.right-64),160,TRUE);
    MoveWindow(nativeProgress,64,top+25,(std::max)(1,(int)rect.right-128),14,TRUE);
    int step=(std::max)(0,_wtoi(SettingGet(L"wizard_step").c_str()));
    bool choose=step==1;
    MoveWindow(nativePrimary,middle-(choose?280:190),top+65,180,34,TRUE);
    MoveWindow(nativeWorkspace,middle-90,top+65,180,34,TRUE);
    MoveWindow(nativeExit,middle+(choose?100:10),top+65,180,34,TRUE);
    ShowWindow(nativeWorkspace,choose?SW_SHOW:SW_HIDE);ShowWindow(nativeProgress,step>=2?SW_SHOW:SW_HIDE);
    auto p=her::SetupSnapshot();bool busy=p.Bool(L"busy");EnableWindow(nativeWorkspace,!busy);
    std::wstring text;
    if(step==0)text=BRAND_NAME L" · 欢迎\n\n先选择工作区，再准备运行组件。\n当前还未下载任何组件。";
    else if(step==1)text=L"工作区\n\n"+WorkspaceDir()+L"\n\n产物、组件和日志将放在这里。点击准备组件后才开始联网。";
    else{
        text=L"准备组件 · 先准备界面运行时\n\n"+p.Str(L"phase",L"等待准备")+L"\n"+std::to_wstring((int)p.Int(L"pct"))+L"%";
        const auto* items=p.Get(L"items");if(items)for(auto& item:items->a)if(item.Str(L"id")==L"webview2"){
            text+=L"\nWebView2 · "+item.Str(L"label",L"等待");break;
        }
        text+=L"\n\n界面运行时就绪后，自动返回六步向导继续准备其余组件。";
        if(p.Str(L"state")==L"failed")text+=L"\n安装未完成，可重试；需要管理员时请允许系统授权。";
    }
    int length=GetWindowTextLengthW(nativeStatus);std::wstring previous(length+1,L'\0');GetWindowTextW(nativeStatus,previous.data(),length+1);previous.resize(length);
    if(text!=previous){SetWindowTextW(nativeStatus,text.c_str());NotifyWinEvent(EVENT_OBJECT_NAMECHANGE,nativeStatus,OBJID_CLIENT,CHILDID_SELF);}
    SetWindowTextW(nativePrimary,step==0?L"下一步":step==1?L"准备组件":busy?L"暂停":p.Str(L"state")==L"failed"?L"重试":L"继续准备");
    if(GetTickCount()-nativeProgressAt>=1000){
        nativeProgressAt=GetTickCount();SendMessageW(nativeProgress,PBM_SETPOS,(WPARAM)p.Int(L"pct"),0);
        SendMessageW(nativeProgress,PBM_SETSTATE,p.Str(L"state")==L"failed"?PBST_ERROR:p.Str(L"state")==L"paused"?PBST_PAUSED:PBST_NORMAL,0);
    }
    bool runtimeDone=false;const auto* items=p.Get(L"items");if(items)for(auto& item:items->a)if(item.Str(L"id")==L"webview2"&&item.Str(L"state")==L"done")runtimeDone=true;
    // The existing debug fallback never installs or silently changes test modes.
    if(step>=2&&runtimeDone&&!busy&&!nativeRetryAttempted&&GetEnvironmentVariableW(BRAND_ENV_KAIROS_UI_FALLBACK,nullptr,0)==0&&WebViewCheckRuntime(true).ok){
        nativeRetryAttempted=true;nativeContinueSetup=true;nativeRestoringUi=true;WebUiClose();WebUiInit(host);
    }
}
bool WebUiNativeCommand(unsigned id){
    if(!NativeSetupVisible()||id<62001||id>62003)return false;
    int step=(std::max)(0,_wtoi(SettingGet(L"wizard_step").c_str()));
    if(id==62003){her::SetupCommand(L"pause");DestroyWindow(host);return true;}
    if(id==62002&&step==1){
        if(her::SetupSnapshot().Bool(L"busy"))return true;
        auto path=PickFolder(host,BRAND_NAME L" · 工作区",WorkspaceDir());
        if(!path.empty()){SetWorkspaceDir(path);SetBaseDir(path);her::SetupTick();}
    }else if(id==62001){
        if(step==0)SettingSet(L"wizard_step",L"1");
        else{
            if(step==1)SettingSet(L"wizard_step",L"2");
            if(her::SetupSnapshot().Bool(L"busy"))her::SetupCommand(L"pause");
            else{
                auto p=her::SetupSnapshot();const auto* items=p.Get(L"items");bool done=false;
                if(items)for(auto& item:items->a)if(item.Str(L"id")==L"webview2"&&item.Str(L"state")==L"done")done=true;
                nativeRetryAttempted=false;
                if(done&&WebViewCheckRuntime(true).ok){
                    // Explicitly opening the full wizard also ends the existing debug preview.
                    SetEnvironmentVariableW(BRAND_ENV_KAIROS_UI_FALLBACK,nullptr);
                }else her::SetupCommand(WebViewCheckRuntime(true).ok?L"start":L"retry",L"webview2");
            }
        }
    }
    WebUiNativeTick();InvalidateRect(host,nullptr,FALSE);return true;
}
bool WebUiNativeMessage(MSG* message){
    if(!NativeSetupVisible()||!nativePrimary)return false;
    if(message->message==WM_KEYDOWN&&message->wParam==VK_ESCAPE){WebUiNativeCommand(62003);return true;}
    if(message->message==WM_KEYDOWN&&message->wParam==VK_RETURN){
        unsigned id=(unsigned)GetDlgCtrlID(GetFocus());WebUiNativeCommand(id>=62001&&id<=62003?id:62001);return true;
    }
    return IsDialogMessageW(host,message)!=FALSE;
}
