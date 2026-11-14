// H-02 回归：Base URL 不合法时，DshChat 必须在发请求之前拦住（stub 调用次数 0），
// 而且错误信息里不能出现 API Key。抄 tests/bili-api-plan.cpp 的 stub 手法。
#include "../src/biliapi.h"

#include <cassert>
#include <cstdio>

// 测试替身：只为把 biliapi.cpp + paths.cpp 链起来，不碰真实工作区。
// FindWhale 在正常构建里来自 src/whale.cpp；paths.cpp 切工作区时会清 her 配置缓存。
WhaleEnv FindWhale() { return WhaleEnv{}; }
namespace her {
void InvalidateConfigCache() {}
}  // namespace her

static her::Http response;
static int calls = 0;
static wstring requestedUrl, requestedKey;

her::Http her::HttpPostJson(const wstring& url, const wstring&, const wstring& bearer, int) {
    calls++;
    requestedUrl = url;
    requestedKey = bearer;
    return response;
}

int main() {
    DshApiCfg api;
    api.enabled = true;
    api.path = L"/chat/completions";
    api.model = L"fixture-model";
    api.key = L"fixture-secret";

    wstring reply, error;

    // 明文 http 打到公网：不发请求，报错不含 Key
    api.base = L"http://evil.test/v1";
    calls = 0;
    assert(!DshChat(L"hi", api, &reply, &error));
    assert(calls == 0);
    assert(!error.empty() && error.find(api.key) == wstring::npos);

    // 带 userinfo：不发请求
    api.base = L"https://user:pass@evil.test/v1";
    calls = 0;
    assert(!DshChat(L"hi", api, &reply, &error));
    assert(calls == 0 && error.find(api.key) == wstring::npos);

    // 片段：不发请求
    api.base = L"https://evil.test/v1#frag";
    calls = 0;
    assert(!DshChat(L"hi", api, &reply, &error));
    assert(calls == 0);

    // 合法公网 https：照常发，Key 作为 Bearer 传出
    api.base = L"https://api.example.test/v1";
    response = {200, LR"({"choices":[{"message":{"content":"ok"}}]})", true, L""};
    calls = 0;
    assert(DshChat(L"hi", api, &reply, &error));
    assert(calls == 1 && requestedKey == api.key && reply == L"ok");
    assert(requestedUrl == L"https://api.example.test/v1/chat/completions");

    // 本机回环 http 是允许的（Ollama 这类本机服务）
    api.base = L"http://127.0.0.1:11434/v1";
    calls = 0;
    assert(DshChat(L"hi", api, &reply, &error));
    assert(calls == 1 && requestedUrl.find(L"http://127.0.0.1:11434/v1/chat/completions") == 0);

    // 传输层被 HttpCall 拒绝时，中文原因要透出来（而不是笼统的「连不上」）
    api.base = L"https://api.example.test/v1";
    response = {0, L"", false, L"这个地址不允许携带 API Key：测试原因"};
    assert(!DshChat(L"hi", api, &reply, &error));
    assert(error == L"这个地址不允许携带 API Key：测试原因");

    std::puts("OK: dsh-api-guard（H-02 非法 Base 0 调用 / 原因透传）");
    return 0;
}
