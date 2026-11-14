// H-01 / H-02 的纯函数测试：不联网、不读配置，注入解析结果。
// 约定跟 tests/run-core-tests.bat 里其他测试一样：assert 失败 = 进程非零退出。
#include "../src/url_policy.h"

#include <cassert>
#include <cstdio>

static bool Allowed(const wchar_t* url, const UrlPolicyConfig& cfg, std::vector<std::wstring> ips) {
    std::wstring err;
    const bool ok = UrlFetchAllowedResolved(url, cfg, ips, &err);
    if (!ok) assert(!err.empty());  // 拒绝必须带中文原因，界面不能只显示空白
    return ok;
}

int main() {
    // ---------------- URL 解析：只认 http(s)，拒绝 userinfo / 反斜杠 / 空白
    ParsedUrl u;
    assert(UrlParse(L"https://Example.COM:8443/a/b?x=1#frag", &u));
    assert(u.scheme == L"https" && u.host == L"example.com" && u.port == 8443 && u.path == L"/a/b?x=1");
    assert(UrlParse(L"http://[::1]:8080/v1", &u) && u.host == L"::1" && u.port == 8080 && u.ipv6_literal);
    assert(UrlParse(L"https://x.test", &u) && u.path == L"/" && u.port == 443);
    assert(UrlParse(L"https://x.test?q=1", &u) && u.path == L"/?q=1");
    assert(!UrlParse(L"ftp://example.com/", &u));
    assert(!UrlParse(L"http://user:pass@example.com/", &u));
    assert(!UrlParse(L"http:///nohost", &u));
    assert(!UrlParse(L"https://x.test\\@127.0.0.1/", &u));
    assert(!UrlParse(L"https://x.test/a b", &u));
    assert(!UrlParse(L"", &u));

    // ---------------- IP 分类
    assert(UrlClassifyIp(L"8.8.8.8") == UrlIpKind::Public);
    assert(UrlClassifyIp(L"93.184.216.34") == UrlIpKind::Public);
    assert(UrlClassifyIp(L"127.0.0.1") == UrlIpKind::Loopback);
    assert(UrlClassifyIp(L"127.10.20.30") == UrlIpKind::Loopback);
    assert(UrlClassifyIp(L"::1") == UrlIpKind::Loopback);
    assert(UrlClassifyIp(L"::ffff:127.0.0.1") == UrlIpKind::Loopback);
    for (const wchar_t* bad : {L"10.0.0.5", L"172.16.3.4", L"192.168.1.10", L"169.254.169.254", L"0.0.0.0",
                               L"100.64.0.1", L"224.0.0.1", L"255.255.255.255", L"198.18.0.1", L"::",
                               L"fc00::1", L"fe80::1", L"ff02::1", L"2002:7f00:1::1"})
        assert(UrlClassifyIp(bad) == UrlIpKind::Private);
    assert(UrlClassifyIp(L"2001:4860:4860::8888") == UrlIpKind::Public);
    assert(UrlClassifyIp(L"not-an-ip") == UrlIpKind::Private);

    // ---------------- 读链接策略：默认拒绝内网/回环/明文 http
    UrlPolicyConfig cfg;  // allow_loopback=false，白名单为空
    const std::vector<std::wstring> pub = {L"93.184.216.34"};
    assert(Allowed(L"https://example.com/", cfg, pub));
    assert(!Allowed(L"http://example.com/", cfg, pub));  // 公网明文 http
    assert(!Allowed(L"http://127.0.0.1/", cfg, {L"127.0.0.1"}));
    assert(!Allowed(L"http://10.0.0.5/", cfg, {L"10.0.0.5"}));
    assert(!Allowed(L"http://172.16.3.4/", cfg, {L"172.16.3.4"}));
    assert(!Allowed(L"http://192.168.1.10/", cfg, {L"192.168.1.10"}));
    assert(!Allowed(L"http://169.254.169.254/latest/meta-data/", cfg, {L"169.254.169.254"}));
    assert(!Allowed(L"http://[::1]/", cfg, {L"::1"}));
    assert(!Allowed(L"http://0.0.0.0/", cfg, {L"0.0.0.0"}));
    assert(!Allowed(L"http://100.64.0.1/", cfg, {L"100.64.0.1"}));
    assert(!Allowed(L"ftp://example.com/", cfg, pub));
    assert(!Allowed(L"http://user:pass@example.com/", cfg, pub));
    assert(!Allowed(L"https://example.com/", cfg, {}));  // 解析失败：不放行

    // 打开 link_allow_private 后，本机回环可以用明文 http（本机服务的老用法）
    UrlPolicyConfig loop = cfg;
    loop.allow_loopback = true;
    assert(Allowed(L"http://127.0.0.1:5109/", loop, {L"127.0.0.1"}));
    assert(Allowed(L"http://[::1]:5109/", loop, {L"::1"}));
    assert(Allowed(L"https://127.0.0.1/", loop, {L"127.0.0.1"}));
    // 但开关只管回环，不管内网
    assert(!Allowed(L"http://192.168.1.10/", loop, {L"192.168.1.10"}));

    // 白名单精确匹配 host:port；端口不对也不行
    UrlPolicyConfig wl;
    UrlPolicyAddHosts(wl, L"http://127.0.0.1:5109/x, LOCALHOST");
    assert(wl.allowed_hosts.size() == 2 && wl.allowed_hosts[0] == L"127.0.0.1:5109" &&
           wl.allowed_hosts[1] == L"localhost");
    assert(Allowed(L"http://127.0.0.1:5109/", wl, {L"127.0.0.1"}));
    assert(!Allowed(L"http://127.0.0.1:5108/", wl, {L"127.0.0.1"}));

    // 内网主机写进白名单就是明确授权：允许明文 http（家用 NAS 常见），端口仍要精确匹配
    UrlPolicyConfig lan;
    UrlPolicyAddHosts(lan, L"192.168.1.5:5000,nas.local");
    assert(Allowed(L"http://192.168.1.5:5000/", lan, {L"192.168.1.5"}));
    assert(!Allowed(L"http://192.168.1.5:5001/", lan, {L"192.168.1.5"}));
    assert(Allowed(L"http://nas.local/", lan, {L"192.168.1.5"}));
    assert(!Allowed(L"http://other.local/", lan, {L"192.168.1.5"}));

    // 域名解析结果里混进内网地址：一律拒绝
    assert(!Allowed(L"https://example.com/", cfg, {L"93.184.216.34", L"10.0.0.5"}));

    // ---------------- 重定向拼接
    const wchar_t* base = L"https://a.example/dir/page?x=1";
    assert(RedirectTarget(base, L"../x") == L"https://a.example/x");
    assert(RedirectTarget(base, L"./y") == L"https://a.example/dir/y");
    assert(RedirectTarget(base, L"/root") == L"https://a.example/root");
    assert(RedirectTarget(base, L"//b.example/p") == L"https://b.example/p");
    assert(RedirectTarget(base, L"?q=1") == L"https://a.example/dir/page?q=1");
    assert(RedirectTarget(base, L"https://c.example/z") == L"https://c.example/z");
    assert(RedirectTarget(base, L"mailto:x@y") == L"");
    assert(RedirectTarget(base, L"//b.example/\r\nX: y") == L"");
    assert(RedirectTarget(base, L"#only-fragment") == L"");

    // ---------------- H-02：API Base 校验
    std::wstring why;
    assert(!ValidateApiBase(L"", &why));
    assert(!ValidateApiBase(L"ftp://x/v1", &why));
    assert(!ValidateApiBase(L"https://user:pass@x/v1", &why));
    assert(!ValidateApiBase(L"https:///v1", &why));
    assert(!ValidateApiBase(L"https://x/v1#frag", &why));
    assert(!ValidateApiBase(L"http://example.com/v1", &why));
    assert(!ValidateApiBase(L"https://x/v1 a", &why));
    assert(!ValidateApiBase(std::wstring(600, L'a'), &why));
    assert(ValidateApiBase(L"https://api.deepseek.com/v1", &why));
    assert(ValidateApiBase(L"http://127.0.0.1:11434/v1", &why));
    assert(ValidateApiBase(L"http://[::1]:8080/v1", &why));
    assert(ValidateApiBase(L"http://localhost:11434/v1", &why));
    assert(ValidateApiBase(L"https://x.test:8443/v1?q=1", &why));

    std::puts("OK: url-policy（H-01 SSRF / H-02 Base 校验）");
    return 0;
}
