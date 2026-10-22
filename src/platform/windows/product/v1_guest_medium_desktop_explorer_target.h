#pragma once

// TEST ONLY. One new file-window on the already active exact Medium Run.
// This adapter discovers identity/location; it has no writer or input API.
// COM is used only by prepare/bind/refresh/retire on the owner STA, never by
// the bounded receipt guard used while a SendInput lease is in flight.
void CALLBACK medium_explorer_geometry_event(HWINEVENTHOOK,DWORD,HWND window,LONG object,LONG child,DWORD,DWORD){
    auto* state=medium_terminal_native_events;if(!state||!window||object!=OBJID_WINDOW||child)return;
    DWORD pid{};GetWindowThreadProcessId(window,&pid);
    if(state->pid){if(pid!=state->pid&&window!=state->root)return;}
    else{std::array<wchar_t,128> name{};if(GetClassNameW(window,name.data(),static_cast<int>(name.size()))<=0||
        !equal(name.data(),L"CabinetWClass"))return;}
    // A notification is only a wake-up. It cannot adopt or control a root.
    if(state->notifications<64)++state->notifications;else state->overflow=true;
    if(state->changed)SetEvent(state->changed);
}

class MediumExplorerGeometryTarget {
    const std::wstring& run_;HDESK desktop_;DWORD session_{},owner_{GetCurrentThreadId()};
    MediumDesktopProductGeometryObservation& out_;
    fs::path directory_,image_;shell::ShellLocationFact location_;
    std::vector<HWND> native_baseline_,shell_baseline_;
    std::unique_ptr<shell::ExplorerConsentTargetObservation> observation_;
    HANDLE launcher_{},launch_thread_{},bound_process_{};
    HWND window_{};DWORD pid_{},tid_{};std::uint64_t birth_{},callback_at_binding_{};
    bool launched_{},bind_attempted_{},bound_{},handed_off_{},retire_attempted_{},retire_ok_{true},handles_ok_{true},candidate_recorded_{};
    HRESULT retire_hresult_{E_PENDING};
    bool shell_matches()const{
        if(!shell::same_filesystem_location(shell::filesystem_location_identity(directory_),location_))return false;
        const auto inventory=shell::capture_shell_window_inventory();if(!inventory.complete)return false;
        const auto* entry=inventory.find(window_);
        if(!entry||entry->shell_entry_count!=1||entry->locations.size()!=1||
           !shell::same_filesystem_location(entry->locations.front(),location_))return false;
        unsigned matches{};for(const auto& candidate:inventory.windows)for(const auto& place:candidate.locations)
            if(shell::same_filesystem_location(place,location_))++matches;
        return matches==1;
    }
    bool native_new(HWND window,DWORD pid,HANDLE process,std::uint64_t birth)const{
        if(!window||!pid||!process||!birth||
           std::find(native_baseline_.begin(),native_baseline_.end(),window)!=native_baseline_.end()||
           std::find(shell_baseline_.begin(),shell_baseline_.end(),window)!=shell_baseline_.end()||
           !medium(process)||!input_actor_same_user(process)||!equal(medium_desktop_image(process),image_.wstring()))return false;
        DWORD session{};if(!ProcessIdToSessionId(pid,&session)||session!=session_)return false;
        const auto first=MediumDesktopWindows::read(desktop_),second=MediumDesktopWindows::read(desktop_);
        if(!first.complete||!second.complete||std::find(first.windows.begin(),first.windows.end(),window)==first.windows.end()||
           std::find(second.windows.begin(),second.windows.end(),window)==second.windows.end())return false;
        const auto a=MediumDesktopWindowFact::read(window,pid,process,birth,L"PaneBindV1_"+run_);
        const auto b=MediumDesktopWindowFact::read(window,pid,process,birth,L"PaneBindV1_"+run_);
        return a.type==L"CabinetWClass"&&a.readonly_same(b)&&a.exact_child&&b.exact_child&&a.root&&b.root&&a.visible&&b.visible;
    }
    void close_handle(HANDLE& handle)noexcept{
        if(handle){const auto actual=handle;handle=nullptr;handles_ok_=CloseHandle(actual)!=FALSE&&handles_ok_;}
    }
public:
    MediumExplorerGeometryTarget(const std::wstring& run,HDESK desktop,DWORD session,
        MediumDesktopProductGeometryObservation& out):run_(run),desktop_(desktop),session_(session),out_(out){}
    ~MediumExplorerGeometryTarget(){finish();}
    void prepare(const MediumDesktopWindows& baseline){
        if(GetCurrentThreadId()!=owner_||!baseline.complete||!session_||!desktop_||
           !equal(medium_desktop_object_name(desktop_),L"PaneBindV1_"+run_))
            throw std::runtime_error("Explorer_geometry_actual_Run_baseline_unknown");
        native_baseline_=baseline.windows;
        const auto inventory=shell::capture_shell_window_inventory();
        if(!inventory.complete)throw std::runtime_error("Explorer_geometry_complete_Shell_baseline_required");
        for(const auto& entry:inventory.windows){if(!entry.window)throw std::runtime_error("Explorer_geometry_baseline_HWND_unknown");
            shell_baseline_.push_back(entry.window);}
        const fs::path guest=L"C:\\PaneBindV1",base=guest/L"Explorer",run=base/run_;
        if(!plain_directory(guest))throw std::runtime_error("Explorer_geometry_guest_root_unknown");
        if(!fs::exists(base)&&!fs::create_directory(base))throw std::runtime_error("Explorer_geometry_base_create_failed");
        if(!plain_directory(base)||fs::exists(run)||!fs::create_directory(run)||!plain_directory(run))
            throw std::runtime_error("Explorer_geometry_fresh_owned_Run_directory_required");
        const auto ownership=run/L"panebind-v1-explorer-run.marker";
        const std::string expected="panebind-v1-explorer-geometry/1\nrun_id="+utf8(run_)+"\ndriver_sha=" PANEBIND_BUILD_SHA "\n";
        Handle marker_file(CreateFileW(ownership.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr));
        if(marker_file.value==INVALID_HANDLE_VALUE)throw std::runtime_error("Explorer_geometry_marker_create_failed");
        DWORD written{};const bool saved=WriteFile(marker_file.value,expected.data(),static_cast<DWORD>(expected.size()),&written,nullptr)&&
            written==expected.size()&&FlushFileBuffers(marker_file.value);
        const bool closed=CloseHandle(marker_file.value)!=FALSE;marker_file.value=nullptr;
        if(!saved||!closed||read(ownership,256)!=expected)throw std::runtime_error("Explorer_geometry_marker_write_unknown");
        directory_=run/L"Medium-Explorer-A";
        if(fs::exists(directory_)||!fs::create_directory(directory_)||!plain_directory(directory_)||!fs::is_empty(directory_))
            throw std::runtime_error("Explorer_geometry_exact_new_empty_leaf_required");
        location_=shell::filesystem_location_identity(directory_);
        if(!location_.filesystem())throw std::runtime_error("Explorer_geometry_FILE_ID_unknown");
        std::array<wchar_t,MAX_PATH> windows{};if(!GetWindowsDirectoryW(windows.data(),static_cast<UINT>(windows.size())))
            throw std::runtime_error("Explorer_geometry_Windows_directory_unknown");
        image_=fs::path(windows.data())/L"explorer.exe";
        std::string file_id;constexpr char hex[]="0123456789abcdef";
        for(const auto value:location_.identity->file_id){const auto byte=std::to_integer<unsigned>(value);file_id+=hex[byte>>4];file_id+=hex[byte&15];}
        out_.emit("medium_product_Explorer_prepared","\"directory\":"+quote(utf8(directory_.wstring()))+
            ",\"marker_sha256\":"+quote(hash(ownership))+",\"native_baseline_count\":"+std::to_string(native_baseline_.size())+
            ",\"Shell_baseline_complete\":true,\"Shell_baseline_count\":"+std::to_string(shell_baseline_.size())+
            ",\"volume_serial\":"+std::to_string(location_.identity->volume_serial_number)+",\"file_id_128\":"+quote(file_id)+
            ",\"leaf_new_empty\":true,\"input_or_product_authority_granted\":false");
    }
    void launch(){
        STARTUPINFOW startup{sizeof(startup)};std::wstring desktop=L"WinSta0\\PaneBindV1_"+run_;
        startup.lpDesktop=desktop.data();PROCESS_INFORMATION info{};
        auto command=L"\""+image_.wstring()+L"\" /separate /n,\""+directory_.wstring()+L"\"";
        const auto before=GetTickCount64();SetLastError(ERROR_SUCCESS);
        launched_=CreateProcessW(image_.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,
            directory_.c_str(),&startup,&info)!=FALSE;const DWORD error=launched_?0:GetLastError();
        if(launched_){launcher_=info.hProcess;launch_thread_=info.hThread;}
        out_.emit("medium_product_Explorer_launch","\"CreateProcess_attempted\":true,\"native_return\":"+
            std::string(launched_?"true":"false")+",\"error\":"+std::to_string(error)+",\"launcher_PID\":"+std::to_string(info.dwProcessId)+
            ",\"launcher_birth\":"+(launcher_?std::to_string(process_birth(launcher_)):"null")+
            ",\"launcher_Medium\":"+(launcher_?(medium(launcher_)?"true":"false"):"null")+
            ",\"lpDesktop\":"+quote(utf8(desktop))+",\"command\":"+quote(utf8(command))+
            ",\"before_tick\":"+std::to_string(before)+",\"after_tick\":"+std::to_string(GetTickCount64())+
            ",\"new_root_not_inferred_from_launcher\":true,\"request_count\":1,\"baseline_window_controlled\":false");
        if(!launched_)throw std::runtime_error("Explorer_geometry_separate_CreateProcess_failed");
    }
    bool try_bind(){
        if(bound_)return guard();
        if(bind_attempted_)throw std::runtime_error("Explorer_geometry_single_binding_failed_no_retry");
        const auto inventory=shell::capture_shell_window_inventory();if(!inventory.complete)return false;
        unsigned matches{};HWND candidate{};
        for(const auto& entry:inventory.windows)for(const auto& place:entry.locations)
            if(shell::same_filesystem_location(place,location_)){++matches;candidate=entry.window;}
        if(matches>1)throw std::runtime_error("Explorer_geometry_ambiguous_Shell_path");
        if(matches!=1||!candidate)return false;
        DWORD pid{};const auto tid=GetWindowThreadProcessId(candidate,&pid);
        Handle process(pid?OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,pid):nullptr);
        const auto born=process.value?process_birth(process.value):0;
        if(!candidate_recorded_){candidate_recorded_=true;int rid=-1;bool elevated{};
            const bool qualified=process.value&&medium(process.value,&rid,&elevated);
            const auto members=MediumDesktopWindows::read(desktop_);
            out_.emit("medium_product_Explorer_candidate","\"actual\":"+
                MediumDesktopWindowFact::read(candidate,pid,process.value,born,L"PaneBindV1_"+run_).json()+
                ",\"actual_birth\":"+std::to_string(born)+",\"actual_image\":"+quote(utf8(medium_desktop_image(process.value)))+
                ",\"integrity_rid\":"+(rid<0?"null":std::to_string(rid))+",\"elevated\":"+(rid<0?"null":(elevated?"true":"false"))+
                ",\"Medium_non_elevated_qualification\":"+(qualified?"true":"false")+
                ",\"held_Run_inventory_complete\":"+(members.complete?"true":"false")+
                ",\"held_Run_member\":"+(members.complete?(std::find(members.windows.begin(),members.windows.end(),candidate)!=members.windows.end()?"true":"false"):"null")+
                ",\"native_baseline_excluded\":"+(std::find(native_baseline_.begin(),native_baseline_.end(),candidate)==native_baseline_.end()?"true":"false")+
                ",\"Shell_baseline_excluded\":"+(std::find(shell_baseline_.begin(),shell_baseline_.end(),candidate)==shell_baseline_.end()?"true":"false")+
                ",\"input_or_product_authority_granted\":false");}
        if(!native_new(candidate,pid,process.value,born))return false;
        bind_attempted_=true;
        auto binding=shell::bind_explorer_consent_target(reinterpret_cast<std::uintptr_t>(candidate),*location_.identity);
        out_.emit("medium_product_Explorer_Shell_binding","\"actual_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(candidate))+
            ",\"actual_PID\":"+std::to_string(pid)+",\"actual_TID\":"+std::to_string(tid)+",\"actual_birth\":"+std::to_string(born)+
            ",\"single_binding_attempt\":true,\"binding_succeeded\":"+(binding.succeeded()?"true":"false")+
            ",\"reason\":"+std::to_string(static_cast<unsigned>(binding.reason))+
            ",\"exact_location_matches\":"+std::to_string(binding.exact_location_match_count)+
            ",\"bound_window_entries\":"+std::to_string(binding.bound_window_entry_count)+
            ",\"HRESULT\":"+(binding.diagnostic?std::to_string(static_cast<long>(binding.diagnostic->hresult)):"null")+
            ",\"input_or_product_authority_granted\":false");
        if(!binding.succeeded())throw std::runtime_error("Explorer_geometry_exact_Shell_binding_failed");
        observation_=std::move(binding.observation);window_=candidate;pid_=pid;tid_=tid;birth_=born;
        const auto facts=observation_->facts();callback_at_binding_=facts.browser.callback_sequence;
        if(!facts.canonical_identity_matches||facts.exact_location_match_count!=1||facts.bound_window_entry_count!=1||
           facts.navigation_changed_since_binding()||!native_new(window_,pid_,process.value,birth_)||!shell_matches())
            throw std::runtime_error("Explorer_geometry_identity_or_location_changed_during_binding");
        bound_process_=process.value;process.value=nullptr;bound_=true;
        return guard();
    }
    bool guard()const noexcept{
        if(!bound_||!observation_||GetCurrentThreadId()!=owner_||observation_->owner_thread_id()!=owner_||
           observation_->bound_window_key()!=reinterpret_cast<std::uintptr_t>(window_))return false;
        const auto facts=observation_->receipt_facts();
        return facts.accepting&&facts.subscribed&&!facts.unadvised&&facts.subscription_diagnostic==S_OK&&
            facts.callback_sequence==callback_at_binding_&&!facts.quit_count&&!facts.malformed_count&&!facts.overflow_count&&
            !facts.wrong_thread_count&&!facts.post_retirement_count&&!facts.identity_query_failure_count&&
            facts.matching_navigate_complete_count==observation_->navigation_epoch_at_binding();
    }
    bool refresh(){
        if(!guard()){
            const auto facts=observation_?observation_->receipt_facts():shell::BrowserReadinessFacts{};
            out_.emit("medium_product_Explorer_location_refresh","\"accepted\":false,\"stage\":\"bounded_receipt_guard\""+
                std::string(",\"COM_outside_input_lease\":true,\"observer_present\":")+(observation_?"true":"false")+
                ",\"callback_sequence\":"+std::to_string(facts.callback_sequence)+",\"callback_at_binding\":"+std::to_string(callback_at_binding_)+
                ",\"quit_count\":"+std::to_string(facts.quit_count)+",\"malformed_count\":"+std::to_string(facts.malformed_count)+
                ",\"overflow_count\":"+std::to_string(facts.overflow_count)+",\"input_or_product_authority_granted\":false");return false;
        }
        const auto key=observation_->current_window_key();
        const auto location=observation_->current_location();const auto facts=observation_->facts();
        const bool accepted=key.succeeded()&&key.window_key==reinterpret_cast<std::uintptr_t>(window_)&&
            shell::same_filesystem_location(location,location_)&&facts.canonical_identity_matches&&
            !facts.navigation_changed_since_binding()&&shell_matches()&&guard();
        out_.emit("medium_product_Explorer_location_refresh","\"actual_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(window_))+
            ",\"owner_STA\":true,\"COM_outside_input_lease\":true,\"canonical_identity_matches\":"+(facts.canonical_identity_matches?"true":"false")+
            ",\"navigation_changed_since_binding\":"+(facts.navigation_changed_since_binding()?"true":"false")+
            ",\"accepted\":"+(accepted?"true":"false")+",\"input_or_product_authority_granted\":false");return accepted;
    }
    void finish()noexcept{
        if(observation_&&!retire_attempted_){retire_attempted_=true;retire_hresult_=observation_->retire();retire_ok_=retire_hresult_==S_OK;observation_.reset();}
        close_handle(bound_process_);close_handle(launch_thread_);close_handle(launcher_);
    }
    // A bound candidate whose final receipt guard rejected never reached the
    // common target/close path. Retiring its observer/handles is not evidence
    // that the launched root was closed; leave that unconfirmed root alone.
    bool cleanup_known()const noexcept{return handles_ok_&&retire_ok_&&(!launched_||handed_off_);}
    bool handles_closed()const noexcept{return handles_ok_;}
    bool retire_attempted()const noexcept{return retire_attempted_;}HRESULT retire_hresult()const noexcept{return retire_hresult_;}
    bool launched()const noexcept{return launched_;}
    HWND window()const noexcept{return window_;}DWORD pid()const noexcept{return pid_;}DWORD tid()const noexcept{return tid_;}
    std::uint64_t birth()const noexcept{return birth_;}const fs::path& image()const noexcept{return image_;}
    HANDLE take_bound_process()noexcept{const auto handle=bound_process_;bound_process_=nullptr;
        if(handle)handed_off_=true;return handle;}
};
