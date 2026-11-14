#include "../src/release_policy.h"
#include "../src/theme_policy.h"
#include <cassert>
#include <cstdio>
namespace her { void InvalidateConfigCache(){} }
int main(){
 using namespace her;
 const auto hash=std::wstring(64,L'a');
 J root=JObj();root.Set(L"schema",1);root.Set(L"version",L"V1.1.0");root.Set(L"min_version",L"1.0.0");root.Set(L"channel",L"stable");root.Set(L"release",L"https://github.com/YYZ-YPC/kairos/releases/tag/V1.1.0");
 J asset=JObj();asset.Set(L"name",L"Kairos-1.1.0-setup.exe");asset.Set(L"url",L"https://github.com/YYZ-YPC/kairos/releases/download/V1.1.0/Kairos-1.1.0-setup.exe");asset.Set(L"sha256",hash);asset.Set(L"size",42);J assets=JArr();assets.Push(asset);root.Set(L"assets",assets);
 assert(ValidateRelease(root,L"stable",L"V1.0.0").ok);assert(!ValidateRelease(root,L"beta",L"V1.0.0").ok);
 auto bad=root;bad.Set(L"min_version",L"2.0.0");assert(!ValidateRelease(bad,L"stable",L"V1.0.0").ok);
 bad=root;bad.Set(L"version",L"invalid");assert(!ValidateRelease(bad,L"stable",L"V1.0.0").ok);
 bad=root;auto a=asset;a.Set(L"sha256",L"bad");J aa=JArr();aa.Push(a);bad.Set(L"assets",aa);assert(!ValidateRelease(bad,L"stable",L"V1.0.0").ok);
 auto beta=root;beta.Set(L"channel",L"beta");beta.Set(L"version",L"V1.1.0-beta.2");a=asset;a.Set(L"name",L"Kairos-1.1.0-beta.2-setup.exe");a.Set(L"url",L"https://github.com/YYZ-YPC/kairos/releases/download/V1.1.0-beta.2/Kairos-1.1.0-beta.2-setup.exe");aa=JArr();aa.Push(a);beta.Set(L"assets",aa);assert(ValidateRelease(beta,L"beta",L"V1.0.0").ok);assert(!ValidateRelease(beta,L"stable",L"V1.0.0").ok);
 assert(!ReleaseHttps(L"http://github.com/YYZ-YPC/kairos/releases/tag/V1"));assert(!ReleaseHttps(L"https://github.com.evil/YYZ-YPC/kairos/releases/tag/V1"));
 assert(ReleaseCompare(L"V1.0.1-beta.2",L"V1.0.1-beta.1")>0);assert(ReleaseCompare(L"V1.0.1",L"V1.0.1-beta.9")>0);
 assert(!ValidateRelease(root,L"stable",L"V1.0.0",true).ok);
 auto previous=root;previous.Set(L"version",L"V1.0.0");a=asset;a.Set(L"name",L"Kairos-1.0.0-setup.exe");a.Set(L"url",L"https://github.com/YYZ-YPC/kairos/releases/download/V1.0.0/Kairos-1.0.0-setup.exe");aa=JArr();aa.Push(a);previous.Set(L"assets",aa);root.Set(L"previous",previous);assert(ValidateRelease(root,L"stable",L"V1.1.0",true).ok);
 assert(CanonicalTheme(L"dark")==L"nightfall");assert(CanonicalTheme(L"light")==L"daylight");assert(CanonicalTheme(L"contrast")==L"boundary");assert(CanonicalTheme(L"moon")==L"moon");
 puts("Release policy passed: versions, channels, HTTPS, hashes, minimum, rollback");
}