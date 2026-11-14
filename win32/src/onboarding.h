#pragma once
#include "her/her.h"
// Only an authenticated response containing a real, online QQ session can unlock first run.
inline bool OnboardingQQOnline(const her::J& live, bool authenticated) {
    const her::J* accounts = live.Get(L"accounts");
    if (!authenticated || !accounts || !accounts->Arr()) return false;
    for (const auto& a : accounts->a)
        if (a.Bool(L"online") && !a.Bool(L"demo") && !a.Str(L"uin").empty()) return true;
    return false;
}
