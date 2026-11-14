// 时序之子 · overview 页（自 ui_pages.inc 拆出，纯移动）
static void DrawHero(Graphics& g, const RectF& r) {
    Round(g, r, S(16), C_PANEL, C_LINE);
    GraphicsPath clip;
    clip.AddArc(r.X, r.Y, S(32), S(32), 180, 90);
    clip.AddArc(r.X + r.Width - S(32), r.Y, S(32), S(32), 270, 90);
    clip.AddArc(r.X + r.Width - S(32), r.Y + r.Height - S(32), S(32), S(32), 0, 90);
    clip.AddArc(r.X, r.Y + r.Height - S(32), S(32), S(32), 90, 90);
    clip.CloseFigure();
    g.SetClip(&clip);

    Image* im = LoadImg(g_artHero);
    float iw = r.Width * 0.62f;
    if (im) {
        float srcW = r.Height / (float)im->GetHeight() * (float)im->GetWidth();
        float iwPx = srcW;
        RectF dst(r.X + r.Width - iw, r.Y, iw + S(2), r.Height);
        float aspect = (float)im->GetWidth() / (float)im->GetHeight();
        float needW = r.Height * aspect;
        dst.X = r.X + r.Width - std::min(needW, r.Width * 0.78f);
        dst.Width = std::min(needW, r.Width * 0.78f);
        g.DrawImage(im, dst, 0.0f, 0.0f, (float)im->GetWidth(), (float)im->GetHeight(), UnitPixel);
        (void)iwPx;
        // 左侧渐隐到白：逐列叠白，避开 GDI+ 线性渐变的直边
        int cols = (int)(dst.Width * 0.58f);
        if (cols > 0) {
            for (int i = 0; i < cols; i += 2) {
                float t = (float)i / (float)cols;
                float k = 1.0f - t;
                int a = (int)(255.0f * k * k * k);
                if (a <= 0) break;
                SolidBrush wb((ARGB)(((unsigned)a << 24) | 0x00FFFFFFu));
                RectF strip(dst.X + (float)i - 1.0f, r.Y, 2.0f, r.Height);
                g.FillRectangle(&wb, strip);
            }
        }
        // 底部薄雾
        LinearGradientBrush mist(PointF(r.X, r.Y + r.Height - S(40)), PointF(r.X, r.Y + r.Height), 0x00FFFFFF,
                                 0xAAFFFFFF);
        g.FillRectangle(&mist, RectF(r.X, r.Y + r.Height - S(40), r.Width, S(40)));
    }
    g.ResetClip();

    float tx = r.X + S(30);
    SYSTEMTIME st;
    GetLocalTime(&st);
    int hh = st.wHour;
    const wchar_t* greet = hh < 5 ? L"凌晨了。" : (hh < 11 ? L"早上好。" : (hh < 14 ? L"中午好。" : (hh < 18 ? L"下午好。" : L"晚上好。")));
    Txt(g, greet, RectF(tx, r.Y + S(30), r.Width * 0.6f, S(44)), Fnt(30, 1, 0), C_TEXT, 0);
    wstring sub = wstring(L"现在 ") + g_clock;
    Txt(g, sub, RectF(tx, r.Y + S(76), r.Width * 0.62f, S(20)), Fnt(BODY_SIZE, 1, 0), C_FAINT, 0);

    static const wchar_t* QUOTES[] = {L"知道了知道了，我这就去看。", L"真拿你没办法……那就看一条吧。",
                                      L"这个封面看着还行，勉为其难。", L"又熬夜？看完就睡。"};
    int qi = (int)(st.wDay) % 4;
    RectF q(tx, r.Y + S(98), r.Width * 0.52f, S(24));
    Butterfly(g, q.X + S(6), q.Y + S(11), S(9), 0x663E5FBF);
    Txt(g, QUOTES[qi], RectF(q.X + S(22), q.Y, q.Width - S(22), S(22)), Fnt(SUBTITLE_SIZE, 1, 0), C_SUB, 0);

    Button(g, RectF(tx, r.Y + r.Height - S(52), S(116), S(34)), L"让她看视频", "btn:goto_bili", 1, false);
    Button(g, RectF(tx + S(126), r.Y + r.Height - S(52), S(104), S(34)), L"打开日记", "btn:diary", 0, false);
}

static void PaintOverview(Graphics& g) {
    float x0 = S(236), y0 = S(22), w = (float)g_w - x0 - S(24), h = (float)g_h - S(44);
    RectF hero(x0, y0, w, S(200));
    DrawHero(g, hero);

    float gap = S(14);
    float cy = hero.Y + hero.Height + gap;
    float cw = (w - gap * 3) / 4.0f;
    float ch = S(112);

    for (int i = 0; i < 3; i++) {
        RectF c(x0 + i * (cw + gap), cy, cw, ch);
        Card(g, c, C_PANEL, C_LINE);
        bool up = i < 2 ? g_svcUp[i] : g_whaleOk;
        const wchar_t* name = i < 2 ? SVCS[i].name : BRAND_RIFT;
        const wchar_t* desc = i < 2 ? SVCS[i].desc : L"看视频";
        wstring port = i < 2 ? SVCS[i].port : L"CLI";
        Dot(g, c.X + S(20), c.Y + S(26), S(4), up ? C_OK : 0xFFC3CCDE);
        Txt(g, name, RectF(c.X + S(34), c.Y + S(14), c.Width - S(44), S(24)), Fnt(TITLE_SIZE, 0, 0), C_TEXT, 0);
        Txt(g, desc, RectF(c.X + S(20), c.Y + S(42), c.Width - S(36), S(18)), Fnt(BODY_SIZE, 1, 0), C_FAINT, 0);
        if (i < 2) {
            wstring st = up ? L"在线 · 端口 " + wstring(port) : L"离线 · 端口 " + wstring(port);
            if (up && g_svcPid[i]) {
                wstring withPid = st + L" · pid " + Num(g_svcPid[i]);
                if (Tw(g, withPid, Fnt(HINT_SIZE, 0, 0)) <= c.Width - S(36)) st = withPid;
            }
            Txt(g, st, RectF(c.X + S(20), c.Y + S(58), c.Width - S(36), S(16)), Fnt(HINT_SIZE, 0, 0),
                up ? C_SUB : C_FAINT, 0, true);
            std::string a = "svc:" + std::to_string(i) + (up ? ":stop" : ":start");
            Button(g, RectF(c.X + S(16), c.Y + ch - S(36), c.Width - S(32), S(26)), up ? L"关闭" : L"启动", a,
                   up ? 2 : 1, true);
        } else {
            Txt(g, g_whaleOk ? L"就绪" : L"没找到插件",
                RectF(c.X + S(20), c.Y + S(58), c.Width - S(36), S(16)), Fnt(HINT_SIZE, 0, 0),
                g_whaleOk ? C_SUB : C_ERR, 0, true);
            Button(g, RectF(c.X + S(16), c.Y + ch - S(36), c.Width - S(32), S(26)), L"去体检", "btn:goto_check", 0,
                   true);
        }
    }

    // 第四格：机器
    {
        RectF c(x0 + 3 * (cw + gap), cy, cw, ch);
        Card(g, c, C_PANEL, C_LINE);
        Txt(g, L"今天", RectF(c.X + S(20), c.Y + S(16), c.Width - S(36), S(20)), Fnt(BODY_SIZE, 1, 0), C_FAINT, 0);
        int n = CountToday();
        Txt(g, Num(n), RectF(c.X + S(18), c.Y + S(36), S(60), S(34)), Fnt(24, 1, 0), C_ACCENT, 0);
        Txt(g, L"条视频", RectF(c.X + S(18) + Tw(g, Num(n), Fnt(24, 1, 0)) + S(6), c.Y + S(48), S(70), S(20)),
            Fnt(BODY_SIZE, 1, 0), C_SUB, 0);
        wchar_t freeBuf[64] = {0};
        ULARGE_INTEGER fbytes;
        if (GetDiskFreeSpaceExW(RootDir().substr(0, 2).c_str(), &fbytes, nullptr, nullptr))
            Txt(g, PrettyBytes((double)fbytes.QuadPart) + L" 可用", RectF(c.X + S(20), c.Y + S(80), c.Width - S(36), S(18)),
                Fnt(HINT_SIZE, 0, 0), C_FAINT, 0);
        (void)freeBuf;
    }

    float by = cy + ch + gap;
    float bh = y0 + h - by;
    float rw = S(292);
    RectF recent(x0, by, w - rw - gap, bh);
    Card(g, recent, C_PANEL, C_LINE);
    Txt(g, L"最近任务", RectF(recent.X + S(22), recent.Y + S(18), S(160), S(22)), Fnt(TITLE_SIZE, 0, 0), C_TEXT, 0);
    Button(g, RectF(recent.X + recent.Width - S(104), recent.Y + S(14), S(84), S(28)), L"看全部", "btn:goto_jobs",
           0, true);
    HLine(g, recent.X + S(20), recent.Y + S(52), recent.X + recent.Width - S(20), C_LINE2);

    if (g_jobs.empty()) {
        Image* em = LoadImg(g_artEmpty);
        float capH = S(26);
        float ih = std::min(recent.Height - S(70) - capH, S(300));
        if (em && ih > S(60)) {
            float iw = ih * (float)em->GetWidth() / (float)em->GetHeight();
            DrawImgFill(g, em, RectF(recent.X + recent.Width / 2 - iw / 2, recent.Y + S(62), iw, ih), 0, 0, 1, 1);
        }
        Txt(g, L"还没有任务，去 B站 页挑一条。", RectF(recent.X, recent.Y + recent.Height - S(34), recent.Width, S(24)),
            Fnt(BODY_SIZE, 1, 0), C_FAINT, 1);
    } else {
        float ry = recent.Y + S(62);
        int shown = 0;
        for (auto& j : g_jobs) {
            if (shown >= 4) break;
            if (ry + S(44) > recent.Y + recent.Height) break;
            Txt(g, Hhmm(j.t), RectF(recent.X + S(22), ry, S(48), S(20)), Fnt(HINT_SIZE, 1, 0), C_FAINT, 0);
            Txt(g, j.title.empty() ? j.bv : j.title, RectF(recent.X + S(76), ry, recent.Width - S(180), S(20)),
                Fnt(BODY_SIZE, 0, 0), C_TEXT, 0, true);
            Txt(g, j.source == L"qq" ? L"群" : L"本机", RectF(recent.X + recent.Width - S(96), ry, S(34), S(20)),
                Fnt(HINT_SIZE, 1, 0), C_FAINT, 2);
            Txt(g, j.status == L"ok" ? L"完成" : (j.status == L"fail" ? L"失败" : j.status),
                RectF(recent.X + recent.Width - S(58), ry, S(38), S(20)), Fnt(HINT_SIZE, 1, 0),
                j.status == L"ok" ? C_OK : C_ERR, 2);
            HLine(g, recent.X + S(20), ry + S(28), recent.X + recent.Width - S(20), C_LINE2);
            ry += S(34);
            shown++;
        }
    }

    RectF quick(x0 + w - rw, by, rw, bh);
    Card(g, quick, C_PANEL, C_LINE);
    Txt(g, L"快捷", RectF(quick.X + S(22), quick.Y + S(18), S(120), S(22)), Fnt(TITLE_SIZE, 0, 0), C_TEXT, 0);
    float qy = quick.Y + S(54);
    float bw2 = (quick.Width - S(44) - S(10)) / 2.0f;
    float bot = quick.Y + quick.Height - S(12);
    auto fits = [&](float y) { return y + S(34) <= bot; };
    if (fits(qy)) {
        Button(g, RectF(quick.X + S(22), qy, bw2, S(34)), L"全部启动", "btn:allstart", 1, true);
        Button(g, RectF(quick.X + S(32) + bw2, qy, bw2, S(34)), L"全部关闭", "btn:allstop", 2, true);
        qy += S(44);
    }
    if (fits(qy)) {
        Button(g, RectF(quick.X + S(22), qy, bw2, S(34)), L"打开日记", "btn:diary", 0, true);
        Button(g, RectF(quick.X + S(32) + bw2, qy, bw2, S(34)), L"日志目录", "btn:logdir", 0, true);
        qy += S(44);
    }
    if (fits(qy)) {
        Button(g, RectF(quick.X + S(22), qy, quick.Width - S(44), S(34)), L"打开 B站 看视频", "btn:goto_bili", 0, true);
        qy += S(44);
    }
    if (fits(qy)) {
        Button(g, RectF(quick.X + S(22), qy, bw2, S(34)), L"ComfyUI", "btn:comfy", 0, true);
        Button(g, RectF(quick.X + S(32) + bw2, qy, bw2, S(34)), L"B站首页", "btn:bilihome", 0, true);
        qy += S(44);
    }
    if (bot - qy > S(20)) {
        Txt(g, L"凭证 " + (g_whaleOk ? wstring(L"已登录") : wstring(L"未就绪")),
            RectF(quick.X + S(22), bot - S(20), quick.Width - S(44), S(18)), Fnt(HINT_SIZE, 0, 0), C_FAINT, 0, true);
    }
}

// ---------------------------------------------------------------- B站
// ---------------------------------------------------------------- B 站：选题 / 看片与读链接 / 结果 / 配置
// 一整页，四个分区。原来两个浮层（rift 配置、问一句）并进来之后，
// 这个页签自己就装得下全部事：挑选题、粘链接、看结果、改配置。
static const wchar_t* kBiliSecName[4] = {L"选题", L"看片与读链接", L"结果", L"配置"};

