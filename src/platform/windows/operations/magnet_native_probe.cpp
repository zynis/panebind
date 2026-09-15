#include "platform/windows/operations/window_rect_adjustment.h"
#include "core/geometry/magnet_constraint_solver.h"
#include <windows.h>
#include <dwmapi.h>
#include <iostream>
using panebind::core::geometry::Rect;
namespace o=panebind::platform::windows::operations;
namespace {
bool clamp_request{};
LRESULT CALLBACK procedure(HWND window,UINT message,WPARAM w,LPARAM l){
    if(message==WM_WINDOWPOSCHANGING&&clamp_request){auto* p=reinterpret_cast<WINDOWPOS*>(l);if(!(p->flags&SWP_NOSIZE))++p->cx;}
    return DefWindowProcW(window,message,w,l);
}
struct Snapshot {Rect visible,positioning;};
std::optional<Snapshot> capture(HWND h){
    DWORD pid{};if(GetWindowThreadProcessId(h,&pid)!=GetCurrentThreadId()||pid!=GetCurrentProcessId())return {};
    RECT p{},v{};if(!GetWindowRect(h,&p)||DwmGetWindowAttribute(h,DWMWA_EXTENDED_FRAME_BOUNDS,&v,sizeof(v))!=S_OK)return {};
    return Snapshot{Rect{v.left,v.top,v.right,v.bottom},Rect{p.left,p.top,p.right,p.bottom}};
}
}
int main(){
    int failures{},calls{};WNDCLASSW cls{};cls.lpfnWndProc=procedure;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"PaneBindC4BOwnedProbe";
    if(!RegisterClassW(&cls))return 2;
    HWND h=CreateWindowExW(WS_EX_NOACTIVATE,cls.lpszClassName,L"PaneBind owned geometry probe",WS_OVERLAPPEDWINDOW,
        200,180,620,460,nullptr,nullptr,cls.hInstance,nullptr);
    if(!h)return 2;ShowWindow(h,SW_SHOWNOACTIVATE);UpdateWindow(h);
    const HWND foreground=GetForegroundWindow();
    for(int action=0;action<14;++action){
        auto before=capture(h);if(!before){++failures;break;}
        auto v=before->visible;const auto previous=GetWindow(h,GW_HWNDPREV);
        const bool move=action<3||action>=12;
        Rect target=v;
        const int signed_gap=action==12?7:action==13?-4:0;
        if(action>=12){
            namespace m=panebind::core::magnet;
            const auto edge=v.left()-signed_gap;
            std::array targets{m::MagnetTarget{panebind::core::model::WindowId{"controlled_geometry_target"},Rect{edge-300,v.top()-50,edge,v.bottom()+50},false}};
            m::MagnetOptions options;options.screen_edges=options.inner_edges=options.corners=options.parallel_edges=false;
            const auto proposal=m::MagnetConstraintSolver::solve({v,v,m::Interaction::Move,{},targets,options,{}});
            if(!proposal.proposal||proposal.proposal->move_delta.x!=-signed_gap){++failures;break;}
            target=proposal.proposal->corrected;
        }
        else if(move)target={v.left()+(action!=1?3:0),v.top()+(action!=0?4:0),v.right()+(action!=1?3:0),v.bottom()+(action!=0?4:0)};
        else {const auto edge=action-3;const bool left=edge==0||edge==4||edge==6,right=edge==1||edge==5||edge==7||edge==8;
            const bool top=edge==2||edge==4||edge==5,bottom=edge==3||edge==6||edge==7;
            target={v.left()-(left?3:0),v.top()-(top?4:0),v.right()+(right?3:0),v.bottom()+(bottom?4:0)};
        }
        const auto prepared=o::prepare_visible_rect_adjustment(before->positioning,v,target);
        if(!prepared.positioning){++failures;break;}const auto p=*prepared.positioning;
        const UINT flags=SWP_NOZORDER|SWP_NOACTIVATE|(move?SWP_NOSIZE:0);
        clamp_request=action==11;
        ++calls;const bool applied=SetWindowPos(h,nullptr,static_cast<int>(p.left()),static_cast<int>(p.top()),
            static_cast<int>(p.width()),static_cast<int>(p.height()),flags)!=FALSE;
        const auto after=capture(h);const bool exact=after&&after->visible==target&&after->positioning==p;
        if(!applied||(action==11?exact:!exact)||GetForegroundWindow()!=foreground||GetWindow(h,GW_HWNDPREV)!=previous)++failures;
        std::cout<<"{\"probe\":\"owned_geometry\",\"action\":"<<action<<",\"flags\":"<<flags<<",\"native_calls\":1,\"exact\":"<<(exact?"true":"false")<<",\"clamp_expected\":"<<(clamp_request?"true":"false")<<",\"synthetic_target\":"<<(action>=12?"true":"false")<<",\"input_signed_gap\":"<<signed_gap<<"}\n";
    }
    clamp_request=false;DestroyWindow(h);UnregisterClassW(cls.lpszClassName,cls.hInstance);
    std::cout<<"{\"probe\":\"owned_geometry_summary\",\"native_calls\":"<<calls<<",\"failures\":"<<failures<<",\"third_party_control\":false}\n";
    return failures?1:0;
}
