#pragma once

// TEST ONLY: a single blank neighbour created by this exact Run's Medium
// fixture STA. It is not product authority, a private writer or a source app.
class MediumDesktopMagnetTarget {
    std::wstring name_,run_;HDESK desktop_{};HWND hwnd_{};DWORD owner_{},pid_{},session_{};
    std::uint64_t birth_{};HMONITOR monitor_{};UINT dpi_{};bool registered_{},closed_{true};
    HINSTANCE instance_{GetModuleHandleW(nullptr)};
    static LRESULT CALLBACK procedure(HWND h,UINT m,WPARAM w,LPARAM l){
        if(m==WM_NCCREATE){const auto* c=reinterpret_cast<const CREATESTRUCTW*>(l);
            SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(c->lpCreateParams));}
        // Keep the ordinary Win32 frame and tracking-limit semantics. Never
        // activate on show or consume/replay TEST mouse/keyboard messages.
        return DefWindowProcW(h,m,w,l);
    }
public:
    Rect initial{};Rect work{};std::optional<Rect> positioning;
    MediumDesktopMagnetTarget()=default;
    MediumDesktopMagnetTarget(const MediumDesktopMagnetTarget&)=delete;
    ~MediumDesktopMagnetTarget(){finish();}
    HWND window()const noexcept{return hwnd_;}
    bool exact()const{
        if(!hwnd_||GetCurrentThreadId()!=owner_||GetCurrentProcessId()!=pid_||
           process_birth(GetCurrentProcess())!=birth_||!medium(GetCurrentProcess())||!input_actor_same_user(GetCurrentProcess())||
           !equal(medium_desktop_object_name(GetThreadDesktop(owner_)),L"PaneBindV1_"+run_)||
           !equal(medium_desktop_object_name(desktop_),L"PaneBindV1_"+run_))return false;
        DWORD session{},pid{};if(!ProcessIdToSessionId(pid_,&session)||session!=session_||
            !IsWindow(hwnd_)||GetWindowThreadProcessId(hwnd_,&pid)!=owner_||pid!=pid_||GetAncestor(hwnd_,GA_ROOT)!=hwnd_||
            GetWindow(hwnd_,GW_OWNER)||GetWindowLongPtrW(hwnd_,GWLP_USERDATA)!=reinterpret_cast<LONG_PTR>(this)||
            gui_class(hwnd_)!=name_||!IsWindowVisible(hwnd_)||IsIconic(hwnd_)||GetDpiForWindow(hwnd_)!=dpi_||
            MonitorFromWindow(hwnd_,MONITOR_DEFAULTTONULL)!=monitor_)return false;
        const auto a=MediumDesktopWindows::read(desktop_),b=MediumDesktopWindows::read(desktop_);
        if(!a.complete||!b.complete||std::find(a.windows.begin(),a.windows.end(),hwnd_)==a.windows.end()||
           std::find(b.windows.begin(),b.windows.end(),hwnd_)==b.windows.end())return false;
        DWORD cloak{};RECT p{};const auto frame=visible(hwnd_);MONITORINFO monitor{sizeof(monitor)};
        return GetWindowRect(hwnd_,&p)&&frame&&frame->width()==initial.width()&&frame->height()==initial.height()&&
            DwmGetWindowAttribute(hwnd_,DWMWA_CLOAKED,&cloak,sizeof(cloak))==S_OK&&!cloak&&
            GetMonitorInfoW(monitor_,&monitor)&&Rect{monitor.rcWork.left,monitor.rcWork.top,monitor.rcWork.right,monitor.rcWork.bottom}==work;
    }
    void create(const std::wstring& run,HDESK desktop,DWORD session,HWND source,Rect desired,UINT dpi,HMONITOR monitor,
        Rect actual_work,MediumDesktopProductGeometryObservation& out){
        run_=run;desktop_=desktop;session_=session;owner_=GetCurrentThreadId();pid_=GetCurrentProcessId();
        birth_=process_birth(GetCurrentProcess());monitor_=monitor;dpi_=dpi;initial=desired;work=actual_work;
        name_=L"PaneBind.V1.MediumMagnetTarget."+run;
        const auto before=MediumDesktopWindows::read(desktop_);WNDCLASSEXW old{sizeof(old)};
        SetLastError(ERROR_SUCCESS);const bool exists=GetClassInfoExW(instance_,name_.c_str(),&old)!=FALSE;
        const auto query_error=GetLastError();
        if(!before.complete||exists||query_error!=ERROR_CLASS_DOES_NOT_EXIST||!birth_||!dpi_||!monitor_||
           GetForegroundWindow()!=source||!medium(GetCurrentProcess())||
           !equal(medium_desktop_object_name(GetThreadDesktop(owner_)),L"PaneBindV1_"+run_))
            throw std::runtime_error("geometry_blank_target_exact_Run_or_new_class_not_verified");
        WNDCLASSEXW wc{sizeof(wc)};wc.hInstance=instance_;wc.lpfnWndProc=procedure;wc.lpszClassName=name_.c_str();
        wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=GetSysColorBrush(COLOR_WINDOW);
        SetLastError(ERROR_SUCCESS);registered_=RegisterClassExW(&wc)!=0;const auto register_error=registered_?0:GetLastError();
        if(registered_){hwnd_=CreateWindowExW(WS_EX_NOACTIVATE,name_.c_str(),L"PaneBind exact Run blank target",WS_OVERLAPPEDWINDOW,
            desired.l,desired.t,desired.width(),desired.height(),nullptr,nullptr,instance_,this);closed_=!hwnd_;}
        const auto create_error=hwnd_?0:GetLastError();
        out.emit("medium_product_geometry_blank_target_create","\"class\":"+quote(utf8(name_))+
            ",\"RegisterClass_attempted\":true,\"RegisterClass_return\":"+(registered_?"true":"false")+
            ",\"register_error\":"+std::to_string(register_error)+",\"CreateWindow_attempted\":"+(registered_?"true":"false")+
            ",\"CreateWindow_return\":"+(registered_?(hwnd_?"true":"false"):"null")+
            ",\"create_error\":"+(registered_?std::to_string(create_error):"null")+
            ",\"actual_Medium\":"+(medium(GetCurrentProcess())?"true":"false")+",\"session\":"+std::to_string(session_)+
            ",\"HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(hwnd_))+",\"foreground_reclaimed\":false");
        if(!registered_||!hwnd_)throw std::runtime_error("geometry_blank_target_creation_failed");
        if(std::find(before.windows.begin(),before.windows.end(),hwnd_)!=before.windows.end())
            throw std::runtime_error("geometry_blank_target_baseline_identity_collision");
        ShowWindow(hwnd_,SW_SHOWNOACTIVATE);RECT p{};const auto first=visible(hwnd_);
        if(!first||!GetWindowRect(hwnd_,&p))throw std::runtime_error("geometry_blank_target_frame_insets_unknown");
        const LONG left=desired.l-(first->l-p.left),top=desired.t-(first->t-p.top);
        const LONG width=desired.width()+(p.right-p.left-first->width()),height=desired.height()+(p.bottom-p.top-first->height());
        SetLastError(ERROR_SUCCESS);const bool placed=SetWindowPos(hwnd_,nullptr,left,top,width,height,SWP_NOACTIVATE|SWP_NOZORDER)!=FALSE;
        const auto placement_error=placed?0:GetLastError();
        IVirtualDesktopManager* manager{};const HRESULT made=CoCreateInstance(CLSID_VirtualDesktopManager,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&manager));
        BOOL current{};const HRESULT queried=manager?manager->IsWindowOnCurrentVirtualDesktop(hwnd_,&current):E_UNEXPECTED;
        if(manager)manager->Release();const bool actual=exact();
        if(GetWindowRect(hwnd_,&p))positioning=Rect{p.left,p.top,p.right,p.bottom};
        out.emit("medium_product_geometry_blank_target","\"HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(hwnd_))+
            ",\"PID\":"+std::to_string(pid_)+",\"TID\":"+std::to_string(owner_)+",\"birth\":"+std::to_string(birth_)+
            ",\"class\":"+quote(utf8(name_))+",\"RegisterClass_return\":"+(registered_?"true":"false")+
            ",\"register_error\":"+std::to_string(register_error)+",\"CreateWindow_return\":true,\"create_error\":"+std::to_string(create_error)+
            ",\"fixture_SetWindowPos_return\":"+(placed?"true":"false")+",\"placement_error\":"+std::to_string(placement_error)+
            ",\"requested_visible\":"+rect(desired)+",\"actual_visible\":"+(visible(hwnd_)?rect(*visible(hwnd_)):"null")+
            ",\"actual_positioning\":"+(positioning?rect(*positioning):"null")+",\"actual_DPI\":"+std::to_string(GetDpiForWindow(hwnd_))+
            ",\"actual_exact_identity_membership\":"+(actual?"true":"false")+",\"VDM_create_hresult\":"+std::to_string(static_cast<long>(made))+
            ",\"VDM_query_hresult\":"+std::to_string(static_cast<long>(queried))+",\"VDM_current\":"+(current?"true":"false")+
            ",\"foreground_unchanged\":"+(GetForegroundWindow()==source?"true":"false")+",\"setup_is_not_product_write\":true");
        if(!placed||!actual||!positioning||visible(hwnd_)!=desired||GetForegroundWindow()!=source||made!=S_OK||queried!=S_OK||!current)
            throw std::runtime_error("geometry_blank_target_actual_frame_VDM_or_foreground_not_verified");
    }
    bool finish()noexcept{
        if(hwnd_){
            // Destruction, unlike operational admission, does not require the
            // window still visible or its geometry still matching an oracle.
            DWORD pid{};if(GetCurrentThreadId()!=owner_||GetWindowThreadProcessId(hwnd_,&pid)!=owner_||pid!=pid_||
                GetWindowLongPtrW(hwnd_,GWLP_USERDATA)!=reinterpret_cast<LONG_PTR>(this))return false;
            const HWND h=hwnd_;closed_=DestroyWindow(h)!=FALSE&&!IsWindow(h);if(closed_)hwnd_=nullptr;
        }
        if(!hwnd_&&registered_){if(!UnregisterClassW(name_.c_str(),instance_))return false;registered_=false;}
        return closed_&&!registered_;
    }
};
