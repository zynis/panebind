#include "platform/windows/explorer/explorer_group_batch.h"
#include <array>
#include <iostream>
namespace d=panebind::platform::windows::explorer::detail;
namespace panebind::platform::windows::explorer::detail {
class GroupBatchTestAccess {
public:
    template<class Api> static auto run(std::span<const GroupNativeTarget> t,bool p,bool r,Api& api) {
        const std::array<bool,2> members{true,p};
        return GroupBatchExecutor::run(t,members,r,api);
    }
};
}
struct Fake {
    int fail{}; int begins{}; int defers{}; int ends{}; bool replacement_valid{true};
    HDWP begin(int count) noexcept { ++begins; replacement_valid=count==2; return fail==1?nullptr:reinterpret_cast<HDWP>(1); }
    HDWP defer(HDWP current,const d::GroupNativeTarget&) noexcept {
        ++defers; replacement_valid &= current==reinterpret_cast<HDWP>(static_cast<std::uintptr_t>(defers));
        return fail==defers+1?nullptr:reinterpret_cast<HDWP>(static_cast<std::uintptr_t>(defers+1));
    }
    bool end(HDWP current) noexcept { ++ends; replacement_valid &= current==reinterpret_cast<HDWP>(3); return fail!=4; }
    DWORD error() const noexcept { return 123; }
};
int main() {
    int failures{};
    const auto check=[&](bool pass){if(!pass)++failures;};
    const std::array<d::GroupNativeTarget,2> targets{{{nullptr,{1,2,101,102}},{nullptr,{101,2,201,102}}}};
    for(int failure=0;failure<=4;++failure) {
        Fake fake{failure};
        const auto result=d::GroupBatchTestAccess::run(targets,true,true,fake);
        check(result.succeeded==(failure==0)); check(fake.replacement_valid);
        check(fake.ends==((failure==0||failure==4)?1:0));
        check(result.native_commit_attempted==(fake.ends==1));
        if(failure)check(result.error==123);
    }
    for(bool registered:{false,true}) {
        Fake fake;
        auto result=d::GroupBatchTestAccess::run(targets,false,registered,fake);
        check(!result.succeeded&&fake.begins==0&&fake.defers==0&&fake.ends==0);
    }
    Fake fake;
    check(!d::GroupBatchTestAccess::run(targets,true,false,fake).succeeded&&fake.begins==0);
    // Real HDWP observation on two hidden windows CREATED BY THIS TEST. No
    // Explorer or preexisting user windows, no activation or resize.
    std::array<HWND,2> windows{};
    for(auto& w:windows)w=CreateWindowExW(0,L"STATIC",L"PaneBind owned HDWP probe",WS_POPUP,20,30,200,150,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    std::array<d::GroupNativeTarget,2> actual_targets;
    for(std::size_t i=0;i<2;++i) {
        check(windows[i]!=nullptr); RECT rect{};check(GetWindowRect(windows[i],&rect)!=FALSE);
        actual_targets[i]={windows[i],{rect.left+10,rect.top+20,rect.right+10,rect.bottom+20}};
    }
    const HWND foreground=GetForegroundWindow();
    d::GroupNativeApi native;
    const auto observed=d::GroupBatchTestAccess::run(actual_targets,true,true,native);
    check(observed.succeeded && observed.deferred==2 && observed.native_commit_attempted);
    for(std::size_t i=0;i<2;++i) {
        RECT r{};check(GetWindowRect(windows[i],&r)!=FALSE);
        check(panebind::core::geometry::Rect{r.left,r.top,r.right,r.bottom}==actual_targets[i].positioning);
        if(windows[i])DestroyWindow(windows[i]);
    }
    check(GetForegroundWindow()==foreground);
    std::cout<<"group-native-batch failures="<<failures<<'\n';
    return failures?1:0;
}
