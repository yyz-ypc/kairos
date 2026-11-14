// 时序之子 · check 页（自 ui_pages.inc 拆出，纯移动）
static void PaintCheck(Graphics& g) {
    float x0 = S(236), y0 = S(22), W = (float)g_w - x0 - S(24), H = (float)g_h - S(44);
    static const wchar_t* BTN[] = {L"会话", L"依赖", L"限额", L"风控", L"自检"};
    float cx = x0;
    for (int i = 0; i < 5; i++) {
        float w = S(i == 2 || i == 3 ? 108 : 112);
        std::string id = "check:" + std::to_string(i);
        Button(g, RectF(cx, y0, w, S(32)), g_checkBusy && g_checkAction == i ? L"跑着呢…" : BTN[i], id,
               g_checkAction == i ? 1 : 0, true, !g_checkBusy);
        cx += w + S(8);
    }
    RectF panel(x0, y0 + S(44), W, H - S(44));
    Card(g, panel, C_PANEL, C_LINE);
    RectF text(panel.X + S(20), panel.Y + S(16), panel.Width - S(40), panel.Height - S(32));
    g_rcCheck = text;
    g_checkView = text.Height;
    float total = DrawRich(g, g_checkText, text, g_checkScroll);
    g_checkH = total;
    ScrollBar(g, text, g_checkScroll, total);
}

// ---------------------------------------------------------------- 绘制入口
#include "ui_maint.inc"
#include "ui_setup.inc"
#include "ui_update.inc"
#include "ui_docs.inc"
#include "ui_help.inc"
#include "ui_bili_cfg.inc"
#include "ui_link_cfg.inc"
#include "ui_dsh.inc"
#include "ui_bili_agent.inc"
#include "ui_center.inc"

// ---------------------------------------------------------------- 潮信（原生兜底只做状态和起停）
