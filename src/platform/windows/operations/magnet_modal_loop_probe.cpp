// Explicit diagnostic executable only. No input generation, foreign handles,
// hooks, timers, retries or product code dependencies.
#include <windows.h>
#include <dwmapi.h>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>

namespace {
constexpr UINT correct_message=WM_APP+41;
HANDLE file=INVALID_HANDLE_VALUE;
HWND owned{};
std::uint64_t sequence{},gesture{};
bool active{},queued{},corrected{},next_seen{},healthy{true};
bool have_move{},have_resize{};
UINT drag_message{};
RECT proposed{},requested{},requested_visible{};
bool requested_visible_known{};
int record_count{};
std::int64_t qpc(){LARGE_INTEGER v{};QueryPerformanceCounter(&v);return v.QuadPart;}
std::string rect(const RECT& r){return "["+std::to_string(r.left)+","+std::to_string(r.top)+","+std::to_string(r.right)+","+std::to_string(r.bottom)+"]";}
bool same(const RECT& a,const RECT& b){return EqualRect(&a,&b)!=FALSE;}
bool identity(HWND h){DWORD pid{};return h==owned&&GetWindowThreadProcessId(h,&pid)==GetCurrentThreadId()&&pid==GetCurrentProcessId();}
struct Geometry {RECT positioning{},visible{};bool p{},v{};DWORD error{};HRESULT hr{E_PENDING};};
Geometry capture(HWND h){Geometry g;if(!identity(h))return g;SetLastError(0);g.p=GetWindowRect(h,&g.positioning)!=FALSE;g.error=g.p?0:GetLastError();g.hr=DwmGetWindowAttribute(h,DWMWA_EXTENDED_FRAME_BOUNDS,&g.visible,sizeof(RECT));g.v=SUCCEEDED(g.hr);return g;}
std::string geometry(const Geometry& g){return ",\"positioning\":"+(g.p?rect(g.positioning):"null")+",\"visible\":"+(g.v?rect(g.visible):"null")+",\"positioning_error\":"+std::to_string(g.error)+",\"visible_hresult\":"+std::to_string(g.hr);}
void record(const char* type,std::string extra={}){
    if(!healthy)return;
    if(++record_count>1024){healthy=false;return;}
    const auto line="{\"schema\":\"r1c4b-owned-modal/v1\",\"sequence\":"+std::to_string(++sequence)+",\"type\":\""+type+"\",\"gesture\":"+std::to_string(gesture)+",\"qpc\":"+std::to_string(qpc())+extra+"}\n";
    DWORD written{};healthy=WriteFile(file,line.data(),static_cast<DWORD>(line.size()),&written,nullptr)&&written==line.size();
}
LRESULT CALLBACK procedure(HWND h,UINT message,WPARAM w,LPARAM l){
    if(message==WM_ENTERSIZEMOVE){
        ++gesture;active=true;queued=corrected=next_seen=false;drag_message=0;
        record("ENTER",geometry(capture(h)));return 0;
    }
    if((message==WM_MOVING||message==WM_SIZING)&&active){
        drag_message=message;proposed=*reinterpret_cast<RECT*>(l);
        const auto g=capture(h);
        record(corrected&&!next_seen?"T2":"DRAG",",\"event\":\""+std::string(message==WM_MOVING?"WM_MOVING":"WM_SIZING")+"\",\"edge\":"+std::to_string(w)+",\"proposed\":"+rect(proposed)+geometry(g));
        if(corrected)next_seen=true;
        // Queue after native callback; never modify lParam drag RECT.
        if(!queued&&!corrected&&gesture<=4&&(message==WM_MOVING||w==WMSZ_BOTTOM)){
            queued=true;if(!PostMessageW(h,correct_message,static_cast<WPARAM>(gesture),0))healthy=false;
        }
        return DefWindowProcW(h,message,w,l);
    }
    if(message==correct_message){
        if(!healthy||!identity(h)||!active||corrected||w!=gesture||!drag_message)return 0;
        auto before=capture(h);if(!before.p||!before.v){record("BLOCKED",",\"reason\":\"capture_failed\"");return 0;}
        requested=before.positioning;requested_visible=before.visible;requested_visible_known=true;
        // Move Y pulse (orthogonal to the planned horizontal drag), or one
        // bottom-edge resize pulse. Intended resize interaction: HTBOTTOM.
        if(drag_message==WM_MOVING){requested.top+=7;requested.bottom+=7;requested_visible.top+=7;requested_visible.bottom+=7;}
        else {requested.bottom+=7;requested_visible.bottom+=7;}
        record("T0",",\"drag_active\":true,\"event\":\""+std::string(drag_message==WM_MOVING?"WM_MOVING":"WM_SIZING")+"\",\"proposed\":"+rect(proposed)+",\"target_positioning\":"+rect(requested)+",\"target_visible\":"+rect(requested_visible)+geometry(before));
        const UINT flags=SWP_NOZORDER|SWP_NOACTIVATE|(drag_message==WM_MOVING?SWP_NOSIZE:0);
        const auto start=qpc();SetLastError(0);
        const BOOL ok=SetWindowPos(h,nullptr,requested.left,requested.top,requested.right-requested.left,requested.bottom-requested.top,flags);
        const DWORD error=ok?0:GetLastError();const auto returned=qpc();const auto actual=capture(h);
        corrected=true;
        record("T1",",\"native_success\":"+std::string(ok?"true":"false")+",\"error\":"+std::to_string(error)+",\"flags\":"+std::to_string(flags)+",\"native_calls\":1,\"native_start_qpc\":"+std::to_string(start)+",\"native_return_qpc\":"+std::to_string(returned)+",\"positioning_exact\":"+(actual.p&&same(actual.positioning,requested)?"true":"false")+",\"visible_exact\":"+(actual.v&&same(actual.visible,requested_visible)?"true":"false")+geometry(actual));
        return 0;
    }
    if(message==WM_WINDOWPOSCHANGED&&active&&corrected)record("POSITION_CHANGED",geometry(capture(h)));
    if(message==WM_EXITSIZEMOVE){
        const auto g=capture(h);record("T3",",\"corrected\":"+std::string(corrected?"true":"false")+",\"next_drag_seen\":"+(next_seen?"true":"false")+",\"target_positioning\":"+(corrected?rect(requested):"null")+",\"target_visible\":"+(corrected&&requested_visible_known?rect(requested_visible):"null")+geometry(g));
        if(corrected&&next_seen){if(drag_message==WM_MOVING)have_move=true;else if(drag_message==WM_SIZING)have_resize=true;}
        active=false;return 0;
    }
    if(message==WM_DESTROY){PostQuitMessage(0);return 0;}
    return DefWindowProcW(h,message,w,l);
}
}
int wmain(int argc,wchar_t** argv){
    if(argc!=3||std::wstring_view(argv[1])!=L"--evidence-log"){
        std::cout<<"Explicit owned modal-loop probe: --evidence-log NEW_FILE\nNo synthetic modal evidence; drag title horizontally, then bottom border vertically; close this empty probe.\n";return 2;
    }
    file=CreateFileW(argv[2],GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return 2;
    WNDCLASSW cls{};cls.lpfnWndProc=procedure;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"PaneBindC4BModalProbe";cls.hCursor=LoadCursor(nullptr,IDC_ARROW);cls.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
    if(!RegisterClassW(&cls)){CloseHandle(file);return 2;}
    owned=CreateWindowExW(0,cls.lpszClassName,L"PaneBind owned modal probe - Move then bottom Resize",WS_OVERLAPPEDWINDOW,300,240,650,480,nullptr,nullptr,cls.hInstance,nullptr);
    if(!owned){CloseHandle(file);return 2;}
    LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);
    record("startup",",\"evidence_kind\":\"controlled_owned_modal\",\"real_explorer\":false,\"sendinput_in_probe\":false,\"pid\":"+std::to_string(GetCurrentProcessId())+",\"tid\":"+std::to_string(GetCurrentThreadId())+",\"hwnd\":"+std::to_string(reinterpret_cast<std::uintptr_t>(owned))+",\"qpc_frequency\":"+std::to_string(frequency.QuadPart));
    ShowWindow(owned,SW_SHOWNOACTIVATE);UpdateWindow(owned);
    MSG message{};BOOL result{};while((result=GetMessageW(&message,nullptr,0,0))>0){TranslateMessage(&message);DispatchMessageW(&message);}
    const bool complete=healthy&&result==0&&have_move&&have_resize;
    record("shutdown",",\"result\":\""+std::string(complete?"CAPTURED_NOT_ACCEPTED":"INCOMPLETE")+"\",\"move_lifecycle\":"+(have_move?"true":"false")+",\"resize_lifecycle\":"+(have_resize?"true":"false"));
    CloseHandle(file);return complete&&healthy?0:2;
}
