#include "platform/windows/explorer/explorer_group_event_source.h"
#include <iostream>
namespace panebind::platform::windows::explorer {
class GroupEventSourceTestAccess final {
public:
    static std::unique_ptr<ExplorerGroupEventSource> make(const std::array<HWND,3>& windows) {
        std::array<detail::GroupMemberBinding,3> bindings;
        for(std::size_t i=0;i<3;++i) {
            DWORD pid{};const auto tid=GetWindowThreadProcessId(windows[i],&pid);
            if(pid!=GetCurrentProcessId())return nullptr;
            bindings[i]={i+1,i+11,i+21,windows[i],pid,tid};
        }
        auto s=std::unique_ptr<ExplorerGroupEventSource>(new ExplorerGroupEventSource(bindings));
        s->synthetic_=true;s->running_=true;s->synthetic_ctrl_={true,true,true,false};
        s->hooks_[0]={reinterpret_cast<HWINEVENTHOOK>(1),GetCurrentProcessId(),EVENT_MIN,EVENT_MAX};
        s->hook_count_=1;return s;
    }
    static void send(ExplorerGroupEventSource& s,HWND w,DWORD kind,LONG object=OBJID_WINDOW) {
        s.receive(reinterpret_cast<HWINEVENTHOOK>(1),kind,w,object,CHILDID_SELF,GetCurrentThreadId(),GetTickCount());
    }
    static auto drain(ExplorerGroupEventSource& s){return s.drain();}
    static auto facts(ExplorerGroupEventSource& s){return s.facts();}
    static bool pending(ExplorerGroupEventSource& s){return s.lifecycle_pending();}
};
}
namespace e=panebind::platform::windows::explorer;
int main() {
    int failures{};const auto check=[&](bool ok){if(!ok)++failures;};
    std::array<HWND,3> windows{};
    for(auto& w:windows)w=CreateWindowExW(0,L"STATIC",L"PaneBind owned event seam",WS_POPUP,0,0,100,100,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    for(auto w:windows)check(w!=nullptr);
    {
        auto s=e::GroupEventSourceTestAccess::make(windows);check(s!=nullptr);
        for(auto w:windows)e::GroupEventSourceTestAccess::send(*s,w,EVENT_SYSTEM_MOVESIZESTART);
        auto events=e::GroupEventSourceTestAccess::drain(*s);check(events.size()==3);
        for(std::size_t i=0;i<events.size();++i)check(events[i].member_index==i && events[i].window_id==i+1 && events[i].ctrl.ctrl && events[i].sequence==i+1);
        e::GroupEventSourceTestAccess::send(*s,windows[1],EVENT_OBJECT_LOCATIONCHANGE);
        events=e::GroupEventSourceTestAccess::drain(*s);check(events.size()==1 && !events[0].ctrl.available);
        e::GroupEventSourceTestAccess::send(*s,GetDesktopWindow(),EVENT_OBJECT_LOCATIONCHANGE);
        e::GroupEventSourceTestAccess::send(*s,windows[1],EVENT_OBJECT_LOCATIONCHANGE,OBJID_CLIENT);
        check(e::GroupEventSourceTestAccess::facts(*s).ignored==2);
        e::GroupEventSourceTestAccess::send(*s,windows[2],EVENT_OBJECT_DESTROY);
        check(e::GroupEventSourceTestAccess::pending(*s));
    }
    {
        auto s=e::GroupEventSourceTestAccess::make(windows);
        for(int i=0;i<4097;++i)e::GroupEventSourceTestAccess::send(*s,windows[0],EVENT_OBJECT_LOCATIONCHANGE);
        const auto f=e::GroupEventSourceTestAccess::facts(*s);
        check(f.accepted==4096 && f.max_depth==4096 && f.overflow==1 && f.poisoned);
    }
    for(auto w:windows)if(w)DestroyWindow(w);
    std::cout<<"group-event-source failures="<<failures<<'\n';return failures?1:0;
}
