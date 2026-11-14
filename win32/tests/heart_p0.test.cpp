// 心核 P0 冒烟测试：启动、HTTP 兼容层、存储、配置、事件总线、停止
#include <winsock2.h>
#include <ws2tcpip.h>

#include "heart/heart.h"
#include "heart/util.h"

#include <cstdio>
#include <string>

namespace {

int g_pass = 0;
int g_fail = 0;

void Check(bool ok, const char* what, int line) {
    if (ok) {
        g_pass++;
    } else {
        g_fail++;
        std::printf("FAIL line %d: %s\n", line, what);
    }
}

#define CHECK(cond) Check((cond), #cond, __LINE__)

struct Reply {
    int code = 0;
    std::string body;
};

Reply HttpGet(int port, const std::string& path) {
    Reply r;
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return r;
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons((u_short)port);
    inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    if (connect(s, (sockaddr*)&a, sizeof(a)) == SOCKET_ERROR) {
        closesocket(s);
        return r;
    }
    std::string req = "GET " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
    send(s, req.data(), (int)req.size(), 0);
    std::string buf;
    char tmp[4096];
    int n = 0;
    while ((n = recv(s, tmp, sizeof(tmp), 0)) > 0) buf.append(tmp, (size_t)n);
    closesocket(s);

    size_t sp = buf.find(' ');
    if (sp != std::string::npos) r.code = std::atoi(buf.c_str() + sp + 1);
    size_t end = buf.find("\r\n\r\n");
    if (end != std::string::npos) r.body = buf.substr(end + 4);
    return r;
}

std::wstring TempHome() {
    wchar_t tmp[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    std::wstring dir = heart::util::Join(tmp, L"heart-p0-" + std::to_wstring(GetCurrentProcessId()));
    heart::util::EnsureDir(dir);
    return dir;
}

}  // namespace

int wmain() {
    SetConsoleOutputCP(CP_UTF8);
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    // 1. 环境变量覆盖默认数据目录
    std::wstring tmp = TempHome();
    SetEnvironmentVariableW(L"KAIROS_STATE_DIR", tmp.c_str());
    CHECK(heart::DefaultHome() == heart::util::Join(tmp, L"heart"));

    // 2. 配置读写
    heart::Config c = heart::Config::Defaults();
    c.port = 6201;
    c.log_level = L"debug";
    std::wstring cfgFile = heart::util::Join(tmp, L"cfg-test.json");
    std::wstring err;
    CHECK(c.Save(cfgFile, &err));
    heart::Config back;
    CHECK(heart::Config::Load(cfgFile, &back, &err));
    CHECK(back.port == 6201 && back.log_level == L"debug");

    // 3. 启动（端口交给系统分配）
    heart::Config run;
    run.port = 0;
    std::wstring home = heart::util::Join(tmp, L"home");
    CHECK(heart::Start(run, home, &err));
    CHECK(heart::Running());
    int port = heart::Current().port;
    CHECK(port > 0);
    CHECK(heart::Current().home == home);

    // 4. HTTP 兼容层
    Reply h = HttpGet(port, "/api/v1/health");
    CHECK(h.code == 200);
    CHECK(h.body.find("\"status\":\"ok\"") != std::string::npos);
    CHECK(h.body.find("0.1.0-p0") != std::string::npos);

    Reply st = HttpGet(port, "/api/v1/state");
    CHECK(st.code == 200);
    CHECK(st.body.find("\"sessions\"") != std::string::npos);
    CHECK(st.body.find("\"messages\"") != std::string::npos);

    Reply miss = HttpGet(port, "/api/v1/nope");
    CHECK(miss.code == 404);
    CHECK(miss.body.find("\"error\"") != std::string::npos);

    // 5. 存储
    heart::Store* db = heart::Db();
    CHECK(db != nullptr && db->Ok());
    long long sid = 0, mid = 0;
    CHECK(db->AddSession(L"第一条会话", &sid, &err));
    CHECK(sid > 0);
    CHECK(db->AddMessage(sid, L"user", L"你好", &mid, &err));
    CHECK(mid > 0);
    long long sessions = 0, messages = 0;
    CHECK(db->CountRows(L"sessions", &sessions));
    CHECK(db->CountRows(L"messages", &messages));
    CHECK(sessions == 1 && messages == 1);
    CHECK(db->SetMeta(L"test.key", L"值", &err));
    std::wstring meta;
    CHECK(db->GetMeta(L"test.key", &meta));
    CHECK(meta == L"值");

    // 6. 事件总线
    int hits = 0;
    int lastPort = 0;
    int sub = heart::Subscribe(L"kernel.stopped", [&](const her::J& j) {
        hits++;
        lastPort = (int)j.Num(L"port", 0);
        CHECK(lastPort > 0);
    });
    CHECK(sub > 0);

    // 7. 快照
    her::J snap = heart::Snapshot();
    CHECK(snap.Bool(L"running"));
    CHECK((int)snap.Num(L"port", 0) == port);

    // 8. 停止
    heart::Stop();
    CHECK(!heart::Running());
    CHECK(hits == 1 && lastPort == port);
    CHECK(heart::Db() == nullptr);
    Reply after = HttpGet(port, "/api/v1/health");
    CHECK(after.code == 0);

    // 9. 重启后数据还在
    heart::Config again;
    again.port = 0;
    CHECK(heart::Start(again, home, &err));
    heart::Status s2 = heart::Current();
    Reply st2 = HttpGet(s2.port, "/api/v1/state");
    CHECK(st2.code == 200);
    CHECK(st2.body.find("\"sessions\":1") != std::string::npos);
    heart::Stop();
    CHECK(hits == 2);

    std::printf("SUMMARY pass=%d fail=%d\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
