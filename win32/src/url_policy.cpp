// 时序之子 · URL 安全策略（H-01 读链接 SSRF / H-02 API Base 外发）
// 说明与残余风险见 url_policy.h 顶部注释。
#include "url_policy.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <cstdint>

namespace {

std::wstring TrimW(const std::wstring& s) {
    size_t b = 0, e = s.size();
    while (b < e && (s[b] == L' ' || s[b] == L'\t' || s[b] == L'\r' || s[b] == L'\n')) b++;
    while (e > b && (s[e - 1] == L' ' || s[e - 1] == L'\t' || s[e - 1] == L'\r' || s[e - 1] == L'\n')) e--;
    return s.substr(b, e - b);
}

std::wstring LowerW(std::wstring s) {
    for (auto& c : s)
        if (c >= L'A' && c <= L'Z') c = (wchar_t)(c - L'A' + L'a');
    return s;
}

void SetErr(std::wstring* err, const std::wstring& text) {
    if (err) *err = text;
}

// IPv4 地址按主机字节序分类。127/8 单独算回环，其余不可路由地址一律 Private。
UrlIpKind ClassifyV4(uint32_t h) {
    if ((h >> 24) == 127) return UrlIpKind::Loopback;
    if ((h >> 24) == 0 || (h >> 24) == 10) return UrlIpKind::Private;
    if ((h & 0xFFFF0000u) == 0xA9FE0000u) return UrlIpKind::Private;  // 169.254/16 链路本地
    if ((h & 0xFFF00000u) == 0xAC100000u) return UrlIpKind::Private;  // 172.16/12
    if ((h & 0xFFFF0000u) == 0xC0A80000u) return UrlIpKind::Private;  // 192.168/16
    if ((h & 0xFFC00000u) == 0x64400000u) return UrlIpKind::Private;  // 100.64/10 CGNAT
    if ((h & 0xFFFFFF00u) == 0xC0000000u) return UrlIpKind::Private;  // 192.0.0/24
    if ((h & 0xFFFFFF00u) == 0xC0000200u) return UrlIpKind::Private;  // 192.0.2/24 文档
    if ((h & 0xFFFE0000u) == 0xC6120000u) return UrlIpKind::Private;  // 198.18/15 基准测试
    if ((h & 0xFFFFFF00u) == 0xC6336400u) return UrlIpKind::Private;  // 198.51.100/24 文档
    if ((h & 0xFFFFFF00u) == 0xCB007100u) return UrlIpKind::Private;  // 203.0.113/24 文档
    if ((h & 0xF0000000u) == 0xE0000000u) return UrlIpKind::Private;  // 224/4 组播
    if ((h & 0xF0000000u) == 0xF0000000u) return UrlIpKind::Private;  // 240/4 保留（含 255.255.255.255）
    return UrlIpKind::Public;
}

bool EnsureWinsock() {
    static const bool ok = [] {
        WSADATA w{};
        return WSAStartup(MAKEWORD(2, 2), &w) == 0;
    }();
    return ok;
}

// host 精确匹配：白名单里可以是 "host"，也可以是 "host:port"。
bool HostAllowed(const UrlPolicyConfig& cfg, const ParsedUrl& u) {
    const std::wstring hp = u.host + L":" + std::to_wstring(u.port);
    for (const auto& e : cfg.allowed_hosts)
        if (e == u.host || e == hp) return true;
    return false;
}

bool IsLoopbackHostText(const std::wstring& host) {
    if (LowerW(host) == L"localhost") return true;
    IN_ADDR a4{};
    if (InetPtonW(AF_INET, host.c_str(), &a4) == 1)
        return ClassifyV4(ntohl(a4.S_un.S_addr)) == UrlIpKind::Loopback;
    IN6_ADDR a6{};
    if (InetPtonW(AF_INET6, host.c_str(), &a6) == 1) return UrlClassifyIp(host) == UrlIpKind::Loopback;
    return false;
}

// 去掉 . 和 ..，结果总以 / 开头。传入的是纯路径（不含查询串）。
std::wstring RemoveDotSegments(const std::wstring& path) {
    std::vector<std::wstring> segs;
    std::wstring seg;
    for (wchar_t c : path) {
        if (c == L'/') {
            if (!seg.empty()) {
                if (seg == L"..") {
                    if (!segs.empty()) segs.pop_back();
                } else if (seg != L".") {
                    segs.push_back(seg);
                }
                seg.clear();
            }
        } else {
            seg += c;
        }
    }
    if (!seg.empty()) {
        if (seg == L"..") {
            if (!segs.empty()) segs.pop_back();
        } else if (seg != L".") {
            segs.push_back(seg);
        }
    }
    std::wstring out = L"/";
    for (size_t i = 0; i < segs.size(); ++i) {
        if (i) out += L"/";
        out += segs[i];
    }
    return out;
}

}  // namespace

bool UrlParse(const std::wstring& url, ParsedUrl* out) {
    if (!out) return false;
    const std::wstring& s = url;
    // 8192 是 WinHTTP 的 URL 长度上限；空白/控制字符/反斜杠直接拒绝，
    // 因为不同解析器对 "http://a\\@b" 这类写法的理解可以不一样。
    if (s.size() < 8 || s.size() > 8192) return false;
    for (wchar_t c : s) {
        if (c <= 0x20 || c == 0x7F || c == L'\\') return false;
    }
    const size_t sep = s.find(L"://");
    if (sep == std::wstring::npos || sep == 0 || sep > 16) return false;
    const std::wstring scheme = LowerW(s.substr(0, sep));
    if (scheme != L"http" && scheme != L"https") return false;

    const size_t a = sep + 3;
    size_t end = s.find_first_of(L"/?#", a);
    if (end == std::wstring::npos) end = s.size();
    const std::wstring auth = s.substr(a, end - a);
    if (auth.empty() || auth.find(L'@') != std::wstring::npos) return false;  // 拒绝 userinfo

    std::wstring host, portText;
    bool ipv6 = false;
    if (auth[0] == L'[') {
        const size_t close = auth.find(L']');
        if (close == std::wstring::npos || close == 1) return false;
        host = auth.substr(1, close - 1);
        const std::wstring rest = auth.substr(close + 1);
        if (!rest.empty()) {
            if (rest[0] != L':') return false;
            portText = rest.substr(1);
            if (portText.empty()) return false;
        }
        ipv6 = true;
    } else {
        const size_t colon = auth.rfind(L':');
        if (colon != std::wstring::npos) {
            if (auth.find(L':') != colon) return false;  // 多个冒号必须用 []
            host = auth.substr(0, colon);
            portText = auth.substr(colon + 1);
            if (portText.empty()) return false;
        } else {
            host = auth;
        }
    }
    host = LowerW(host);
    if (host.empty()) return false;
    for (wchar_t c : host) {
        if (c == L'/' || c == L'?' || c == L'#' || c == L'[' || c == L']' || c == L'@' || c == L'%')
            return false;
        if (ipv6) {
            // 方括号里只认十六进制数字、冒号和内嵌 IPv4 的点
            const bool hex = (c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f') || c == L':' || c == L'.';
            if (!hex) return false;
        } else if (c == L':') {
            return false;  // 不带方括号的冒号只可能是端口，已经在上面切掉了
        }
    }

    int port = (scheme == L"https") ? 443 : 80;
    if (!portText.empty()) {
        if (portText.size() > 5) return false;
        int n = 0;
        for (wchar_t c : portText) {
            if (c < L'0' || c > L'9') return false;
            n = n * 10 + (int)(c - L'0');
        }
        if (n < 1 || n > 65535) return false;
        port = n;
    }

    std::wstring path = (end == std::wstring::npos) ? L"/" : s.substr(end);
    const size_t frag = path.find(L'#');
    if (frag != std::wstring::npos) path = path.substr(0, frag);
    if (path.empty() || path[0] == L'#') path = L"/";
    if (path[0] == L'?') path = L"/" + path;

    out->scheme = scheme;
    out->host = host;
    out->port = port;
    out->path = path;
    out->ipv6_literal = ipv6;
    return true;
}

void UrlPolicyAddHosts(UrlPolicyConfig& cfg, const std::wstring& text) {
    std::wstring cur;
    auto flush = [&] {
        std::wstring t = LowerW(TrimW(cur));
        cur.clear();
        if (t.empty()) return;
        const size_t sc = t.find(L"://");
        if (sc != std::wstring::npos) t = t.substr(sc + 3);
        const size_t slash = t.find_first_of(L"/?#");
        if (slash != std::wstring::npos) t = t.substr(0, slash);
        if (t.empty() || t.find(L'@') != std::wstring::npos) return;
        cfg.allowed_hosts.push_back(t);
    };
    for (wchar_t c : text) {
        if (c == L',' || c == L';' || c == L'、' || c == L'，' || c == L'；' || c == L' ' || c == L'\t' ||
            c == L'\r' || c == L'\n')
            flush();
        else
            cur += c;
    }
    flush();
}

UrlIpKind UrlClassifyIp(const std::wstring& ip) {
    const std::wstring s = TrimW(ip);
    IN_ADDR a4{};
    if (InetPtonW(AF_INET, s.c_str(), &a4) == 1) return ClassifyV4(ntohl(a4.S_un.S_addr));
    IN6_ADDR a6{};
    if (InetPtonW(AF_INET6, s.c_str(), &a6) != 1) return UrlIpKind::Private;

    const unsigned char* b = a6.u.Byte;
    bool allZero = true;
    for (int i = 0; i < 16; ++i)
        if (b[i]) {
            allZero = false;
            break;
        }
    if (allZero) return UrlIpKind::Private;  // ::
    bool isLoop = true;
    for (int i = 0; i < 15; ++i)
        if (b[i]) {
            isLoop = false;
            break;
        }
    if (isLoop && b[15] == 1) return UrlIpKind::Loopback;  // ::1

    // ::ffff:a.b.c.d —— getaddrinfo 有时会回这种写法，按内层 v4 判
    bool mapped = true;
    for (int i = 0; i < 10; ++i)
        if (b[i]) {
            mapped = false;
            break;
        }
    if (mapped && b[10] == 0xFF && b[11] == 0xFF)
        return ClassifyV4(((uint32_t)b[12] << 24) | ((uint32_t)b[13] << 16) | ((uint32_t)b[14] << 8) |
                          (uint32_t)b[15]);
    // 64:ff9b::/96（NAT64）与 2002::/16（6to4）可以藏内网 v4，统一按不可信处理
    if (b[0] == 0x00 && b[1] == 0x64 && b[2] == 0xFF && b[3] == 0x9B) return UrlIpKind::Private;
    if (b[0] == 0x20 && b[1] == 0x02) return UrlIpKind::Private;
    if ((b[0] & 0xFE) == 0xFC) return UrlIpKind::Private;                       // fc00::/7 ULA
    if (b[0] == 0xFE && (b[1] & 0xC0) == 0x80) return UrlIpKind::Private;       // fe80::/10
    if (b[0] == 0xFF) return UrlIpKind::Private;                                // 组播
    if (b[0] == 0x20 && b[1] == 0x01 && b[2] == 0x0D && b[3] == 0xB8) return UrlIpKind::Private;  // 2001:db8
    return UrlIpKind::Public;
}

bool UrlFetchAllowedResolved(const std::wstring& url, const UrlPolicyConfig& cfg,
                             const std::vector<std::wstring>& ips, std::wstring* err) {
    if (err) err->clear();
    ParsedUrl u;
    if (!UrlParse(url, &u)) {
        SetErr(err, L"这不是一个能读的网页地址：只认 http:// 或 https://，而且不能带账号密码");
        return false;
    }
    if (ips.empty()) {
        SetErr(err, L"域名解析不了，为了安全没有去连");
        return false;
    }

    bool allLoopback = true;
    bool allPublic = true;
    for (const auto& ip : ips) {
        const UrlIpKind k = UrlClassifyIp(ip);
        if (k != UrlIpKind::Loopback) allLoopback = false;
        if (k != UrlIpKind::Public) allPublic = false;
    }

    if (allLoopback) {
        if (cfg.allow_loopback || HostAllowed(cfg, u)) return true;
        SetErr(err,
               L"这个地址指向本机（回环地址）。读链接默认不碰本机服务；确实要读，请在设置里打开 "
               L"link_allow_private，或把主机写进 link_allow_hosts 白名单");
        return false;
    }
    if (!allPublic) {
        // 内网（NAS、路由、局域网服务）默认不读；用户把主机写进白名单就是明确的「我要读这台」，
        // 此时连明文 http 也放行——家用 NAS 基本只给 http。
        if (HostAllowed(cfg, u)) return true;
        SetErr(err,
               L"这个地址落在内网或本机网段（例如 10.x、192.168.x、169.254）。要读它，请把主机"
               L"（含端口，如 192.168.1.5:5000）写进设置里的 link_allow_hosts 白名单");
        return false;
    }
    if (u.scheme != L"https") {
        SetErr(err, L"为了安全，明文 http 只允许本机服务；公网地址请用 https");
        return false;
    }
    return true;
}

bool UrlFetchAllowed(const std::wstring& url, const UrlPolicyConfig& cfg, std::wstring* err) {
    if (err) err->clear();
    ParsedUrl u;
    if (!UrlParse(url, &u)) {
        SetErr(err, L"这不是一个能读的网页地址：只认 http:// 或 https://，而且不能带账号密码");
        return false;
    }

    std::vector<std::wstring> ips;
    IN_ADDR a4{};
    IN6_ADDR a6{};
    if (InetPtonW(AF_INET, u.host.c_str(), &a4) == 1 || InetPtonW(AF_INET6, u.host.c_str(), &a6) == 1) {
        ips.push_back(u.host);
    } else {
        if (!EnsureWinsock()) {
            SetErr(err, L"网络组件没起来，读不了链接");
            return false;
        }
        ADDRINFOW hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        ADDRINFOW* res = nullptr;
        const int rc = GetAddrInfoW(u.host.c_str(), nullptr, &hints, &res);
        if (rc != 0 || !res) {
            if (res) FreeAddrInfoW(res);
            SetErr(err, L"域名解析不了（地址写错或没网），为了安全没有去连");
            return false;
        }
        for (ADDRINFOW* p = res; p; p = p->ai_next) {
            wchar_t buf[64] = {0};
            if (p->ai_family == AF_INET && p->ai_addr) {
                const sockaddr_in* sa = reinterpret_cast<const sockaddr_in*>(p->ai_addr);
                if (InetNtopW(AF_INET, &sa->sin_addr, buf, 64)) ips.push_back(buf);
            } else if (p->ai_family == AF_INET6 && p->ai_addr) {
                const sockaddr_in6* sa = reinterpret_cast<const sockaddr_in6*>(p->ai_addr);
                if (InetNtopW(AF_INET6, &sa->sin6_addr, buf, 64)) ips.push_back(buf);
            }
        }
        FreeAddrInfoW(res);
    }
    return UrlFetchAllowedResolved(url, cfg, ips, err);
}

bool ValidateApiBase(const std::wstring& base, std::wstring* err) {
    if (err) err->clear();
    const std::wstring s = TrimW(base);
    if (s.empty()) {
        SetErr(err, L"Base URL 不能为空");
        return false;
    }
    if (s.size() > 512) {
        SetErr(err, L"Base URL 太长了（512 字符以内）");
        return false;
    }
    for (wchar_t c : s) {
        if (c <= 0x20 || c == 0x7F) {
            SetErr(err, L"地址里不能有空格、制表符或换行");
            return false;
        }
    }
    if (s.find(L'#') != std::wstring::npos) {
        SetErr(err, L"地址里不能带 # 片段");
        return false;
    }
    ParsedUrl u;
    if (!UrlParse(s, &u)) {
        SetErr(err, L"Base URL 要是完整的 http(s):// 地址，而且不能带账号密码");
        return false;
    }
    if (u.scheme == L"https") return true;
    if (!IsLoopbackHostText(u.host)) {
        SetErr(err, L"API 地址必须用 https；只有本机回环地址（127.0.0.1、[::1]、localhost）可以用 http");
        return false;
    }
    return true;
}

std::wstring RedirectTarget(const std::wstring& currentUrl, const std::wstring& location) {
    std::wstring loc = TrimW(location);
    if (loc.empty()) return L"";
    for (wchar_t c : loc) {
        if (c <= 0x20 || c == 0x7F || c == L'\\') return L"";
    }
    const size_t frag = loc.find(L'#');
    if (frag != std::wstring::npos) loc = loc.substr(0, frag);
    if (loc.empty()) return L"";  // 只有 #片段，等于没跳

    // 绝对地址：scheme://...
    const size_t colon = loc.find(L':');
    if (colon != std::wstring::npos && colon > 0) {
        bool schemeOk = true;
        for (size_t i = 0; i < colon; ++i) {
            const wchar_t c = loc[i];
            const bool alpha = (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z');
            if (i == 0 && !alpha) {
                schemeOk = false;
                break;
            }
            if (!(alpha || (c >= L'0' && c <= L'9') || c == L'+' || c == L'-' || c == L'.')) {
                schemeOk = false;
                break;
            }
        }
        if (schemeOk && loc.compare(colon + 1, 2, L"//") == 0) return loc;
        if (schemeOk) return L"";  // mailto: 之类，读链接用不上
    }

    ParsedUrl base;
    if (!UrlParse(currentUrl, &base)) return L"";
    std::wstring origin = base.scheme + L"://";
    origin += base.ipv6_literal ? (L"[" + base.host + L"]") : base.host;
    const int defPort = (base.scheme == L"https") ? 443 : 80;
    if (base.port != defPort) origin += L":" + std::to_wstring(base.port);

    if (loc.compare(0, 2, L"//") == 0) return base.scheme + L":" + loc;
    if (loc[0] == L'/') return origin + loc;

    std::wstring basePath = base.path;
    const size_t q = basePath.find(L'?');
    if (q != std::wstring::npos) basePath = basePath.substr(0, q);
    if (loc[0] == L'?') return origin + basePath + loc;

    // 相对路径：目录 + loc，再消掉 . 和 ..
    std::wstring locPath = loc, locQuery;
    const size_t lq = loc.find(L'?');
    if (lq != std::wstring::npos) {
        locPath = loc.substr(0, lq);
        locQuery = loc.substr(lq);
    }
    const size_t slash = basePath.rfind(L'/');
    const std::wstring dir = (slash == std::wstring::npos) ? L"/" : basePath.substr(0, slash + 1);
    return origin + RemoveDotSegments(dir + locPath) + locQuery;
}
