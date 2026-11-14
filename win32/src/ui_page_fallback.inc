// 时序之子 · 原生兜底页（WebView2 不可用时）
// 只保留三件事：状态、启停、装 WebView2 的入口。
// 会话、消息、设置这些细节不在这里重做——那需要网页版。
static void PaintFallback(Graphics& g) {
    float W = (float)g_w;
    Txt(g, BRAND_NAME, RectF(S(40), S(34), S(240), S(40)), Fnt(26, 0, 1), C_TEXT, 0);
    Txt(g, APP_VERSION, RectF(S(42), S(72), S(240), S(18)), Fnt(HINT_SIZE, 0, 0), C_FAINT, 0);

    // 三色状态点：青=正常，灯光=待办，浪红=出错。
    bool tideOk = false, tideUp = false, tideManaged = false, tideBusy = false;
    {
        bool parsed = false;
        her::J t = her::J::Parse(her::TideSnapshot(), &parsed);
        if (parsed) {
            tideUp = t.Bool(L"reachable");
            tideManaged = t.Bool(L"managed");
            tideBusy = t.Bool(L"busy");
            tideOk = tideUp && tideManaged;
        }
    }
    auto dot = [&](float x, float y, unsigned color) { Dot(g, x, y, S(4.2f), color); };

    float y = S(120);
    RectF card(S(40), y, W - S(80), S(140));
    Card(g, card, C_PANEL, C_LINE);
    Txt(g, L"现在怎么样", RectF(card.X + S(20), card.Y + S(16), S(200), S(20)), Fnt(16, 0, 1), C_TEXT, 0);
    float ly = card.Y + S(50);
    dot(card.X + S(26), ly + S(9), g_svcUp[0] ? C_OK : C_ERR);
    Txt(g, L"大脑（AstrBot）", RectF(card.X + S(42), ly, S(220), S(20)), Fnt(BODY_SIZE, 0, 0), C_SUB, 0);
    Txt(g, g_svcUp[0] ? L"在跑" : L"没起来", RectF(card.X + card.Width - S(150), ly, S(130), S(20)),
        Fnt(BODY_SIZE, 0, 0), g_svcUp[0] ? C_SUB : C_ERR, 2);
    ly += S(30);
    dot(card.X + S(26), ly + S(9), tideOk ? C_OK : (tideUp ? C_WARN : C_ERR));
    Txt(g, L"潮信（QQ）", RectF(card.X + S(42), ly, S(220), S(20)), Fnt(BODY_SIZE, 0, 0), C_SUB, 0);
    Txt(g, !tideUp ? L"没在运行" : (tideManaged ? L"在跑" : L"在跑（未托管）"),
        RectF(card.X + card.Width - S(200), ly, S(180), S(20)), Fnt(BODY_SIZE, 0, 0), C_SUB, 2);

    // 启停
    float bx = S(40), by = card.Y + card.Height + S(18), bw = S(120), bh = S(34);
    Button(g, RectF(bx, by, bw, bh), L"都叫起来", "btn:allstart", 0, true);
    Button(g, RectF(bx + bw + S(10), by, bw, bh), L"让她们歇着", "btn:allstop", 0, true);
    Button(g, RectF(bx + (bw + S(10)) * 2, by, bw, bh), tideUp ? L"停潮信" : L"起潮信",
           tideUp ? "tide:stop" : "tide:start", 0, false, tideBusy);

    // WebView2
    float wy = by + bh + S(26);
    Txt(g, L"界面", RectF(bx, wy, S(160), S(20)), Fnt(BODY_SIZE, 0, 1), C_TEXT, 0);
    wstring why = WebViewLastError();
    if (why.empty()) why = L"内嵌界面没能启动；现在是原生兜底。";
    Txt(g, why, RectF(bx, wy + S(24), W - S(80), S(44)), Fnt(HINT_SIZE, 0, 0), C_FAINT, 0);
    Button(g, RectF(bx, wy + S(74), S(190), bh), L"安装 WebView2", "up:wbdl", 0, true);
    Txt(g, L"装好后关掉重开" BRAND_NAME L"，完整界面就回来了。", RectF(bx + S(202), wy + S(80), W - S(280), S(24)),
        Fnt(HINT_SIZE, 0, 0), C_FAINT, 0);
}
