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
    shell::ShellWindowInventory discovery_inventory_;
    std::uint64_t discovery_attempt_sequence_{},discovery_cache_sequence_{},discovery_before_qpc_{},discovery_after_qpc_{};
    ULONGLONG discovery_before_tick_{},discovery_after_tick_{};
    unsigned discovery_matches_{};HWND discovery_candidate_{};
    bool discovery_call_completed_{},discovery_recorded_{};
    const char* discovery_reason_{"no_postlaunch_Shell_observation"};
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
        // Cache the observation already required by selection. Exit diagnostics
        // must not add a second COM request, retry or alternative admission.
        ++discovery_attempt_sequence_;discovery_call_completed_=false;
        discovery_reason_="Shell_inventory_call_not_completed";
        const auto before_tick=GetTickCount64();LARGE_INTEGER before{},after{};
        const bool before_known=QueryPerformanceCounter(&before)!=FALSE;
        auto inventory=shell::capture_shell_window_inventory();
        const bool after_known=QueryPerformanceCounter(&after)!=FALSE;
        discovery_inventory_=std::move(inventory);discovery_cache_sequence_=discovery_attempt_sequence_;
        discovery_before_tick_=before_tick;discovery_after_tick_=GetTickCount64();
        discovery_before_qpc_=before_known&&before.QuadPart>0?static_cast<std::uint64_t>(before.QuadPart):0;
        discovery_after_qpc_=after_known&&after.QuadPart>0?static_cast<std::uint64_t>(after.QuadPart):0;
        discovery_call_completed_=true;discovery_matches_=0;discovery_candidate_=nullptr;
        const auto& observed=discovery_inventory_;
        unsigned matches{};HWND candidate{};
        for(const auto& entry:observed.windows)for(const auto& place:entry.locations)
            if(shell::same_filesystem_location(place,location_)){++matches;candidate=entry.window;}
        discovery_matches_=matches;discovery_candidate_=candidate;
        if(!observed.complete){discovery_reason_="Shell_inventory_incomplete";return false;}
        if(matches>1){discovery_reason_="ambiguous_exact_FILE_ID_match";throw std::runtime_error("Explorer_geometry_ambiguous_Shell_path");}
        if(matches!=1||!candidate){discovery_reason_=matches?"exact_location_HWND_unavailable":"no_exact_FILE_ID_match";return false;}
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
        if(!native_new(candidate,pid,process.value,born)){discovery_reason_="native_identity_membership_or_qualification_rejected";return false;}
        bind_attempted_=true;
        discovery_reason_="canonical_Shell_binding_not_completed";
        auto binding=shell::bind_explorer_consent_target(reinterpret_cast<std::uintptr_t>(candidate),*location_.identity);
        out_.emit("medium_product_Explorer_Shell_binding","\"actual_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(candidate))+
            ",\"actual_PID\":"+std::to_string(pid)+",\"actual_TID\":"+std::to_string(tid)+",\"actual_birth\":"+std::to_string(born)+
            ",\"single_binding_attempt\":true,\"binding_succeeded\":"+(binding.succeeded()?"true":"false")+
            ",\"reason\":"+std::to_string(static_cast<unsigned>(binding.reason))+
            ",\"exact_location_matches\":"+std::to_string(binding.exact_location_match_count)+
            ",\"bound_window_entries\":"+std::to_string(binding.bound_window_entry_count)+
            ",\"HRESULT\":"+(binding.diagnostic?std::to_string(static_cast<long>(binding.diagnostic->hresult)):"null")+
            ",\"input_or_product_authority_granted\":false");
        if(!binding.succeeded()){discovery_reason_="canonical_Shell_binding_rejected";throw std::runtime_error("Explorer_geometry_exact_Shell_binding_failed");}
        observation_=std::move(binding.observation);window_=candidate;pid_=pid;tid_=tid;birth_=born;
        const auto facts=observation_->facts();callback_at_binding_=facts.browser.callback_sequence;
        discovery_reason_="post_binding_identity_or_location_not_confirmed";
        if(!facts.canonical_identity_matches||facts.exact_location_match_count!=1||facts.bound_window_entry_count!=1||
           facts.navigation_changed_since_binding()||!native_new(window_,pid_,process.value,birth_)||!shell_matches())
            throw std::runtime_error("Explorer_geometry_identity_or_location_changed_during_binding");
        bound_process_=process.value;process.value=nullptr;bound_=true;
        discovery_reason_="bound_receipt_guard_not_confirmed";
        return guard();
    }
    void record_unbound_discovery(std::string_view original_failure,bool input_lease_idle)noexcept{
        if(discovery_recorded_||!launched_||handed_off_)return;discovery_recorded_=true;
        try{
            if(!input_lease_idle||GetCurrentThreadId()!=owner_||!desktop_||
               !equal(medium_desktop_object_name(desktop_),L"PaneBindV1_"+run_))
                throw std::runtime_error("readonly_discovery_owner_or_held_Run_unknown");
            // Exactly two physical inventories on the held Run desktop. An
            // empty sample never proves that a window was never created.
            const auto began=GetTickCount64();LARGE_INTEGER begin_qpc{},end_qpc{};
            const bool begin_known=QueryPerformanceCounter(&begin_qpc)!=FALSE;
            const auto first=MediumDesktopWindows::read(desktop_),second=MediumDesktopWindows::read(desktop_);
            std::vector<HWND> windows=first.windows;
            for(HWND h:second.windows)if(std::find(windows.begin(),windows.end(),h)==windows.end())windows.push_back(h);
            unsigned new_roots{},recorded{},class_unknown{},Cabinet_classes{},root_unknown{};bool locations_truncated{};
            for(HWND h:windows){
                std::array<wchar_t,128> name{};if(GetClassNameW(h,name.data(),static_cast<int>(name.size()))<=0){++class_unknown;continue;}
                if(!equal(name.data(),L"CabinetWClass"))continue;++Cabinet_classes;
                const HWND root=GetAncestor(h,GA_ROOT);if(!root){++root_unknown;continue;}
                if(root!=h||
                   std::find(native_baseline_.begin(),native_baseline_.end(),h)!=native_baseline_.end())continue;
                ++new_roots;if(recorded>=8)continue;++recorded;
                const bool member_a=std::find(first.windows.begin(),first.windows.end(),h)!=first.windows.end();
                const bool member_b=std::find(second.windows.begin(),second.windows.end(),h)!=second.windows.end();
                DWORD pid{};const DWORD tid=GetWindowThreadProcessId(h,&pid);SetLastError(ERROR_SUCCESS);
                const bool open_attempted=pid!=0;Handle process(open_attempted?OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,pid):nullptr);
                const DWORD open_error=process.value?0:GetLastError();const auto born=process.value?process_birth(process.value):0;
                const auto a=MediumDesktopWindowFact::read(h,pid,process.value,born,L"PaneBindV1_"+run_);
                const auto b=MediumDesktopWindowFact::read(h,pid,process.value,born,L"PaneBindV1_"+run_);
                int rid=-1;bool elevated{};const bool is_medium=process.value&&medium(process.value,&rid,&elevated);
                DWORD session{};const bool session_known=pid&&ProcessIdToSessionId(pid,&session);
                const auto image=process.value?medium_desktop_image(process.value):std::wstring{};
                const auto* entry=discovery_cache_sequence_?discovery_inventory_.find(h):nullptr;
                std::string locations="[";unsigned location_count{};
                if(entry)for(const auto& place:entry->locations){if(location_count>=8){locations_truncated=true;break;}
                    if(location_count++)locations+=",";
                    locations+="{\"status\":"+std::to_string(static_cast<unsigned>(place.status))+
                        ",\"source\":"+std::to_string(static_cast<unsigned>(place.source))+
                        ",\"HRESULT\":"+std::to_string(static_cast<long>(place.diagnostic))+
                        ",\"identity_present\":"+(place.identity?"true":"false")+
                        ",\"matches_expected_FILE_ID\":"+(shell::same_filesystem_location(place,location_)?"true":"false")+"}";}
                locations+="]";
                out_.emit("medium_product_Explorer_unbound_physical_candidate","\"HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(h))+
                    ",\"PID\":"+std::to_string(pid)+",\"TID\":"+std::to_string(tid)+",\"birth\":"+(born?std::to_string(born):"null")+
                    ",\"class\":\"CabinetWClass\",\"first_member\":"+(member_a?"true":"false")+",\"second_member\":"+(member_b?"true":"false")+
                    ",\"native_baseline_excluded\":true,\"Shell_baseline_excluded\":"+
                    (std::find(shell_baseline_.begin(),shell_baseline_.end(),h)==shell_baseline_.end()?"true":"false")+
                    ",\"OpenProcess_attempted\":"+(open_attempted?"true":"false")+",\"OpenProcess_return\":"+(open_attempted?(process.value?"true":"false"):"null")+
                    ",\"OpenProcess_error\":"+(open_attempted?std::to_string(open_error):"null")+
                    ",\"identity_stable\":"+(a.exact_child&&b.exact_child&&a.pid==b.pid&&a.tid==b.tid&&a.type==b.type&&b.type==L"CabinetWClass"?"true":"false")+
                    ",\"actual_root\":"+(a.root?"true":"false")+",\"actual_visible\":"+(a.visible?"true":"false")+
                    ",\"foreign_desktop_query_known\":"+(a.name_query_return?"true":"false")+
                    ",\"foreign_desktop_conflicts\":"+(a.desktop_conflict?"true":"false")+
                    ",\"foreign_GetThreadDesktop_error\":"+std::to_string(a.thread_query_error)+
                    ",\"foreign_name_query_attempted\":"+(a.name_query_attempted?"true":"false")+
                    ",\"foreign_name_query_error\":"+(a.name_query_attempted?std::to_string(a.name_query_error):"null")+
                    ",\"integrity_rid\":"+(rid<0?"null":std::to_string(rid))+",\"elevated\":"+(rid<0?"null":(elevated?"true":"false"))+
                    ",\"Medium_non_elevated_allowed\":"+(is_medium?"true":"false")+
                    ",\"same_SID_allowed\":"+(process.value?(input_actor_same_user(process.value)?"true":"false"):"null")+
                    ",\"session\":"+(session_known?std::to_string(session):"null")+
                    ",\"image_query_known\":"+(!image.empty()?"true":"false")+
                    ",\"image_matches_system_Explorer\":"+(!image.empty()?(equal(image,image_.wstring())?"true":"false"):"null")+
                    ",\"cached_Shell_entry_count\":"+(entry?std::to_string(entry->shell_entry_count):"null")+
                    ",\"cached_location_count\":"+(entry?std::to_string(entry->locations.size()):"null")+
                    ",\"cached_locations\":"+locations+",\"paths_or_contents_read_out\":false,\"input_cancel_or_write_authority\":false");
            }
            std::string issues="[";unsigned issue_count{};
            for(const auto& issue:discovery_inventory_.issues){if(issue_count>=8)break;if(issue_count++)issues+=",";
                issues+="{\"stage\":"+std::to_string(static_cast<unsigned>(issue.stage))+
                    ",\"HRESULT\":"+std::to_string(static_cast<long>(issue.hresult))+",\"item_index\":"+std::to_string(issue.item_index)+"}";}
            issues+="]";const bool end_known=QueryPerformanceCounter(&end_qpc)!=FALSE;
            out_.emit("medium_product_Explorer_unbound_discovery","\"original_failure\":"+quote(original_failure)+
                ",\"observation_only\":true,\"physical_before_tick\":"+std::to_string(began)+",\"physical_after_tick\":"+std::to_string(GetTickCount64())+
                ",\"physical_before_qpc\":"+(begin_known?std::to_string(begin_qpc.QuadPart):"null")+
                ",\"physical_after_qpc\":"+(end_known?std::to_string(end_qpc.QuadPart):"null")+
                ",\"first_complete\":"+(first.complete?"true":"false")+",\"first_error\":"+std::to_string(first.error)+
                ",\"second_complete\":"+(second.complete?"true":"false")+",\"second_error\":"+std::to_string(second.error)+
                ",\"new_Cabinet_roots_in_sample_union\":"+std::to_string(new_roots)+",\"recorded_root_limit\":8,\"recorded_roots\":"+std::to_string(recorded)+
                ",\"roots_truncated\":"+(new_roots>recorded?"true":"false")+",\"class_query_unknown\":"+std::to_string(class_unknown)+
                ",\"Cabinet_class_observations\":"+std::to_string(Cabinet_classes)+",\"Cabinet_root_query_unknown\":"+std::to_string(root_unknown)+
                ",\"Shell_attempt_sequence\":"+std::to_string(discovery_attempt_sequence_)+",\"Shell_cache_sequence\":"+std::to_string(discovery_cache_sequence_)+
                ",\"last_Shell_call_completed\":"+(discovery_call_completed_?"true":"false")+
                ",\"cached_Shell_before_tick\":"+std::to_string(discovery_before_tick_)+",\"cached_Shell_after_tick\":"+std::to_string(discovery_after_tick_)+
                ",\"cached_Shell_before_qpc\":"+(discovery_before_qpc_?std::to_string(discovery_before_qpc_):"null")+
                ",\"cached_Shell_after_qpc\":"+(discovery_after_qpc_?std::to_string(discovery_after_qpc_):"null")+
                ",\"cached_Shell_complete\":"+(discovery_cache_sequence_?(discovery_inventory_.complete?"true":"false"):"null")+
                ",\"cached_Shell_reported_count\":"+(discovery_cache_sequence_?std::to_string(discovery_inventory_.reported_entry_count):"null")+
                ",\"cached_Shell_window_count\":"+std::to_string(discovery_inventory_.windows.size())+
                ",\"cached_Shell_issue_count\":"+std::to_string(discovery_inventory_.issues.size())+
                ",\"cached_Shell_issues\":"+issues+",\"issues_truncated\":"+(discovery_inventory_.issues.size()>issue_count?"true":"false")+
                ",\"locations_truncated\":"+(locations_truncated?"true":"false")+
                ",\"cached_exact_location_matches\":"+std::to_string(discovery_matches_)+
                ",\"cached_matches_exhaustive\":"+(discovery_cache_sequence_&&discovery_inventory_.complete?"true":"false")+
                ",\"cached_candidate_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(discovery_candidate_))+
                ",\"selection_result\":"+quote(discovery_reason_)+
                ",\"new_COM_query_at_exit\":false,\"samples_are_not_atomic\":true,\"root_never_created_proven\":false,\"input_cancel_or_write_authority\":false");
        }catch(...){try{out_.emit("medium_product_Explorer_unbound_discovery",
            "\"observation_only\":true,\"result\":\"UNKNOWN_observation_failed\",\"original_failure_preserved\":true,\"new_COM_query_at_exit\":false,\"input_cancel_or_write_authority\":false");}catch(...){}}
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
