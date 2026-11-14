// 时序之子 · jobs 页（自 ui_pages.inc 拆出，纯移动）
static void PaintJobs(Graphics& g) {
    float x0 = S(236), y0 = S(22), W = (float)g_w - x0 - S(24), H = (float)g_h - S(44);
    float th = H * 0.50f;
    RectF table(x0, y0, W, th);
    Card(g, table, C_PANEL, C_LINE);
    Txt(g, L"任务队列", RectF(table.X + S(22), table.Y + S(16), S(160), S(24)), Fnt(TITLE_SIZE, 0, 0), C_TEXT, 0);
    Txt(g, Num((long long)g_jobs.size()) + L" 条", RectF(table.X + S(104), table.Y + S(18), S(80), S(20)),
        Fnt(HINT_SIZE, 1, 0), C_FAINT, 0);
    Button(g, RectF(table.X + table.Width - S(96), table.Y + S(14), S(74), S(28)), L"刷新", "btn:jobs_refresh", 0, true);
    HLine(g, table.X + S(20), table.Y + S(54), table.X + table.Width - S(20), C_LINE2);

    float col1 = S(78), col2 = S(72), col3 = S(112), col5 = S(70);
    float hx = table.X + S(22);
    Txt(g, L"时间", RectF(hx, table.Y + S(58), col1, S(18)), Fnt(HINT_SIZE, 1, 0), C_FAINT, 0);
    Txt(g, L"来源", RectF(hx + col1, table.Y + S(58), col2, S(18)), Fnt(HINT_SIZE, 1, 0), C_FAINT, 0);
    Txt(g, L"BV", RectF(hx + col1 + col2, table.Y + S(58), col3, S(18)), Fnt(HINT_SIZE, 1, 0), C_FAINT, 0);
    float titleW = table.Width - (col1 + col2 + col3 + col5) - S(44);
    Txt(g, L"标题", RectF(hx + col1 + col2 + col3, table.Y + S(58), titleW, S(18)), Fnt(HINT_SIZE, 1, 0), C_FAINT, 0);
    Txt(g, L"状态", RectF(hx + col1 + col2 + col3 + titleW, table.Y + S(58), col5, S(18)), Fnt(HINT_SIZE, 1, 0),
        C_FAINT, 0);

    RectF rows(table.X + S(10), table.Y + S(80), table.Width - S(20), table.Height - S(92));
    g_rcJobs = rows;
    g.SetClip(rows);
    float ry = rows.Y - g_jobsScroll;
    for (int i = 0; i < (int)g_jobs.size(); i++) {
        JobRec& j = g_jobs[i];
        RectF r(rows.X + S(8), ry, rows.Width - S(16), S(36));
        std::string id = "job:" + std::to_string(i);
        if (g_jobSel == i) Round(g, r, S(9), C_ACCENT_S, 0x00000000);
        else if (Hov(id)) Round(g, r, S(9), 0xFFF4F8FF, 0x00000000);
        float ty = ry + S(9);
        Txt(g, Hhmm(j.t), RectF(r.X + S(6), ty, col1, S(18)), Fnt(BODY_SIZE, 0, 1), C_SUB, 0);
        Txt(g, j.source == L"qq" ? L"群" : L"本机", RectF(r.X + S(6) + col1, ty, col2, S(18)), Fnt(HINT_SIZE, 1, 0), C_FAINT,
            0);
        Txt(g, j.bv, RectF(r.X + S(6) + col1 + col2, ty, col3, S(18)), Fnt(HINT_SIZE, 0, 1), C_ACCENT2, 0);
        Txt(g, j.title.empty() ? L"（无标题）" : j.title,
            RectF(r.X + S(6) + col1 + col2 + col3, ty, titleW, S(18)), Fnt(BODY_SIZE, 0, 0), C_TEXT, 0, true);
        Txt(g, j.status == L"ok" ? L"完成" : (j.status == L"fail" ? L"失败" : j.status),
            RectF(r.X + S(4) + col1 + col2 + col3 + titleW, ty, col5, S(18)), Fnt(HINT_SIZE, 1, 0),
            j.status == L"ok" ? C_OK : C_ERR, 2);
        AddHit(r, id);
        ry += S(38);
    }
    g.ResetClip();
    ScrollBar(g, rows, g_jobsScroll, S(38) * g_jobs.size());

    if (g_jobs.empty()) {
        Txt(g, L"队列是空的：看完一条视频就会记在这里。",
            RectF(rows.X, rows.Y + S(30), rows.Width, S(22)), Fnt(BODY_SIZE, 1, 0), C_FAINT, 1);
    }

    RectF detail(x0, y0 + th + S(14), W, H - th - S(14));
    Card(g, detail, C_PANEL, C_LINE);
    Txt(g, L"任务详情", RectF(detail.X + S(22), detail.Y + S(14), detail.Width - S(200), S(24)),
        Fnt(TITLE_SIZE, 0, 0), C_TEXT, 0);
    if (g_jobSel >= 0 && g_jobSel < (int)g_jobs.size()) {
        JobRec& j = g_jobs[g_jobSel];
        Button(g, RectF(detail.X + detail.Width - S(88), detail.Y + S(12), S(66), S(26)), L"文件夹", "btn:open_shotdir",
               0, true);
        Button(g, RectF(detail.X + detail.Width - S(164), detail.Y + S(12), S(66), S(26)), L"复制", "btn:copy_job", 0,
               true);
        (void)j;
    }
    HLine(g, detail.X + S(20), detail.Y + S(48), detail.X + detail.Width - S(20), C_LINE2);
    RectF dtext(detail.X + S(22), detail.Y + S(58), detail.Width - S(44), detail.Height - S(72));
    g_rcWatch = dtext;
    if (!g_jobDetail.empty()) {
        float total = DrawRich(g, g_jobDetail, dtext, g_jobDetailScroll);
        g_jobDetailH = total;
        g_jobDetailView = dtext.Height;
        ScrollBar(g, dtext, g_jobDetailScroll, total);
    } else {
        Txt(g, L"点上面一行看详情", RectF(dtext.X, dtext.Y + S(16), dtext.Width, S(24)), Fnt(BODY_SIZE, 1, 0), C_FAINT, 1);
    }
}

// ---------------------------------------------------------------- 日志
