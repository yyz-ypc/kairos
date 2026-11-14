#pragma once
#include <string>
inline std::wstring CanonicalTheme(const std::wstring& v) {
    if(v==L"dark") return L"nightfall";
    if(v==L"light") return L"daylight";
    if(v==L"contrast") return L"boundary";
    return v;
}
