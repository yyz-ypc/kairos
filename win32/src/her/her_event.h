// 时序之子 · 结构化事件日志（E4）
// 一行一条 JSON，落在 <StateDir>\logs\events-YYYYMMDD.jsonl，给「出了事能查」用：
//   {"ts":"2026-11-11T21:03:04+08:00","op":"op:backup","ok":true,"ms":1234,"err":""}
//
// 用法：
//   her::Event(L"op:backup", true, 1234);
//   { her::EventScope s(L"op:backup"); ...; if (bad) s.Fail(L"备份目录不可写"); }  // 析构时写一条
//
// 规矩：追加写、UTF-8 无 BOM、每条一行；单文件超过 2MB 滚成 .1（最多留 7 份）；
// err 换成单行、截断到 500 字符、疑似 token/密码替换成 ***。
// 任何失败都静默：不抛异常、不返回错误、绝不耽误调用方的主流程。
#pragma once

#include "../core.h"

#include <string>

namespace her {

// 写一条事件。op 形如 "op:backup"；ms 是这次操作花了多少毫秒；err 为空表示成功。
void Event(const wchar_t* op, bool ok, long long ms, const std::wstring& err = L"");

// RAII 计时：构造时开始，析构时写一条（默认成功，耗时自己算）。
struct EventScope {
    explicit EventScope(const wchar_t* op);
    ~EventScope();
    void Fail(const std::wstring& err) { ok_ = false; err_ = err; }
    void Ok() { ok_ = true; err_.clear(); }
    EventScope(const EventScope&) = delete;
    EventScope& operator=(const EventScope&) = delete;

private:
    const wchar_t* op_;
    unsigned long long t0_;
    bool ok_ = true;
    std::wstring err_;
};

// 测试用：把「拼一行 JSON」「脱敏」「落盘/滚动」拆开，才能在没有工作台环境的机器上验。
namespace event_internal {

long long Now();  // Unix 秒
std::wstring LogsDir();                                        // <StateDir>\logs
std::wstring FileFor(long long unixNow);                       // <StateDir>\logs\events-YYYYMMDD.jsonl
std::wstring IsoLocal(long long unixNow);                      // 2026-11-11T21:03:04+08:00
std::wstring Sanitize(const std::wstring& err);                // 单行 + 截断 500 + token 脱敏
std::string Line(const wchar_t* op, bool ok, long long ms, const std::wstring& err, long long unixNow);
// 追加一行（自己补 \n）；超过 rotateBytes 就把当前文件滚成 .1，最多留 keep 份。
bool Append(const std::wstring& path, const std::string& utf8Line, unsigned long long rotateBytes, int keep);

}  // namespace event_internal
}  // namespace her
