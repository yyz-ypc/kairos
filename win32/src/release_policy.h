#pragma once
#include "her/her.h"
#include <array>
#include <regex>
namespace her {
inline bool ReleaseVersion(const std::wstring& s,std::array<int,4>& out) {
    std::wsmatch m;
    if(!std::regex_match(s,m,std::wregex(L"V?([0-9]{1,5})\\.([0-9]{1,5})\\.([0-9]{1,5})(?:-beta\\.([0-9]{1,5}))?")))return false;
    for(int i=0;i<3;i++)out[i]=std::stoi(m[i+1].str());
    out[3]=m[4].matched?std::stoi(m[4].str()):100000;
    return true;
}
inline int ReleaseCompare(const std::wstring& a,const std::wstring& b) {
    std::array<int,4> av{},bv{};if(!ReleaseVersion(a,av)||!ReleaseVersion(b,bv))return 0;
    return av==bv?0:av<bv?-1:1;
}
inline bool ReleaseHttps(const std::wstring& s) {
    const std::wstring prefix=L"https://github.com/YYZ-YPC/kairos/releases/";
    return s.rfind(prefix,0)==0&&s.find_first_of(L"\r\n\\\t ")==s.npos&&s.find(L"..",prefix.size())==s.npos;
}
struct ReleaseChoice {bool ok=false;std::wstring version,url,hash,name,release,error;unsigned long long size=0;};
inline ReleaseChoice ValidateRelease(const J& root,const std::wstring& channel,const std::wstring& local,bool previous=false) {
    ReleaseChoice c;const J* p=&root;
    if(previous){p=root.Get(L"previous");if(!p||!p->Obj()){c.error=L"no previous release";return c;}}
    const J& j=*p;std::array<int,4> v{},m{};
    c.version=j.Str(L"version");c.release=j.Str(L"release");const auto min=j.Str(L"min_version");
    if(j.Int(L"schema")!=1||j.Str(L"channel")!=channel||!ReleaseVersion(c.version,v)||!ReleaseVersion(min,m)||ReleaseCompare(local,min)<0||!ReleaseHttps(c.release)||c.release.find(L"/tag/")==c.release.npos||(channel==L"stable"&&v[3]!=100000)) {c.error=L"invalid release metadata";return c;}
    if(previous&&ReleaseCompare(c.version,root.Str(L"version"))>=0){c.error=L"previous is not older";return c;}
    const J* assets=j.Get(L"assets");if(!assets||!assets->Arr()){c.error=L"missing assets";return c;}
    for(const auto& a:assets->a) {
        const auto name=a.Str(L"name");if(name.rfind(L"Kairos-",0)!=0||name.size()<10||name.substr(name.size()-10)!=L"-setup.exe")continue;
        auto hash=a.Str(L"sha256");bool hex=hash.size()==64;for(auto ch:hash)if(!((ch>=L'0'&&ch<=L'9')||(ch>=L'a'&&ch<=L'f')))hex=false;
        const auto size=a.Int(L"size");
        if(!hex||size<=0||size>2147483648LL||!ReleaseHttps(a.Str(L"url"))||a.Str(L"url").find(L"/download/")==std::wstring::npos||name.find_first_of(L"/\\:")!=name.npos){c.error=L"invalid asset";return c;}
        auto shortVersion=c.version;if(shortVersion[0]==L'V')shortVersion.erase(0,1);
        if(a.Str(L"url").size()<name.size()||name!=L"Kairos-"+shortVersion+L"-setup.exe"||a.Str(L"url").substr(a.Str(L"url").size()-name.size())!=name){c.error=L"version/asset mismatch";return c;}
        c.url=a.Str(L"url");c.name=name;c.hash=hash;c.size=(unsigned long long)size;c.ok=true;return c;
    }
    c.error=L"no setup asset";return c;
}
}
