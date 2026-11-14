// 心核 P0 · 内核生命周期与 /api/v1 兼容层
#include "heart.h"
#include "util.h"

#include <cstdlib>
#include <ctime>
#include <mutex>

namespace heart {

namespace {

std::mutex g_mu;
std::unique_ptr<Store> g_store;
std::unique_ptr<Server> g_server;
Status g_status;
bool g_running = false;

her::J Envelope(bool ok, const her::J& data, const std::wstring& message) {
    her::J j = her::JObj();
    j.Set(L"status", her::J(ok ? L"ok" : L"error"));
    if (!message.empty()) j.Set(L"message", her::J(message));
    if (!data.IsNull()) j.Set(L"data", data);
    return j;
}

Response Json(const her::J& j, int code = 200) {
    Response r;
    r.code = code;
    r.body = j.Dump();
    return r;
}

long long UptimeSeconds() { return g_status.started_at ? (long long)std::time(nullptr) - g_status.started_at : 0; }

}  // namespace

std::wstring Version() { return L"0.1.0-p0"; }

Config Config::Defaults() { return Config(); }

bool Config::Load(const std::wstring& file, Config* out, std::wstring* err) {
    if (!out) return false;
    std::string raw;
    if (!util::ReadFileUtf8(file, &raw)) {
        if (err) *err = L"读不到 " + file;
        return false;
    }
    bool ok = false;
    her::J j = her::J::Parse(util::Widen(raw), &ok);
    if (!ok || !j.Obj()) {
        if (err) *err = L"配置文件不是合法 JSON：" + file;
        return false;
    }
    Config c;
    c.port = (int)j.Num(L"port", c.port);
    c.bind = j.Str(L"bind", c.bind);
    c.auto_start = j.Bool(L"auto_start", c.auto_start);
    c.log_level = j.Str(L"log_level", c.log_level);
    *out = c;
    return true;
}

bool Config::Save(const std::wstring& file, std::wstring* err) const {
    her::J j = her::JObj();
    j.Set(L"port", her::J(port));
    j.Set(L"bind", her::J(bind));
    j.Set(L"auto_start", her::J(auto_start));
    j.Set(L"log_level", her::J(log_level));
    if (!util::WriteFileAtomic(file, util::Narrow(j.Dump(2)))) {
        if (err) *err = L"写不了 " + file;
        return false;
    }
    return true;
}

std::wstring DefaultHome() {
    wchar_t buf[MAX_PATH * 2];
    DWORD n = GetEnvironmentVariableW(L"KAIROS_STATE_DIR", buf, (DWORD)(sizeof(buf) / sizeof(buf[0])));
    if (n > 0 && n < sizeof(buf) / sizeof(buf[0])) return util::Join(buf, L"heart");
    n = GetEnvironmentVariableW(L"APPDATA", buf, (DWORD)(sizeof(buf) / sizeof(buf[0])));
    if (n > 0 && n < sizeof(buf) / sizeof(buf[0])) return util::Join(util::Join(buf, L"Kairos"), L"heart");
    return L"heart";
}

bool Start(const Config& in, const std::wstring& home, std::wstring* err) {
    std::unique_lock<std::mutex> lk(g_mu);
    if (g_running) return true;

    Config cfg = in;
    std::wstring h = home.empty() ? DefaultHome() : home;
    if (!util::EnsureDir(h)) {
        if (err) *err = L"建不了数据目录：" + h;
        return false;
    }
    std::wstring cfgFile = util::Join(h, L"heart.json");
    Config fromFile;
    if (Config::Load(cfgFile, &fromFile, nullptr)) {
        // 调用方显式给的端口/绑定优先，其余沿用文件
        cfg.port = in.port;
        cfg.bind = in.bind;
        cfg.auto_start = in.auto_start;
        cfg.log_level = fromFile.log_level;
    }
    cfg.Save(cfgFile, nullptr);

    g_store.reset(new Store());
    if (!g_store->Open(util::Join(h, L"heart.db"), err)) {
        g_store.reset();
        return false;
    }

    g_server.reset(new Server());
    g_server->Add(L"GET", L"/api/v1/health", [](const Request&) {
        her::J d = her::JObj();
        d.Set(L"ok", her::J(true));
        d.Set(L"version", her::J(Version()));
        d.Set(L"uptime", her::J((double)UptimeSeconds()));
        return Json(Envelope(true, d, L""));
    });
    g_server->Add(L"GET", L"/api/v1/version", [](const Request&) {
        her::J d = her::JObj();
        d.Set(L"version", her::J(Version()));
        return Json(Envelope(true, d, L""));
    });
    g_server->Add(L"GET", L"/api/v1/state", [](const Request&) {
        long long sessions = 0, messages = 0;
        Store* db = Db();
        if (db) {
            db->CountRows(L"sessions", &sessions);
            db->CountRows(L"messages", &messages);
        }
        her::J d = her::JObj();
        d.Set(L"running", her::J(true));
        d.Set(L"port", her::J((double)Current().port));
        d.Set(L"home", her::J(Current().home));
        d.Set(L"version", her::J(Version()));
        d.Set(L"db", her::J(db ? db->Path() : L""));
        d.Set(L"sessions", her::J((double)sessions));
        d.Set(L"messages", her::J((double)messages));
        d.Set(L"started_at", her::J((double)Current().started_at));
        return Json(Envelope(true, d, L""));
    });
    g_server->Add(L"GET", L"/api/v1/ping", [](const Request&) {
        her::J d = her::JObj();
        d.Set(L"pong", her::J(true));
        return Json(Envelope(true, d, L""));
    });

    if (!g_server->Start(cfg.bind, cfg.port, err)) {
        g_server.reset();
        g_store.reset();
        return false;
    }

    g_status.running = true;
    g_status.port = g_server->Port();
    g_status.home = h;
    g_status.version = Version();
    g_status.started_at = (long long)std::time(nullptr);
    g_running = true;
    g_store->SetMeta(L"kernel.started_at", std::to_wstring(g_status.started_at), nullptr);
    g_store->SetMeta(L"kernel.version", Version(), nullptr);

    long long sessions = 0, messages = 0;
    g_store->CountRows(L"sessions", &sessions);
    g_store->CountRows(L"messages", &messages);
    her::J snap = her::JObj();
    snap.Set(L"running", her::J(true));
    snap.Set(L"port", her::J((double)g_status.port));
    snap.Set(L"home", her::J(h));
    snap.Set(L"version", her::J(Version()));
    snap.Set(L"started_at", her::J((double)g_status.started_at));
    snap.Set(L"sessions", her::J((double)sessions));
    snap.Set(L"messages", her::J((double)messages));
    lk.unlock();
    Publish(L"kernel.started", snap);
    return true;
}

void Stop() {
    Server* srv = nullptr;
    Store* store = nullptr;
    Status old;
    {
        std::lock_guard<std::mutex> lk(g_mu);
        if (!g_running) return;
        g_running = false;
        srv = g_server.release();
        store = g_store.release();
        old = g_status;
        g_status = Status();
    }
    // 先停监听、再关库；两步都不能握着 g_mu，否则处理中的请求会卡死 join。
    if (srv) {
        srv->Stop();
        delete srv;
    }
    if (store) {
        store->Close();
        delete store;
    }
    her::J j = her::JObj();
    j.Set(L"port", her::J((double)old.port));
    j.Set(L"home", her::J(old.home));
    Publish(L"kernel.stopped", j);
}

bool Running() {
    std::lock_guard<std::mutex> lk(g_mu);
    return g_running;
}

Status Current() {
    std::lock_guard<std::mutex> lk(g_mu);
    return g_status;
}

her::J Snapshot() {
    std::lock_guard<std::mutex> lk(g_mu);
    her::J j = her::JObj();
    j.Set(L"running", her::J(g_running));
    j.Set(L"port", her::J((double)g_status.port));
    j.Set(L"home", her::J(g_status.home));
    j.Set(L"version", her::J(g_status.version.empty() ? Version() : g_status.version));
    j.Set(L"started_at", her::J((double)g_status.started_at));
    long long sessions = 0, messages = 0;
    if (g_store) {
        g_store->CountRows(L"sessions", &sessions);
        g_store->CountRows(L"messages", &messages);
    }
    j.Set(L"sessions", her::J((double)sessions));
    j.Set(L"messages", her::J((double)messages));
    return j;
}

Store* Db() {
    std::lock_guard<std::mutex> lk(g_mu);
    return g_store.get();
}

Server* Http() {
    std::lock_guard<std::mutex> lk(g_mu);
    return g_server.get();
}

}  // namespace heart
