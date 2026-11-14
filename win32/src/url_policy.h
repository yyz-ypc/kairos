// 时序之子 · URL 安全策略（H-01 读链接 SSRF / H-02 API Base 外发）
//
// 为什么单独放一个文件：读链接和对话 API 都会把用户给的地址交给 WinHTTP，
// 两边必须用同一套「什么地址可以连」的判断；写在 webfetch.cpp 里会让
// biliapi.cpp / her_probe.cpp 反向依赖界面层。本文件只依赖 Windows SDK，
// 不读配置、不建连接，方便单测。
//
// 硬性语义（与 docs/m0/sec-auditor.md 的 H-01/H-02 对齐）：
//   - 只认 http/https；带 userinfo（user:pass@）的地址直接拒绝，凭据不该跟着 URL 走；
//   - 明文 http 只允许回环目标（本机服务），公网必须 https；
//   - 域名先解析、再按「解析结果」判 IP：私网 / 回环 / 链路本地 / CGNAT /
//     组播 / 保留地址一律不许，除非它是用户显式配置的回环服务；
//   - 白名单精确匹配 host 或 host:port，不做通配、不做后缀匹配。
//
// 已知残余风险（本轮不解决，别以为这里堵死了）：这里校验的是本次解析结果，
// WinHTTP 随后会自己再解析一次，理论上存在 DNS rebinding 的 TOCTOU 窗口。
// 要彻底堵死得把已校验的 IP 直接交给连接层再手填 Host/SNI，而那样会破坏 TLS
// 证书校验，代价比风险大；读链接面向公开网页，攻击者还得先控住 DNS。
// 因此这里只保证「用户粘进来的地址 / 对方 302 给的地址」不会直接打到内网。
#pragma once

#include <string>
#include <vector>

// 一个最常见的 URL 拆解结果。path 里含查询串、不含 #片段。
struct ParsedUrl {
    std::wstring scheme;      // 小写：http / https
    std::wstring host;        // 小写；IPv6 字面量不带方括号
    int port = 0;
    std::wstring path;        // 至少是 "/"
    bool ipv6_literal = false;
};

// 严格解析（手写，不用 WinHttpCrackUrl，避免两套解析器出现判定差异）。
// 失败原因不细分，调用方按场景给中文提示。以下一律失败：
// 空串 / 超长 / 含空白或控制字符 / 含反斜杠 / 非 http(s) / 无主机名 /
// 带 userinfo / 端口非法。
bool UrlParse(const std::wstring& url, ParsedUrl* out);

struct UrlPolicyConfig {
    std::vector<std::wstring> allowed_hosts;  // 小写；"host" 或 "host:port"
    bool allow_loopback = false;              // settings: link_allow_private=1
};

// 设置里的白名单文本：逗号、分号、空白、顿号分隔；允许写 host、host:port，
// 也容忍误粘的 http://host:port/path（只取 host:port）。全部转小写。
void UrlPolicyAddHosts(UrlPolicyConfig& cfg, const std::wstring& text);

// 单个 IP 文本的分类。认不出来也按 Private 对待（fail closed）。
enum class UrlIpKind { Public, Loopback, Private };
UrlIpKind UrlClassifyIp(const std::wstring& ip);

// 纯函数：用调用方注入的解析结果判定，单测靠它离线跑。
// ips 为空 = 解析失败，按拒绝处理。
bool UrlFetchAllowedResolved(const std::wstring& url, const UrlPolicyConfig& cfg,
                             const std::vector<std::wstring>& ips, std::wstring* err);

// 真解析（字面量 IP 直接判，域名走 GetAddrInfoW）后判定。解析不出来一律拒绝。
bool UrlFetchAllowed(const std::wstring& url, const UrlPolicyConfig& cfg, std::wstring* err);

// H-02：对话 API 的 Base URL。https 必需；127.0.0.0/8、::1、localhost 例外允许 http。
// 另外拒绝 userinfo、#片段、空白/换行，长度上限 512。
bool ValidateApiBase(const std::wstring& base, std::wstring* err);

// Location / 相对链接 → 绝对地址（RFC 3986 的常用子集：./ ../ / ? // 和绝对地址）。
// 返回空串表示这个 Location 没法用（含控制字符、只有片段等）。只做拼接，不做安全判定。
std::wstring RedirectTarget(const std::wstring& currentUrl, const std::wstring& location);
