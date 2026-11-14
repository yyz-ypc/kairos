// 心核 P0 · 独立可执行入口
#include <winsock2.h>

#include "heart.h"
#include "util.h"

#include <atomic>
#include <cstdio>
#include <string>

namespace {

std::atomic<bool> g_quit{false};

BOOL WINAPI OnCtrl(DWORD type) {
    if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT) {
        g_quit = true;
        return TRUE;
    }
    return FALSE;
}

void Usage() {
    std::printf(
        "heart.exe — 心核 P0 骨架\n"
        "  --home <目录>   数据目录（默认 %%APPDATA%%\\Kairos\\heart）\n"
        "  --port <端口>   监听端口（默认 6190，0 = 系统分配）\n"
        "  --bind <地址>   绑定地址（默认 127.0.0.1）\n"
        "  --check         启动后打印状态并退出（冒烟用）\n");
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    SetConsoleOutputCP(CP_UTF8);
    heart::Config cfg = heart::Config::Defaults();
    std::wstring home;
    bool check = false;

    for (int i = 1; i < argc; i++) {
        std::wstring a = argv[i];
        auto next = [&](std::wstring* out) {
            if (i + 1 >= argc) return false;
            *out = argv[++i];
            return true;
        };
        if (a == L"--home") {
            if (!next(&home)) return Usage(), 2;
        } else if (a == L"--port") {
            std::wstring v;
            if (!next(&v)) return Usage(), 2;
            cfg.port = (int)_wtoi64(v.c_str());
        } else if (a == L"--bind") {
            if (!next(&cfg.bind)) return Usage(), 2;
        } else if (a == L"--check") {
            check = true;
        } else if (a == L"--help" || a == L"-h") {
            Usage();
            return 0;
        } else {
            std::printf("未知参数：%ls\n", a.c_str());
            Usage();
            return 2;
        }
    }

    std::wstring err;
    if (!heart::Start(cfg, home, &err)) {
        std::printf("心核启动失败：%ls\n", err.c_str());
        return 1;
    }
    heart::Status st = heart::Current();
    std::printf("心核已启动：http://127.0.0.1:%d（数据目录 %ls，版本 %ls）\n", st.port, st.home.c_str(),
                st.version.c_str());
    if (check) {
        std::printf("%s\n", heart::util::Narrow(heart::Snapshot().Dump(2)).c_str());
        heart::Stop();
        return 0;
    }

    SetConsoleCtrlHandler(OnCtrl, TRUE);
    while (!g_quit && heart::Running()) Sleep(200);
    heart::Stop();
    std::printf("心核已停止。\n");
    return 0;
}
