#pragma once

// TEST ONLY: the same frozen production EXE on an exact manifest-bound Run.
// The Terminal functional case uses existing diagnostic observations; the
// Explorer branch retains its ordinary entry. Only the new owned blank
// neighbour receives TEST setup placement. No private product writer, Default
// input helper, permission override, fake START/END or client nonce is used.
#include "platform/windows/product/v1_guest_medium_desktop_explorer_target.h"
#include "platform/windows/product/v1_guest_medium_desktop_magnet_target.h"
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
    bool explorer_case_{};std::wstring target_class_{L"CASCADIA_HOSTING_WINDOW_CLASS"};
    std::unique_ptr<MediumExplorerGeometryTarget> explorer_;
    std::unique_ptr<MediumDesktopMagnetTarget> magnet_;
    bool magnet_completed_{},ctrl_completed_{},minimum_completed_{},minimum_chain_active_{},control_inserted_{},control_release_attempted_{},control_release_observed_{true};
    POINT minimum_native_{},maximum_native_{};UINT minimum_dpi_{};
    struct TerminalSizingProposal {Rect original,requested,corrected;RECT positioning;UINT dpi;HMONITOR monitor;};
    bool sizing_probe_attempted_{},sizing_comparison_completed_{},sizing_successor_completed_{};
    std::optional<bool> sizing_preference_matched_;
    POINT last_sample_{};ULONGLONG last_sample_tick_{};UINT pacing_dpi_{};
    bool down_inserted_{},up_attempted_{},up_inserted_{};std::uint64_t down_qpc_{},up_qpc_{},last_mouse_qpc_{};
    bool native_threshold_step_{},continuation_ready_{};
    std::optional<POINT> down_anchor_;HWND down_hit_{};int down_region_{};
    unsigned notifications_{};bool fair_wait_pump_used_{};std::string terminal_reason_;
    std::string last_status_;
    static std::string manifest_string(std::string_view text,std::string_view field){
        const std::string key="\""+std::string(field)+"\"";auto at=text.find(key);
        if(at==text.npos||text.find(key,at+key.size())!=text.npos)return {};
        at+=key.size();const auto spaces=[&]{while(at<text.size()&&(text[at]==' '||text[at]=='\t'||text[at]=='\r'||text[at]=='\n'))++at;};
        spaces();if(at==text.size()||text[at++]!=':')return {};spaces();if(at==text.size()||text[at++]!='"')return {};
        const auto begin=at;while(at<text.size()&&text[at]!='"'){if(text[at]=='\\'||static_cast<unsigned char>(text[at])<32)return {};++at;}
        return at<text.size()?std::string(text.substr(begin,at-begin)):std::string{};
    }
    static bool manifest_explorer(std::string_view text){
        constexpr std::string_view key="\"medium_explorer_geometry\"";auto at=text.find(key);
        if(at==text.npos)return false;
        if(text.find(key,at+key.size())!=text.npos)throw std::runtime_error("geometry_manifest_Explorer_flag_ambiguous");
        at+=key.size();const auto spaces=[&]{while(at<text.size()&&(text[at]==' '||text[at]=='\t'||text[at]=='\r'||text[at]=='\n'))++at;};
        spaces();if(at==text.size()||text[at++]!=':')throw std::runtime_error("geometry_manifest_Explorer_flag_not_boolean");spaces();
        bool result{};if(text.substr(at,4)=="true"){result=true;at+=4;}
        else if(text.substr(at,5)=="false")at+=5;else throw std::runtime_error("geometry_manifest_Explorer_flag_not_boolean");
        spaces();if(at==text.size()||(text[at]!=','&&text[at]!='}'))throw std::runtime_error("geometry_manifest_Explorer_flag_delimiter");
        return result;
    }
    const char* target_kind()const noexcept{return explorer_case_?"Explorer":"Terminal";}
    bool refresh_target(){
        // The Explorer COM observer is refreshed only outside SendInput's
        // in-flight claim. The final send guard reads its bounded receipts.
        return !explorer_||(lease_&&!(lease_->flags.load(std::memory_order_acquire)&MediumDesktopUiInputLease::in_flight)&&explorer_->refresh());
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
    bool target_exact(bool require_neighbour=true)const{
        if((explorer_&&!explorer_->guard())||(require_neighbour&&magnet_&&!magnet_->exact())||!target_process_||!target_pid_||!target_birth_||GetProcessId(target_process_)!=target_pid_||
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
            first.type==target_class_&&second.type==first.type&&first.root&&second.root&&first.visible&&second.visible;
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
        // Bound each operation's wakeups, not the sum of unrelated completed
        // operations. Diagnostic logging adds real notifications across the
        // sequence; it must not consume a later phase's fresh wait budget.
        unsigned wait_notifications{};
        const auto rejected=[&](const char* reason,DWORD error=0){
            out_.emit("medium_product_geometry_wait_rejected","\"reason\":"+quote(reason)+
                ",\"generation\":"+std::to_string(generation_)+",\"cap_ms\":"+std::to_string(cap)+
                ",\"actual_elapsed_ms\":"+std::to_string(GetTickCount64()-begin)+
                ",\"wait_notifications\":"+std::to_string(wait_notifications)+
                ",\"total_notifications\":"+std::to_string(notifications_)+",\"native_error\":"+std::to_string(error));
            return false;
        };
        const auto pump=[&]{
            fair_wait_pump_used_=true;
            // This same STA owns the blank neighbour. Service its incoming
            // sent messages even when log/event handles win the native wait;
            // never let the expensive evidence predicate starve that target.
            // No wait, input or callback logging is added here. Retain the
            // existing bounded final predicate after the phase wait expires.
            unsigned messages=256;
            while(messages--&&GetTickCount64()<end){
                if(quit_seen_||(require_active&&!admission()))return rejected("pump_quit_or_admission_revoked");
                MSG message{};
                // Peek dispatches incoming nonqueued messages too. Do not
                // remove a queued message if those calls consumed the budget.
                if(!PeekMessageW(&message,nullptr,0,0,PM_NOREMOVE))break;
                if(message.message==WM_QUIT){quit_seen_=true;return rejected("WM_QUIT");}
                if(GetTickCount64()>=end)break;
                if(quit_seen_||(require_active&&!admission()))return rejected("pump_context_changed_after_sent_messages");
                if(!PeekMessageW(&message,nullptr,0,0,PM_REMOVE))continue;
                if(message.message==WM_QUIT){quit_seen_=true;return rejected("WM_QUIT");}
                if(GetTickCount64()>=end)return rejected("pump_phase_deadline_before_dispatch");
                if(quit_seen_||(require_active&&!admission()))return rejected("pump_admission_revoked_before_dispatch");
                TranslateMessage(&message);DispatchMessageW(&message);
            }
            if(quit_seen_||(require_active&&!admission()))return rejected("pump_final_quit_or_admission_revoked");
            return true;
        };
        for(;;){
            if(!pump())return false;
            if(events_.changed&&!ResetEvent(events_.changed))return rejected("ResetEvent_failed",GetLastError());
            if(predicate())return true;
            const auto now=GetTickCount64();if(now>=end)return rejected("bounded_deadline");
            if(!events_.changed)return rejected("event_missing");
            HANDLE handles[5]{events_.changed,product_process_,target_process_,nullptr,nullptr};
            DWORD count=target_process_?3:2;const DWORD log_index=count;
            if(log_change_!=INVALID_HANDLE_VALUE)handles[count++]=log_change_;
            const DWORD extra_index=count;if(extra)handles[count++]=extra;
            const auto result=MsgWaitForMultipleObjectsEx(count,handles,static_cast<DWORD>(end-now),QS_ALLINPUT,MWMO_INPUTAVAILABLE);
            if(result==WAIT_FAILED)return rejected("MsgWait_failed",GetLastError());
            // Includes QS wakes as well as log/observer/timer/process wakes.
            // The next predicate always follows the same bounded STA service.
            if(!pump())return false;
            if(result==WAIT_OBJECT_0+1)return rejected("product_exited");
            if(target_process_&&result==WAIT_OBJECT_0+2)return predicate()||rejected("target_exited_before_predicate");
            if(extra&&result==WAIT_OBJECT_0+extra_index)return predicate()||rejected("extra_signalled_predicate_false");
            if(log_change_!=INVALID_HANDLE_VALUE&&result==WAIT_OBJECT_0+log_index){
                ++notifications_;
                if(++wait_notifications>64)return rejected("per_wait_notification_limit");
                if(!FindNextChangeNotification(log_change_))return rejected("FindNextChangeNotification_failed",GetLastError());
            }
            if(result==WAIT_TIMEOUT)return ((!require_active||admission())&&predicate())||rejected("native_timeout_predicate_false");
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
        // A threshold kick is not continuation. If native MoveSize or the
        // shield takes over during this read, discard the unissued kick and
        // wait for the real handoff instead of sending into that transition.
        GUITHREADINFO gui{sizeof(gui)};POINT point{};
        return GetGUIThreadInfo(target_tid_,&gui)&&(!gui.hwndCapture||gui.hwndCapture==target_)&&
            !gui.hwndMoveSize&&!gui.hwndMenuOwner&&
            !(gui.flags&(GUI_INMOVESIZE|GUI_INMENUMODE|GUI_SYSTEMMENUMODE|GUI_POPUPMENUMODE))&&
            down_anchor_&&GetCursorPos(&point)&&point.x==down_anchor_->x&&point.y==down_anchor_->y;
    }
    bool skip_unissued_threshold(const char* stage,bool guard_changed){
        // A threshold is only a TEST kick for a still-idle native DOWN. A
        // real START/MoveSize or an in-progress handoff makes it unnecessary.
        // Dropping an unissued kick is NOT permission to send through unknown
        // GUI state: the normal real START/END/capture gate must still pass.
        if(!admission()||!target_exact()||GetForegroundWindow()!=target_||!generation_||
            !down_inserted_||up_attempted_||retired())
            throw std::runtime_error("geometry_native_threshold_source_or_generation_revoked");
        const auto started=row(0,"native_move_start",generation_),ready=row(0,"gesture_start",generation_),
            capture=row(0,"gesture_input_resource",generation_,"capture_ready");
        GUITHREADINFO gui{sizeof(gui)};const bool gui_known=GetGUIThreadInfo(target_tid_,&gui)!=FALSE;
        const bool own_native=gui_known&&gui.hwndMoveSize==target_&&
            (!gui.hwndCapture||gui.hwndCapture==target_)&&!gui.hwndMenuOwner;
        const bool matching_start=!started.empty()&&number(started,"\"source\":")==reinterpret_cast<std::uintptr_t>(target_)&&
            number(started,"\"event_tid\":")==target_tid_&&number(started,"\"original_DOWN_qpc\":")>=down_qpc_;
        const bool matching_ready=!ready.empty()&&number(ready,"\"key\":")==source_key_;
        const bool matching_capture=!capture.empty()&&number(capture,"\"source\":")==reinterpret_cast<std::uintptr_t>(target_)&&
            number(capture,"\"source_pid\":")==target_pid_&&number(capture,"\"source_tid\":")==target_tid_&&
            number(capture,"\"resource_observed_qpc\":")>=down_qpc_;
        const bool progressed=matching_start||matching_ready||matching_capture||own_native;
        if(!guard_changed&&!progressed)return false;
        out_.emit("medium_product_geometry_threshold_not_sent","\"generation\":"+std::to_string(generation_)+
            ",\"stage\":"+quote(stage)+",\"reason\":"+quote(progressed?"real_native_or_handoff_already_progressed":"fresh_GUI_guard_changed_wait_for_actual_handoff")+
            ",\"matching_actual_START\":"+(matching_start?"true":"false")+",\"matching_actual_gesture_start\":"+(matching_ready?"true":"false")+
            ",\"matching_actual_capture_ready\":"+(matching_capture?"true":"false")+",\"source_GUI_known\":"+(gui_known?"true":"false")+
            ",\"actual_source_capture\":"+(gui_known?std::to_string(reinterpret_cast<std::uintptr_t>(gui.hwndCapture)):"null")+
            ",\"actual_source_MoveSize\":"+(gui_known?std::to_string(reinterpret_cast<std::uintptr_t>(gui.hwndMoveSize)):"null")+
            ",\"SendInput_attempted\":false,\"cursor_readback_required\":false,\"input_or_write_authority_granted\":false,\"threshold_retried\":false");
        return true;
    }
    bool send_mouse(DWORD flags,POINT point,bool cursor,bool up=false,bool cleanup=false){
        // The normal release contains a final MOVE. Once a real terminal
        // fact is observed it may not send that MOVE, even if capture has
        // not yet been released. Abort cleanup remains a separate single UP.
        const auto allowed=[&]{return admission()&&target_exact()&&control_state()&&(up?((cleanup||!retired())&&up_target(cleanup)):
            fresh_move_route(flags));};
        const bool threshold=native_threshold_step_&&!continuation_ready_&&!up&&flags==MOUSEEVENTF_MOVE;
        if(threshold&&skip_unissued_threshold("before_first_guard",false))return false;
        if(!allowed()){
            if(threshold&&skip_unissued_threshold("first_fresh_guard_changed",true))return false;
            throw std::runtime_error("geometry_Run_input_target_or_lease_rejected");
        }
        if(!lease_->begin())throw std::runtime_error("geometry_Run_input_target_or_lease_rejected");
        struct End {MediumDesktopUiInputLease& value;~End(){value.end();}} end{*lease_};
        if(threshold&&skip_unissued_threshold("after_inflight_claim",false))return false;
        if(!allowed()){
            if(threshold&&skip_unissued_threshold("second_fresh_guard_changed",true))return false;
            throw std::runtime_error("geometry_input_revoked_after_inflight_claim");
        }
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
        last_mouse_qpc_=static_cast<std::uint64_t>(before.QuadPart);
        const bool after_known=QueryPerformanceCounter(&after)!=FALSE;out_.input_inserted=out_.input_inserted||inserted>0;
        if(flags&MOUSEEVENTF_LEFTDOWN){down_inserted_=inserted==1;down_qpc_=static_cast<std::uint64_t>(before.QuadPart);
            if(!down_inserted_)lease_->incomplete_pair.store(control_inserted_,std::memory_order_release);}
        if(up){up_inserted_=inserted==count;up_qpc_=static_cast<std::uint64_t>(before.QuadPart);
            if(up_inserted_)lease_->incomplete_pair.store(control_inserted_,std::memory_order_release);}
        out_.emit("medium_product_geometry_input","\"generation\":"+std::to_string(generation_)+",\"packet_count\":"+std::to_string(count)+
            ",\"mouse_flags\":["+std::to_string(packet.mi.dwFlags)+(serial_up?","+std::to_string(packets[1].mi.dwFlags):"")+
            "],\"single_UP_packet_index\":"+(up?std::to_string(count-1):"null")+
            ",\"normal_UP_variant\":"+(serial_up?quote("one_serial_final_actual_cursor_MOVE_then_single_UP"):"null")+
            ",\"requested_point\":["+std::to_string(point.x)+","+std::to_string(point.y)+"],\"SendInput_return\":"+std::to_string(inserted)+
            ",\"short_error\":"+std::to_string(error)+",\"before_qpc\":"+std::to_string(before.QuadPart)+",\"after_qpc\":"+(after_known?std::to_string(after.QuadPart):"null")+
            ",\"cleanup_only\":"+(cleanup?"true":"false")+",\"Default_helpers_used\":false,\"API_insert_not_product_delivery\":true");
        if(inserted!=count)throw std::runtime_error("geometry_input_short_insert_no_replay");
        return true;
    }
    bool cursor(POINT point){
        if(!send_mouse(MOUSEEVENTF_MOVE,point,true))return false;POINT actual{};
        if(!wait_for([&]{return target_exact()&&GetCursorPos(&actual)&&actual.x==point.x&&actual.y==point.y;},200))
            throw std::runtime_error("geometry_actual_cursor_not_exact");
        return true;
    }
    bool control_state()const{
        const bool left=(GetAsyncKeyState(VK_LCONTROL)&0x8000)!=0,right=(GetAsyncKeyState(VK_RCONTROL)&0x8000)!=0;
        return control_inserted_?left&&!right:!left&&!right;
    }
    void control(bool down,bool cleanup=false){
        if(down?(control_inserted_||!no_buttons()):( !control_inserted_||control_release_attempted_))
            throw std::runtime_error("geometry_TEST_Ctrl_pair_not_owned_or_already_released");
        const auto allowed=[&]{return (cleanup?base_context():admission())&&target_exact(!cleanup)&&GetForegroundWindow()==target_;};
        if(!allowed()||!lease_||!lease_->valid())throw std::runtime_error("geometry_TEST_Ctrl_context_not_exact");
        bool claimed=lease_->begin();bool retired_cleanup=false;
        if(!claimed&&cleanup&&!down&&control_inserted_){
            // Only this successfully inserted TEST key's one abort UP may
            // claim an otherwise idle retired lease. New input stays retired.
            if(!ResetEvent(lease_->input_done))throw std::runtime_error("geometry_TEST_Ctrl_cleanup_event_failed");
            unsigned expected=MediumDesktopUiInputLease::retired_bit;
            claimed=lease_->flags.compare_exchange_strong(expected,MediumDesktopUiInputLease::retired_bit|MediumDesktopUiInputLease::in_flight,
                std::memory_order_acq_rel);retired_cleanup=claimed;
        }
        if(!claimed)throw std::runtime_error("geometry_TEST_Ctrl_lease_rejected");
        struct End{MediumDesktopUiInputLease& value;~End(){value.end();}} end{*lease_};
        if(!allowed()||(!down&&!control_inserted_))throw std::runtime_error("geometry_TEST_Ctrl_revoked_before_native_input");
        INPUT packet{};packet.type=INPUT_KEYBOARD;packet.ki.wVk=VK_LCONTROL;packet.ki.dwExtraInfo=0x50425631;
        if(!down)packet.ki.dwFlags=KEYEVENTF_KEYUP;
        LARGE_INTEGER before{},after{};if(!QueryPerformanceCounter(&before))throw std::runtime_error("geometry_TEST_Ctrl_clock_unknown");
        if(down)lease_->incomplete_pair.store(true,std::memory_order_release);else control_release_attempted_=true;
        out_.input_attempted=true;SetLastError(ERROR_SUCCESS);const UINT inserted=SendInput(1,&packet,sizeof(packet));
        const DWORD error=inserted==1?0:GetLastError();const bool clock=QueryPerformanceCounter(&after)!=FALSE;
        out_.input_inserted=out_.input_inserted||inserted>0;
        if(down){control_inserted_=inserted==1;if(control_inserted_)control_release_observed_=false;}
        else if(inserted==1){control_inserted_=false;
            control_release_observed_=wait_for([&]{return base_context()&&target_exact(!cleanup)&&GetForegroundWindow()==target_&&control_state();},200,!cleanup);}
        const bool key_pair_unknown=control_inserted_||!control_release_observed_;
        lease_->incomplete_pair.store(key_pair_unknown||(down_inserted_&&!up_inserted_),std::memory_order_release);
        out_.emit("medium_product_geometry_TEST_Ctrl_input","\"DOWN\":"+std::string(down?"true":"false")+
            ",\"SendInput_attempted\":true,\"SendInput_return\":"+std::to_string(inserted)+",\"short_error\":"+std::to_string(error)+
            ",\"before_qpc\":"+std::to_string(before.QuadPart)+",\"after_qpc\":"+(clock?std::to_string(after.QuadPart):"null")+
            ",\"owned_DOWN_still_held\":"+(control_inserted_?"true":"false")+",\"abort_cleanup_only\":"+(cleanup?"true":"false")+
            ",\"Ctrl_release_state_observed\":"+(control_release_observed_?"true":"false")+
            ",\"retired_lease_cleanup_claim\":"+(retired_cleanup?"true":"false")+",\"NormalUp_inferred\":false");
        if(inserted!=1)throw std::runtime_error("geometry_TEST_Ctrl_short_insert_no_replay");
        if(!down&&!control_release_observed_)throw std::runtime_error("geometry_TEST_Ctrl_release_state_unknown_no_replay");
    }
    void pace(POINT next){
        const auto magnitude=[](LONG a,LONG b){const auto d=static_cast<long long>(a)-b;return static_cast<ULONGLONG>(d<0?-d:d);};
        const auto distance=(std::max)(magnitude(next.x,last_sample_.x),magnitude(next.y,last_sample_.y));
        const auto speed=static_cast<ULONGLONG>((static_cast<std::uint64_t>(2000)*pacing_dpi_+48)/96);
        if(!speed||distance>2048)throw std::runtime_error("geometry_TEST_phase_speed_or_distance_invalid");
        const auto required=(static_cast<ULONGLONG>(distance)*1000+speed-1)/speed+25;
        const auto now=GetTickCount64();const auto elapsed=now-last_sample_tick_;
        const DWORD remaining=elapsed<required?static_cast<DWORD>(required-elapsed):0;
        if(remaining>1000||now+remaining>=deadline_)throw std::runtime_error("geometry_TEST_phase_pacing_deadline_insufficient");
        const auto began=GetTickCount64();LARGE_INTEGER before{},after{};
        if(!QueryPerformanceCounter(&before))throw std::runtime_error("geometry_TEST_phase_before_clock_unknown");
        if(remaining){Handle timer(CreateWaitableTimerW(nullptr,TRUE,nullptr));if(!timer.value)throw std::runtime_error("geometry_TEST_phase_timer_create_failed");
            LARGE_INTEGER due{};due.QuadPart=-static_cast<LONGLONG>(remaining)*10000;
            if(!SetWaitableTimer(timer.value,&due,0,nullptr,nullptr,FALSE)){
                const auto error=GetLastError();out_.emit("medium_product_geometry_phase_timer_failure",
                    "\"SetWaitableTimer_attempted\":true,\"SetWaitableTimer_return\":false,\"native_error\":"+std::to_string(error));
                throw std::runtime_error("geometry_TEST_phase_SetWaitableTimer_failed");
            }
            if(!wait_for([&]{return WaitForSingleObject(timer.value,0)==WAIT_OBJECT_0;},remaining+100,true,timer.value))
                throw std::runtime_error("geometry_TEST_phase_one_shot_wait_failed");}
        if(!QueryPerformanceCounter(&after))throw std::runtime_error("geometry_TEST_phase_after_clock_unknown");
        if(retired()||!target_exact()||!capture_route()||!control_state())throw std::runtime_error("geometry_TEST_phase_retired_during_one_shot_wait");
        out_.emit("medium_product_geometry_phase_pacing","\"generation\":"+std::to_string(generation_)+
            ",\"from\":["+std::to_string(last_sample_.x)+","+std::to_string(last_sample_.y)+"],\"to\":["+std::to_string(next.x)+","+std::to_string(next.y)+
            "],\"configured_speed_DIP_per_second\":2000,\"actual_DPI\":"+std::to_string(pacing_dpi_)+",\"speed_pixels_per_second\":"+std::to_string(speed)+
            ",\"minimum_interval_ms\":"+std::to_string(required)+",\"one_shot_wait_requested_ms\":"+std::to_string(remaining)+
            ",\"actual_wait_span_ms\":"+std::to_string(GetTickCount64()-began)+",\"before_qpc\":"+std::to_string(before.QuadPart)+
            ",\"after_qpc\":"+std::to_string(after.QuadPart)+",\"polling\":false,\"product_clock_modified\":false");
    }
    std::string phase_placement(std::size_t mark,Rect expected,std::uint64_t key,std::uint64_t exclude=0){
        (void)log();const auto suffix=evidence_.since(mark);if(!suffix)throw std::runtime_error("geometry_phase_log_mark_not_current_launch");
        std::istringstream lines{std::string(*suffix)};std::string line,result;
        while(std::getline(lines,line)){
            const auto actual_key=number(line,"\"key\":");
            if(line.find("\"type\":\"placement\"")==line.npos||number(line,"\"generation\":")!=generation_||
               !actual_key||(key&&actual_key!=key)||(exclude&&actual_key==exclude)||
               number(line,"\"hook_observed_qpc\":")<last_mouse_qpc_||
               line.find("\"target\":"+rect(expected)+",")==line.npos||line.find("\"actual\":"+rect(expected)+",")==line.npos||
               line.find("\"exact\":true")==line.npos||line.find("\"identity_valid\":true")==line.npos||
               line.find("\"native_completion_known\":true")==line.npos||line.find("\"normal_up_completion\":false")==line.npos)continue;
            const auto attempted=product::guest::up_evidence_detail::unique_value(line,"\"native_attempted\":");
            if(!attempted||(*attempted!="true"&&*attempted!="false")||
               (*attempted=="true"&&line.find("\"native_succeeded\":true")==line.npos))continue;
            result=line;
        }return result;
    }
    std::string application_right_placement(std::size_t mark,Rect raw,Rect original,const RECT& positioning,
        Rect actual,std::string& callback){
        // Only the ordinary Terminal right-edge case adopts a returned
        // application proposal. The raw R remains independent of the solver.
        if(actual.width()<=0||actual.height()<=0||actual.l!=original.l||actual.t!=original.t||actual.b!=original.b)return {};
        const auto r=static_cast<std::int64_t>(positioning.right)+raw.r-original.r;
        const auto c=static_cast<std::int64_t>(positioning.right)+actual.r-original.r;
        if(r<std::numeric_limits<LONG>::min()||r>std::numeric_limits<LONG>::max()||
            c<std::numeric_limits<LONG>::min()||c>std::numeric_limits<LONG>::max())return {};
        const Rect r_native{positioning.left,positioning.top,static_cast<LONG>(r),positioning.bottom};
        const Rect c_native{positioning.left,positioning.top,static_cast<LONG>(c),positioning.bottom};
        RECT observed_native{};if(!GetWindowRect(target_,&observed_native)||
            Rect{observed_native.left,observed_native.top,observed_native.right,observed_native.bottom}!=c_native)return {};
        (void)log();const auto suffix=evidence_.since(mark);if(!suffix)throw std::runtime_error("geometry_resize_callback_log_mark_unknown");
        const std::string snapshot{*suffix};std::istringstream rows{snapshot};std::string line;
        while(std::getline(rows,line)){
            using product::guest::up_evidence_detail::unique_value;
            const auto hook=number(line,"\"hook_observed_qpc\":"),before=number(line,"\"before_qpc\":"),after=number(line,"\"after_qpc\":");
            const auto operation=number(line,"\"operation\":");const auto api=unique_value(line,"\"API_return\":");
            LONGLONG api_result{};char tail{};std::istringstream api_text{api?std::string(*api):std::string{}};
            if(!(api_text>>api_result)||!api_result||(api_text>>tail)||
                line.find("\"type\":\"application_resize_proposal\"")==line.npos||number(line,"\"generation\":")!=generation_||
                number(line,"\"key\":")!=source_key_||!operation||number(line,"\"source_HWND\":")!=reinterpret_cast<std::uintptr_t>(target_)||
                number(line,"\"source_PID\":")!=target_pid_||number(line,"\"source_TID\":")!=target_tid_||number(line,"\"edge\":")!=WMSZ_RIGHT||
                !hook||hook<last_mouse_qpc_||before<hook||after<before||
                unique_value(line,"\"callback_attempted\":")!="true"||unique_value(line,"\"context_revalidated\":")!="true"||
                unique_value(line,"\"callback_is_placement_attempt\":")!="false"||unique_value(line,"\"global_application_grid_known\":")!="false"||
                line.find("\"R_visible\":"+rect(raw)+",")==line.npos||line.find("\"R_native\":"+rect(r_native)+",")==line.npos||
                line.find("\"candidate_visible\":"+rect(actual)+",")==line.npos||line.find("\"C_native\":"+rect(c_native)+",")==line.npos)continue;
            const auto result=unique_value(line,"\"message_LRESULT\":");
            if(result!="1"&&!(result=="0"&&actual==raw&&c_native==r_native))continue;
            const auto placement=phase_placement(mark,actual,source_key_);
            if(placement.empty()||number(placement,"\"operation\":")!=operation||number(placement,"\"hook_observed_qpc\":")!=hook||
                snapshot.find(placement,snapshot.find(line)+line.size())==snapshot.npos)continue;
            callback=line;return placement;
        }return {};
    }
    Rect terminal_minimum_resize(Rect original,const RECT& positioning,UINT dpi,LONG& dy){
        if(explorer_case_||magnet_||!admission()||!target_exact()||!no_buttons()||GetForegroundWindow()!=target_)
            throw std::runtime_error("geometry_Terminal_minimum_fresh_idle_context_required");
        const HMONITOR monitor=MonitorFromWindow(target_,MONITOR_DEFAULTTONULL);MONITORINFO info{sizeof(info)};
        const int system_min_x=GetSystemMetricsForDpi(SM_CXMINTRACK,dpi),system_min_y=GetSystemMetricsForDpi(SM_CYMINTRACK,dpi);
        const int system_max_x=GetSystemMetricsForDpi(SM_CXMAXTRACK,dpi),system_max_y=GetSystemMetricsForDpi(SM_CYMAXTRACK,dpi);
        if(!dpi||!monitor||!GetMonitorInfoW(monitor,&info)||system_min_x<=0||system_min_y<=0||system_max_x<=0||system_max_y<=0)
            throw std::runtime_error("geometry_Terminal_minimum_system_seed_UNKNOWN");
        MINMAXINFO initial{};initial.ptMinTrackSize={system_min_x,system_min_y};initial.ptMaxTrackSize={system_max_x,system_max_y};
        initial.ptMaxSize={info.rcWork.right-info.rcWork.left,info.rcWork.bottom-info.rcWork.top};
        initial.ptMaxPosition={info.rcWork.left,info.rcWork.top};auto actual=initial;DWORD_PTR native_result{};
        const auto before_tick=GetTickCount64();SetLastError(ERROR_SUCCESS);
        const bool returned=SendMessageTimeoutW(target_,WM_GETMINMAXINFO,0,reinterpret_cast<LPARAM>(&actual),
            SMTO_ABORTIFHUNG|SMTO_BLOCK|SMTO_ERRORONEXIT,30,&native_result)!=FALSE;
        const DWORD error=returned?0:GetLastError();const auto after_tick=GetTickCount64();
        RECT after{};GUITHREADINFO gui{sizeof(gui)};
        const bool context=admission()&&target_exact()&&GetForegroundWindow()==target_&&GetDpiForWindow(target_)==dpi&&
            MonitorFromWindow(target_,MONITOR_DEFAULTTONULL)==monitor&&GetWindowRect(target_,&after)&&
            after.left==positioning.left&&after.top==positioning.top&&after.right==positioning.right&&after.bottom==positioning.bottom&&
            visible(target_)==original&&GetGUIThreadInfo(target_tid_,&gui)&&!gui.hwndCapture&&!gui.hwndMoveSize&&!gui.hwndMenuOwner;
        const std::int64_t frame_width=static_cast<std::int64_t>(positioning.right)-positioning.left-original.width();
        const std::int64_t frame_height=static_cast<std::int64_t>(positioning.bottom)-positioning.top-original.height();
        const std::int64_t min_width=static_cast<std::int64_t>(actual.ptMinTrackSize.x)-frame_width;
        const std::int64_t min_height=static_cast<std::int64_t>(actual.ptMinTrackSize.y)-frame_height;
        const bool application_changed_min=actual.ptMinTrackSize.x!=initial.ptMinTrackSize.x||actual.ptMinTrackSize.y!=initial.ptMinTrackSize.y;
        const bool application_changed_height=actual.ptMinTrackSize.y!=initial.ptMinTrackSize.y;
        const bool legal=returned&&context&&application_changed_height&&actual.ptMinTrackSize.x>0&&actual.ptMinTrackSize.y>0&&
            actual.ptMaxTrackSize.x>=actual.ptMinTrackSize.x&&actual.ptMaxTrackSize.y>=actual.ptMinTrackSize.y&&
            min_width>0&&min_width<=original.width()&&min_height>1&&min_height<original.height()&&
            original.width()<=static_cast<std::int64_t>(actual.ptMaxTrackSize.x)-frame_width&&
            original.height()<=static_cast<std::int64_t>(actual.ptMaxTrackSize.y)-frame_height;
        const auto point_json=[](POINT p){return "["+std::to_string(p.x)+","+std::to_string(p.y)+"]";};
        out_.emit("medium_product_geometry_Terminal_declared_minimum","\"target_PID\":"+std::to_string(target_pid_)+
            ",\"target_birth\":"+std::to_string(target_birth_)+",\"target_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(target_))+
            ",\"WM_GETMINMAXINFO_attempted\":true,\"native_return\":"+(returned?"true":"false")+",\"native_error\":"+std::to_string(error)+
            ",\"native_message_result\":"+(returned?std::to_string(native_result):"null")+",\"timeout_ms\":30,\"before_tick\":"+std::to_string(before_tick)+
            ",\"after_tick\":"+std::to_string(after_tick)+",\"actual_DPI\":"+std::to_string(dpi)+
            ",\"system_seed_min_native\":"+point_json(initial.ptMinTrackSize)+",\"system_seed_max_native\":"+point_json(initial.ptMaxTrackSize)+
            ",\"system_seed_max_size\":"+point_json(initial.ptMaxSize)+",\"system_seed_max_position\":"+point_json(initial.ptMaxPosition)+
            ",\"application_returned_min_native\":"+(returned?point_json(actual.ptMinTrackSize):"null")+
            ",\"application_returned_max_native\":"+(returned?point_json(actual.ptMaxTrackSize):"null")+
            ",\"application_changed_minimum\":"+(returned?(application_changed_min?"true":"false"):"null")+
            ",\"application_changed_height_minimum\":"+(returned?(application_changed_height?"true":"false"):"null")+
            ",\"original_visible\":"+rect(original)+",\"original_positioning\":"+rect({positioning.left,positioning.top,positioning.right,positioning.bottom})+
            ",\"native_minus_visible_size\":["+std::to_string(frame_width)+","+std::to_string(frame_height)+"]"+
            ",\"independent_min_visible\":"+(returned?"["+std::to_string(min_width)+","+std::to_string(min_height)+"]":"null")+
            ",\"post_query_exact_context\":"+(context?"true":"false")+",\"bounded_legal_oracle_known\":"+(legal?"true":"false")+
            ",\"application_grid_known\":false,\"Terminal_client_ready_inferred\":false,\"product_solver_used_for_expected\":false");
        if(!legal)throw std::runtime_error("geometry_Terminal_declared_minimum_or_legal_oracle_UNKNOWN");
        const auto delta=min_height-1-original.height();
        if(delta<std::numeric_limits<LONG>::min()||delta>std::numeric_limits<LONG>::max())
            throw std::runtime_error("geometry_Terminal_below_minimum_input_delta_out_of_range");
        const auto bottom=static_cast<std::int64_t>(original.t)+min_height;
        if(bottom<std::numeric_limits<LONG>::min()||bottom>std::numeric_limits<LONG>::max())
            throw std::runtime_error("geometry_Terminal_minimum_expected_bottom_out_of_range");
        minimum_native_=actual.ptMinTrackSize;maximum_native_=actual.ptMaxTrackSize;minimum_dpi_=dpi;
        dy=static_cast<LONG>(delta);return {original.l,original.t,original.r,static_cast<LONG>(bottom)};
    }
    std::optional<TerminalSizingProposal> terminal_sizing_proposal(){
        // A single application-affecting proposal, not a read-only query, real
        // mouse input, or proof of the application's complete legal-size set.
        std::string reason="fresh_idle_or_prior_cleanup_UNKNOWN";RECT before{},after{};RECT requested{},corrected{};
        std::optional<Rect> original;UINT dpi{};HMONITOR monitor{};MONITORINFO info{sizeof(info)};
        bool attempted{},context{},eligible{},after_geometry_known{};LRESULT api{};DWORD error{};DWORD_PTR message_result{};
        std::optional<Rect> after_visible;
        ULONGLONG before_tick{},after_tick{};LARGE_INTEGER before_qpc{},after_qpc{};bool before_clock{},after_clock{};
        const auto unchanged=[&]{
            GUITHREADINFO gui{sizeof(gui)};RECT current{};DWORD pid{};
            return admission()&&target_exact()&&no_buttons()&&!down_inserted_&&!control_inserted_&&
                lease_->flags.load(std::memory_order_acquire)==0&&!lease_->incomplete_pair.load()&&
                GetWindowThreadProcessId(target_,&pid)==target_tid_&&pid==target_pid_&&target_tid_!=GetCurrentThreadId()&&
                GetForegroundWindow()==target_&&GetDpiForWindow(target_)==dpi&&MonitorFromWindow(target_,MONITOR_DEFAULTTONULL)==monitor&&
                GetWindowRect(target_,&current)&&current.left==before.left&&current.top==before.top&&current.right==before.right&&current.bottom==before.bottom&&
                visible(target_)==original&&GetGUIThreadInfo(target_tid_,&gui)&&!gui.hwndCapture&&!gui.hwndMoveSize&&!gui.hwndMenuOwner&&
                !(gui.flags&(GUI_INMOVESIZE|GUI_INMENUMODE|GUI_POPUPMENUMODE));
        };
        try{
            if(explorer_case_||magnet_||!minimum_completed_||!last_generation_||generation_!=last_generation_||
                !status()||!admission()||!target_exact()||!no_buttons())throw std::runtime_error(reason);
            const product::guest::UpEvidenceIdentity previous{product_pid_,source_,last_generation_,up_qpc_};
            if(!product::guest::matching_clean_captured_up(row(0,"gesture_input_resource",last_generation_,"legacy_left_up"),
                row(0,"gesture_input_resource",last_generation_,"isolation_gone"),previous))
                throw std::runtime_error("actual_prior_NormalUp_release_detach_gone_UNKNOWN");
            original=visible(target_);dpi=GetDpiForWindow(target_);monitor=MonitorFromWindow(target_,MONITOR_DEFAULTTONULL);
            if(!original||!dpi||dpi!=minimum_dpi_||!monitor||!GetMonitorInfoW(monitor,&info)||!GetWindowRect(target_,&before)||
                before.bottom==std::numeric_limits<LONG>::max()||GetTickCount64()+30>=deadline_||!unchanged())
                throw std::runtime_error("single_proposal_fresh_geometry_context_or_budget_UNKNOWN");
            requested=before;++requested.bottom;corrected=requested;
            before_tick=GetTickCount64();before_clock=QueryPerformanceCounter(&before_qpc)!=FALSE;SetLastError(ERROR_SUCCESS);
            attempted=sizing_probe_attempted_=true;
            api=SendMessageTimeoutW(target_,WM_SIZING,WMSZ_BOTTOM,reinterpret_cast<LPARAM>(&corrected),
                SMTO_ABORTIFHUNG|SMTO_BLOCK|SMTO_ERRORONEXIT,30,&message_result);
            error=GetLastError();after_clock=QueryPerformanceCounter(&after_qpc)!=FALSE;after_tick=GetTickCount64();
            after_geometry_known=GetWindowRect(target_,&after)!=FALSE;after_visible=visible(target_);
            context=unchanged()&&after_geometry_known&&after.left==before.left&&after.top==before.top&&after.right==before.right&&after.bottom==before.bottom&&after_visible==original;
            const std::int64_t width=static_cast<std::int64_t>(corrected.right)-corrected.left;
            const std::int64_t height=static_cast<std::int64_t>(corrected.bottom)-corrected.top;
            const auto visible_bottom=static_cast<std::int64_t>(original->b)+corrected.bottom-before.bottom;
            const bool bottom_fits=visible_bottom>=std::numeric_limits<LONG>::min()&&visible_bottom<=std::numeric_limits<LONG>::max();
            const bool shape=corrected.left==requested.left&&corrected.top==requested.top&&corrected.right==requested.right&&corrected.bottom!=requested.bottom;
            const bool bounds=width>=minimum_native_.x&&height>=minimum_native_.y&&width<=maximum_native_.x&&height<=maximum_native_.y&&
                bottom_fits&&visible_bottom>original->t&&original->l>=info.rcWork.left&&original->t>=info.rcWork.top&&
                original->r<=info.rcWork.right&&visible_bottom<=info.rcWork.bottom&&static_cast<std::int64_t>(original->b)+1<=info.rcWork.bottom;
            eligible=api!=FALSE&&message_result==TRUE&&context&&shape&&bounds;
            reason=eligible?"one_changed_bottom_proposal_eligible_for_real_comparison":
                !api?"SMTO_failed_or_timeout_UNKNOWN":message_result!=TRUE?"message_not_processed_TRUE_UNKNOWN":
                !context?"actual_geometry_or_context_changed_UNKNOWN":!shape?"unchanged_or_non_bottom_proposal_UNKNOWN":"proposal_bounds_UNKNOWN";
        }catch(const std::exception& failure){reason=failure.what();}
        const auto native_rect=[](const RECT& r){return "["+std::to_string(r.left)+","+std::to_string(r.top)+","+
            std::to_string(r.right)+","+std::to_string(r.bottom)+"]";};
        out_.emit("medium_product_geometry_Terminal_WM_SIZING_proposal","\"result\":"+quote(eligible?"DISCRIMINATING_PROPOSAL":"UNKNOWN")+
            ",\"reason\":"+quote(reason)+",\"target_PID\":"+std::to_string(target_pid_)+",\"target_TID\":"+std::to_string(target_tid_)+
            ",\"caller_TID\":"+std::to_string(GetCurrentThreadId())+",\"target_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(target_))+
            ",\"prior_completed_generation\":"+std::to_string(last_generation_)+",\"WM_SIZING_attempted\":"+(attempted?"true":"false")+
            ",\"edge\":6,\"timeout_ms\":30,\"API_return\":"+(attempted?std::to_string(api):"null")+
            ",\"API_last_error\":"+(attempted?std::to_string(error):"null")+",\"last_error_is_failure_diagnostic_only\":true"+
            ",\"message_LRESULT\":"+(attempted&&api?std::to_string(message_result):"null")+
            ",\"R_native\":"+(attempted?native_rect(requested):"null")+",\"C_native\":"+(attempted&&api?native_rect(corrected):"null")+
            ",\"before_native\":"+(attempted?native_rect(before):"null")+",\"after_native\":"+(after_geometry_known?native_rect(after):"null")+
            ",\"before_visible\":"+(original?rect(*original):"null")+",\"actual_DPI\":"+std::to_string(dpi)+
            ",\"after_visible\":"+(after_visible?rect(*after_visible):"null")+
            ",\"prior_observed_min_native\":["+std::to_string(minimum_native_.x)+","+std::to_string(minimum_native_.y)+
            "],\"prior_observed_max_native\":["+std::to_string(maximum_native_.x)+","+std::to_string(maximum_native_.y)+"]"+
            ",\"before_tick\":"+(attempted?std::to_string(before_tick):"null")+",\"after_tick\":"+(attempted?std::to_string(after_tick):"null")+
            ",\"before_qpc\":"+(before_clock?std::to_string(before_qpc.QuadPart):"null")+",\"after_qpc\":"+(after_clock?std::to_string(after_qpc.QuadPart):"null")+
            ",\"post_proposal_exact_context_unchanged\":"+(attempted?(context?"true":"false"):"null")+
            ",\"read_only\":false,\"real_input\":false,\"application_grid_known\":false,\"global_size_legality_inferred\":false,\"retry_or_scan\":false");
        if(!eligible)return {};
        return TerminalSizingProposal{*original,{original->l,original->t,original->r,original->b+1},
            {original->l,original->t,original->r,static_cast<LONG>(static_cast<std::int64_t>(original->b)+corrected.bottom-before.bottom)},before,dpi,monitor};
    }
    // Read current-launch diagnostics, not a private writer or a substitute
    // for the exact geometry and matching real release checks below.
    std::string minimum_rejection(std::size_t mark,std::uint64_t input_qpc){
        (void)log();const auto suffix=evidence_.since(mark);if(!suffix)throw std::runtime_error("geometry_minimum_log_mark_not_current");
        std::istringstream rows{std::string(*suffix)};std::string line,rejected;
        while(std::getline(rows,line))if(line.find("\"type\":\"placement\"")!=line.npos&&number(line,"\"generation\":")==generation_){
            const auto claim=product::guest::up_evidence_detail::unique_value(line,"\"native_callsite_claimed\":");
            const auto attempted=product::guest::up_evidence_detail::unique_value(line,"\"native_attempted\":");
            if(claim!="false"||attempted!="false")return {};
            if(input_qpc&&number(line,"\"key\":")==source_key_&&number(line,"\"hook_observed_qpc\":")>=input_qpc&&
               line.find("\"native_completion_known\":true")!=line.npos&&
               (product::guest::up_evidence_detail::quoted(line,"\"reason\":","target_violates_size_constraints")||
                product::guest::up_evidence_detail::quoted(line,"\"reason\":","batch_preflight_target_violates_size_constraints")))rejected=line;
        }return rejected;
    }
    void one_gesture(int region,LONG dx,LONG dy,const char* phase,bool five=false,bool ctrl=false,bool minimum=false,
        const TerminalSizingProposal* sizing=nullptr){
        unsigned expected_region{};
        switch(region){case HTCAPTION:expected_region=0;break;case HTRIGHT:expected_region=2;break;
            case HTBOTTOM:expected_region=4;break;default:throw std::runtime_error("geometry_unsupported_test_nonclient_region");}
        if(!admission()||!refresh_target()||!admission()||!target_exact()||!status()||!no_buttons())throw std::runtime_error("geometry_fresh_idle_not_ready");
        const auto original=visible(target_);RECT positioning{};const auto original_dpi=GetDpiForWindow(target_);
        const auto original_monitor=MonitorFromWindow(target_,MONITOR_DEFAULTTONULL);
        if(!original||!GetWindowRect(target_,&positioning)||!original_dpi||!original_monitor)throw std::runtime_error("geometry_original_frame_unknown");
        const bool application_right=!explorer_case_&&region==HTRIGHT&&!five&&!ctrl&&!minimum&&!sizing;
        if(sizing&&(region!=HTBOTTOM||dx||dy!=1||five||ctrl||minimum||explorer_case_||magnet_||*original!=sizing->original||
            original_dpi!=sizing->dpi||original_monitor!=sizing->monitor||positioning.left!=sizing->positioning.left||
            positioning.top!=sizing->positioning.top||positioning.right!=sizing->positioning.right||positioning.bottom!=sizing->positioning.bottom))
            throw std::runtime_error("geometry_single_sizing_proposal_stale_before_real_DOWN");
        std::optional<Rect> minimum_expected;
        if(minimum){
            if(region!=HTBOTTOM||five||ctrl||dx)throw std::runtime_error("geometry_minimum_case_must_be_single_Terminal_bottom_edge");
            minimum_expected=terminal_minimum_resize(*original,positioning,original_dpi,dy);
        }
        const auto hit=hit_point(target_,region,nullptr,!explorer_case_&&region==HTCAPTION);if(!hit)throw std::runtime_error("geometry_actual_nonclient_target_not_found");
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
        if(ctrl){control_release_attempted_=false;control(true);
            if(!wait_for([&]{return control_state();},200))throw std::runtime_error("geometry_TEST_Ctrl_DOWN_not_observed");}
        send_mouse(MOUSEEVENTF_LEFTDOWN,anchor,false);
        if(!wait_for([&]{const auto pending=row(mark,"native_move_pending");if(pending.empty())return false;
            const auto id=number(pending,"\"generation\":");const auto expected="\"down_point\":["+std::to_string(anchor.x)+","+std::to_string(anchor.y)+"]";
            if(!id||id<=last_generation_||number(pending,"\"source\":")!=reinterpret_cast<std::uintptr_t>(target_)||
                number(pending,"\"source_pid\":")!=target_pid_||number(pending,"\"source_tid\":")!=target_tid_||
                !number(pending,"\"source_key\":")||number(pending,"\"down_hook_qpc\":")<down_qpc_||pending.find(expected)==pending.npos||
                number(pending,"\"actual_DOWN_region\":")!=expected_region||number(pending,"\"locked_modifiers\":")!=(ctrl?2u:0u))
                throw std::runtime_error("geometry_actual_DOWN_identity_generation_or_anchor_mismatch");
            generation_=id;source_key_=number(pending,"\"source_key\":");
            out_.emit("medium_product_geometry_actual_DOWN","\"phase\":"+quote(phase)+",\"receipt\":"+quote(pending));return true;},1000))
            throw std::runtime_error("geometry_actual_native_pending_DOWN_not_observed");
        // At most one real drag-threshold MOVE, only while native entry has
        // not progressed. A skipped/unissued kick still waits for the same
        // real START/END/capture gate below, with the original DOWN anchor.
        const LONG threshold=GetSystemMetrics(SM_CXDRAG);
        if(threshold<0||threshold>128)throw std::runtime_error("geometry_system_drag_metric_unknown");
        native_threshold_step_=true;cursor({anchor.x+threshold+1,anchor.y});native_threshold_step_=false;
        if(!wait_for([&]{if(retired())throw std::runtime_error("geometry_terminated_before_CAPTURE_READY");
            const auto started=row(mark,"native_move_start",generation_),ended=row(mark,"native_move_end",generation_),ready=row(mark,"gesture_start",generation_);
            const auto same_source=[&](const std::string& actual){return !actual.empty()&&
                number(actual,"\"source\":")==reinterpret_cast<std::uintptr_t>(target_)&&number(actual,"\"event_tid\":")==target_tid_&&
                number(actual,"\"original_DOWN_qpc\":")>=down_qpc_&&actual.find("\"native_physical_button_gate\":true")!=actual.npos;};
            return same_source(started)&&same_source(ended)&&number(started,"\"owner_sequence\":")&&
                number(ended,"\"owner_sequence\":")>number(started,"\"owner_sequence\":")&&
                number(ready,"\"key\":")==source_key_&&ready.find("\"native_first_handoff\":true")!=ready.npos&&
                ready.find(ctrl?"\"ctrl\":true":"\"ctrl\":false")!=ready.npos&&
                (!ctrl||(number(ready,"\"group_members\":")==2&&ready.find("\"original_DOWN_group_frozen\":true")!=ready.npos))&&capture_route()&&
                ready.find(region==HTCAPTION?"\"resize\":false":"\"resize\":true")!=ready.npos;},1500))
            throw std::runtime_error("geometry_real_START_END_capture_handoff_not_observed");
        continuation_ready_=true;
        struct Phase {const char* name;LONG x,y;Rect expected;};
        const auto pixels=[&](int dip){return static_cast<LONG>((static_cast<std::uint64_t>(dip)*original_dpi+48)/96);};
        std::vector<Phase> phases;
        const auto move=[&](LONG x,LONG y){return translated(*original,x,y);};
        if(five){
            if(!magnet_||region!=HTCAPTION||ctrl)throw std::runtime_error("geometry_five_phase_target_missing");
            const LONG travel=pixels(40),entry=pixels(10),release=pixels(16),approach_offset=(std::max)(1L,entry/2),hold=(entry+release)/2;
            if(entry<=0||hold<=entry||hold>=release||magnet_->initial.l-original->r!=travel||magnet_->initial.t-original->t!=travel)
                throw std::runtime_error("geometry_independent_magnet_oracle_or_DPI_invalid");
            phases={{"Free",-travel,-travel,move(-travel,-travel)},
                {"XY_Snap",travel-approach_offset,travel+approach_offset,move(travel,travel)},
                {"Hold_outside_entry_inside_release",travel-hold,travel+hold,move(travel,travel)},
                {"Detach",-travel,-travel,move(-travel,-travel)},
                {"Resnap",travel-approach_offset+1,travel+approach_offset,move(travel,travel)}};
        }else phases.push_back({phase,dx,dy,minimum_expected?*minimum_expected:
            region==HTCAPTION?move(dx,dy):Rect{original->l,original->t,original->r+dx,original->b+dy}});
        const auto neighbour_before=magnet_?visible(magnet_->window()):std::optional<Rect>{};
        if(magnet_&&(!neighbour_before||(!ctrl&&*neighbour_before!=magnet_->initial)))
            throw std::runtime_error("geometry_neighbour_before_input_not_exact");
        // The real handoff can move the cursor by the native threshold. Pace
        // from that actual point, but never change the original DOWN anchor.
        if(!GetCursorPos(&last_sample_))throw std::runtime_error("geometry_actual_pacing_origin_unknown");
        last_sample_tick_=GetTickCount64();pacing_dpi_=original_dpi;
        Rect expected{};bool minimum_declined{};std::string minimum_decline_receipt,application_callback;std::size_t minimum_placement_mark{};std::uint64_t minimum_input_qpc{};
        for(const auto& step:phases){
            expected=step.expected;const POINT next{anchor.x+step.x,anchor.y+step.y};
            if(five||ctrl)pace(next);
            const auto placement_mark=log().size();if(minimum)minimum_placement_mark=placement_mark;
            cursor(next);last_sample_=next;last_sample_tick_=GetTickCount64();std::string placement,neighbour_receipt;
            if(minimum)minimum_input_qpc=last_mouse_qpc_;
            const auto expected_neighbour=ctrl&&neighbour_before?std::optional<Rect>{translated(*neighbour_before,step.x,step.y)}:neighbour_before;
            if(!wait_for([&]{
                if(minimum){
                    minimum_decline_receipt=minimum_rejection(placement_mark,minimum_input_qpc);
                    if(!minimum_decline_receipt.empty()&&visible(target_)==original&&target_exact()){
                        minimum_declined=true;expected=*original;return true;
                    }
                }
                if(retired())throw std::runtime_error("geometry_terminated_before_target_completion");
                if(sizing){
                    // Do not reject the raw R in favour of C while the button
                    // is held. First preserve the actual exact production
                    // completion and real UP; compare preference afterwards.
                    const auto observed=visible(target_);
                    if(!observed||observed->width()<=0||observed->height()<=0||observed->l!=original->l||
                        observed->t!=original->t||observed->r!=original->r)return false;
                    placement=phase_placement(placement_mark,*observed,source_key_);
                    if(placement.empty())return false;
                    expected=*observed;
                }else if(application_right){
                    const auto observed=visible(target_);if(!observed)return false;
                    placement=application_right_placement(placement_mark,step.expected,*original,positioning,*observed,application_callback);
                    if(placement.empty())return false;expected=*observed;
                }else placement=(five||ctrl||minimum)?phase_placement(placement_mark,expected,source_key_):row(placement_mark,"placement",generation_);
                if(ctrl&&expected_neighbour)neighbour_receipt=phase_placement(placement_mark,*expected_neighbour,0,source_key_);
                // Ordinary product omits successful per-call diagnostics.
                // Real source AND neighbour geometry are the black-box oracle.
                return (!(five||ctrl||minimum||sizing||application_right)||!placement.empty())&&(!ctrl||!neighbour_receipt.empty())&&visible(target_)==expected&&capture_route()&&(!magnet_||
                    (magnet_->exact()&&visible(magnet_->window())==expected_neighbour));},1200))
                throw std::runtime_error("geometry_actual_native_exact_source_or_neighbour_not_observed");
            RECT after{},neighbour_position{};const auto actual_visible=visible(target_);
            if(!GetWindowRect(target_,&after)||!actual_visible||*actual_visible!=expected||GetDpiForWindow(target_)!=original_dpi||
                MonitorFromWindow(target_,MONITOR_DEFAULTTONULL)!=original_monitor||
                (magnet_&&!GetWindowRect(magnet_->window(),&neighbour_position)))throw std::runtime_error("geometry_positioning_monitor_DPI_postverify_unknown");
            const Rect original_positioning{positioning.left,positioning.top,positioning.right,positioning.bottom};
            const Rect expected_positioning=region==HTCAPTION?translated(original_positioning,expected.l-original->l,expected.t-original->t):
                Rect{positioning.left,positioning.top,positioning.right+expected.r-original->r,positioning.bottom+expected.b-original->b};
            if(Rect{after.left,after.top,after.right,after.bottom}!=expected_positioning)
                throw std::runtime_error("geometry_source_positioning_postverify_not_exact");
            if(magnet_&&magnet_->positioning){const auto wanted=ctrl?translated(*magnet_->positioning,step.x,step.y):*magnet_->positioning;
                if(Rect{neighbour_position.left,neighbour_position.top,neighbour_position.right,neighbour_position.bottom}!=wanted)
                    throw std::runtime_error("geometry_neighbour_positioning_postverify_not_exact");}
            const auto per_call=product::guest::up_evidence_detail::unique_value(placement,"\"native_attempted\":");
            const Rect raw_intent=region==HTCAPTION?move(step.x,step.y):
                Rect{original->l,original->t,original->r+step.x,original->b+step.y};
            out_.emit("medium_product_geometry_observed_geometry","\"phase\":"+quote(step.name)+",\"generation\":"+std::to_string(generation_)+
                ",\"original_visible\":"+rect(*original)+",\"raw_intent_from_original_DOWN\":"+rect(raw_intent)+
                ",\"independent_expected_visible\":"+rect(sizing?sizing->corrected:application_right?step.expected:expected)+",\"actual_visible\":"+rect(*actual_visible)+
                ",\"expected_kind\":"+quote(application_right?"actual_application_callback_not_product_solver":"independent_TEST_geometry")+
                ",\"actual_application_callback\":"+(application_callback.empty()?"null":quote(application_callback))+
                ",\"actual_positioning\":"+rect({after.left,after.top,after.right,after.bottom})+",\"actual_DPI\":"+std::to_string(original_dpi)+
                ",\"actual_neighbour_visible\":"+(magnet_?rect(*visible(magnet_->window())):"null")+
                ",\"Ctrl_START_locked\":"+(ctrl?"true":"false")+",\"product_solver_used_for_expected\":false,\"TEST_one_MOVE_per_phase\":true"+
                ",\"phase_consumption_from_fresh_placement_including_noop\":"+(minimum_declined?"false":(five||ctrl||minimum||sizing||application_right)?"true":"null")+
                ",\"diagnostic_mode_receipt_required\":"+((five||ctrl||minimum||sizing||application_right)?"true":"false")+
                ",\"single_WM_SIZING_preference_not_checked_until_real_UP\":"+(sizing?"true":"false")+
                ",\"minimum_constraint_declined_before_native\":"+(minimum_declined?"true":"false")+
                ",\"constraint_decline_receipt\":"+(minimum_decline_receipt.empty()?"null":quote(minimum_decline_receipt))+
                ",\"phase_SendInput_before_qpc\":"+std::to_string(last_mouse_qpc_)+
                ",\"neighbour_production_receipt\":"+(neighbour_receipt.empty()?"null":quote(neighbour_receipt))+
                ",\"native_attempt_from_per_call_receipt\":"+(per_call&&(*per_call=="true"||*per_call=="false")?std::string(*per_call):"null")+
                ",\"production_per_call_receipt\":"+(placement.empty()?"null":quote(placement))+",\"missing_diagnostic_is_UNKNOWN\":true");
        }
        const auto up_mark=log().size();send_mouse(MOUSEEVENTF_LEFTUP,{},false,true,minimum_declined);std::string ended,legacy,gone,raw;bool post_release_status_observed{};
        if(minimum_declined){
            std::string released;
            if(!wait_for([&]{ended=row(mark,"gesture_end",generation_);gone=row(mark,"gesture_input_resource",generation_,"isolation_gone");
                released=row(mark,"gesture_input_resource",generation_,"capture_released");
                return !ended.empty()&&!gone.empty();},1200)||!status())
                throw std::runtime_error("geometry_minimum_rejection_retirement_or_idle_unknown");
            const product::guest::UpEvidenceIdentity identity{product_pid_,source_,generation_,up_qpc_};
            if(!product::guest::matching_abort_cleanup_up(last_status_,ended,gone,identity,released)||visible(target_)!=original)
                throw std::runtime_error("geometry_minimum_rejection_actual_abort_UP_or_unchanged_geometry_unknown");
            post_release_status_observed=true;
            out_.emit("medium_product_geometry_constraint_abort_UP","\"phase\":"+quote(phase)+",\"generation\":"+std::to_string(generation_)+
                ",\"single_cleanup_UP_send_before_qpc\":"+std::to_string(up_qpc_)+",\"actual_same_generation_LL_UP_and_abort_cleanup\":true"+
                ",\"end_receipt\":"+quote(ended)+",\"resources_receipt\":"+quote(gone)+",\"capture_release_receipt\":"+quote(released)+
                ",\"actual_status\":"+quote(last_status_)+",\"NormalUp_claimed\":false,\"legacy_target_UP_delivery_claimed\":false");
        }else if(!wait_for([&]{ended=row(up_mark,"gesture_end",generation_);legacy=row(up_mark,"gesture_input_resource",generation_,"legacy_left_up");
            gone=row(up_mark,"gesture_input_resource",generation_,"isolation_gone");raw=row(up_mark,"gesture_input_resource",generation_,"raw_mouse");
            const product::guest::UpEvidenceIdentity expected_up{product_pid_,source_,generation_,up_qpc_};
            return ended.find("\"normal_up\":true")!=ended.npos&&ended.find("\"end\":0")!=ended.npos&&
                product::guest::matching_clean_captured_up(legacy,gone,expected_up)&&
                (number(raw,"\"raw_button_flags\":")&RI_MOUSE_LEFT_BUTTON_UP)&&number(raw,"\"resource_observed_qpc\":")>=up_qpc_;
        },1200))throw std::runtime_error("geometry_actual_Raw_legacy_NormalUp_or_cleanup_not_observed");
        if(ctrl){control(false);if(!wait_for([&]{return control_state();},200))throw std::runtime_error("geometry_TEST_Ctrl_UP_not_observed");}
        GUITHREADINFO gui{sizeof(gui)};if(!target_exact()||!GetGUIThreadInfo(target_tid_,&gui)||gui.hwndCapture||gui.hwndMoveSize||gui.hwndMenuOwner||
            visible(target_)!=expected||(!post_release_status_observed&&!status()))throw std::runtime_error("geometry_terminal_context_or_idle_not_confirmed");
        if(!minimum_declined)out_.emit("medium_product_geometry_NormalUp","\"phase\":"+quote(phase)+",\"generation\":"+std::to_string(generation_)+
            ",\"single_UP_send_before_qpc\":"+std::to_string(up_qpc_)+",\"actual_visible\":"+rect(expected)+
            ",\"actual_source_capture\":0,\"end_receipt\":"+quote(ended)+",\"Raw_receipt\":"+quote(raw)+
            ",\"legacy_receipt\":"+quote(legacy)+",\"resources_receipt\":"+quote(gone)+",\"global_zero_late_history_claimed\":false");
        out_.emit("medium_product_geometry_status_delta","\"phase\":"+quote(phase)+",\"generation\":"+std::to_string(generation_)+
            ",\"before_actual_status\":"+quote(before_status)+",\"after_actual_status\":"+quote(last_status_)+
            ",\"aggregate_counter_is_not_per_call_API_receipt\":true");
        if(minimum){
            const auto before_claims=product::guest::up_evidence_detail::number(before_status,"\"native_callsite_claims\":");
            const auto after_claims=product::guest::up_evidence_detail::number(last_status_,"\"native_callsite_claims\":");
            const auto suffix=evidence_.since(minimum_placement_mark);if(!suffix)throw std::runtime_error("geometry_minimum_final_log_mark_unknown");
            std::istringstream rows{std::string(*suffix)};std::string actual;unsigned calls{};bool native_unknown{};
            while(std::getline(rows,actual))if(actual.find("\"type\":\"placement\"")!=actual.npos&&number(actual,"\"generation\":")==generation_){
                const auto claimed=product::guest::up_evidence_detail::unique_value(actual,"\"native_callsite_claimed\":");
                const auto attempted=product::guest::up_evidence_detail::unique_value(actual,"\"native_attempted\":");
                if(claimed=="true"){++calls;if(attempted!="true"||actual.find("\"native_completion_known\":true")==actual.npos)native_unknown=true;}
                else if(claimed!="false"||attempted!="false")native_unknown=true;
            }
            const unsigned wanted=minimum_declined?0u:1u;
            if(!before_claims||!after_claims||*after_claims<*before_claims||*after_claims-*before_claims!=wanted||calls!=wanted||native_unknown||
               (minimum_declined&&minimum_rejection(minimum_placement_mark,minimum_input_qpc).empty()))
                throw std::runtime_error("geometry_minimum_actual_native_count_or_no_recursion_unknown");
            out_.emit("medium_product_geometry_Terminal_minimum_result","\"generation\":"+std::to_string(generation_)+
                ",\"outcome\":"+quote(minimum_declined?"explicit_constraint_rejection_zero_native_unchanged_then_actual_abort_UP":
                    "independent_declared_minimum_clamped_exact_then_real_NormalUp")+
                ",\"actual_visible\":"+rect(expected)+",\"actual_native_callsite_claims_delta\":"+std::to_string(wanted)+
                ",\"actual_current_generation_placement_native_claims\":"+std::to_string(calls)+
                ",\"recursive_placement_observed\":false,\"application_grid_known\":false,\"whole_M03_F03_PASS\":false");
        }
        if(application_right)out_.emit("medium_product_geometry_actual_application_resize_after_NormalUp","\"generation\":"+std::to_string(generation_)+
            ",\"independent_R_visible\":"+rect({original->l,original->t,original->r+dx,original->b+dy})+",\"actual_C_visible\":"+rect(expected)+
            ",\"application_changed_R\":"+(expected!=Rect{original->l,original->t,original->r+dx,original->b+dy}?"true":"false")+
            ",\"expected_kind\":\"actual_application_callback_not_product_solver\",\"actual_callback_receipt\":"+quote(application_callback)+
            ",\"actual_Raw_legacy_NormalUp_and_release_detach_gone\":true,\"global_application_grid_known\":false");
        if(sizing){
            const bool comparable=expected==sizing->requested||expected==sizing->corrected;
            sizing_comparison_completed_=true;
            if(comparable)sizing_preference_matched_=expected==sizing->corrected;
            out_.emit("medium_product_geometry_Terminal_WM_SIZING_comparison","\"generation\":"+std::to_string(generation_)+
                ",\"R_visible\":"+rect(sizing->requested)+",\"C_visible\":"+rect(sizing->corrected)+",\"actual_visible\":"+rect(expected)+
                ",\"actual_matches_R\":"+(expected==sizing->requested?"true":"false")+",\"actual_matches_C\":"+(expected==sizing->corrected?"true":"false")+
                ",\"one_point_preference_result\":"+quote(comparable?(expected==sizing->corrected?"MATCH":"MISMATCH"):"UNKNOWN_OTHER_EXACT_RESULT")+
                ",\"actual_Raw_legacy_NormalUp_and_release_detach_gone\":true,\"actual_idle_status_confirmed\":true"+
                ",\"application_grid_known\":false,\"programmatic_size_illegal_inferred\":false,\"whole_M03_F03_FAIL_inferred\":false");
        }
        last_generation_=generation_;down_inserted_=false;
    }
    bool no_buttons()const{for(int key:{VK_LBUTTON,VK_RBUTTON,VK_MBUTTON,VK_XBUTTON1,VK_XBUTTON2,VK_CONTROL,VK_SHIFT,VK_MENU,VK_LWIN,VK_RWIN,VK_RETURN,VK_ESCAPE})
        if((GetAsyncKeyState(key)&0x8000)&&!(key==VK_CONTROL&&control_inserted_&&control_state()))return false;return true;}
    void prepare_magnet_target(){
        if(explorer_case_||!admission()||!target_exact()||!no_buttons()||GetForegroundWindow()!=target_)
            throw std::runtime_error("geometry_magnet_fresh_Terminal_context_required");
        const auto original=visible(target_);const UINT dpi=GetDpiForWindow(target_);
        const HMONITOR monitor=MonitorFromWindow(target_,MONITOR_DEFAULTTONULL);MONITORINFO info{sizeof(info)};
        if(!original||!dpi||dpi>384||!monitor||!GetMonitorInfoW(monitor,&info))throw std::runtime_error("geometry_magnet_actual_display_unknown");
        const Rect work{info.rcWork.left,info.rcWork.top,info.rcWork.right,info.rcWork.bottom};
        const auto pixels=[&](int dip){return static_cast<LONG>((static_cast<std::uint64_t>(dip)*dpi+48)/96);};
        const LONG travel=pixels(40),padding=pixels(32);const Rect neighbour{original->r+travel,original->t+travel,
            original->r+travel+pixels(200),original->t+travel+pixels(220)};
        const auto fits=[&](Rect r){return r.width()>0&&r.height()>0&&r.l>=work.l+padding&&r.t>=work.t+padding&&
            r.r<=work.r-padding&&r.b<=work.b-padding;};
        if(!fits(*original)||!fits(translated(*original,-travel,-travel))||!fits(translated(*original,travel,travel))||
           !fits(neighbour)||!fits(translated(neighbour,-pixels(24),pixels(24)))||
           !fits(translated(*original,travel-pixels(24),travel+pixels(24))))
            throw std::runtime_error("geometry_magnet_and_Ctrl_independent_layout_does_not_fit_actual_work_area");
        const auto config_file=product_.parent_path()/L"settings.ini";SetLastError(ERROR_SUCCESS);
        const auto attributes=GetFileAttributesW(config_file.c_str());const auto file_error=attributes==INVALID_FILE_ATTRIBUTES?GetLastError():0;
        const bool absent=attributes==INVALID_FILE_ATTRIBUTES&&file_error==ERROR_FILE_NOT_FOUND;
        auto configuration=v1::default_settings();std::string settings_hash;
        if(!absent){
            if(attributes==INVALID_FILE_ATTRIBUTES||(attributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)))
                throw std::runtime_error("geometry_magnet_portable_settings_identity_unknown");
            const auto bytes=read(config_file);if(bytes.empty()||bytes.size()>65536)throw std::runtime_error("geometry_magnet_settings_size_invalid");
            const auto parsed=v1::parse(bytes);if(!parsed.value)throw std::runtime_error("geometry_magnet_settings_invalid");
            configuration=*parsed.value;settings_hash=hash(config_file);
        }
        if(!configuration.enabled||!configuration.magnet_move||!configuration.glue||!configuration.rules.empty()||
           configuration.attraction_dip!=10||configuration.release_dip!=16||configuration.speed_dip!=2000||configuration.adjacency_dip!=2)
            throw std::runtime_error("geometry_magnet_requires_actual_default_10_16_2000_2_settings");
        out_.emit("medium_product_geometry_independent_magnet_oracle","\"source_visible\":"+rect(*original)+
            ",\"work_area\":"+rect(work)+",\"actual_DPI\":"+std::to_string(dpi)+",\"attraction_DIP\":10,\"release_DIP\":16,\"adjacency_DIP\":2"+
            ",\"attraction_pixels\":"+std::to_string(pixels(10))+",\"release_pixels\":"+std::to_string(pixels(16))+
            ",\"travel_pixels\":"+std::to_string(travel)+",\"neighbour_visible\":"+rect(neighbour)+
            ",\"expected_XY_snap\":"+rect(translated(*original,travel,travel))+
            ",\"settings_basis\":"+quote(absent?"portable_settings_file_absent_actual_product_default":"actual_portable_settings_file_parsed")+
            ",\"settings_sha256\":"+(settings_hash.empty()?"null":quote(settings_hash))+
            ",\"product_solver_called_for_expected\":false,\"fixture_SetWindowPos_controls_only_new_blank_target\":true");
        magnet_=std::make_unique<MediumDesktopMagnetTarget>();
        magnet_->create(run_,desktop_,session_,target_,neighbour,dpi,monitor,work,out_);
    }
    void cleanup_target()noexcept{
        try{
            try{if(down_inserted_&&!up_attempted_){
                    if(up_target(true))send_mouse(MOUSEEVENTF_LEFTUP,{},false,true,true);
                    else out_.cleanup_known=false;
                }}catch(...){out_.cleanup_known=false;}
            if(down_inserted_&&!up_inserted_)out_.cleanup_known=false;
            if(control_inserted_&&!control_release_attempted_){
                try{control(false,true);}catch(...){out_.cleanup_known=false;}
            }
            if(control_inserted_||!control_release_observed_)out_.cleanup_known=false;
            if(target_&&IsWindow(target_)){
                target_closed_=false;
                // Public ordinary close only after our known test pair is no
                // longer held. Never terminate or retry an unverified window.
                if((!down_inserted_||up_inserted_)&&!control_inserted_&&control_release_observed_&&refresh_target()&&target_exact(false)){
                    SetLastError(ERROR_SUCCESS);const bool sent=PostMessageW(target_,WM_CLOSE,0,0)!=FALSE;const auto error=sent?0:GetLastError();
                    target_closed_=sent&&wait_for([&]{return !IsWindow(target_);},1000,false);
                    const auto process_exit=medium_observe_held_process_exit(target_process_,target_pid_,target_birth_,deadline_,quit_seen_,sent?1000:0);
                    const auto process_exit_json=process_exit.json();
                    out_.emit("medium_product_geometry_target_process_exit",process_exit_json.substr(1,process_exit_json.size()-2));
                    DWORD code=STILL_ACTIVE;const bool known=target_process_&&GetExitCodeProcess(target_process_,&code);
                    out_.emit("medium_product_geometry_target_close","\"one_WM_CLOSE_attempted\":true,\"PostMessage_return\":"+std::string(sent?"true":"false")+
                        ",\"error\":"+std::to_string(error)+",\"exact_root_gone\":"+(target_closed_?"true":"false")+
                        ",\"process_wait\":"+std::to_string(WaitForSingleObject(target_process_,0))+",\"exit_code\":"+(known?std::to_string(code):"null")+
                        ",\"bounded_exact_process_signalled\":"+(process_exit.signalled?"true":"false")+
                        ",\"bounded_exact_process_normal_exit\":"+(process_exit.signalled&&process_exit.exit_known?(process_exit.exit_code==0?"true":"false"):"null")+
                        ",\"samples_not_atomic_or_all_descendant_exit\":true,\"forced_termination\":false");
                }
            }
        }catch(...){out_.cleanup_known=false;}
        out_.cleanup_known=out_.cleanup_known&&!quit_seen_&&target_closed_&&(!lease_||!lease_->incomplete_pair.load());
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
        explorer_case_=manifest_explorer(manifest);
        auto at=asset==manifest.npos?manifest.npos:manifest.find(':',asset);
        if(at!=manifest.npos){do{++at;}while(at<manifest.size()&&(manifest[at]==' '||manifest[at]=='\t'||manifest[at]=='\r'||manifest[at]=='\n'));}
        const bool terminal_asset=at!=manifest.npos&&at<manifest.size()&&manifest[at]=='{';
        if(!explorer_case_&&!terminal_asset)return;out_.requested=true;
        try{
            if(explorer_case_&&terminal_asset)throw std::runtime_error("geometry_manifest_Explorer_and_Terminal_mutually_exclusive");
            const auto files=manifest.find("\"files\"");
            if(files==manifest.npos)throw std::runtime_error("geometry_manifest_identity_missing");const std::string_view identity(manifest.data(),files);
            const std::string run(run_.begin(),run_.end());fs::path root;
            if(manifest_string(identity,"run_id")!=run||manifest_string(identity,"product_source_commit")!=source_||
               manifest_string(manifest,"guest_suite")!="MediumDesktopUiOnly"||
               hash(product_)!=product_hash_||!ProcessIdToSessionId(GetCurrentProcessId(),&session_)||!session_||!admission()||!no_buttons())
                throw std::runtime_error("geometry_exact_manifest_asset_or_actual_Run_product_not_verified");
            if(explorer_case_){target_class_=L"CabinetWClass";explorer_=std::make_unique<MediumExplorerGeometryTarget>(run_,desktop_,session_,out_);}
            else{
                const auto finish=manifest.find('}',at);if(finish==manifest.npos||manifest.find("\"terminal_test_asset\"",asset+21)!=manifest.npos)
                    throw std::runtime_error("geometry_manifest_asset_ambiguous");
                const std::string_view spec(manifest.data()+at,finish-at+1);
                const auto configuration=equal(product_.parent_path().filename().wstring(),run_+L"-product-Debug")?L"Debug":L"Release";
                root=fs::path(L"C:\\PaneBindV1\\Terminal")/run_/configuration/L"terminal-1.25.2733.0";target_image_=root/L"WindowsTerminal.exe";
                constexpr const char* terminal_hash="1CBE52914F7437A4F064327383F1B4C45FE2EB8E92D4F8DAE0AD7EFCB13A20A1";
                if(manifest_string(spec,"kind")!="WindowsTerminalPortable"||manifest_string(spec,"version")!="1.25.2733.0"||
                   manifest_string(spec,"relative_path")!="Terminal-test.zip"||manifest_string(spec,"exe_sha256")!=terminal_hash||
                   hash(L"C:\\PaneBindV1\\Input\\Terminal-test.zip")!="BF3EF2012F6C44D8340A4C58125ACC9498D19B580F9890DC043CDF831852E796"||
                   !plain_directory(root)||!marker(root/L"panebind-v1-test-application.marker",run_)||hash(target_image_)!=terminal_hash)
                    throw std::runtime_error("geometry_exact_manifest_Terminal_asset_not_verified");
            }
            evidence_.begin(product_pid_,product_birth_,launch_tick_,source_);
            events_.changed=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!events_.changed)throw std::runtime_error("geometry_own_change_event_failed");
            const auto route=row(0,"input_route"),context=row(0,"desktop_context");
            if(route.find("\"capture_resource_route_enabled\":true")==route.npos||context.find("\"route\":\"exact_manifest_Run\"")==context.npos||
               context.find("\"run_id\":"+quote(run))==context.npos||!status())throw std::runtime_error("geometry_actual_product_Run_route_or_health_not_observed");
            out_.emit("medium_product_geometry_scope","\"run_id\":"+quote(run)+",\"manifest_sha256\":"+quote(hash(L"C:\\PaneBindV1\\Input\\manifest.json"))+
                ",\"product_PID\":"+std::to_string(product_pid_)+",\"product_birth\":"+std::to_string(product_birth_)+
                ",\"actual_route_receipt\":"+quote(route)+",\"actual_context_receipt\":"+quote(context)+
                ",\"target_kind\":"+quote(target_kind())+",\"medium_explorer_geometry\":"+(explorer_case_?"true":"false")+
                ",\"product_absent_client_control_not_reused\":true,\"Terminal_content_ready_inferred\":false");
            const auto baseline=MediumDesktopWindows::read(desktop_);if(!baseline.complete)throw std::runtime_error("geometry_target_baseline_unknown");
            if(medium_terminal_native_events)throw std::runtime_error("geometry_native_observer_already_owned");
            medium_terminal_native_events=&events_;hook_=SetWinEventHook(EVENT_OBJECT_CREATE,EVENT_OBJECT_LOCATIONCHANGE,nullptr,
                explorer_case_?medium_explorer_geometry_event:medium_terminal_native_event,0,0,WINEVENT_OUTOFCONTEXT);
            if(!hook_)throw std::runtime_error("geometry_native_observer_unavailable");
            log_change_=FindFirstChangeNotificationW(product_.parent_path().c_str(),FALSE,FILE_NOTIFY_CHANGE_FILE_NAME|FILE_NOTIFY_CHANGE_SIZE|FILE_NOTIFY_CHANGE_LAST_WRITE);
            if(log_change_==INVALID_HANDLE_VALUE)throw std::runtime_error("geometry_product_log_change_notification_failed");
            if(explorer_){
                explorer_->prepare(baseline);target_image_=explorer_->image();
                if(!admission())throw std::runtime_error("Explorer_geometry_Run_revoked_before_launch");
                explorer_->launch();
                if(!wait_for([&]{
                    if(target_)return target_exact();
                    if(events_.overflow)throw std::runtime_error("Explorer_geometry_bounded_event_buffer_overflow");
                    if(!explorer_->try_bind())return false;
                    target_=explorer_->window();target_pid_=explorer_->pid();target_tid_=explorer_->tid();target_birth_=explorer_->birth();
                    target_process_=explorer_->take_bound_process();events_.pid=target_pid_;events_.root=target_;target_closed_=false;
                    return target_exact();},3500))throw std::runtime_error("geometry_new_exact_Run_Explorer_root_not_observed");
            }else{
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
            }
            IVirtualDesktopManager* manager{};const HRESULT made=CoCreateInstance(CLSID_VirtualDesktopManager,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&manager));
            BOOL current{};const HRESULT queried=manager?manager->IsWindowOnCurrentVirtualDesktop(target_,&current):E_UNEXPECTED;if(manager)manager->Release();
            GUITHREADINFO gui{sizeof(gui)};const bool gui_known=GetGUIThreadInfo(target_tid_,&gui)!=FALSE;
            out_.emit("medium_product_geometry_target","\"actual\":"+MediumDesktopWindowFact::read(target_,target_pid_,target_process_,target_birth_,L"PaneBindV1_"+run_).json()+
                ",\"target_kind\":"+quote(target_kind())+
                ",\"actual_Medium\":"+(medium(target_process_)?"true":"false")+",\"actual_same_SID\":"+(input_actor_same_user(target_process_)?"true":"false")+
                ",\"VDM_create_hresult\":"+std::to_string(static_cast<long>(made))+",\"VDM_query_hresult\":"+std::to_string(static_cast<long>(queried))+
                ",\"VDM_current\":"+(current?"true":"false")+",\"actual_foreground\":"+std::to_string(reinterpret_cast<std::uintptr_t>(GetForegroundWindow()))+
                ",\"owner_GTI_known\":"+(gui_known?"true":"false")+",\"foreground_reclaimed\":false,\"client_semantics_inferred\":false");
            if(made!=S_OK||queried!=S_OK||!current||!gui_known||gui.hwndCapture||gui.hwndMoveSize||gui.hwndMenuOwner||GetForegroundWindow()!=target_)
                throw std::runtime_error("geometry_actual_VDM_foreground_or_source_GUI_not_ready");
            one_gesture(HTCAPTION,72,36,"Move1");one_gesture(HTRIGHT,64,0,"Resize");one_gesture(HTCAPTION,-36,24,"SuccessorMove");
            if(!explorer_case_){
                prepare_magnet_target();one_gesture(HTCAPTION,0,0,"MagnetFivePhase",true);magnet_completed_=true;out_.magnet_completed=true;
                const auto source=visible(target_),other=visible(magnet_->window());const UINT dpi=GetDpiForWindow(target_);
                if(!source||!other||source->r!=other->l||source->t!=other->t||source->b<=other->t||!magnet_->exact())
                    throw std::runtime_error("geometry_Ctrl_actual_adjacency_not_observed");
                const LONG step=static_cast<LONG>((static_cast<std::uint64_t>(24)*dpi+48)/96);
                out_.emit("medium_product_geometry_Ctrl_adjacency","\"source_actual\":"+rect(*source)+",\"neighbour_actual\":"+rect(*other)+
                    ",\"shared_edge_exact\":true,\"test_controlled_members\":2,\"group_motion_expected_delta\":["+std::to_string(-step)+","+std::to_string(step)+"]");
                one_gesture(HTCAPTION,-step,step,"CtrlAdjacentMove",false,true);ctrl_completed_=true;out_.ctrl_completed=true;
                const bool blank_closed=magnet_->finish();out_.cleanup_known=out_.cleanup_known&&blank_closed;
                out_.emit("medium_product_geometry_blank_target_cleanup","\"phase\":\"before_real_Terminal_minimum_case\",\"owner_DestroyWindow_and_UnregisterClass_confirmed\":"+
                    std::string(blank_closed?"true":"false")+",\"foreign_target_closed\":false");
                if(!blank_closed)throw std::runtime_error("geometry_blank_target_cleanup_before_minimum_UNKNOWN");
                magnet_.reset();minimum_chain_active_=true;
                one_gesture(HTBOTTOM,0,0,"DeclaredMinimumResize",false,false,true);
                one_gesture(HTCAPTION,24,18,"AfterMinimumMove");minimum_completed_=true;minimum_chain_active_=false;
                if(const auto proposal=terminal_sizing_proposal()){
                    minimum_chain_active_=true;
                    one_gesture(HTBOTTOM,0,1,"SingleSizingProposalResize",false,false,false,&*proposal);
                    one_gesture(HTCAPTION,24,18,"AfterSingleSizingProposalMove");sizing_successor_completed_=true;minimum_chain_active_=false;
                }
            }
            out_.completed=true;out_.reason=explorer_case_?"one_new_Explorer_Move_UP_Resize_UP_successor_Move_UP_only":
                "one_new_Terminal_geometry_magnet_Ctrl_declared_minimum_and_successor_Move_only";
        }catch(const std::exception& error){out_.reason=error.what();out_.completed=false;
            // An abort release/closing the source is not NormalUp or proof
            // that a new Ctrl/five-phase writer's resources have retired.
            if((magnet_||minimum_chain_active_)&&(down_inserted_||control_inserted_||!control_release_observed_))out_.cleanup_known=false;}
        catch(...){out_.reason="geometry_unexpected_exception";out_.completed=false;out_.cleanup_known=false;}
        if(explorer_&&!out_.completed)explorer_->record_unbound_discovery(out_.reason,
            lease_&&!(lease_->flags.load(std::memory_order_acquire)&MediumDesktopUiInputLease::in_flight));
        cleanup_target();
        if(magnet_){const bool stopped=magnet_->finish();out_.cleanup_known=out_.cleanup_known&&stopped;
            out_.emit("medium_product_geometry_blank_target_cleanup","\"owner_DestroyWindow_and_UnregisterClass_confirmed\":"+std::string(stopped?"true":"false")+
                ",\"foreign_target_closed\":false,\"test_Ctrl_pair_still_held\":"+(control_inserted_?"true":"false"));}
        if(explorer_){explorer_->finish();const bool known=explorer_->cleanup_known();out_.cleanup_known=out_.cleanup_known&&known;
            out_.emit("medium_product_Explorer_adapter_cleanup","\"observer_and_launcher_handles_cleanup_known\":"+std::string(known?"true":"false")+
                ",\"observer_retire_attempted\":"+(explorer_->retire_attempted()?"true":"false")+
                ",\"observer_retire_hresult\":"+(explorer_->retire_attempted()?std::to_string(static_cast<long>(explorer_->retire_hresult())):"null")+
                ",\"unconfirmed_created_roots_closed\":false,\"test_directory_retained\":true,\"all_Explorer_processes_exited_inferred\":false");}
        resources_.finish();
        if(log_change_!=INVALID_HANDLE_VALUE){const auto handle=log_change_;log_change_=INVALID_HANDLE_VALUE;change_closed_=FindCloseChangeNotification(handle)!=FALSE;}
        out_.cleanup_known=out_.cleanup_known&&!resources_.unknown&&change_closed_;
        out_.completed=out_.completed&&out_.cleanup_known;
        out_.emit("medium_product_geometry_summary","\"result\":"+quote(out_.completed?
                (explorer_case_?"PASS_SINGLE_EXACT_Explorer_GEOMETRY_ONLY":"PASS_SINGLE_EXACT_Terminal_GEOMETRY_ONLY"):"FAIL_OR_BLOCKED")+
            ",\"reason\":"+quote(out_.reason)+",\"product_PID\":"+std::to_string(product_pid_)+",\"target_PID\":"+std::to_string(target_pid_)+
            ",\"target_kind\":"+quote(target_kind())+
            ",\"bounded_STA_pre_predicate_and_post_wake_pump_used\":"+(fair_wait_pump_used_?"true":"false")+
            ",\"last_completed_generation\":"+std::to_string(last_generation_)+",\"cleanup_known\":"+(out_.cleanup_known?"true":"false")+
            ",\"test_pair_still_unknown\":"+(lease_&&lease_->incomplete_pair.load()?"true":"false")+
            ",\"own_hook_stopped\":"+(resources_.hook_ok?"true":"false")+",\"own_handles_closed\":"+
            (resources_.process_ok&&resources_.thread_ok&&resources_.event_ok&&change_closed_&&(!explorer_||explorer_->handles_closed())?"true":"false")+
            ",\"client_nonce_or_content\":\"NOT_RUN\",\"five_phase_magnet_completed\":"+(magnet_completed_?"true":"false")+
            ",\"real_Terminal_declared_minimum_and_successor_Move_completed\":"+(minimum_completed_?"true":"false")+
            ",\"single_Terminal_WM_SIZING_probe_attempted\":"+(sizing_probe_attempted_?"true":"false")+
            ",\"single_Terminal_WM_SIZING_real_comparison_completed\":"+(sizing_comparison_completed_?"true":"false")+
            ",\"single_Terminal_WM_SIZING_successor_Move_completed\":"+(sizing_successor_completed_?"true":"false")+
            ",\"single_Terminal_WM_SIZING_preference_matched\":"+(sizing_preference_matched_?(*sizing_preference_matched_?"true":"false"):"null")+
            ",\"application_grid_known\":false"+
            ",\"Ctrl_Glue\":"+quote(ctrl_completed_?"SCOPED_TWO_MEMBER_MOVE_COMPLETED":"NOT_COMPLETED")+
            ",\"five_applications_or_whole_version_PASS\":false,\"human_UAT\":\"NOT_RUN\"");
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
