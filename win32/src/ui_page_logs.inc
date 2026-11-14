// 时序之子 · logs 页（自 ui_pages.inc 拆出，纯移动）
static void PaintLogs(Graphics& g) {
    float x0 = S(236), y0 = S(22), W = (float)g_w - x0 - S(24), H = (float)g_h - S(44);
    float cx = x0;
    for (int i = 0; i < 3; i++) {
        float w = S(i == 0 ? 132 : 92);
        RectF r(cx, y0, w, S(30));
        std::string id = "logsrc:" + std::to_string(i);
        bool sel = (g_logSrc == i);
        Round(g, r, S(15), sel ? C_ACCENT : C_PANEL, sel ? C_ACCENT : C_LINE);
        Txt(g, LOG_NAMES[i], r, Fnt(BODY_SIZE, 0, 0), sel ? 0xFFFFFFFF : C_SUB, 1);
        AddHit(r, id);
        cx += w + S(8);
    }
    Button(g, RectF(cx, y0, S(74), S(30)), L"刷新", "btn:log_refresh", 0, true);
    Button(g, RectF(cx + S(82), y0, S(104), S(30)), L"日志目录", "btn:logdir", 0, true);

    RectF panel(x0, y0 + S(42), W, H - S(42));
    Card(g, panel, C_PANEL, C_LINE);
    RectF text(panel.X + S(20), panel.Y + S(16), panel.Width - S(40), panel.Height - S(32));
    g_rcLog = text;
    g_logView = text.Height;
    float total = DrawRich(g, g_logText.empty() ? L"（没有日志）" : g_logText, text, g_logScroll);
    g_logH = total;
    ScrollBar(g, text, g_logScroll, total);
}

// ---------------------------------------------------------------- 体检
