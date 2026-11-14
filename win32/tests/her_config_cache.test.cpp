// 配置缓存失效回归测试（任务 E1）
//
// 验的是：切工作区之后，her 层缓存的 answers.json / Base 必须换成新工作区的，
// 不能再把上一个工作区的答案和路径沿用下去。
//
// 从仓库根目录编译运行（只在临时目录 + 环境变量里折腾，不碰真实安装）：
//   cd win32
//   ..\..\F\biancheng... （见下，用你自己的 MinGW）
//   g++ -std=c++17 -O2 -Wall -DUNICODE -D_UNICODE -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 ^
//       -Isrc tests\her_config_cache.test.cpp src\her\her_config.cpp src\her\her_json.cpp ^
//       src\util.cpp src\paths.cpp -o build\her_config_cache.test.exe ^
//       -lgdiplus -lole32 -loleaut32 -luuid -lshell32 -lshlwapi -lcomdlg32 -lcomctl32 ^
//       -lws2_32 -liphlpapi -lgdi32 -luser32 -ladvapi32
//   build\her_config_cache.test.exe
//
// 或者直接跑 win32\tests\run-core-tests.bat（会编译并运行本测试与 her_event.test）。
#include "../src/her/her.h"

#include <cassert>
#include <cstdio>

// 只测配置缓存：网络/系统状态这些不在范围内，给最朴素的桩。
her::Http her::HttpGetJson(const std::wstring&, const std::wstring&, int) { return her::Http{}; }
bool her::Listening(int) { return false; }
her::ActionResult her::RunAction(const std::wstring&, const std::wstring&) { return {false, L"stub"}; }
bool her::Sqlite::OpenRead(const std::wstring&, std::wstring*) { return false; }
void her::Sqlite::Close() {}
bool her::Sqlite::QueryLongs(const std::wstring&, std::vector<long long>&, std::wstring*) { return false; }
bool her::Sqlite::QueryTexts(const std::wstring&, std::vector<std::vector<std::wstring>>&, std::wstring*) {
    return false;
}

namespace {

std::wstring TempRoot() {
    wchar_t cur[2048] = {0};
    GetCurrentDirectoryW(2048, cur);
    const std::wstring dir = JoinPath(cur, L"build\\cfgcache-test-" + Num(GetCurrentProcessId()));
    CreateDirectoryW(JoinPath(cur, L"build").c_str(), nullptr);
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir;
}

void WriteAnswers(const std::wstring& dir, int voice, const wchar_t* who) {
    her::J j = her::JObj();
    j.Set(L"voice", her::JNum(voice));
    j.Set(L"who", her::JStr(who));
    assert(her::WriteJsonAtomic(JoinPath(dir, L"answers.json"), j));
}

void WriteMarker(const std::wstring& dir, const wchar_t* text) {
    CreateDirectoryW(JoinPath(dir, L"private").c_str(), nullptr);
    assert(WriteTextFile(JoinPath(dir, L"private\\marker.txt"), text));
}

bool SamePath(const std::wstring& a, const std::wstring& b) { return _wcsicmp(a.c_str(), b.c_str()) == 0; }

void RmTree(const std::wstring& dir) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(JoinPath(dir, L"*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            const std::wstring n = fd.cFileName;
            if (n == L"." || n == L"..") continue;
            const std::wstring full = JoinPath(dir, n);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) RmTree(full);
            else DeleteFileW(full.c_str());
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryW(dir.c_str());
}

}  // namespace

int main() {
    const std::wstring root = TempRoot();
    const std::wstring A = JoinPath(root, L"A");
    const std::wstring B = JoinPath(root, L"B");
    const std::wstring state = JoinPath(root, L"state");
    for (const std::wstring& d : {A, B, state}) CreateDirectoryW(d.c_str(), nullptr);

    SetEnvironmentVariableW(L"KAIROS_WORKSPACE", A.c_str());
    SetEnvironmentVariableW(L"KAIROS_STATE_DIR", state.c_str());
    for(const auto& pair:std::vector<std::pair<std::wstring,std::wstring>>{{L"dark",L"nightfall"},{L"light",L"daylight"},{L"contrast",L"boundary"},{L"moon",L"moon"},{L"suit",L"suit"}}){SettingSet(L"theme",pair.first);SettingSet(L"migration_keep",L"untouched");assert(SettingGet(L"theme")==pair.second);assert(SettingRaw(L"theme")==L"\""+pair.second+L"\"");assert(SettingGet(L"migration_keep")==L"untouched");}


    WriteAnswers(A, 1, L"A");
    WriteAnswers(B, 2, L"B");
    WriteMarker(A, L"A 的 private");
    WriteMarker(B, L"B 的 private");

    her::InvalidateConfigCache();

    // ① 当前工作区是 A：Base / answers / private 都要指 A
    assert(SamePath(her::Base(), A));
    assert(her::Answers(L"who") == L"A");
    assert(her::Voice() == 1);
    assert(SamePath(her::P(L"private"), JoinPath(A, L"private")));
    assert(ReadTextFile(her::P(L"private\\marker.txt")) == L"A 的 private");
    // 再读一次（走缓存）还是 A
    assert(SamePath(her::Base(), A));
    assert(her::Answers(L"voice") == L"1");

    // ② 切到 B：环境变量优先于设置，先撤掉 KAIROS_WORKSPACE，让 SetWorkspaceDir 说了算
    SetEnvironmentVariableW(L"KAIROS_WORKSPACE", nullptr);
    ::SetWorkspaceDir(B);

    // 这一段的断言就是 E1 的回归点：以前这里还会返回 A
    assert(SamePath(her::Base(), B));
    assert(her::Answers(L"who") == L"B");
    assert(her::Voice() == 2);
    assert(SamePath(her::P(L"private"), JoinPath(B, L"private")));
    assert(ReadTextFile(her::P(L"private\\marker.txt")) == L"B 的 private");
    // 工作区目录本身也要跟着变（Base 不是 workaround 出来的）
    assert(SamePath(WorkspaceDir(), B));

    // ③ answers.json 在别处被改过（自设定 / 功能页写的），不等失效也要重读
    WriteAnswers(B, 7, L"B2");
    assert(her::Answers(L"voice") == L"7");

    // 未预配置的旧安装采用潮信；之后显式回退不能被读取逻辑偷偷切回。
    assert(WriteTextFile(JoinPath(B, L"private\\protocol.txt"), L"snowluma"));
    assert(her::Protocol() == L"snowluma");
    CreateDirectoryW(JoinPath(B, L"bots").c_str(), nullptr);
    CreateDirectoryW(JoinPath(B, L"bots\\Hermes").c_str(), nullptr);
    assert(WriteTextFile(JoinPath(B, L"bots\\Hermes\\start.cmd"), L"@echo off"));
    assert(her::Protocol() == L"hermes");
    assert(her::SetFeature(L"protocol/snowluma").ok);
    assert(her::Protocol() == L"snowluma");
    auto selected = her::SetFeature(L"protocol/hermes");
    assert(selected.ok);
    assert(her::Protocol() == L"hermes");
    assert(her::SetFeature(L"protocol/snowluma").ok);
    assert(her::Protocol() == L"snowluma");
    assert(!her::SetFeature(L"protocol/arbitrary").ok);
    assert(her::Protocol() == L"snowluma");

    std::puts("PASS: her_config_cache（切工作区后 Base/answers/private 跟着变）");
    SetEnvironmentVariableW(L"KAIROS_WORKSPACE", nullptr);
    SetEnvironmentVariableW(L"KAIROS_STATE_DIR", nullptr);
    RmTree(root);
    return 0;
}
