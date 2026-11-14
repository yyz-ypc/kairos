// 心核 P0 · 本地 HTTP（Winsock，127.0.0.1，单连接串行）
#include <winsock2.h>
#include <ws2tcpip.h>

#include "heart.h"
#include "util.h"

#include <atomic>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

namespace heart {

namespace {

using util::Lower;
using util::Narrow;
using util::Trim;
using util::Widen;

bool WsaReady() {
    static bool ok = [] {
        WSADATA d;
        return WSAStartup(MAKEWORD(2, 2), &d) == 0;
    }();
    return ok;
}

const char* Reason(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        default: return "OK";
    }
}

struct Route {
    std::wstring method;
    std::wstring path;
    std::function<Response(const Request&)> fn;
};

bool SendAll(SOCKET s, const std::string& data) {
    size_t sent = 0;
    while (sent < data.size()) {
        int n = send(s, data.data() + sent, (int)(data.size() - sent), 0);
        if (n <= 0) return false;
        sent += (size_t)n;
    }
    return true;
}

void HandleConn(SOCKET s, std::vector<Route>& routes) {
    DWORD timeout = 4000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));

    std::string buf;
    char tmp[4096];
    size_t headerEnd = std::string::npos;
    while (buf.size() < 262144) {
        int n = recv(s, tmp, sizeof(tmp), 0);
        if (n <= 0) break;
        buf.append(tmp, (size_t)n);
        headerEnd = buf.find("\r\n\r\n");
        if (headerEnd != std::string::npos) break;
    }
    if (headerEnd == std::string::npos) {
        closesocket(s);
        return;
    }

    std::string head = buf.substr(0, headerEnd);
    std::string body = buf.substr(headerEnd + 4);

    Request req;
    size_t pos = 0;
    auto nextLine = [&](std::string* line) {
        size_t e = head.find("\r\n", pos);
        if (e == std::string::npos) {
            *line = head.substr(pos);
            pos = head.size();
        } else {
            *line = head.substr(pos, e - pos);
            pos = e + 2;
        }
        return !line->empty() || pos <= head.size();
    };

    std::string line;
    nextLine(&line);
    {
        size_t a = line.find(' ');
        size_t b = (a == std::string::npos) ? std::string::npos : line.find(' ', a + 1);
        if (a == std::string::npos || b == std::string::npos) {
            closesocket(s);
            return;
        }
        req.method = Widen(line.substr(0, a));
        std::wstring target = Widen(line.substr(a + 1, b - a - 1));
        size_t q = target.find(L'?');
        if (q == std::wstring::npos) {
            req.path = target;
        } else {
            req.path = target.substr(0, q);
            std::wstring qs = target.substr(q + 1);
            size_t start = 0;
            while (start <= qs.size()) {
                size_t amp = qs.find(L'&', start);
                std::wstring kv = qs.substr(start, amp == std::wstring::npos ? std::wstring::npos : amp - start);
                size_t eq = kv.find(L'=');
                if (eq != std::wstring::npos) req.query[kv.substr(0, eq)] = kv.substr(eq + 1);
                if (amp == std::wstring::npos) break;
                start = amp + 1;
            }
        }
    }

    size_t contentLength = 0;
    while (pos < head.size() && nextLine(&line)) {
        if (line.empty()) break;
        size_t c = line.find(':');
        if (c == std::string::npos) continue;
        std::wstring key = Lower(Trim(Widen(line.substr(0, c))));
        std::wstring val = Trim(Widen(line.substr(c + 1)));
        req.headers[key] = val;
        if (key == L"content-length") contentLength = (size_t)_wtoi64(val.c_str());
    }
    while (body.size() < contentLength) {
        int n = recv(s, tmp, sizeof(tmp), 0);
        if (n <= 0) break;
        body.append(tmp, (size_t)n);
    }
    req.body = Widen(body.substr(0, contentLength ? contentLength : body.size()));

    Response resp;
    resp.code = 404;
    resp.body = L"{\"status\":\"error\",\"message\":\"not found\"}";
    for (auto& r : routes) {
        if (r.method == req.method && r.path == req.path) {
            resp = r.fn(req);
            break;
        }
    }

    std::string payload = Narrow(resp.body);
    std::string out = "HTTP/1.1 " + std::to_string(resp.code) + " " + Reason(resp.code) +
                      "\r\nContent-Type: application/json; charset=utf-8\r\nContent-Length: " +
                      std::to_string(payload.size()) + "\r\nConnection: close\r\n\r\n" + payload;
    SendAll(s, out);
    shutdown(s, SD_SEND);
    closesocket(s);
}

}  // namespace

struct Server::Impl {
    SOCKET listener = INVALID_SOCKET;
    std::thread thread;
    std::atomic<bool> running{false};
    int port = 0;
    std::mutex mu;
    std::vector<Route> routes;

    void Loop() {
        while (running) {
            SOCKET c = accept(listener, nullptr, nullptr);
            if (c == INVALID_SOCKET) {
                if (!running) break;
                continue;
            }
            std::vector<Route> copy;
            {
                std::lock_guard<std::mutex> lk(mu);
                copy = routes;
            }
            HandleConn(c, copy);
        }
    }
};

Server::Server() : impl_(new Impl) {}
Server::~Server() { Stop(); }

bool Server::Running() const { return impl_ && impl_->running; }
int Server::Port() const { return impl_ ? impl_->port : 0; }

void Server::Add(const std::wstring& method, const std::wstring& path,
                 std::function<Response(const Request&)> route) {
    if (!impl_) return;
    std::lock_guard<std::mutex> lk(impl_->mu);
    impl_->routes.push_back({method, path, std::move(route)});
}

bool Server::Start(const std::wstring& bindAddr, int port, std::wstring* err) {
    if (!impl_) return false;
    if (impl_->running) return true;
    if (!WsaReady()) {
        if (err) *err = L"Winsock 初始化失败";
        return false;
    }
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        if (err) *err = L"建不了监听 socket";
        return false;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)port);
    std::string host = Narrow(bindAddr.empty() ? L"127.0.0.1" : bindAddr);
    if (host == "0.0.0.0" || host.empty()) {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    }
    if (bind(s, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        if (err) *err = L"端口 " + std::to_wstring(port) + L" 绑不上（可能被占用）";
        closesocket(s);
        return false;
    }
    if (listen(s, 16) == SOCKET_ERROR) {
        if (err) *err = L"listen 失败";
        closesocket(s);
        return false;
    }
    sockaddr_in got{};
    int len = sizeof(got);
    if (getsockname(s, (sockaddr*)&got, &len) == 0) impl_->port = ntohs(got.sin_port);
    impl_->listener = s;
    impl_->running = true;
    impl_->thread = std::thread([this] { impl_->Loop(); });
    return true;
}

void Server::Stop() {
    if (!impl_ || !impl_->running) return;
    impl_->running = false;
    if (impl_->listener != INVALID_SOCKET) {
        shutdown(impl_->listener, SD_BOTH);
        closesocket(impl_->listener);
        impl_->listener = INVALID_SOCKET;
    }
    if (impl_->thread.joinable()) impl_->thread.join();
}

}  // namespace heart
