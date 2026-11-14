#pragma once
#include "her/her.h"
inline int SetupWizardStepV2(int step,int version,bool complete) {
    if(!complete&&version<2&&step>=2)++step;
    return (std::max)(0,(std::min)(5,step));
}
inline bool SetupMayAutoRepair(bool complete,bool enabled) { return complete&&enabled; }
inline bool SetupRequiredItemsReady(const her::J& catalog,const her::J& progress) {
    const auto* items=progress.Get(L"items");if(!items||!items->Arr())return false;
    int count=0;
    for(const auto& c:catalog.a)if(c.Str(L"tier")==L"required"){
        bool found=false;for(const auto& i:items->a)if(i.Str(L"id")==c.Str(L"id")&&i.Str(L"state")==L"done")found=true;
        if(!found)return false;
        ++count;
    }
    return count==8;
}
