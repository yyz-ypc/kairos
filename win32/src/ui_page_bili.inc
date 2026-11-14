// 时序之子 · bili 页（自 ui_pages.inc 拆出，纯移动）
static void PaintBiliChips(Graphics& g, float x, float y) {
    float cx = x;
    for (int i = 0; i < 4; i++) {
        const wchar_t* nm = kBiliSecName[i];
        float cw = S(88) + S(8) * (float)wcslen(nm) / 4.0f;
        if (i == 1) cw = S(132);
        if (i == 0 || i == 2 || i == 3) cw = S(84);
        RectF r(cx, y, cw, S(32));
        std::string id = "bsec:" + std::to_string(i);
        bool sel = (g_bSec == i);
        Round(g, r, S(9), sel ? C_ACCENT_S : (Hov(id) ? 0xFFF4F8FF : C_PANEL), sel ? 0x00000000 : C_LINE);
        Txt(g, nm, r, Fnt(SUBTITLE_SIZE, sel ? 0 : 1, 0), sel ? C_ACCENT : C_SUB, 1);
        AddHit(r, id);
        cx += cw + S(7);
    }
}

// ---- 分区一：选题
static void PaintBiliFeed(Graphics& g, const RectF& body) {
    Card(g, body, C_PANEL, C_LINE);
    Txt(g, L"今晚看什么", RectF(body.X + S(22), body.Y + S(16), S(200), S(26)), Fnt(TITLE_SIZE, 0, 0), C_TEXT, 0);
    wstring note = g_feedBusy ? wstring(L"正在捞…") : (g_feedNote.empty() ? wstring(L"她按你的口味挑的") : g_feedNote);
    Txt(g, note, RectF(body.X + S(22), body.Y + S(46), body.Width - S(260), S(18)), Fnt(HINT_SIZE, 1, 0), C_FAINT, 0, true);
    Button(g, RectF(body.X + body.Width - S(112), body.Y + S(14), S(88), S(30)), g_feedBusy ? L"…" : L"换一批",
           "btn:feed", 0, true, !g_feedBusy);

    const float gap = S(12);
    const float cw = (body.Width - S(40) - gap) / 2;
    const float ch = S(94);
    RectF list(body.X + S(10), body.Y + S(72), body.Width - S(20), body.Height - S(72) - S(52));
    g_rcFeed = list;
    int rows = ((int)g_feed.size() + 1) / 2;
    float totalH = rows > 0 ? rows * (ch + gap) : 0;
    float maxScroll = std::max(0.0f, totalH - list.Height);
    if (g_feedScroll > maxScroll) g_feedScroll = maxScroll;
    if (g_feedScroll < 0) g_feedScroll = 0;

    g.SetClip(list);
    for (int i = 0; i < (int)g_feed.size(); i++) {
        FeedItem& f = g_feed[i];
        float cx = list.X + S(10) + (i % 2) * (cw + gap);
        float cy = list.Y + S(3) + (i / 2) * (ch + gap) - g_feedScroll;
        RectF r(cx, cy, cw, ch);
        if (r.Y + r.Height < list.Y - S(4) || r.Y > list.Y + list.Height + S(4)) continue;
        std::string id = "feed:" + std::to_string(i);
        bool sel = (g_feedSel == i);
        Round(g, r, S(11), sel ? C_ACCENT_S : (Hov(id) ? 0xFFF7FAFF : C_PANEL2), sel ? C_ACCENT2 : C_LINE2);
        TxtWrap(g, f.title, RectF(r.X + S(14), r.Y + S(12), r.Width - S(84), S(38)), Fnt(SUBTITLE_SIZE, 0, 0), C_TEXT);
        if (!f.dur.empty()) {
            int sec = _wtoi(f.dur.c_str());
            wstring dur = Num(sec / 60) + L":" + Pad2(sec % 60);
            RectF db(r.X + r.Width - S(58), r.Y + S(10), S(46), S(18));
            Round(g, db, S(9), 0x22000000, 0x00000000);
            Txt(g, dur, db, Fnt(MICRO_SIZE, 0, 0), C_SUB, 1);
        }
        wstring meta = f.up;
        if (!f.play.empty()) meta += (meta.empty() ? L"" : L"  ·  ") + f.play + L"播放";
        if (!f.dm.empty()) meta += L"  ·  " + f.dm + L"弹幕";
        if (meta.empty()) meta = f.bv;
        Txt(g, meta, RectF(r.X + S(14), r.Y + S(56), r.Width - S(28), S(18)), Fnt(HINT_SIZE, 1, 0), C_FAINT, 0, true);
        Txt(g, f.bv, RectF(r.X + S(14), r.Y + r.Height - S(20), r.Width - S(28), S(14)), Fnt(MICRO_SIZE, 1, 1), C_FAINT, 0, true);
        AddHit(r, id);
    }
    g.ResetClip();
    ScrollBar(g, list, g_feedScroll, totalH);

    if (g_feed.empty()) {
        Image* em = LoadImg(g_artEmpty);
        float ih = std::min(list.Height - S(80), S(220) * g_scale);
        if (em && ih > S(70)) {
            float iw = ih * (float)em->GetWidth() / (float)em->GetHeight();
            DrawImgFill(g, em, RectF(list.X + list.Width / 2 - iw / 2, list.Y + S(10), iw, ih), 0, 0, 1, 1);
        }
        Txt(g, g_feedBusy ? L"正在筛…" : L"点右上「换一批」捞选题", RectF(list.X, list.Y + list.Height - S(46), list.Width, S(24)),
            Fnt(BODY_SIZE, 1, 0), C_FAINT, 1);
    }

    // 底部：当前工作区（产物落这里，点一下可以换）
    float wy = body.Y + body.Height - S(34);
    HLine(g, body.X + S(20), wy - S(10), body.X + body.Width - S(20), C_LINE2);
    Txt(g, L"工作区", RectF(body.X + S(20), wy, S(52), S(16)), Fnt(MICRO_SIZE, 1, 0), C_FAINT, 0);
    Txt(g, WorkspaceDir(), RectF(body.X + S(20) + S(50), wy, body.Width - S(190), S(16)), Fnt(MICRO_SIZE, 1, 0), C_SUB, 0,
        true);
    RectF b(body.X + body.Width - S(96), wy - S(5), S(76), S(26));
    bool hov = Hov("ws:pick");
    Round(g, b, S(8), hov ? C_ACCENT_S : 0x00000000, 0x00000000);
    Txt(g, L"换工作区", b, Fnt(MICRO_SIZE, hov ? 0 : 1, 0), hov ? C_ACCENT : C_ACCENT2, 1);
    AddHit(b, "ws:pick");
    Button(g, RectF(body.X + body.Width - S(196), wy - S(5), S(88), S(26)), L"打开产物", "bfiles", 0, true);
}

// ---- 分区二：看片与读链接
static void PaintBiliWork(Graphics& g, const RectF& body) {
    RectF top(body.X, body.Y, body.Width, S(152));
    Card(g, top, C_PANEL, C_LINE);
    Txt(g, L"看片与读链接", RectF(top.X + S(22), top.Y + S(14), S(200), S(24)), Fnt(TITLE_SIZE, 0, 0), C_TEXT, 0);
    Txt(g, L"粘 B 站 链接 / BV、普通网页链接或分享文本，回车就开跑", RectF(top.X + S(130), top.Y + S(20), top.Width - S(160), S(18)),
        Fnt(HINT_SIZE, 1, 0), C_FAINT, 0, true);

    RectF ed(top.X + S(22), top.Y + S(48), top.Width - S(22) * 2 - S(128), S(52));
    g_rcInput = ed;
    Round(g, ed, S(11), g_editFocus ? 0xFFFFFFFF : C_PANEL2, g_editFocus ? C_ACCENT2 : C_LINE);
    float txx = ed.X + S(16);
    if (g_bvInput.empty()) {
        Txt(g, L"粘 B站 链接 / BV，或者别的网页链接、分享文本…（Ctrl+V / 右键）", RectF(txx, ed.Y, ed.Width - S(24), ed.Height),
            Fnt(SUBTITLE_SIZE, 1, 0), 0xFFAAB5C9, 0, true);
    } else {
        RectF tr(txx, ed.Y, ed.Width - S(24), ed.Height);
        if (g_inputSelAll)
            Round(g, RectF(ed.X + S(3), ed.Y + S(3), ed.Width - S(6), ed.Height - S(6)), S(9), 0x2E3E5FBF, 0x00000000);
        Txt(g, g_bvInput, tr, Fnt(SUBTITLE_SIZE, 0, 0), g_inputSelAll ? C_ACCENT : C_TEXT, 0, true);
    }
    AddHit(ed, "edit:bv");
    Button(g, RectF(ed.X + ed.Width + S(12), ed.Y, S(116), S(52)), g_watchBusy ? L"看着呢…" : L"让她看", "btn:watch", 1,
           false, !g_watchBusy);

    wstring status = g_watchBusy ? g_watchStatus
                                 : (g_hasResult ? (g_linkMode ? L"读完了 · 结果在「结果」分区" : L"看完了 · 结果在「结果」分区")
                                                : (g_linkMode ? L"读标题和正文摘要；B 站链接照旧走看片" : L"读字幕、弹幕、热评，再看几帧画面"));
    Txt(g, status, RectF(top.X + S(22), top.Y + S(108), top.Width - S(44), S(18)), Fnt(HINT_SIZE, 1, 0),
        g_watchBusy ? C_ACCENT : C_FAINT, 0, true);
    if (g_watchBusy) {
        RectF bar(top.X + S(22), top.Y + S(132), top.Width - S(44), S(3));
        Round(g, bar, S(1.5f), 0x1A3E5FBF, 0, 0);
        float wd = (top.Width - S(44)) * (g_watchPct / 100.0f);
        if (wd > S(3)) Round(g, RectF(bar.X, bar.Y, wd, S(3)), S(1.5f), C_ACCENT, 0, 0);
    } else {
        HLine(g, top.X + S(22), top.Y + S(133), top.X + top.Width - S(22), C_LINE2);
    }

    // 最近一次（结果分区是全文，这里只给一眼）
    float ly = top.Y + top.Height + S(14);
    RectF last(body.X, ly, body.Width, body.Height - (ly - body.Y) - S(66));
    Card(g, last, C_PANEL, C_LINE);
    Txt(g, L"最近一次", RectF(last.X + S(22), last.Y + S(14), S(160), S(22)), Fnt(SUBTITLE_SIZE, 0, 0), C_TEXT, 0);
    if (!g_hasResult) {
        Image* em = LoadImg(g_artEmpty);
        float ih = std::min(last.Height - S(130), S(190) * g_scale);
        float blockH = ih + S(46);
        float iy = last.Y + S(40) + std::max(0.0f, (last.Height - S(140) - blockH) / 2.0f);
        if (em && ih > S(64)) {
            float iw = ih * (float)em->GetWidth() / (float)em->GetHeight();
            DrawImgFill(g, em, RectF(last.X + last.Width / 2 - iw / 2, iy, iw, ih), 0, 0, 1, 1);
        }
        wstring word = g_linkMode ? wstring(L"她还没读过网页") : wstring(L"她还没看过视频");
        Txt(g, word, RectF(last.X, iy + ih + S(10), last.Width, S(22)), Fnt(BODY_SIZE, 0, 0), C_SUB, 1);
        Txt(g, L"上面粘一条，或者去「选题」里挑一个", RectF(last.X, iy + ih + S(34), last.Width, S(18)), Fnt(MICRO_SIZE, 1, 0),
            C_FAINT, 1);
    } else {
        wstring kind = g_linkMode ? L"读链接" : L"看视频";
        Chip(g, RectF(last.X + S(100), last.Y + S(16), S(58), S(20)), kind, C_ACCENT, C_ACCENT_S);
        Txt(g, g_result.title.empty() ? L"（无标题）" : g_result.title,
            RectF(last.X + S(22), last.Y + S(46), last.Width - S(44), S(26)), Fnt(TITLE_SIZE, 0, 0), C_TEXT, 0, true);
        wstring meta = g_result.up;
        if (g_result.seconds > 0) meta += (meta.empty() ? L"" : L"  ·  ") + Num((long long)g_result.seconds) + L" 秒";
        Txt(g, meta, RectF(last.X + S(22), last.Y + S(76), last.Width - S(44), S(18)), Fnt(HINT_SIZE, 1, 0), C_FAINT, 0, true);
        // 摘要：优先用站点写的 description，没有就取正文前两行
        wstring brief;
        for (auto& ln : SplitLines(g_result.raw)) {
            if (ln.find(L"══════") != wstring::npos) continue;
            if (ln.rfind(L"标题:", 0) == 0 || ln.rfind(L"来源:", 0) == 0 || ln.rfind(L"地址:", 0) == 0 ||
                ln.rfind(L"状态:", 0) == 0 || ln.rfind(L"耗时:", 0) == 0 || ln.rfind(L"注意:", 0) == 0)
                continue;
            if (Trim(ln).empty()) continue;
            brief = Trim(ln);
            break;
        }
        if (brief.size() > 160) brief = brief.substr(0, 160) + L"…";
        TxtWrap(g, brief, RectF(last.X + S(22), last.Y + S(102), last.Width - S(44), std::min(S(60), last.Height - S(150))),
                Fnt(BODY_SIZE, 1, 0), C_SUB);
        Button(g, RectF(last.X + last.Width - S(112), last.Y + S(46), S(90), S(30)), L"看全文", "bsec:2", 0, true);
        Button(g, RectF(last.X + last.Width - S(112), last.Y + S(82), S(90), S(30)), L"复制全文", "btn:copy", 0, true);
        Button(g, RectF(last.X + last.Width - S(112), last.Y + S(118), S(90), S(30)), L"打开文件夹", "btn:open_shotdir", 0,
               true);
    }

    // 一键入口：配置两个、诊断四个、产物
    float qy = body.Y + body.Height - S(54);
    float sx = body.X;
    Button(g, RectF(sx, qy, S(120), S(32)), L"rift 配置", "bcfg", 0, true);
    sx += S(128);
    Button(g, RectF(sx, qy, S(104), S(32)), L"链接与应用", "lcfg", 0, true);
    sx += S(112);
    const wchar_t* diag[] = {L"登录", L"配额", L"风控", L"自检"};
    const int dact[] = {0, 2, 3, 4};
    for (int i = 0; i < 4; i++) {
        Button(g, RectF(sx, qy, S(58), S(32)), diag[i], "bchk:" + std::to_string(dact[i]), 0, true);
        sx += S(64);
    }
    if (sx + S(64) < body.X + body.Width) Button(g, RectF(sx, qy, S(60), S(32)), L"产物", "bfiles", 0, true);
}

// ---- 分区三：结果（「问一句」现在也长在这里，不再是浮层）
static void PaintBiliResult(Graphics& g, const RectF& body) {
    Card(g, body, C_PANEL, C_LINE);
    wstring kind = g_linkMode ? L"读链接" : L"看视频";
    Txt(g, L"结果", RectF(body.X + S(22), body.Y + S(12), S(80), S(24)), Fnt(TITLE_SIZE, 0, 0), C_TEXT, 0);
    if (g_hasResult) Chip(g, RectF(body.X + S(76), body.Y + S(15), S(58), S(20)), kind, C_ACCENT, C_ACCENT_S);

    float bx = body.X + body.Width;
    Button(g, RectF(bx - S(98), body.Y + S(10), S(84), S(28)), L"问一句", "bdsh", 0, true, g_hasResult || g_dshOpen);
    Button(g, RectF(bx - S(190), body.Y + S(10), S(84), S(28)), L"复制全文", "btn:copy", 0, true, g_hasResult);
    Button(g, RectF(bx - S(290), body.Y + S(10), S(92), S(28)), L"打开文件夹", "btn:open_shotdir", 0, true, g_hasResult);
    HLine(g, body.X + S(20), body.Y + S(48), bx - S(20), C_LINE2);

    RectF area(body.X + S(10), body.Y + S(54), body.Width - S(20), body.Height - S(54) - S(10));
    float askW = g_dshOpen ? std::min(S(400), area.Width * 0.42f) : 0;
    RectF textArea = area;
    if (askW > 0) textArea.Width = area.Width - askW - S(14);
    g_rcWatch = textArea;
    g_watchViewH = textArea.Height;

    if (!g_hasResult) {
        Image* em = LoadImg(g_artEmpty);
        float ih = std::min(area.Height - S(190), S(210) * g_scale);
        float blockH = ih + S(96);
        float iy = area.Y + std::max(S(12), (area.Height - blockH) / 2.0f);
        if (em && ih > S(80)) {
            float iw = ih * (float)em->GetWidth() / (float)em->GetHeight();
            DrawImgFill(g, em, RectF(area.X + area.Width / 2 - iw / 2, iy, iw, ih), 0, 0, 1, 1);
        }
        wstring emptyWord = g_linkMode ? (g_watchBusy ? L"正在读… " : L"她还没读过网页")
                                       : (g_watchBusy ? L"正在看… " : L"她还没看过视频");
        if (g_watchBusy) emptyWord += Num((long long)g_watchPct) + L"%";
        Txt(g, emptyWord, RectF(area.X, iy + ih + S(12), area.Width, S(24)), Fnt(TITLE_SIZE, 0, 0), C_SUB, 1);
        Button(g, RectF(area.X + area.Width / 2 - S(60), iy + ih + S(46), S(120), S(32)), L"去粘一条",
               "bsec:1", 1, true);
    } else {
        float total = DrawRich(g, g_result.raw, textArea, g_watchScroll);
        g_watchContentH = total;
        ScrollBar(g, textArea, g_watchScroll, total);
        if (askW <= 0) {
            bool hasArt = (!g_result.sheet.empty() || !g_result.cover.empty());
            if (hasArt) {
                float artW = std::min(S(260), area.Width * 0.3f);
                float ax = area.X + area.Width - artW;
                float ay = area.Y;
                Image* cover = g_result.cover.empty() ? nullptr : LoadImg(g_result.cover);
                if (cover) {
                    float cw = artW, chh = cw * 0.5625f;
                    Round(g, RectF(ax, ay, cw, chh), S(10), C_PANEL2, C_LINE);
                    GraphicsPath cp;
                    cp.AddArc(ax, ay, S(20), S(20), 180, 90);
                    cp.AddArc(ax + cw - S(20), ay, S(20), S(20), 270, 90);
                    cp.AddArc(ax + cw - S(20), ay + chh - S(20), S(20), S(20), 0, 90);
                    cp.AddArc(ax, ay + chh - S(20), S(20), S(20), 90, 90);
                    cp.CloseFigure();
                    g.SetClip(&cp);
                    DrawImgFill(g, cover, RectF(ax, ay, cw, chh), 0, 0, 1, 1);
                    g.ResetClip();
                    AddHit(RectF(ax, ay, cw, chh), "btn:open_cover");
                    ay += chh + S(10);
                }
                Image* sheet = g_result.sheet.empty() ? nullptr : LoadImg(g_result.sheet);
                if (sheet) {
                    float sh = artW;
                    float ih2 = sh * (float)sheet->GetHeight() / (float)sheet->GetWidth();
                    if (ay + ih2 > area.Y + area.Height) ih2 = std::max(S(80), area.Y + area.Height - ay);
                    Round(g, RectF(ax, ay, sh, ih2), S(10), C_PANEL2, C_LINE);
                    g.SetClip(RectF(ax, ay, sh, ih2));
                    DrawImgFill(g, sheet, RectF(ax, ay, sh, ih2), 0, 0, 1, 1);
                    g.ResetClip();
                    AddHit(RectF(ax, ay, sh, ih2), "btn:open_sheet");
                }
            }
        }
    }
    if (askW > 0) {
        RectF ask(area.X + area.Width - askW, area.Y, askW, area.Height);
        PaintDshAsk(g, ask);
    }
}

// ---- 分区四：配置（rift / 链接与应用）
static void PaintBiliCfgArea(Graphics& g, const RectF& body) {
    const wchar_t* nm[2] = {BRAND_RIFT, L"链接与应用"};
    float cx = body.X;
    for (int i = 0; i < 2; i++) {
        float cw = i == 0 ? S(112) : S(104);
        RectF r(cx, body.Y, cw, S(30));
        std::string id = "bcfgsub:" + std::to_string(i);
        bool sel = (g_bCfgSub == i);
        Round(g, r, S(9), sel ? C_ACCENT_S : (Hov(id) ? 0xFFF4F8FF : C_PANEL), sel ? 0x00000000 : C_LINE);
        Txt(g, nm[i], r, Fnt(HINT_SIZE, sel ? 0 : 1, 0), sel ? C_ACCENT : C_SUB, 1);
        AddHit(r, id);
        cx += cw + S(8);
    }
    Txt(g, L"改完点保存；旧配置备份成 .bak-时间", RectF(body.X + S(240), body.Y + S(6), body.Width - S(240), S(18)),
        Fnt(MICRO_SIZE, 1, 0), C_FAINT, 0, true);
    RectF card(body.X, body.Y + S(38), body.Width, body.Height - S(38));
    if (g_bCfgSub == 0) PaintBiliCfg(g, card);
    else PaintLinkCfg(g, card);
}

static void PaintBili(Graphics& g) {
    float x0 = S(236), y0 = S(22), W = (float)g_w - x0 - S(24), H = (float)g_h - S(44);
    g_rcFeed = RectF();
    g_rcInput = RectF();
    g_rcWatch = RectF();

    PaintBiliChips(g, x0, y0);
    {
        wstring right = g_watchBusy ? (g_watchStatus.empty() ? wstring(L"正在跑…") : g_watchStatus)
                                    : (g_hasResult ? (g_linkMode ? wstring(L"刚读完一条链接") : wstring(L"刚看完一条视频"))
                                                   : wstring(L"粘链接、挑选题，都从这一页开始"));
        Txt(g, right, RectF(x0 + S(390), y0 + S(7), W - S(390), S(18)), Fnt(HINT_SIZE, 1, 0), C_FAINT, 2, true);
    }
    RectF body(x0, y0 + S(42), W, H - S(42));
    switch (g_bSec) {
        case 0: PaintBiliFeed(g, body); break;
        case 2: PaintBiliResult(g, body); break;
        case 3: PaintBiliCfgArea(g, body); break;
        default: PaintBiliWork(g, body); break;
    }
}

// ---------------------------------------------------------------- 队列
