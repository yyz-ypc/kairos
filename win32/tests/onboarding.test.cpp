#include "onboarding.h"
#include "setup_policy.h"
#include <cassert>
#include <iostream>
int main() {
    using her::J;
    auto live = J::Parse(L"{\"accounts\":[{\"uin\":\"10001\",\"online\":true}]}");
    assert(OnboardingQQOnline(live, true));
    assert(!OnboardingQQOnline(live, false));
    assert(!OnboardingQQOnline(J::Parse(L"{}"), true));
    assert(!OnboardingQQOnline(J::Parse(L"{\"accounts\":{}}"), true));
    assert(!OnboardingQQOnline(J::Parse(L"{\"accounts\":[]}"), true));
    assert(!OnboardingQQOnline(J::Parse(L"{\"accounts\":[{\"uin\":\"10001\",\"online\":false}]}"), true));
    assert(!OnboardingQQOnline(J::Parse(L"{\"accounts\":[{\"uin\":\"10001\",\"online\":true,\"demo\":true}]}"), true));
    assert(!OnboardingQQOnline(J::Parse(L"{\"accounts\":[{\"online\":true}]}"), true));
    for(int step=0;step<5;++step){
        int migrated=SetupWizardStepV2(step,1,false);
        assert(migrated==step+(step>=2?1:0));
        assert(SetupWizardStepV2(migrated,2,false)==migrated);
        assert(SetupWizardStepV2(step,1,true)==step);
    }
    assert(!SetupMayAutoRepair(false,true));assert(!SetupMayAutoRepair(true,false));assert(SetupMayAutoRepair(true,true));
    her::J catalog=her::JArr(),progress=her::JObj(),items=her::JArr();
    for(int i=0;i<8;++i){her::J c=her::JObj(),item=her::JObj();c.Set(L"id",std::to_wstring(i));c.Set(L"tier",L"required");catalog.Push(c);item.Set(L"id",std::to_wstring(i));item.Set(L"state",L"done");items.Push(item);}
    progress.Set(L"items",items);assert(SetupRequiredItemsReady(catalog,progress));
    for(auto state:{L"pending",L"checking",L"downloading",L"verifying",L"installing",L"failed",L"manual",L"skipped",L"elevation"}){
        items.a[0].Set(L"state",state);progress.Set(L"items",items);assert(!SetupRequiredItemsReady(catalog,progress));
    }
    std::cout << "OK: onboarding QQ gate (8 cases)\n";
}
