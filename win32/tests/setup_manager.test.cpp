// SetupManager integration: isolated settings, actual timers and child processes.
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


#include "../src/ui.h"
#include <iostream>
std::wstring lastToast;
void UiToast(const std::wstring& text){lastToast=text;}
void UiRequestRepaint(){}
static void tickFor(DWORD duration){DWORD start=GetTickCount();while(GetTickCount()-start<duration){her::SetupTick();Sleep(100);}}
int main(int argc,char** argv){
    assert(argc==2);std::string scenario=argv[1];
    wchar_t cwd[4096]{};GetCurrentDirectoryW(4096,cwd);
    auto root=JoinPath(JoinPath(cwd,L".tmp"),L"native-"+Utf8ToW(scenario)+L"-"+Num(GetCurrentProcessId()));
    auto state=JoinPath(root,L"state");
    SetEnvironmentVariableW(L"KAIROS_STATE_DIR",state.c_str());SetEnvironmentVariableW(L"KAIROS_WORKSPACE",root.c_str());SetEnvironmentVariableW(L"KAIROS_BASE",root.c_str());
    EnsureWorkspaceLayout(root);
    SetWorkspaceDir(root);SetBaseDir(root);her::InvalidateConfigCache();
    SettingSet(L"wizard_complete",scenario=="welcome"?L"0":L"1");SettingSet(L"wizard_step",L"4");SettingSet(L"wizard_ver",L"1");
    SettingSet(L"setup_auto_repair",scenario=="disabled"?L"0":L"1");
    auto progress=JoinPath(JoinPath(JoinPath(root,L"logs"),L"setup"),L"progress.json");
    if(scenario=="repair"||scenario=="optional"){
        her::J p=her::JObj(),items=her::JArr(),selection=her::JArr();
        p.Set(L"schema",2);p.Set(L"state",L"paused");selection.Push(her::J(L"breeze"));p.Set(L"selection",selection);
        for(auto id:{L"webview2",L"node",L"ffmpeg",L"uv",L"python",L"astrbot",L"hermes",L"bots-launchers",L"breeze"}){
            her::J item=her::JObj();item.Set(L"id",id);item.Set(L"state",scenario=="optional"&&std::wstring(id)!=L"breeze"?L"done":L"pending");items.Push(item);
        }
        p.Set(L"items",items);CreateDirectoryW(ParentDir(progress).c_str(),nullptr);assert(WriteTextFile(progress,p.Dump()));
    }
    if(scenario=="future"){
        SettingSet(L"wizard_complete",L"0");SettingSet(L"wizard_ver",L"3");SettingSet(L"wizard_step",L"4");her::SetupTick();
        assert(SettingGet(L"wizard_ver")==L"3");assert(SettingGet(L"wizard_step")==L"4");assert(!FileExists(progress));
    }else if(scenario=="optional"){
        SettingSet(L"wizard_complete",L"0");SettingSet(L"wizard_ver",L"2");SettingSet(L"wizard_step",L"4");her::SetupTick();
        assert(SettingGet(L"wizard_step")==L"4");assert(her::SetupRequiredReady());
    }else if(scenario=="welcome"){
        SettingSet(L"wizard_step",L"0");tickFor(10000);assert(!her::SetupSnapshot().Bool(L"busy"));assert(!FileExists(progress));assert(SettingGet(L"wizard_ver")==L"2");assert(SettingGet(L"wizard_step")==L"0");
    }else if(scenario=="disabled"){
        tickFor(12000);assert(!her::SetupSnapshot().Bool(L"busy"));assert(!FileExists(progress));assert(SettingGet(L"wizard_complete")==L"1");assert(SettingGet(L"wizard_step")==L"4");assert(her::SetupSnapshot().Str(L"state")==L"failed");
    }else{
        DWORD start=GetTickCount();bool downloading=false;
        while(GetTickCount()-start<18000){her::SetupTick();auto p=her::SetupSnapshot();if(p.Str(L"state")==L"downloading"){downloading=true;break;}Sleep(50);}
        assert(downloading);assert(GetTickCount()-start>=8000);assert(FileExists(progress));
        her::SetupCommand(L"pause");start=GetTickCount();while(her::SetupSnapshot().Bool(L"busy")&&GetTickCount()-start<10000){her::SetupTick();Sleep(50);}
        assert(!her::SetupSnapshot().Bool(L"busy"));assert(her::SetupSnapshot().Str(L"state")==L"paused");
        her::SetupCommand(L"skip-optional",L"node");her::SetupTick();assert(lastToast==L"运行必需，不能跳过");assert(SettingGet(L"setup_select_node")!=L"0");
        // Changing a default choice while paused preserves both the pause and queue.
        her::SetupCommand(L"select",L"breeze");tickFor(250);
        assert(!her::SetupSnapshot().Bool(L"busy"));assert(her::SetupSnapshot().Str(L"state")==L"paused");
        auto retained=her::ReadJson(progress);bool hasBreeze=false,hasNode=false;
        for(auto& id:retained.Get(L"selection")->a){hasBreeze=hasBreeze||id.s==L"breeze";hasNode=hasNode||id.s==L"node";}
        assert(hasBreeze&&hasNode);
        her::SetupCommand(L"cancel");auto saved=her::ReadJson(progress);assert(saved.Get(L"selection")&&saved.Get(L"selection")->a.empty());
    }
    her::SetupShutdown();std::cout<<"OK: native setup "<<scenario<<"\n";
}
