// 潮信（Hermes）托管通道
// 时序之子启动潮信时生成一次性控制 token，经环境变量 HERMES_HOST_TOKEN 交给子进程；
// 之后所有 /api/* 调用都带 X-Hermes-Token 走回环第二通道（不依赖登录 Cookie）。
// token 只在内存里：不落盘、不进日志、不进页面快照。
#pragma once

#include "her.h"

namespace her {

// 本进程本次运行的控制 token（首次调用时随机生成，64 个十六进制字符）
const std::wstring& TideToken();
// 是否已经拿到过 token（只有我们启动过潮信才为 true）
bool TideManaged();

struct TideReply {
    bool ok = false;        // HTTP 2xx
    int code = 0;           // 0 = 连不上
    std::wstring body;      // 原始响应（UTF-8 → wide）
    std::wstring err;       // 给用户看的一句话
};
// 走控制 token 调潮信本机接口；path 形如 L"/api/state"
TideReply TideApi(const wchar_t* method, const std::wstring& path, const std::wstring& body = L"",
                  int timeoutMs = 4000);

// 启动潮信：生成 token、带 HERMES_HOST_TOKEN 起 start.cmd、等端口。返回 pid（0 = 失败）
DWORD TideLaunch(std::wstring* errOut = nullptr);
// 随包潮信在不在（完整的一份；决定要不要自动拉起）
bool TideInstalled();
// 随包潮信的本地就绪语义：missing / initializing / ready（不联网、不代表 QQ 已登录）
std::wstring TideSetupState();
// 开机自动拉起：端口空着才起；已经有一套在跑就只连上去刷新状态，不杀别人的进程
void TideAutoStart();
// 停掉潮信（只杀本次由时序之子拉起的那棵进程树）
bool TideStop(std::wstring* errOut = nullptr);

// 清掉老装机留下的潮信明文密码文件（secrets\ 与 private\ 各一份，删不掉不报错）
void TideForgetLegacySecret();

// 后台刷新（不阻塞 UI 线程；同一时刻只跑一个）。force = 忽略 1.5 秒节流。
void TideRefreshAsync(bool force = false);
// 页面动作：在后台线程执行，结果写进快照的 opMsg/opOk
void TideAction(const std::wstring& op, const J& args);
bool TideBusy();

// UI 线程读的快照（JSON 字符串）：
// { reachable, managed, error, state:{...}, logs:[...],
//   messages:{ "<uin>": [...] }, opMsg, opOk, busy, starting, installed,
//   bundled, legacy, home, setup }
std::wstring TideSnapshot();

// 页面动作白名单（WebUiCommand 用它挡掉未知动作）
bool TideKnownOp(const std::wstring& op);

}  // namespace her
