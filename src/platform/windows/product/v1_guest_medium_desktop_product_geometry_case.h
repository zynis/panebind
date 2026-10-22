#pragma once

// TEST ONLY: one manifest-carried, exact new Terminal under the already
// running ordinary product on this Run desktop. No Default input helper,
// product permission override, test placement, fake START/END or client nonce.
class MediumDesktopProductGeometryCase {
    const std::wstring& run_;HDESK desktop_;const fs::path& product_;
    HANDLE product_process_;DWORD product_pid_;std::uint64_t product_birth_,launch_tick_;
    const std::string source_,product_hash_;
    const std::function<bool()>& still_exact_;const std::function<bool()>& exact_product_;
    const std::shared_ptr<MediumDesktopUiInputLease>& lease_;ULONGLONG deadline_;
    MediumDesktopProductGeometryObservation& out_;
    MediumTerminalNativeEvents events_;HWINEVENTHOOK hook_{};HANDLE target_process_{},target_thread_{};
    MediumTerminalNativeResources resources_{hook_,target_process_,target_thread_,events_};
    HANDLE log_change_{INVALID_HANDLE_VALUE};bool change_closed_{true},target_closed_{true},quit_seen_{};
    DWORD target_pid_{},target_tid_{},session_{};std::uint64_t target_birth_{},generation_{},last_generation_{},source_key_{};
    HWND target_{};fs::path target_image_;product::guest::ProductLogEvidence evidence_;
    bool down_inserted_{},up_attempted_{},up_inserted_{};std::uint64_t down_qpc_{},up_qpc_{};
    bool native_threshold_step_{},continuation_ready_{};
    std::optional<POINT> down_anchor_;HWND down_hit_{};int down_region_{};
    unsigned notifications_{};std::string terminal_reason_;
    std::string last_status_;
    static std::string manifest_string(std::string_view text,std::string_view field){
        const std::string key="\""+std::string(field)+"\"";auto at=text.find(key);
        if(at==text.npos||text.find(key,at+key.size())!=text.npos)return {};
        at+=key.size();const auto spaces=[&]{while(at<text.size()&&(text[at]==' '||text[at]=='\t'||text[at]=='\r'||text[at]=='\n'))++at;};
        spaces();if(at==text.size()||text[at++]!=':')return {};spaces();if(at==text.size()||text[at++]!='"')return {};
        const auto begin=at;while(at<text.size()&&text[at]!='"'){if(text[at]=='\\'||static_cast<unsigned char>(text[at])<32)return {};++at;}
        return at<text.size()?std::string(text.substr(begin,at-begin)):std::string{};
    }

    bool base_context()const{
        if(!desktop_||!still_exact_||!still_exact_()||!exact_product_||!exact_product_()||
           !product_process_||GetProcessId(product_process_)!=product_pid_||process_birth(product_process_)!=product_birth_||
           WaitForSingleObject(product_process_,0)!=WAIT_TIMEOUT||!medium(GetCurrentProcess())||!medium(product_process_)||
           !input_actor_same_user(product_process_)||
           !equal(medium_desktop_object_name(GetProcessWindowStation()),L"WinSta0")||
           !equal(medium_desktop_object_name(desktop_),L"PaneBindV1_"+run_)||
           !equal(medium_desktop_object_name(GetThreadDesktop(GetCurrentThreadId())),L"PaneBindV1_"+run_))return false;
        DWORD own{},other{};if(!ProcessIdToSessionId(GetCurrentProcessId(),&own)||!own||
            !ProcessIdToSessionId(product_pid_,&other)||own!=other||own!=session_)return false;
        const HDESK input=OpenInputDesktop(0,FALSE,DESKTOP_READOBJECTS);if(!input)return false;
        BOOL io{};DWORD bytes{};const bool active=equal(medium_desktop_object_name(input),L"PaneBindV1_"+run_)&&
            GetUserObjectInformationW(input,UOI_IO,&io,sizeof(io),&bytes)&&io;
        return CloseDesktop(input)!=FALSE&&active;
    }
    bool admission()const{return GetTickCount64()<deadline_&&lease_&&lease_->valid()&&!lease_->retired()&&base_context();}
    bool target_exact()const{
        if(!target_process_||!target_pid_||!target_birth_||GetProcessId(target_process_)!=target_pid_||
           process_birth(target_process_)!=target_birth_||WaitForSingleObject(target_process_,0)!=WAIT_TIMEOUT||
           !medium(target_process_)||!input_actor_same_user(target_process_)||
           !equal(medium_desktop_image(target_process_),target_image_.wstring()))return false;
        DWORD session{};if(!ProcessIdToSessionId(target_pid_,&session)||session!=session_)return false;
        const auto a=MediumDesktopWindows::read(desktop_),b=MediumDesktopWindows::read(desktop_);
        if(!a.complete||!b.complete||std::find(a.windows.begin(),a.windows.end(),target_)==a.windows.end()||
           std::find(b.windows.begin(),b.windows.end(),target_)==b.windows.end())return false;
        const auto first=MediumDesktopWindowFact::read(target_,target_pid_,target_process_,target_birth_,L"PaneBindV1_"+run_);
        const auto second=MediumDesktopWindowFact::read(target_,target_pid_,target_process_,target_birth_,L"PaneBindV1_"+run_);
        // Identity/membership is separate from changing actual geometry. Our
        // own in-flight writer may complete between these read-only samples.
        return first.known&&second.known&&first.exact_child&&second.exact_child&&!first.desktop_conflict&&!second.desktop_conflict&&
            first.hwnd==second.hwnd&&first.pid==second.pid&&first.tid==second.tid&&first.tid==target_tid_&&
            first.type==L"CASCADIA_HOSTING_WINDOW_CLASS"&&second.type==first.type&&first.root&&second.root&&first.visible&&second.visible;
    }
    const std::string& log(){
        if(!base_context())throw std::runtime_error("geometry_actual_product_or_Run_context_lost");
        std::string why;for(unsigned attempt=0;attempt<2;++attempt){
            auto previous=product_.parent_path()/L"panebind.log.jsonl.1",current=product_.parent_path()/L"panebind.log.jsonl";
            product::guest::ProductLogPhysicalFile old(previous),now(current);auto a=old.snapshot(),b=now.snapshot();
            if(a.state==product::guest::ProductLogPhysicalState::Failure||b.state==product::guest::ProductLogPhysicalState::Failure)
                throw std::runtime_error("geometry_product_log_physical_snapshot_failed");
            if(evidence_.observe(a.bytes,b.bytes,why))return evidence_.text();
        }
        evidence_.reject();throw std::runtime_error("geometry_current_product_evidence_"+why);
    }
    std::string row(std::size_t mark,std::string_view type,std::uint64_t generation=0,std::string_view fact={}){
        (void)log();const auto suffix=evidence_.since(mark);if(!suffix)throw std::runtime_error("geometry_log_mark_not_current_launch");
        std::istringstream lines{std::string(*suffix)};std::string line,found;
        while(std::getline(lines,line))if(line.ends_with('}')&&line.find("\"type\":"+quote(type))!=line.npos&&
            (!generation||number(line,"\"generation\":")==generation)&&
            (fact.empty()||line.find("\"fact\":"+quote(fact))!=line.npos))found=line;
        return found;
    }
    bool wait_for(const std::function<bool()>& predicate,DWORD cap,bool require_active=true,HANDLE extra=nullptr){
        const auto begin=GetTickCount64(),end=require_active?(std::min<ULONGLONG>)(deadline_,begin+cap):begin+cap;
        for(;;){
            if(quit_seen_||(require_active&&!admission()))return false;
            if(events_.changed&&!ResetEvent(events_.changed))return false;
            if(predicate())return true;
            const auto now=GetTickCount64();if(now>=end)return false;
            if(!events_.changed)return false;
            HANDLE handles[5]{events_.changed,product_process_,target_process_,nullptr,nullptr};
            DWORD count=target_process_?3:2;const DWORD log_index=count;
            if(log_change_!=INVALID_HANDLE_VALUE)handles[count++]=log_change_;
            const DWORD extra_index=count;if(extra)handles[count++]=extra;
            const auto result=MsgWaitForMultipleObjectsEx(count,handles,static_cast<DWORD>(end-now),QS_ALLINPUT,MWMO_INPUTAVAILABLE);
            if(result==WAIT_FAILED)return false;
            if(result==WAIT_OBJECT_0+1)return false;
            if(target_process_&&result==WAIT_OBJECT_0+2)return predicate();
            if(extra&&result==WAIT_OBJECT_0+extra_index)return predicate();
            if(log_change_!=INVALID_HANDLE_VALUE&&result==WAIT_OBJECT_0+log_index){
                if(++notifications_>64||!FindNextChangeNotification(log_change_))return false;
            }
            if(result==WAIT_OBJECT_0+count){MSG message{};unsigned messages=256;
                while(messages--&&PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){
                    if(message.message==WM_QUIT){quit_seen_=true;return false;}TranslateMessage(&message);DispatchMessageW(&message);}}
            if(result==WAIT_TIMEOUT)return (!require_active||admission())&&predicate();
        }
    }
    bool status(){
        const auto mark=log().size();STARTUPINFOW startup{sizeof(startup)};
        std::wstring desktop=L"WinSta0\\PaneBindV1_"+run_;startup.lpDesktop=desktop.data();PROCESS_INFORMATION info{};
        auto command=L"\""+product_.wstring()+L"\" --status";const bool attempted=admission();SetLastError(ERROR_SUCCESS);
        const bool created=attempted&&CreateProcessW(product_.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,
            product_.parent_path().c_str(),&startup,&info)!=FALSE;const DWORD error=created?0:GetLastError();
        Handle process(info.hProcess),thread(info.hThread);DWORD code=STILL_ACTIVE;bool exited{};
        if(created)exited=wait_for([&]{return WaitForSingleObject(process.value,0)==WAIT_OBJECT_0;},1000,true,process.value);
        if(exited)GetExitCodeProcess(process.value,&code);
        std::string actual;const bool observed=created&&exited&&code==0&&wait_for([&]{actual=row(mark,"status");return !actual.empty();},500);
        last_status_=actual;
        out_.emit("medium_product_geometry_status","\"CreateProcess_attempted\":"+std::string(attempted?"true":"false")+
            ",\"command_created\":"+(created?"true":"false")+
            ",\"create_error\":"+(attempted?std::to_string(error):"null")+",\"command_exited\":"+(exited?"true":"false")+
            ",\"exit_code\":"+(exited?std::to_string(code):"null")+",\"actual_receipt\":"+(actual.empty()?"null":quote(actual)));
        const product::guest::UpEvidenceIdentity identity{product_pid_,source_,generation_,up_qpc_};
        return observed&&actual.find("\"enabled\":true")!=actual.npos&&actual.find("\"hook_owner_running\":true")!=actual.npos&&
            actual.find("\"gesture_active\":false")!=actual.npos&&actual.find("\"native_move_handoff_pending\":false")!=actual.npos&&
            actual.find("\"native_operation_pending\":false")!=actual.npos&&actual.find("\"native_completion_unknown\":false")!=actual.npos&&
            (!generation_||product::guest::matching_idle_status(actual,identity));
    }
    bool capture_route(HWND* observed_overlay=nullptr) {
        const auto actual=row(0,"gesture_input_resource",generation_,"capture_ready");
        const HWND overlay=reinterpret_cast<HWND>(static_cast<std::uintptr_t>(number(actual,"\"overlay\":")));
        const auto owner=static_cast<DWORD>(number(actual,"\"capture_owner_tid\":"));DWORD pid{};
        GUITHREADINFO source{sizeof(source)},resource{sizeof(resource)};
        const auto first=MediumDesktopWindows::read(desktop_),second=MediumDesktopWindows::read(desktop_);
        const bool allowed=!actual.empty()&&generation_&&overlay&&owner&&first.complete&&second.complete&&
            std::find(first.windows.begin(),first.windows.end(),overlay)!=first.windows.end()&&
            std::find(second.windows.begin(),second.windows.end(),overlay)!=second.windows.end()&&
            number(actual,"\"source\":")==reinterpret_cast<std::uintptr_t>(target_)&&
            number(actual,"\"source_pid\":")==target_pid_&&number(actual,"\"source_tid\":")==target_tid_&&
            number(actual,"\"capture_actual_after\":")==reinterpret_cast<std::uintptr_t>(overlay)&&
            number(actual,"\"resource_observed_qpc\":")>=down_qpc_&&down_qpc_&&
            actual.find("\"capture_attempted\":true")!=actual.npos&&actual.find("\"capture_failure\":0,")!=actual.npos&&
            actual.find("\"attach_succeeded\":true")!=actual.npos&&
            GetWindowThreadProcessId(overlay,&pid)==owner&&pid==product_pid_&&IsWindow(overlay)&&GetAncestor(overlay,GA_ROOT)==overlay&&
            GetGUIThreadInfo(target_tid_,&source)&&GetGUIThreadInfo(owner,&resource)&&
            source.hwndCapture==overlay&&resource.hwndCapture==overlay&&!source.hwndMoveSize&&!source.hwndMenuOwner&&
            !resource.hwndMoveSize&&!resource.hwndMenuOwner&&
            !(source.flags&(GUI_INMOVESIZE|GUI_INMENUMODE|GUI_SYSTEMMENUMODE|GUI_POPUPMENUMODE))&&
            !(resource.flags&(GUI_INMOVESIZE|GUI_INMENUMODE|GUI_SYSTEMMENUMODE|GUI_POPUPMENUMODE))&&GetForegroundWindow()==target_;
        if(allowed&&observed_overlay)*observed_overlay=overlay;return allowed;
    }
    bool retired(){
        if(!generation_)return false;std::istringstream lines(log());std::string actual;
        while(std::getline(lines,actual))if(number(actual,"\"generation\":")==generation_){
            bool ended=actual.find("\"type\":\"gesture_end\"")!=actual.npos||
                actual.find("\"type\":\"native_move_passthrough_release\"")!=actual.npos||
                actual.find("\"type\":\"gesture_begin_rejected\"")!=actual.npos||
                (actual.find("\"type\":\"native_move_handoff\"")!=actual.npos&&
                    actual.find("\"accepted\":false")!=actual.npos);
            if(actual.find("\"type\":\"gesture_input_resource\"")!=actual.npos){
                for(const auto* fact:{"isolation_gone","capture_lost","capture_failure","context_lost","resource_failure",
                    "deadline","hotkey_stop","thread_association_failure","legacy_left_up","capture_released","thread_detached"})
                    ended=ended||actual.find("\"fact\":"+quote(fact))!=actual.npos;
                ended=ended||(actual.find("\"fact\":\"raw_mouse\"")!=actual.npos&&
                    (number(actual,"\"raw_button_flags\":")&RI_MOUSE_LEFT_BUTTON_UP));
            }
            if(ended){terminal_reason_=actual;return true;}
        }return false;
    }
    bool up_target(bool cleanup){
        if(!admission()||!target_exact()||GetForegroundWindow()!=target_)return false;
        POINT point{};if(!GetCursorPos(&point))return false;const HWND hit=GetAncestor(WindowFromPoint(point),GA_ROOT);
        HWND overlay{};if(generation_&&capture_route(&overlay)&&(hit==overlay||hit==target_))return true;
        if(!cleanup)return false;
        GUITHREADINFO gui{sizeof(gui)};
        return hit==target_&&GetGUIThreadInfo(target_tid_,&gui)&&(!gui.hwndCapture||gui.hwndCapture==target_)&&
            (!gui.hwndMoveSize||gui.hwndMoveSize==target_)&&!gui.hwndMenuOwner&&
            !(gui.flags&(GUI_INMENUMODE|GUI_SYSTEMMENUMODE|GUI_POPUPMENUMODE));
    }
    bool fresh_move_route(DWORD flags){
        if(GetForegroundWindow()!=target_)return false;
        if(!generation_){GUITHREADINFO gui{sizeof(gui)};
            if(!GetGUIThreadInfo(target_tid_,&gui)||gui.hwndCapture||gui.hwndMoveSize||gui.hwndMenuOwner||
                (gui.flags&(GUI_INMOVESIZE|GUI_INMENUMODE|GUI_SYSTEMMENUMODE|GUI_POPUPMENUMODE)))return false;
            if(!(flags&MOUSEEVENTF_LEFTDOWN))return true;
            POINT point{};DWORD pid{};DWORD_PTR region{};
            return down_anchor_&&down_hit_&&no_buttons()&&GetCursorPos(&point)&&point.x==down_anchor_->x&&point.y==down_anchor_->y&&
                WindowFromPoint(point)==down_hit_&&GetAncestor(down_hit_,GA_ROOT)==target_&&
                GetWindowThreadProcessId(down_hit_,&pid)&&pid==target_pid_&&
                SendMessageTimeoutW(down_hit_,WM_NCHITTEST,0,MAKELPARAM(static_cast<short>(point.x),static_cast<short>(point.y)),
                    SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&region)&&static_cast<LRESULT>(region)==down_region_;}
        if(retired())return false;
        if(continuation_ready_)return capture_route();
        if(!native_threshold_step_)return false;
        // Handoff can become ready between the first threshold decision and
        // this final send check. Only this exact generation's actual shield
        // may replace source-owned native capture; never accept foreign GUI.
        if(capture_route())return true;
        GUITHREADINFO gui{sizeof(gui)};
        return GetGUIThreadInfo(target_tid_,&gui)&&(!gui.hwndCapture||gui.hwndCapture==target_)&&
            (!gui.hwndMoveSize||gui.hwndMoveSize==target_)&&!gui.hwndMenuOwner&&
            !(gui.flags&(GUI_INMENUMODE|GUI_SYSTEMMENUMODE|GUI_POPUPMENUMODE));
    }
    void send_mouse(DWORD flags,POINT point,bool cursor,bool up=false,bool cleanup=false){
        // The normal release contains a final MOVE. Once a real terminal
        // fact is observed it may not send that MOVE, even if capture has
        // not yet been released. Abort cleanup remains a separate single UP.
        const auto allowed=[&]{return admission()&&target_exact()&&(up?((cleanup||!retired())&&up_target(cleanup)):
            fresh_move_route(flags));};
        if(!allowed()||!lease_->begin())throw std::runtime_error("geometry_Run_input_target_or_lease_rejected");
        struct End {MediumDesktopUiInputLease& value;~End(){value.end();}} end{*lease_};
        if(!allowed())throw std::runtime_error("geometry_input_revoked_after_inflight_claim");
        std::array<INPUT,2> packets{};UINT count=1;for(auto& packet:packets){packet.type=INPUT_MOUSE;packet.mi.dwExtraInfo=0x50425631;}
        const bool serial_up=up&&!cleanup;HWND release_hit{};
        if(serial_up){
            if(!GetCursorPos(&point))throw std::runtime_error("geometry_final_actual_cursor_unknown");
            release_hit=GetAncestor(WindowFromPoint(point),GA_ROOT);cursor=true;count=2;
            packets[0].mi.dwFlags=MOUSEEVENTF_MOVE;packets[1].mi.dwFlags=MOUSEEVENTF_LEFTUP;
        }else packets[0].mi.dwFlags=flags;
        auto& packet=packets[0];
        if(cursor){const int x=GetSystemMetrics(SM_XVIRTUALSCREEN),y=GetSystemMetrics(SM_YVIRTUALSCREEN),w=GetSystemMetrics(SM_CXVIRTUALSCREEN),h=GetSystemMetrics(SM_CYVIRTUALSCREEN);
            if(w<2||h<2||point.x<x||point.y<y||point.x>=x+w||point.y>=y+h)throw std::runtime_error("geometry_cursor_outside_virtual_screen");
            packet.mi.dwFlags|=MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_VIRTUALDESK;
            packet.mi.dx=static_cast<LONG>(((static_cast<long long>(point.x-x)*2+1)*65536)/(static_cast<long long>(w)*2));
            packet.mi.dy=static_cast<LONG>(((static_cast<long long>(point.y-y)*2+1)*65536)/(static_cast<long long>(h)*2));}
        POINT actual_release{};
        if(serial_up&&(!allowed()||!GetCursorPos(&actual_release)||actual_release.x!=point.x||actual_release.y!=point.y||
            GetAncestor(WindowFromPoint(actual_release),GA_ROOT)!=release_hit))
            throw std::runtime_error("geometry_single_UP_route_or_actual_cursor_changed_before_insert");
        LARGE_INTEGER before{},after{};if(!QueryPerformanceCounter(&before)||before.QuadPart<=0)throw std::runtime_error("geometry_input_clock_unknown");
        if(flags&MOUSEEVENTF_LEFTDOWN)lease_->incomplete_pair.store(true,std::memory_order_release);
        if(up)up_attempted_=true;
        out_.input_attempted=true;SetLastError(ERROR_SUCCESS);const UINT inserted=SendInput(count,packets.data(),sizeof(INPUT));const auto error=inserted==count?0:GetLastError();
        const bool after_known=QueryPerformanceCounter(&after)!=FALSE;out_.input_inserted=out_.input_inserted||inserted>0;
        if(flags&MOUSEEVENTF_LEFTDOWN){down_inserted_=inserted==1;down_qpc_=static_cast<std::uint64_t>(before.QuadPart);
            if(!down_inserted_)lease_->incomplete_pair.store(false,std::memory_order_release);}
        if(up){up_inserted_=inserted==count;up_qpc_=static_cast<std::uint64_t>(before.QuadPart);
            if(up_inserted_)lease_->incomplete_pair.store(false,std::memory_order_release);}
        out_.emit("medium_product_geometry_input","\"generation\":"+std::to_string(generation_)+",\"packet_count\":"+std::to_string(count)+
            ",\"mouse_flags\":["+std::to_string(packet.mi.dwFlags)+(serial_up?","+std::to_string(packets[1].mi.dwFlags):"")+
            "],\"single_UP_packet_index\":"+(up?std::to_string(count-1):"null")+
            ",\"normal_UP_variant\":"+(serial_up?quote("one_serial_final_actual_cursor_MOVE_then_single_UP"):"null")+
            ",\"requested_point\":["+std::to_string(point.x)+","+std::to_string(point.y)+"],\"SendInput_return\":"+std::to_string(inserted)+
            ",\"short_error\":"+std::to_string(error)+",\"before_qpc\":"+std::to_string(before.QuadPart)+",\"after_qpc\":"+(after_known?std::to_string(after.QuadPart):"null")+
            ",\"cleanup_only\":"+(cleanup?"true":"false")+",\"Default_helpers_used\":false,\"API_insert_not_product_delivery\":true");
        if(inserted!=count)throw std::runtime_error("geometry_input_short_insert_no_replay");
    }
    void cursor(POINT point){
        send_mouse(MOUSEEVENTF_MOVE,point,true);POINT actual{};
        if(!wait_for([&]{return target_exact()&&GetCursorPos(&actual)&&actual.x==point.x&&actual.y==point.y;},200))
            throw std::runtime_error("geometry_actual_cursor_not_exact");
    }
    void one_gesture(int region,LONG dx,LONG dy,const char* phase){
        if(!admission()||!target_exact()||!status()||!no_buttons())throw std::runtime_error("geometry_fresh_idle_not_ready");
        const auto original=visible(target_);RECT positioning{};const auto original_dpi=GetDpiForWindow(target_);
        const auto original_monitor=MonitorFromWindow(target_,MONITOR_DEFAULTTONULL);
        if(!original||!GetWindowRect(target_,&positioning)||!original_dpi||!original_monitor)throw std::runtime_error("geometry_original_frame_unknown");
        const auto hit=hit_point(target_,region,nullptr,region==HTCAPTION);if(!hit)throw std::runtime_error("geometry_actual_nonclient_target_not_found");
        generation_=0;down_inserted_=up_attempted_=up_inserted_=false;native_threshold_step_=continuation_ready_=false;terminal_reason_.clear();
        cursor(*hit);if(!status())throw std::runtime_error("geometry_product_prepare_status_not_healthy");
        const auto before_status=last_status_;const HWND actual_hit=WindowFromPoint(*hit);DWORD actual_pid{};DWORD_PTR actual_region{};
        if(hit->x<std::numeric_limits<short>::min()||hit->x>std::numeric_limits<short>::max()||
            hit->y<std::numeric_limits<short>::min()||hit->y>std::numeric_limits<short>::max()||
            !actual_hit||GetAncestor(actual_hit,GA_ROOT)!=target_||!GetWindowThreadProcessId(actual_hit,&actual_pid)||actual_pid!=target_pid_||
            !SendMessageTimeoutW(actual_hit,WM_NCHITTEST,0,MAKELPARAM(static_cast<short>(hit->x),static_cast<short>(hit->y)),
                SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&actual_region)||static_cast<LRESULT>(actual_region)!=region||GetForegroundWindow()!=target_)
            throw std::runtime_error("geometry_fresh_actual_nonclient_hit_changed_before_DOWN");
        const auto mark=log().size();const auto anchor=*hit;down_anchor_=anchor;down_hit_=actual_hit;down_region_=region;
        send_mouse(MOUSEEVENTF_LEFTDOWN,anchor,false);
        if(!wait_for([&]{const auto pending=row(mark,"native_move_pending");if(pending.empty())return false;
            const auto id=number(pending,"\"generation\":");const auto expected="\"down_point\":["+std::to_string(anchor.x)+","+std::to_string(anchor.y)+"]";
            if(!id||id<=last_generation_||number(pending,"\"source\":")!=reinterpret_cast<std::uintptr_t>(target_)||
                number(pending,"\"source_pid\":")!=target_pid_||number(pending,"\"source_tid\":")!=target_tid_||
                !number(pending,"\"source_key\":")||number(pending,"\"down_hook_qpc\":")<down_qpc_||pending.find(expected)==pending.npos||
                number(pending,"\"actual_DOWN_region\":")!=(region==HTCAPTION?0u:2u)||number(pending,"\"locked_modifiers\":")!=0)
                throw std::runtime_error("geometry_actual_DOWN_identity_generation_or_anchor_mismatch");
            generation_=id;source_key_=number(pending,"\"source_key\":");
            out_.emit("medium_product_geometry_actual_DOWN","\"phase\":"+quote(phase)+",\"receipt\":"+quote(pending));return true;},1000))
            throw std::runtime_error("geometry_actual_native_pending_DOWN_not_observed");
        // Exactly one real drag-threshold MOVE, retaining the original DOWN
        // anchor. Native START/END must come from product's real WinEvents.
        const LONG threshold=GetSystemMetrics(SM_CXDRAG);
        if(threshold<0||threshold>128)throw std::runtime_error("geometry_system_drag_metric_unknown");
        if(!capture_route()){
            native_threshold_step_=true;cursor({anchor.x+threshold+1,anchor.y});native_threshold_step_=false;
        }
        if(!wait_for([&]{if(retired())throw std::runtime_error("geometry_terminated_before_CAPTURE_READY");
            const auto started=row(mark,"native_move_start",generation_),ended=row(mark,"native_move_end",generation_),ready=row(mark,"gesture_start",generation_);
            const auto same_source=[&](const std::string& actual){return !actual.empty()&&
                number(actual,"\"source\":")==reinterpret_cast<std::uintptr_t>(target_)&&number(actual,"\"event_tid\":")==target_tid_&&
                number(actual,"\"original_DOWN_qpc\":")>=down_qpc_&&actual.find("\"native_physical_button_gate\":true")!=actual.npos;};
            return same_source(started)&&same_source(ended)&&number(started,"\"owner_sequence\":")&&
                number(ended,"\"owner_sequence\":")>number(started,"\"owner_sequence\":")&&
                number(ready,"\"key\":")==source_key_&&ready.find("\"native_first_handoff\":true")!=ready.npos&&
                ready.find("\"ctrl\":false")!=ready.npos&&capture_route()&&
                ready.find(region==HTCAPTION?"\"resize\":false":"\"resize\":true")!=ready.npos;},1500))
            throw std::runtime_error("geometry_real_START_END_capture_handoff_not_observed");
        continuation_ready_=true;
        const Rect expected=region==HTCAPTION?Rect{original->l+dx,original->t+dy,original->r+dx,original->b+dy}:
            Rect{original->l,original->t,original->r+dx,original->b+dy};
        const auto placement_mark=log().size();cursor({anchor.x+dx,anchor.y+dy});std::string placement;
        if(!wait_for([&]{if(retired())throw std::runtime_error("geometry_terminated_before_target_completion");
            placement=row(placement_mark,"placement",generation_);
            // Ordinary no-argument product intentionally omits successful
            // per-call diagnostics. Do not manufacture them or block the
            // independent real geometry/termination observation for that.
            return visible(target_)==expected&&capture_route();},1200))
            throw std::runtime_error("geometry_actual_native_exact_target_not_observed");
        RECT after{};const auto actual_visible=visible(target_);
        if(!GetWindowRect(target_,&after)||!actual_visible||*actual_visible!=expected||GetDpiForWindow(target_)!=original_dpi||
            MonitorFromWindow(target_,MONITOR_DEFAULTTONULL)!=original_monitor)throw std::runtime_error("geometry_positioning_monitor_DPI_postverify_unknown");
        const auto per_call=product::guest::up_evidence_detail::unique_value(placement,"\"native_attempted\":");
        out_.emit("medium_product_geometry_observed_geometry","\"phase\":"+quote(phase)+",\"generation\":"+std::to_string(generation_)+
            ",\"original_visible\":"+rect(*original)+",\"actual_visible\":"+rect(*actual_visible)+
            ",\"actual_positioning\":"+rect({after.left,after.top,after.right,after.bottom})+",\"actual_DPI\":"+std::to_string(original_dpi)+
            ",\"native_attempt_from_per_call_receipt\":"+(per_call&&(*per_call=="true"||*per_call=="false")?std::string(*per_call):"null")+
            ",\"production_per_call_receipt\":"+(placement.empty()?"null":quote(placement))+",\"missing_diagnostic_is_UNKNOWN\":true");
        const auto up_mark=log().size();send_mouse(MOUSEEVENTF_LEFTUP,{},false,true);std::string ended,legacy,gone,raw;
        if(!wait_for([&]{ended=row(up_mark,"gesture_end",generation_);legacy=row(up_mark,"gesture_input_resource",generation_,"legacy_left_up");
            gone=row(up_mark,"gesture_input_resource",generation_,"isolation_gone");raw=row(up_mark,"gesture_input_resource",generation_,"raw_mouse");
            const product::guest::UpEvidenceIdentity expected_up{product_pid_,source_,generation_,up_qpc_};
            return ended.find("\"normal_up\":true")!=ended.npos&&ended.find("\"end\":0")!=ended.npos&&
                product::guest::matching_clean_captured_up(legacy,gone,expected_up)&&
                (number(raw,"\"raw_button_flags\":")&RI_MOUSE_LEFT_BUTTON_UP)&&number(raw,"\"resource_observed_qpc\":")>=up_qpc_;
        },1200))throw std::runtime_error("geometry_actual_Raw_legacy_NormalUp_or_cleanup_not_observed");
        GUITHREADINFO gui{sizeof(gui)};if(!target_exact()||!GetGUIThreadInfo(target_tid_,&gui)||gui.hwndCapture||gui.hwndMoveSize||gui.hwndMenuOwner||
            visible(target_)!=expected||!status())throw std::runtime_error("geometry_terminal_context_or_idle_not_confirmed");
        out_.emit("medium_product_geometry_NormalUp","\"phase\":"+quote(phase)+",\"generation\":"+std::to_string(generation_)+
            ",\"single_UP_send_before_qpc\":"+std::to_string(up_qpc_)+",\"actual_visible\":"+rect(expected)+
            ",\"actual_source_capture\":0,\"end_receipt\":"+quote(ended)+",\"Raw_receipt\":"+quote(raw)+
            ",\"legacy_receipt\":"+quote(legacy)+",\"resources_receipt\":"+quote(gone)+",\"global_zero_late_history_claimed\":false");
        out_.emit("medium_product_geometry_status_delta","\"phase\":"+quote(phase)+",\"generation\":"+std::to_string(generation_)+
            ",\"before_actual_status\":"+quote(before_status)+",\"after_actual_status\":"+quote(last_status_)+
            ",\"aggregate_counter_is_not_per_call_API_receipt\":true");
        last_generation_=generation_;down_inserted_=false;
    }
    static bool no_buttons(){for(int key:{VK_LBUTTON,VK_RBUTTON,VK_MBUTTON,VK_XBUTTON1,VK_XBUTTON2,VK_CONTROL,VK_SHIFT,VK_MENU,VK_LWIN,VK_RWIN,VK_RETURN,VK_ESCAPE})
        if(GetAsyncKeyState(key)&0x8000)return false;return true;}
    void cleanup_target()noexcept{
        try{
            if(down_inserted_&&!up_attempted_){
                if(up_target(true))send_mouse(MOUSEEVENTF_LEFTUP,{},false,true,true);
                else out_.cleanup_known=false;
            }
            if(down_inserted_&&!up_inserted_)out_.cleanup_known=false;
            if(target_&&IsWindow(target_)){
                target_closed_=false;
                // Public ordinary close only after our known test pair is no
                // longer held. Never terminate or retry an unverified window.
                if((!down_inserted_||up_inserted_)&&target_exact()){
                    SetLastError(ERROR_SUCCESS);const bool sent=PostMessageW(target_,WM_CLOSE,0,0)!=FALSE;const auto error=sent?0:GetLastError();
                    target_closed_=sent&&wait_for([&]{return !IsWindow(target_);},1000,false);
                    DWORD code=STILL_ACTIVE;const bool known=target_process_&&GetExitCodeProcess(target_process_,&code);
                    out_.emit("medium_product_geometry_target_close","\"one_WM_CLOSE_attempted\":true,\"PostMessage_return\":"+std::string(sent?"true":"false")+
                        ",\"error\":"+std::to_string(error)+",\"exact_root_gone\":"+(target_closed_?"true":"false")+
                        ",\"process_wait\":"+std::to_string(WaitForSingleObject(target_process_,0))+",\"exit_code\":"+(known?std::to_string(code):"null")+
                        ",\"samples_not_atomic_or_all_descendant_exit\":true,\"forced_termination\":false");
                }
            }
        }catch(...){out_.cleanup_known=false;}
        out_.cleanup_known=out_.cleanup_known&&target_closed_&&(!lease_||!lease_->incomplete_pair.load());
    }
public:
    MediumDesktopProductGeometryCase(const std::wstring& run,HDESK desktop,const fs::path& product,HANDLE process,DWORD pid,
        std::uint64_t birth,std::uint64_t launch,std::string_view source,std::string_view sha,const std::function<bool()>& still_exact,
        const std::function<bool()>& exact_product,const std::shared_ptr<MediumDesktopUiInputLease>& lease,ULONGLONG deadline,
        MediumDesktopProductGeometryObservation& out):run_(run),desktop_(desktop),product_(product),product_process_(process),product_pid_(pid),
        product_birth_(birth),launch_tick_(launch),source_(source),product_hash_(sha),still_exact_(still_exact),exact_product_(exact_product),
        lease_(lease),deadline_(deadline),out_(out){}
    ~MediumDesktopProductGeometryCase(){if(log_change_!=INVALID_HANDLE_VALUE)FindCloseChangeNotification(log_change_);}
    void execute(){
        const auto manifest=read(L"C:\\PaneBindV1\\Input\\manifest.json");const auto asset=manifest.find("\"terminal_test_asset\"");
        if(asset==manifest.npos)return;auto at=manifest.find(':',asset);if(at==manifest.npos)return;
        do{++at;}while(at<manifest.size()&&(manifest[at]==' '||manifest[at]=='\t'||manifest[at]=='\r'||manifest[at]=='\n'));
        if(at==manifest.size()||manifest[at]!='{')return;out_.requested=true;
        try{
            const auto finish=manifest.find('}',at);if(finish==manifest.npos||manifest.find("\"terminal_test_asset\"",asset+21)!=manifest.npos)
                throw std::runtime_error("geometry_manifest_asset_ambiguous");
            const std::string_view spec(manifest.data()+at,finish-at+1);const auto files=manifest.find("\"files\"");
            if(files==manifest.npos)throw std::runtime_error("geometry_manifest_identity_missing");const std::string_view identity(manifest.data(),files);
            const std::string run(run_.begin(),run_.end());const auto configuration=equal(product_.parent_path().filename().wstring(),run_+L"-product-Debug")?L"Debug":L"Release";
            const auto root=fs::path(L"C:\\PaneBindV1\\Terminal")/run_/configuration/L"terminal-1.25.2733.0";target_image_=root/L"WindowsTerminal.exe";
            constexpr const char* terminal_hash="1CBE52914F7437A4F064327383F1B4C45FE2EB8E92D4F8DAE0AD7EFCB13A20A1";
            if(manifest_string(identity,"run_id")!=run||manifest_string(identity,"product_source_commit")!=source_||
               manifest_string(manifest,"guest_suite")!="MediumDesktopUiOnly"||
               manifest_string(spec,"kind")!="WindowsTerminalPortable"||manifest_string(spec,"version")!="1.25.2733.0"||
               manifest_string(spec,"relative_path")!="Terminal-test.zip"||manifest_string(spec,"exe_sha256")!=terminal_hash||
               hash(L"C:\\PaneBindV1\\Input\\Terminal-test.zip")!="BF3EF2012F6C44D8340A4C58125ACC9498D19B580F9890DC043CDF831852E796"||
               !plain_directory(root)||!marker(root/L"panebind-v1-test-application.marker",run_)||hash(target_image_)!=terminal_hash||
               hash(product_)!=product_hash_||!ProcessIdToSessionId(GetCurrentProcessId(),&session_)||!session_||!admission()||!no_buttons())
                throw std::runtime_error("geometry_exact_manifest_asset_or_actual_Run_product_not_verified");
            evidence_.begin(product_pid_,product_birth_,launch_tick_,source_);
            events_.changed=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!events_.changed)throw std::runtime_error("geometry_own_change_event_failed");
            const auto route=row(0,"input_route"),context=row(0,"desktop_context");
            if(route.find("\"capture_resource_route_enabled\":true")==route.npos||context.find("\"route\":\"exact_manifest_Run\"")==context.npos||
               context.find("\"run_id\":"+quote(run))==context.npos||!status())throw std::runtime_error("geometry_actual_product_Run_route_or_health_not_observed");
            out_.emit("medium_product_geometry_scope","\"run_id\":"+quote(run)+",\"manifest_sha256\":"+quote(hash(L"C:\\PaneBindV1\\Input\\manifest.json"))+
                ",\"product_PID\":"+std::to_string(product_pid_)+",\"product_birth\":"+std::to_string(product_birth_)+
                ",\"actual_route_receipt\":"+quote(route)+",\"actual_context_receipt\":"+quote(context)+
                ",\"product_absent_client_control_not_reused\":true,\"Terminal_content_ready_inferred\":false");
            const auto baseline=MediumDesktopWindows::read(desktop_);if(!baseline.complete)throw std::runtime_error("geometry_target_baseline_unknown");
            if(medium_terminal_native_events)throw std::runtime_error("geometry_native_observer_already_owned");
            medium_terminal_native_events=&events_;hook_=SetWinEventHook(EVENT_OBJECT_CREATE,EVENT_OBJECT_LOCATIONCHANGE,nullptr,
                medium_terminal_native_event,0,0,WINEVENT_OUTOFCONTEXT);if(!hook_)throw std::runtime_error("geometry_native_observer_unavailable");
            log_change_=FindFirstChangeNotificationW(product_.parent_path().c_str(),FALSE,FILE_NOTIFY_CHANGE_FILE_NAME|FILE_NOTIFY_CHANGE_SIZE|FILE_NOTIFY_CHANGE_LAST_WRITE);
            if(log_change_==INVALID_HANDLE_VALUE)throw std::runtime_error("geometry_product_log_change_notification_failed");
            STARTUPINFOW startup{sizeof(startup)};std::wstring desktop=L"WinSta0\\PaneBindV1_"+run_;startup.lpDesktop=desktop.data();PROCESS_INFORMATION info{};
            auto command=L"\""+target_image_.wstring()+L"\" --window new new-tab --title \"PaneBind-"+run_+
                L"-Geometry\" --suppressApplicationTitle \"C:\\Windows\\System32\\cmd.exe\" /d /q /k";
            const auto begin=GetTickCount64();SetLastError(ERROR_SUCCESS);const bool created=CreateProcessW(target_image_.c_str(),command.data(),nullptr,nullptr,FALSE,
                CREATE_NEW_PROCESS_GROUP,nullptr,root.c_str(),&startup,&info)!=FALSE;const auto error=created?0:GetLastError();
            if(created){target_process_=info.hProcess;target_thread_=info.hThread;target_pid_=info.dwProcessId;target_birth_=process_birth(target_process_);events_.pid=target_pid_;target_closed_=false;}
            out_.emit("medium_product_geometry_target_launch","\"CreateProcess_attempted\":true,\"native_return\":"+std::string(created?"true":"false")+
                ",\"error\":"+std::to_string(error)+",\"PID\":"+std::to_string(target_pid_)+",\"birth\":"+std::to_string(target_birth_)+
                ",\"before_tick\":"+std::to_string(begin)+",\"after_tick\":"+std::to_string(GetTickCount64())+",\"existing_product_PID\":"+std::to_string(product_pid_));
            if(!created||!wait_for([&]{const auto first=MediumDesktopWindows::read(desktop_),second=MediumDesktopWindows::read(desktop_);
                if(!first.complete||!second.complete)return false;unsigned matches{};HWND found{};
                for(HWND h:first.windows)if(std::find(baseline.windows.begin(),baseline.windows.end(),h)==baseline.windows.end()&&
                    std::find(second.windows.begin(),second.windows.end(),h)!=second.windows.end()){
                    const auto a=MediumDesktopWindowFact::read(h,target_pid_,target_process_,target_birth_,L"PaneBindV1_"+run_);
                    const auto b=MediumDesktopWindowFact::read(h,target_pid_,target_process_,target_birth_,L"PaneBindV1_"+run_);
                    if(a.type==L"CASCADIA_HOSTING_WINDOW_CLASS"&&a.readonly_same(b)){found=h;++matches;}}
                if(matches!=1)return false;target_=found;target_tid_=GetWindowThreadProcessId(target_,nullptr);events_.root=target_;return target_exact();},3500))
                throw std::runtime_error("geometry_new_exact_Terminal_root_not_observed");
            IVirtualDesktopManager* manager{};const HRESULT made=CoCreateInstance(CLSID_VirtualDesktopManager,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&manager));
            BOOL current{};const HRESULT queried=manager?manager->IsWindowOnCurrentVirtualDesktop(target_,&current):E_UNEXPECTED;if(manager)manager->Release();
            GUITHREADINFO gui{sizeof(gui)};const bool gui_known=GetGUIThreadInfo(target_tid_,&gui)!=FALSE;
            out_.emit("medium_product_geometry_target","\"actual\":"+MediumDesktopWindowFact::read(target_,target_pid_,target_process_,target_birth_,L"PaneBindV1_"+run_).json()+
                ",\"actual_Medium\":"+(medium(target_process_)?"true":"false")+",\"actual_same_SID\":"+(input_actor_same_user(target_process_)?"true":"false")+
                ",\"VDM_create_hresult\":"+std::to_string(static_cast<long>(made))+",\"VDM_query_hresult\":"+std::to_string(static_cast<long>(queried))+
                ",\"VDM_current\":"+(current?"true":"false")+",\"actual_foreground\":"+std::to_string(reinterpret_cast<std::uintptr_t>(GetForegroundWindow()))+
                ",\"owner_GTI_known\":"+(gui_known?"true":"false")+",\"foreground_reclaimed\":false,\"client_semantics_inferred\":false");
            if(made!=S_OK||queried!=S_OK||!current||!gui_known||gui.hwndCapture||gui.hwndMoveSize||gui.hwndMenuOwner||GetForegroundWindow()!=target_)
                throw std::runtime_error("geometry_actual_VDM_foreground_or_source_GUI_not_ready");
            one_gesture(HTCAPTION,72,36,"Move1");one_gesture(HTRIGHT,64,0,"Resize");one_gesture(HTCAPTION,-36,24,"SuccessorMove");
            out_.completed=true;out_.reason="one_new_Terminal_Move_UP_Resize_UP_successor_Move_UP_only";
        }catch(const std::exception& error){out_.reason=error.what();out_.completed=false;}
        catch(...){out_.reason="geometry_unexpected_exception";out_.completed=false;out_.cleanup_known=false;}
        cleanup_target();resources_.finish();
        if(log_change_!=INVALID_HANDLE_VALUE){const auto handle=log_change_;log_change_=INVALID_HANDLE_VALUE;change_closed_=FindCloseChangeNotification(handle)!=FALSE;}
        out_.cleanup_known=out_.cleanup_known&&!resources_.unknown&&change_closed_;
        out_.completed=out_.completed&&out_.cleanup_known;
        out_.emit("medium_product_geometry_summary","\"result\":"+quote(out_.completed?"PASS_SINGLE_EXACT_Terminal_GEOMETRY_ONLY":"FAIL_OR_BLOCKED")+
            ",\"reason\":"+quote(out_.reason)+",\"product_PID\":"+std::to_string(product_pid_)+",\"target_PID\":"+std::to_string(target_pid_)+
            ",\"last_completed_generation\":"+std::to_string(last_generation_)+",\"cleanup_known\":"+(out_.cleanup_known?"true":"false")+
            ",\"test_pair_still_unknown\":"+(lease_&&lease_->incomplete_pair.load()?"true":"false")+
            ",\"own_hook_stopped\":"+(resources_.hook_ok?"true":"false")+",\"own_handles_closed\":"+
            (resources_.process_ok&&resources_.thread_ok&&resources_.event_ok&&change_closed_?"true":"false")+
            ",\"client_nonce_or_content\":\"NOT_RUN\",\"Ctrl_Glue\":\"NOT_RUN\",\"five_applications_or_whole_version_PASS\":false,\"human_UAT\":\"NOT_RUN\"");
    }
};

MediumDesktopProductGeometryObservation medium_desktop_product_geometry_case(
    const std::wstring& run,HDESK desktop,const fs::path& product,HANDLE process,DWORD pid,std::uint64_t birth,std::uint64_t launch,
    std::string_view source,std::string_view sha,const std::function<bool()>& still_exact,const std::function<bool()>& exact_product,
    const std::shared_ptr<MediumDesktopUiInputLease>& lease,ULONGLONG deadline){
    MediumDesktopProductGeometryObservation out;
    try{MediumDesktopProductGeometryCase test(run,desktop,product,process,pid,birth,launch,source,sha,still_exact,exact_product,lease,deadline,out);test.execute();}
    catch(...){out.requested=true;out.completed=false;out.cleanup_known=false;out.reason="geometry_reporting_exception_actual_cleanup_unknown";}
    return out;
}
