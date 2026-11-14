// Included by her_setup.cpp; owns the managed process, never the visual layout.
namespace {
struct SetupManager {
    std::mutex mutex;
    J progress=JObj(), catalog=JArr();
    std::atomic<bool> busy{false}, stop{false}, cancel{false};
    std::thread worker;
    bool boot=false, startupChecked=false;
    std::atomic<bool> restart{false}, repairPending{false};
    ULONGLONG bootAt=0, pauseAt=0;
    std::wstring toast, workspace;
    std::vector<std::wstring> queued;
    bool queuedForce=false;
};
SetupManager supply;
thread_local std::wstring setupWorkerWorkspace;
std::wstring SetupDir() { return JoinPath(JoinPath(setupWorkerWorkspace.empty()?WorkspaceDir():setupWorkerWorkspace,L"logs"),L"setup"); }
std::wstring SetupPath(const wchar_t* name) { return JoinPath(SetupDir(),name); }
J Catalog() {
    J doc=ReadJson(JoinPath(JoinPath(Root(),L"tools"),L"components.json"));
    const J* c=doc.Get(L"components"); return c&&c->Arr()?*c:JArr();
}
std::vector<std::wstring> Ids(const std::wstring& text) {
    std::vector<std::wstring> out; size_t pos=0;
    while(pos<text.size()) { size_t end=text.find(L',',pos); auto v=Trim(text.substr(pos,end==text.npos?text.npos:end-pos)); if(!v.empty())out.push_back(v); if(end==text.npos)break;pos=end+1; }
    return out;
}
bool Allowed(const J& c) { return c.Str(L"tier")!=L"manual" && !c.Str(L"id").empty(); }
J* SupplyItem(J& p,const std::wstring& id) {
    J* items=p.GetM(L"items"); if(!items||!items->Arr())return nullptr;
    for(auto& i:items->a) { if(i.Str(L"id")==id) return &i; }
    return nullptr;
}
void LoadCheckpoint() {
    J p=ReadJson(SetupPath(L"progress.json"));
    if(p.Int(L"schema")==2) {
        std::lock_guard<std::mutex> lock(supply.mutex);
        if(supply.cancel){p.Set(L"selection",JArr());p.Set(L"state",L"idle");}
        if(!supply.queued.empty()&&!supply.cancel){
            J selection=p.Get(L"selection")?*p.Get(L"selection"):JArr();
            for(auto& id:supply.queued){
                bool found=false;for(auto& v:selection.a)if(v.s==id)found=true;
                if(!found)selection.Push(J(id));
                if(auto* row=SupplyItem(p,id)){if(supply.queuedForce||row->Str(L"state")!=L"done"){row->Set(L"state",L"pending");row->Set(L"label",L"已加入队列");}}
            }
            p.Set(L"selection",selection);
            auto tmp=SetupPath(L"progress.json.queue.tmp");
            if(WriteTextFile(tmp,p.Dump()))MoveFileExW(tmp.c_str(),SetupPath(L"progress.json").c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
        }
        supply.progress=p;
    }
}
void SetupLine(const std::wstring& raw,void*) {
    const auto line=Trim(raw);
    bool total=StartsWith(line,L"##TOTAL "),item=StartsWith(line,L"##ITEM "),event=StartsWith(line,L"##EVENT ");
    if(!total&&!item&&!event)return;
    std::vector<std::wstring> fields;size_t pos=item?7:8;
    while(pos<=line.size()){auto end=line.find(L'|',pos);fields.push_back(line.substr(pos,end==line.npos?line.npos:end-pos));if(end==line.npos)break;pos=end+1;}
    std::lock_guard<std::mutex> lock(supply.mutex);
    if(total&&fields.size()==6){
        supply.progress.Set(L"pct",(std::max)(0,(std::min)(100,_wtoi(fields[0].c_str()))));
        supply.progress.Set(L"done",_wtoi64(fields[1].c_str()));supply.progress.Set(L"total",_wtoi64(fields[2].c_str()));
        supply.progress.Set(L"speed",_wtoi64(fields[3].c_str()));supply.progress.Set(L"eta",_wtoi64(fields[4].c_str()));supply.progress.Set(L"phase",fields[5]);return;
    }
    if((item&&fields.size()!=6)||(event&&fields.size()!=3))return;
    auto* row=SupplyItem(supply.progress,fields[0]);if(!row)return;
    row->Set(L"state",fields[1]);row->Set(L"label",fields[item?5:2]);
    if(item){
        row->Set(L"done",_wtoi64(fields[2].c_str()));row->Set(L"total",_wtoi64(fields[3].c_str()));row->Set(L"speed",_wtoi64(fields[4].c_str()));
        if(fields[1]==L"checking"||fields[1]==L"downloading"||fields[1]==L"verifying"||fields[1]==L"installing")supply.progress.Set(L"state",fields[1]);
    }
}
void StartSupply(std::vector<std::wstring> ids,bool check=false,bool repair=false,bool remove=false,bool force=false) {
    if(supply.busy.exchange(true))return;
    if(supply.worker.joinable())supply.worker.join();
    if(ids.empty()){supply.busy=false;return;}
    supply.stop=false;supply.cancel=false;supply.pauseAt=0;
    {std::lock_guard<std::mutex> lock(supply.mutex);supply.progress.Set(L"state",check?L"checking":L"installing");supply.progress.Set(L"phase",check?L"检查组件":L"准备安装");}
    supply.worker=std::thread([ids,check,repair,remove,force,workspace=WorkspaceDir(),bots=BotRoot()]{
        setupWorkerWorkspace=workspace;
        HANDLE gate=CreateMutexW(nullptr,FALSE,L"Local\\Kairos.Setup");
        DWORD acquired=gate?WaitForSingleObject(gate,0):WAIT_FAILED;
        if(acquired!=WAIT_OBJECT_0&&acquired!=WAIT_ABANDONED){
            if(gate)CloseHandle(gate);
            std::lock_guard<std::mutex> lock(supply.mutex);supply.toast=L"另一个组件任务正在运行";supply.progress.Set(L"state",L"idle");supply.busy=false;return;
        }
        EnsureWorkspaceLayout(workspace);CreateDirectoryW(SetupDir().c_str(),nullptr);
        HANDLE file=CreateFileW(SetupPath(L"lock").c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file==INVALID_HANDLE_VALUE){
            ReleaseMutex(gate);CloseHandle(gate);std::lock_guard<std::mutex> lock(supply.mutex);supply.toast=L"组件目录正被使用，稍后重试";supply.progress.Set(L"state",L"failed");supply.busy=false;return;
        }
        DeleteFileW(SetupPath(L"pause").c_str());
        std::wstring list;for(auto& id:ids){if(!list.empty())list+=L",";list+=id;}
        auto args=L"-NoProfile -ExecutionPolicy Bypass -File \""+JoinPath(JoinPath(Root(),L"tools"),L"bootstrap.ps1")+
            L"\" -Base \""+ArgPath(workspace)+L"\" -Workspace \""+ArgPath(workspace)+L"\" -Bots \""+ArgPath(bots)+L"\" -Component "+list;
        if(check)args+=L" -Check";
        if(remove)args+=L" -Uninstall";
        if(force)args+=L" -Force";
        // Never use -Resume: an interrupted in-flight component is not in the old end-of-run file.
        ULONGLONG poll=0,itemAt=GetTickCount64();std::wstring current;
        auto heartbeat=[&]{
            ULONGLONG now=GetTickCount64();
            if(now-poll>=500){
                if(!check) LoadCheckpoint();
                poll=now;
                std::lock_guard<std::mutex> lock(supply.mutex);
                const J* items=supply.progress.Get(L"items");
                if(items)for(auto& i:items->a)if(i.Str(L"state")==L"checking"||i.Str(L"state")==L"downloading"||i.Str(L"state")==L"installing"||i.Str(L"state")==L"verifying"){
                    if(i.Str(L"id")!=current){current=i.Str(L"id");itemAt=now;}break;
                }
            }
            if(supply.stop){if(!supply.pauseAt)supply.pauseAt=now;WriteTextFile(SetupPath(L"pause"),L"pause");if(now-supply.pauseAt>=8000)return false;}
            return now-itemAt<30ULL*60*1000;
        };
        CmdResult result=RunCaptureStream(PowerShellExe(),args,Root(),EnvList(),90*60*1000,SetupLine,nullptr,heartbeat);
        if(!check)LoadCheckpoint();
        bool needsRepair=false;
        {std::lock_guard<std::mutex> lock(supply.mutex);
            bool paused=supply.stop||result.code==75;
            supply.progress.Set(L"state",paused?L"paused":(result.code==0?L"done":L"failed"));
            if(supply.cancel){supply.progress.Set(L"selection",JArr());supply.progress.Set(L"state",L"idle");}
            if(paused){
                supply.progress.Set(L"phase",supply.cancel?L"已取消队列，下载文件保留":L"已暂停，下次继续");
                supply.progress.Set(L"speed",0);supply.progress.Set(L"eta",0);
                if(auto* items=supply.progress.GetM(L"items"))for(auto& item:items->a){
                    auto state=item.Str(L"state");
                    if(state==L"checking"||state==L"downloading"||state==L"verifying"||state==L"installing"){
                        item.Set(L"speed",0);item.Set(L"label",supply.cancel?L"已取消，下载文件保留":L"已暂停，下次继续");
                    }
                }
            }
            if(!check&&!paused)supply.toast=result.code==0?L"本轮组件准备结束":L"组件安装未完成，请到设置重试";
            if(check&&repair&&!paused&&result.code==2&&SetupMayAutoRepair(SettingGet(L"wizard_complete")==L"1",SettingGet(L"setup_auto_repair")!=L"0")){
                supply.restart=true;supply.repairPending=true;needsRepair=true;supply.toast=L"发现缺少运行必需组件，开始自动补装";
            }
            if(check&&result.code!=0&&result.code!=2&&!paused)supply.toast=L"组件检查未完成，请到设置重试";
            if(result.timeout&&!paused)supply.progress.Set(L"phase",L"安装超时，请重试；已有下载已保留");
            // Persist only install checkpoints, never overwrite a paused queue with a check.
            if(!check){auto tmp=SetupPath(L"progress.json.tmp");if(WriteTextFile(tmp,supply.progress.Dump()))MoveFileExW(tmp.c_str(),SetupPath(L"progress.json").c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);}
        }
        (void)needsRepair;
        CloseHandle(file);ReleaseMutex(gate);CloseHandle(gate);InvalidateConfigCache();supply.busy=false;
    });
}
std::vector<std::wstring> RequiredIds() {
    std::vector<std::wstring> ids;for(auto& c:supply.catalog.a)if(c.Str(L"tier")==L"required")ids.push_back(c.Str(L"id"));return ids;
}
std::vector<std::wstring> Unfinished() {
    std::vector<std::wstring> ids;std::lock_guard<std::mutex> lock(supply.mutex);
    const J* selection=supply.progress.Get(L"selection");if(!selection)return ids;
    for(auto& v:selection->a){auto* item=SupplyItem(supply.progress,v.s);if(item&&item->Str(L"state")!=L"done"&&item->Str(L"state")!=L"manual"&&item->Str(L"state")!=L"skipped"&&SettingGet(L"setup_select_"+v.s)!=L"0")ids.push_back(v.s);}
    return ids;
}
} // namespace
J SetupSnapshot() {
    std::lock_guard<std::mutex> lock(supply.mutex);J p=supply.progress;
    p.Set(L"busy",supply.busy.load());p.Set(L"auto_repair",SettingGet(L"setup_auto_repair")!=L"0");
    J catalog=supply.catalog;
    for(auto& c:catalog.a){
        auto tier=c.Str(L"tier"),id=c.Str(L"id"),choice=SettingGet(L"setup_select_"+id);
        c.Set(L"selected",tier==L"required"||(tier==L"default"&&choice!=L"0"&&(id!=L"ollama"||choice==L"1"))||(tier==L"on-demand"&&choice==L"1"));
    }
    if(auto* items=p.GetM(L"items"))for(auto& i:items->a)if(SettingGet(L"setup_select_"+i.Str(L"id"))==L"0"&&i.Str(L"state")!=L"done"){i.Set(L"state",L"skipped");i.Set(L"label",L"本次不安装");}
    p.Set(L"components",catalog);return p;
}
bool SetupRequiredReady() {
    if(supply.workspace!=WorkspaceDir())return false;
    std::lock_guard<std::mutex> lock(supply.mutex);return SetupRequiredItemsReady(supply.catalog,supply.progress);
}
void SetupCommand(const std::wstring& op,const std::wstring& id,const std::wstring& selection) {
    if(supply.boot&&supply.workspace!=WorkspaceDir()){
        if(!supply.busy)SetupTick();
        else if(op!=L"pause"&&op!=L"cancel"&&op!=L"log"){
            std::lock_guard<std::mutex> lock(supply.mutex);supply.toast=L"正在切换工作区，请稍后重试";return;
        }
    }
    if(op==L"auto-repair"){SettingSet(L"setup_auto_repair",id==L"0"?L"0":L"1");return;}
    if(op==L"log"){ShellExecuteW(nullptr,L"open",SetupDir().c_str(),nullptr,nullptr,SW_SHOWNORMAL);return;}
    if(op==L"pause"||op==L"cancel"){
        supply.stop=true;WriteTextFile(SetupPath(L"pause"),L"pause");
        if(op==L"cancel"){supply.cancel=true;supply.restart=false;supply.repairPending=false;std::lock_guard<std::mutex> lock(supply.mutex);supply.queued.clear();supply.progress.Set(L"selection",JArr());supply.restart=false;supply.repairPending=false;
            auto tmp=SetupPath(L"progress.json.cancel.tmp");if(WriteTextFile(tmp,supply.progress.Dump()))MoveFileExW(tmp.c_str(),SetupPath(L"progress.json").c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
        }
        return;
    }
    const J* comp=nullptr;for(auto& c:supply.catalog.a)if(c.Str(L"id")==id)comp=&c;
    if(op==L"select"||op==L"skip-optional"||op==L"uninstall"){
        if(!comp)return;
        if(comp->Str(L"tier")==L"required"){std::lock_guard<std::mutex> lock(supply.mutex);supply.toast=L"运行必需，不能跳过";return;}
        if(op==L"skip-optional"&&comp->Str(L"tier")!=L"default")return;
        if(op==L"select"){
            if(comp->Str(L"tier")!=L"default")return;
            SettingSet(L"setup_select_"+id,L"1");
            std::vector<std::wstring> ids{id};
            if(id==L"ollama"&&SettingGet(L"setup_select_ollama-service")!=L"0")ids.push_back(L"ollama-service");
            if(supply.busy){
                std::lock_guard<std::mutex> lock(supply.mutex);
                for(auto& cid:ids)if(std::find(supply.queued.begin(),supply.queued.end(),cid)==supply.queued.end())supply.queued.push_back(cid);
            }else{
                bool paused=false;
                {std::lock_guard<std::mutex> lock(supply.mutex);paused=supply.progress.Str(L"state")==L"paused";
                    if(paused){
                        J selection=supply.progress.Get(L"selection")?*supply.progress.Get(L"selection"):JArr();
                        for(auto& cid:ids){bool found=false;for(auto& v:selection.a)if(v.s==cid)found=true;if(!found)selection.Push(J(cid));}
                        supply.progress.Set(L"selection",selection);
                        auto tmp=SetupPath(L"progress.json.queue.tmp");if(WriteTextFile(tmp,supply.progress.Dump()))MoveFileExW(tmp.c_str(),SetupPath(L"progress.json").c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
                    }
                }
                if(!paused)StartSupply(ids);
            }
            return;
        }
        if(op==L"uninstall"&&supply.busy){std::lock_guard<std::mutex> lock(supply.mutex);supply.toast=L"请先暂停当前组件任务，再卸载";return;}
        SettingSet(L"setup_select_"+id,L"0");
        if(op==L"uninstall"){if(!supply.busy)StartSupply({id},false,false,true);return;}
        {std::lock_guard<std::mutex> lock(supply.mutex);auto* item=SupplyItem(supply.progress,id);if(item&&item->Str(L"state")!=L"done")item->Set(L"state",L"skipped");}
        if(supply.busy){supply.stop=true;supply.restart=true;WriteTextFile(SetupPath(L"pause"),L"pause");}
        return;
    }
    if(op==L"resume"){
        auto ids=Unfinished();
        if(ids.empty()&&!SetupRequiredReady()) ids=RequiredIds();
        StartSupply(ids);return;
    }
    if(op!=L"start"&&op!=L"retry"&&op!=L"mirror")return;
    if(op==L"mirror"){WriteTextFile(SetupPath(L"next-mirror"),id);}
    std::vector<std::wstring> ids;
    if(!id.empty()){if(!comp||!Allowed(*comp))return;ids.push_back(id);SettingSet(L"setup_select_"+id,L"1");if(id==L"ollama"&&SettingGet(L"setup_select_ollama-service")!=L"0")ids.push_back(L"ollama-service");}
    else {
        auto chosen=Ids(selection);
        for(auto& c:supply.catalog.a){auto cid=c.Str(L"id"),tier=c.Str(L"tier");
            if(tier==L"required"||(tier==L"default"&&SettingGet(L"setup_select_"+cid)!=L"0"&&(cid!=L"ollama"||SettingGet(L"setup_select_ollama")==L"1")&&(chosen.empty()||std::find(chosen.begin(),chosen.end(),cid)!=chosen.end())))ids.push_back(cid);
        }
    }
    if(supply.busy){std::lock_guard<std::mutex> lock(supply.mutex);for(auto& cid:ids)if(std::find(supply.queued.begin(),supply.queued.end(),cid)==supply.queued.end())supply.queued.push_back(cid);supply.queuedForce=supply.queuedForce||op==L"retry";return;}
    StartSupply(ids,false,false,false,op==L"retry");
}
void SetupTick() {
    if(supply.boot&&supply.workspace!=WorkspaceDir()){
        if(supply.busy){supply.stop=true;return;}
        if(supply.worker.joinable())supply.worker.join();
        {std::lock_guard<std::mutex> lock(supply.mutex);supply.progress=JObj();supply.queued.clear();}
        supply.boot=false;supply.startupChecked=false;
    }
    if(!supply.boot){
        supply.workspace=WorkspaceDir();
        supply.boot=true;supply.bootAt=GetTickCount64();supply.catalog=Catalog();LoadCheckpoint();
        if(_wtoi(SettingGet(L"wizard_ver").c_str())<2){
            if(SettingGet(L"wizard_complete")!=L"1"){int step=SetupWizardStepV2(_wtoi(SettingGet(L"wizard_step").c_str()),_wtoi(SettingGet(L"wizard_ver").c_str()),false);SettingSet(L"wizard_step",std::to_wstring(step));}
            SettingSet(L"wizard_ver",L"2");
        }
        {std::lock_guard<std::mutex> lock(supply.mutex);if(!supply.progress.Obj())supply.progress=JObj();
            if(!supply.progress.Has(L"items")){J items=JArr();for(auto& c:supply.catalog.a){J i=JObj();i.Set(L"id",c.Str(L"id"));i.Set(L"state",c.Str(L"tier")==L"manual"?L"manual":L"pending");i.Set(L"label",L"等待");items.Push(i);}supply.progress.Set(L"items",items);supply.progress.Set(L"state",L"idle");}}
        if(SettingGet(L"wizard_complete")!=L"1"){
            auto ids=Unfinished();if(!ids.empty()){if(!SetupRequiredReady()||_wtoi(SettingGet(L"wizard_step").c_str())<2)SettingSet(L"wizard_step",L"2");StartSupply(ids);}
        }
    }
    std::wstring toast;
    {std::lock_guard<std::mutex> lock(supply.mutex);toast.swap(supply.toast);}
    if(!toast.empty())UiToast(toast);
    if(!supply.busy&&supply.restart.exchange(false)){auto ids=supply.repairPending.exchange(false)?RequiredIds():Unfinished();if(ids.empty())ids=RequiredIds();StartSupply(ids);}
    if(!supply.busy&&!supply.stop){
        std::vector<std::wstring> queued;bool force=false;
        {std::lock_guard<std::mutex> lock(supply.mutex);queued.swap(supply.queued);force=supply.queuedForce;supply.queuedForce=false;}
        queued.erase(std::remove_if(queued.begin(),queued.end(),[](const std::wstring& id){return SettingGet(L"setup_select_"+id)==L"0";}),queued.end());
        if(!queued.empty())StartSupply(queued,false,false,false,force);
    }
    if(!supply.startupChecked&&SettingGet(L"wizard_complete")==L"1"&&GetTickCount64()-supply.bootAt>=8000&&!supply.busy){supply.startupChecked=true;StartSupply(RequiredIds(),true,true);}
    UiRequestRepaint();
}
void SetupShutdown(){supply.stop=true;WriteTextFile(SetupPath(L"pause"),L"pause");if(supply.worker.joinable())supply.worker.join();}
