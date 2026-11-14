// 心核（Heart）· P0 骨架
// 独立可执行的内核雏形：配置、SQLite、事件总线、HTTP 兼容层。
// 目前只提供 /api/v1/health、/api/v1/state、/api/v1/version，对话能力后续阶段接入。
#pragma once

#include "../core.h"
#include "../her/her.h"

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace heart {

std::wstring Version();

// ================================================================ 配置
struct Config {
    int port = 6190;
    std::wstring bind = L"127.0.0.1";
    bool auto_start = true;
    std::wstring log_level = L"info";

    static Config Defaults();
    static bool Load(const std::wstring& file, Config* out, std::wstring* err);
    bool Save(const std::wstring& file, std::wstring* err) const;
};

std::wstring DefaultHome();

// ================================================================ 存储
class Store {
public:
    Store();
    ~Store();
    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;

    bool Open(const std::wstring& file, std::wstring* err);
    void Close();
    bool Ok() const;
    std::wstring Path() const;

    bool Exec(const std::wstring& sql, std::wstring* err);
    bool GetMeta(const std::wstring& key, std::wstring* value);
    bool SetMeta(const std::wstring& key, const std::wstring& value, std::wstring* err);

    bool AddSession(const std::wstring& title, long long* id, std::wstring* err);
    bool AddMessage(long long session, const std::wstring& role, const std::wstring& text, long long* id,
                    std::wstring* err);
    bool CountRows(const std::wstring& table, long long* n);
    bool LastError(std::wstring* err) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ================================================================ 事件总线
int Subscribe(const std::wstring& topic, std::function<void(const her::J&)> handler);
void Unsubscribe(int id);
int Publish(const std::wstring& topic, const her::J& payload);

// ================================================================ HTTP
struct Request {
    std::wstring method;
    std::wstring path;
    std::wstring body;
    std::map<std::wstring, std::wstring> headers;  // key 小写
    std::map<std::wstring, std::wstring> query;
};

struct Response {
    int code = 200;
    std::wstring body;
};

class Server {
public:
    Server();
    ~Server();
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    bool Start(const std::wstring& bind, int port, std::wstring* err);
    void Stop();
    bool Running() const;
    int Port() const;
    void Add(const std::wstring& method, const std::wstring& path, std::function<Response(const Request&)> route);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ================================================================ 内核
struct Status {
    bool running = false;
    int port = 0;
    std::wstring home;
    std::wstring version;
    long long started_at = 0;
};

bool Start(const Config& cfg, const std::wstring& home, std::wstring* err);
void Stop();
bool Running();
Status Current();
her::J Snapshot();
Store* Db();
Server* Http();

}  // namespace heart
