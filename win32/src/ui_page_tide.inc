// 时序之子 · tide 页（自 ui_pages.inc 拆出，纯移动）
static void PaintTide(Graphics& g) {
    float x0 = S(236), y0 = S(22), W = (float)g_w - x0 - S(24), H = (float)g_h - S(44);
    bool parsed = false;
    her::J s = her::J::Parse(her::TideSnapshot(), &parsed);
    bool up = parsed && s.Bool(L"reachable");
    bool managed = parsed && s.Bool(L"managed");
    Button(g, RectF(x0, y0, S(96), S(32)), L"启动", "tide:start", 0, true, up);
    Button(g, RectF(x0 + S(104), y0, S(96), S(32)), L"停止", "tide:stop", 0, true, !up);
    Button(g, RectF(x0 + S(208), y0, S(96), S(32)), L"刷新", "tide:refresh", 0, true);

    RectF panel(x0, y0 + S(44), W, H - S(44));
    Card(g, panel, C_PANEL, C_LINE);
    RectF text(panel.X + S(22), panel.Y + S(20), panel.Width - S(44), panel.Height - S(40));
    std::wstring body;
    body += up ? L"潮信：在运行" : L"潮信：没在运行";
    body += managed ? L"（托管，控制通道正常）" : (up ? L"（独立运行，这里只能启停）" : L"");
    body += L"\r\n\r\n";
    std::wstring err = parsed ? s.Str(L"error") : std::wstring();
    if (!err.empty()) body += L"提示：" + err + L"\r\n\r\n";
    body += L"这个页面在原生兜底模式里只显示状态和启停。要看会话、发消息、改设置，"
            L"需要装 Microsoft Edge WebView2 Runtime。";
    DrawRich(g, body, text, 0);
}

