#pragma once

// TEST only. Two copies of this exact driver image create blank, ordinary
// Medium windows at different immutable Input paths. No product eligibility
// exception and no generic command channel are introduced.
std::uint64_t input_actor_unsigned(const wchar_t*);
bool input_actor_same_user(HANDLE);
DWORD input_actor_parent_pid();
bool input_actor_desktop(DWORD,DWORD);

bool rules_path_plain_image(const fs::path& path){
    const auto attributes=GetFileAttributesW(path.c_str());std::error_code error;
    const auto actual=fs::canonical(path,error);
    return attributes!=INVALID_FILE_ATTRIBUTES&&!(attributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))&&
        !error&&equal(actual.wstring(),path.wstring());
}
std::wstring rules_path_class(const std::wstring& run){return L"PaneBind.V1.RulePathTarget."+run;}
LRESULT CALLBACK rules_path_window(HWND h,UINT message,WPARAM w,LPARAM l){
    if(message==WM_CLOSE){DestroyWindow(h);return 0;}
    if(message==WM_DESTROY){PostQuitMessage(0);return 0;}
    return DefWindowProcW(h,message,w,l);
}
int rules_path_child(int count,LPWSTR* args){
    // This branch is selected before the ordinary fixture/identity entry. No
    // window, output, synthetic input or COM startup occurs before this proof.
    if(count!=7)return 78;const std::wstring run=args[2],slot=args[6];
    if(run.size()!=32||!std::all_of(run.begin(),run.end(),[](wchar_t c){return(c>=L'0'&&c<=L'9')||(c>=L'a'&&c<=L'f');})||
        (slot!=L"A"&&slot!=L"B"))return 78;
    std::array<wchar_t,128> user{};DWORD user_length=static_cast<DWORD>(user.size());
    if(!GetUserNameW(user.data(),&user_length)||!equal(user.data(),L"WDAGUtilityAccount")||!medium(GetCurrentProcess()))return 78;
    const fs::path input=L"C:\\PaneBindV1\\Input",output=L"C:\\PaneBindV1\\Output";
    if(!plain_directory(input)||!plain_directory(output)||!marker(input/L"panebind-v1-input.marker",run)||
        !marker(output/L"panebind-v1-output.marker",run))return 78;
    const auto parent_value=input_actor_unsigned(args[3]),birth=input_actor_unsigned(args[4]),tid_value=input_actor_unsigned(args[5]);
    if(!parent_value||parent_value>MAXDWORD||!birth||!tid_value||tid_value>MAXDWORD)return 78;
    const auto pid=static_cast<DWORD>(parent_value),tid=static_cast<DWORD>(tid_value);
    if(input_actor_parent_pid()!=pid)return 78;
    Handle parent(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,pid));
    Handle parent_thread(OpenThread(THREAD_QUERY_LIMITED_INFORMATION,FALSE,tid));
    if(!parent.value||!parent_thread.value||GetProcessIdOfThread(parent_thread.value)!=pid||
        WaitForSingleObject(parent.value,0)!=WAIT_TIMEOUT||process_birth(parent.value)!=birth||
        !medium(parent.value)||!input_actor_same_user(parent.value)||!input_actor_desktop(tid,pid))return 78;
    std::array<wchar_t,32768> self_name{},parent_name{};DWORD parent_length=static_cast<DWORD>(parent_name.size());
    const auto length=GetModuleFileNameW(nullptr,self_name.data(),static_cast<DWORD>(self_name.size()));
    if(!length||length>=self_name.size()||!QueryFullProcessImageNameW(parent.value,0,parent_name.data(),&parent_length))return 78;
    const fs::path self(self_name.data());const auto relative=fs::path(L"RuleTargets")/slot/L"panebind-rule-app.exe";
    const bool debug=equal(self.wstring(),(input/L"Debug"/relative).wstring());const auto base=debug?input/L"Debug":input;
    if(!equal(self.wstring(),(base/relative).wstring())||!equal(parent_name.data(),(base/L"panebind-v1-guest-fixture.exe").wstring())||
        !plain_directory(base)||!plain_directory(base/L"RuleTargets")||!plain_directory(base/L"RuleTargets"/slot)||
        !rules_path_plain_image(self)||!rules_path_plain_image(parent_name.data()))return 78;
    const auto package=read(input/L"manifest.json",256*1024);const auto files=package.find("\"files\"");
    if(files==package.npos||portable_field(package,"guest_suite")!="RulesPathOnly")return 78;
    const std::string_view identity(package.data(),files);const auto image_hash=hash(self);
    if(portable_field(identity,"run_id")!=utf8(run)||portable_field(identity,"driver_source_commit")!=PANEBIND_BUILD_SHA||
        portable_field(identity,"guest_contract")!="panebind-v1-guest-contract/2"||
        portable_field(identity,"product_interface")!="panebind-v1-product-guest-interface/1"||
        image_hash.size()!=64||image_hash!=hash(parent_name.data())||
        image_hash!=portable_field(identity,debug?"driver_debug_sha256":"driver_release_sha256"))return 78;
    const bool dpi_set=SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)!=FALSE;
    if(!dpi_set&&!AreDpiAwarenessContextsEqual(GetThreadDpiAwarenessContext(),DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))return 78;
    MONITORINFO monitor{sizeof(monitor)};POINT origin{};
    if(!GetMonitorInfoW(MonitorFromPoint(origin,MONITOR_DEFAULTTOPRIMARY),&monitor)||
        monitor.rcWork.right-monitor.rcWork.left<1100||monitor.rcWork.bottom-monitor.rcWork.top<600)return 78;
    const auto name=rules_path_class(run);const auto instance=GetModuleHandleW(nullptr);
    WNDCLASSW cls{};cls.lpfnWndProc=rules_path_window;cls.hInstance=instance;cls.lpszClassName=name.c_str();
    cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);cls.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
    if(!RegisterClassW(&cls))return 78;
    // A's partially off-screen signed source is a geometry-bridge subset,
    // not a negative monitor/work-area-origin or multi-display observation.
    const int x=slot==L"A"?monitor.rcWork.left-40:monitor.rcWork.left+620;
    const HWND window=CreateWindowExW(0,name.c_str(),L"PaneBind 规则路径空白测试",WS_OVERLAPPEDWINDOW,
        x,monitor.rcWork.top+150,460,340,nullptr,nullptr,instance,nullptr);
    if(!window){UnregisterClassW(name.c_str(),instance);return 78;}
    ShowWindow(window,SW_SHOWNOACTIVATE);UpdateWindow(window);
    bool normal=false;const auto deadline=GetTickCount64()+180000;
    while(!normal){
        const auto now=GetTickCount64();if(now>=deadline)break;
        const HANDLE handles[]{parent.value};
        const auto result=MsgWaitForMultipleObjectsEx(1,handles,static_cast<DWORD>(deadline-now),QS_ALLINPUT,MWMO_INPUTAVAILABLE);
        if(result==WAIT_OBJECT_0)break;if(result!=WAIT_OBJECT_0+1)break;
        MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){
            if(message.message==WM_QUIT){normal=true;break;}TranslateMessage(&message);DispatchMessageW(&message);
        }
    }
    const bool destroyed=!IsWindow(window)||DestroyWindow(window)!=FALSE;
    const bool unregistered=UnregisterClassW(name.c_str(),instance)!=FALSE;
    return destroyed&&unregistered&&(normal||WaitForSingleObject(parent.value,0)==WAIT_OBJECT_0)?0:1;
}

struct RulesPathTarget {
    State& state;std::wstring slot;fs::path image;HANDLE process{};DWORD pid{},tid{};std::uint64_t birth{};HWND hwnd{};
    RulesPathTarget(State& s,const wchar_t* value):state(s),slot(value){}
    ~RulesPathTarget(){if(process)CloseHandle(process);}
    bool owns()const{
        if(!process||!pid||!tid||!birth||!hwnd||WaitForSingleObject(process,0)!=WAIT_TIMEOUT||process_birth(process)!=birth||
            !medium(process)||!input_actor_same_user(process)||!input_actor_desktop(tid,pid)||GetAncestor(hwnd,GA_ROOT)!=hwnd||
            gui_class(hwnd)!=rules_path_class(state.run))return false;
        DWORD actual_pid{};std::array<wchar_t,32768> actual{};DWORD size=static_cast<DWORD>(actual.size());
        return GetWindowThreadProcessId(hwnd,&actual_pid)==tid&&actual_pid==pid&&
            QueryFullProcessImageNameW(process,0,actual.data(),&size)&&equal(actual.data(),image.wstring());
    }
    void create(){
        settings_guest_scope(state);if(state.suite!="RulesPathOnly"||process)throw std::runtime_error("rules_path_exact_suite_fresh_child_required");
        const fs::path base=state.configuration=="Debug"?fs::path(L"C:\\PaneBindV1\\Input\\Debug"):fs::path(L"C:\\PaneBindV1\\Input");
        image=base/L"RuleTargets"/slot/L"panebind-rule-app.exe";const auto expected=hash(base/L"panebind-v1-guest-fixture.exe");
        if(!plain_directory(base/L"RuleTargets")||!plain_directory(image.parent_path())||!rules_path_plain_image(image)||
            expected.size()!=64||hash(image)!=expected)throw std::runtime_error("rules_path_exact_readonly_image_hash_required");
        std::wstring command=L"\""+image.wstring()+L"\" --rule-test-window "+state.run+L" "+std::to_wstring(GetCurrentProcessId())+
            L" "+std::to_wstring(process_birth(GetCurrentProcess()))+L" "+std::to_wstring(state.owner)+L" "+slot;
        STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION info{};SetLastError(0);
        const bool launched=CreateProcessW(image.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_UNICODE_ENVIRONMENT,nullptr,
            image.parent_path().c_str(),&startup,&info)!=FALSE;const auto error=launched?0:GetLastError();
        if(launched){process=info.hProcess;pid=info.dwProcessId;tid=info.dwThreadId;CloseHandle(info.hThread);birth=process_birth(process);}
        state.log.check("rules_path_exact_child_launch",launched&&birth,"\"slot\":"+quote(utf8(slot))+",\"image\":"+quote(utf8(image.wstring()))+
            ",\"image_sha256\":"+quote(expected)+",\"native_return\":"+(launched?"true":"false")+",\"error\":"+std::to_string(error)+
            ",\"PID\":"+std::to_string(pid)+",\"TID\":"+std::to_string(tid)+",\"birth\":"+std::to_string(birth)+",\"command\":"+quote(utf8(command)));
        const bool ready=wait([&]{
            if(!process||WaitForSingleObject(process,0)!=WAIT_TIMEOUT)return false;
            struct Search {RulesPathTarget* target;unsigned count{};HWND found{};} search{this};
            const bool enumerated=EnumWindows([](HWND h,LPARAM value)->BOOL{auto& q=*reinterpret_cast<Search*>(value);DWORD actual{};
                if(GetWindowThreadProcessId(h,&actual)==q.target->tid&&actual==q.target->pid&&GetAncestor(h,GA_ROOT)==h&&
                    gui_class(h)==rules_path_class(q.target->state.run)){++q.count;q.found=h;}return TRUE;},reinterpret_cast<LPARAM>(&search))!=FALSE;
            hwnd=search.found;return enumerated&&search.count==1&&owns()&&IsWindowVisible(hwnd)&&visible(hwnd).has_value();
        },5000);
        DWORD exit_code=STILL_ACTIVE;if(process)GetExitCodeProcess(process,&exit_code);
        state.log.check("rules_path_exact_child_window",ready,"\"slot\":"+quote(utf8(slot))+",\"HWND\":"+
            std::to_string(reinterpret_cast<std::uintptr_t>(hwnd))+",\"class\":"+quote(utf8(rules_path_class(state.run)))+
            ",\"exit_code\":"+std::to_string(exit_code)+",\"one_blank_new_root\":true,\"inherited_consent_or_product_authority\":false");
    }
    bool close(){
        if(!process)return true;bool requested=false;DWORD error{};
        if(owns()){SetLastError(0);requested=PostMessageW(hwnd,WM_CLOSE,0,0)!=FALSE;if(!requested)error=GetLastError();}
        const bool exited=wait([&]{return WaitForSingleObject(process,0)==WAIT_OBJECT_0;},5000);DWORD code=STILL_ACTIVE;
        const bool exit_read=GetExitCodeProcess(process,&code)!=FALSE;
        state.log.emit("rules_path_exact_child_close","\"slot\":"+quote(utf8(slot))+",\"PID\":"+std::to_string(pid)+
            ",\"birth\":"+std::to_string(birth)+",\"HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(hwnd))+
            ",\"WM_CLOSE_posted\":"+(requested?"true":"false")+",\"error\":"+std::to_string(error)+
            ",\"process_handle_signaled\":"+(exited?"true":"false")+",\"exit_code\":"+(exit_read?std::to_string(code):"null")+
            ",\"forced_termination\":false,\"parent_exit_wait_is_bounded_TEST_lifecycle\":true");
        return exited&&exit_read&&code==0;
    }
};
void rules_path_front(RulesPathTarget& target){
    auto& s=target.state;if(s.left||!target.owns()||!IsWindowVisible(target.hwnd))throw std::runtime_error("rules_path_released_exact_visible_target_required");
    const bool called=SetForegroundWindow(target.hwnd)!=FALSE;
    const bool front=wait([&]{return target.owns()&&GetForegroundWindow()==target.hwnd;});
    RECT client{};POINT point{};const bool mapped=front&&GetClientRect(target.hwnd,&client)&&
        (point={(client.left+client.right)/2,(client.top+client.bottom)/2},ClientToScreen(target.hwnd,&point));
    if(mapped)cursor(s,point);
    s.log.check("rules_path_exact_test_target_foreground",called&&front&&mapped&&WindowFromPoint(point)==target.hwnd,
        "\"slot\":"+quote(utf8(target.slot))+",\"HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(target.hwnd))+
        ",\"SetForegroundWindow_return\":"+(called?"true":"false")+",\"exact_client_hit\":"+(mapped&&WindowFromPoint(point)==target.hwnd?"true":"false"));
}
void rules_path_command(RulesPathTarget& target,RulesPathTarget& other,const wchar_t* action,bool allowed,const char* label){
    namespace e=product::guest::up_evidence_detail;auto& s=target.state;rules_path_front(target);
    const auto before=visible(target.hwnd),before_position=geometry_history_positioning(target.hwnd);
    const auto other_before=visible(other.hwnd),other_position=geometry_history_positioning(other.hwnd);
    if(!before||!before_position||!other.owns()||!other_before||!other_position)throw std::runtime_error("rules_path_source_geometry_unknown");
    std::string preparation;std::uint64_t source_key{};
    if(allowed){
        // Independently identify this product's real Catalog key. A class or
        // process name is not sufficient to bind a placement to A versus B.
        POINT caption{};bool found=false;const auto mark=read_product(s).size();
        const LONG x=before->l+before->width()/3;
        for(LONG y=before->t;y<std::min(before->b,before->t+96)&&!found;++y){
            const POINT point{x,y};DWORD_PTR hit{};
            if(target.owns()&&WindowFromPoint(point)==target.hwnd&&SendMessageTimeoutW(target.hwnd,WM_NCHITTEST,0,
                MAKELPARAM(static_cast<short>(x),static_cast<short>(y)),SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&hit)&&hit==HTCAPTION){caption=point;found=true;}
        }
        if(!found)throw std::runtime_error("rules_path_actual_caption_missing");cursor(s,caption);
        const bool prepared=wait([&]{preparation=record(s,mark,"input_preparation");
            source_key=e::number(preparation,"\"key\":").value_or(0);
            return target.owns()&&geometry_history_receipt(s,preparation,"input_preparation")&&source_key&&
                e::quoted(preparation,"\"reason\":","prepared_actual_source_and_hit")&&
                e::number(preparation,"\"root\":")==reinterpret_cast<std::uintptr_t>(target.hwnd)&&
                e::number(preparation,"\"hit\":")==reinterpret_cast<std::uintptr_t>(target.hwnd)&&
                e::number(preparation,"\"hit_pid\":")==target.pid&&e::number(preparation,"\"region\":")==0&&
                e::number(preparation,"\"foreground\":")==reinterpret_cast<std::uintptr_t>(target.hwnd);});
        s.log.check("rules_path_actual_product_source_key",prepared,"\"slot\":"+quote(utf8(target.slot))+
            ",\"actual_image\":"+quote(utf8(target.image.wstring()))+",\"actual_preparation\":"+quote(preparation)+
            ",\"source_key\":"+std::to_string(source_key)+",\"preparation_is_not_write_authority\":true");
    }
    const auto work=chain_work(target.hwnd);const auto config=chain_settings(s);const auto dpi=GetDpiForWindow(target.hwnd);
    const auto step=static_cast<LONG>((static_cast<long long>(config.step_dip)*dpi+48)/96);
    Rect expected=*before;const std::wstring name(action);
    if(name==L"Center"){const auto x=work.l+(work.width()-before->width())/2,y=work.t+(work.height()-before->height())/2;
        expected={x,y,x+before->width(),y+before->height()};}
    else if(name==L"MoveRight")expected=translated(*before,step,0);
    else if(name==L"MoveLeft")expected=translated(*before,-step,0);
    else throw std::runtime_error("rules_path_unexpected_test_command");
    const auto prior=geometry_history_status(s);const auto mark=read_product(s).size();std::string receipt;
    const bool sent=launch(s,L"--command "+name,true);
    const bool observed=sent&&wait([&]{receipt=record(s,mark,allowed?"command":"command_rejected");
        if(!allowed&&receipt.empty())receipt=record(s,mark,"notice");
        return geometry_history_receipt(s,receipt,allowed?"command":receipt.find("\"type\":\"command_rejected\"")!=receipt.npos?"command_rejected":"notice")&&
            (allowed?e::quoted(receipt,"\"action\":",utf8(name)):
                (e::quoted(receipt,"\"reason\":","application_excluded")||e::quoted(receipt,"\"reason\":",utf8(L"应用规则禁用了窗口命令。"))));
    });
    const auto status=geometry_history_status(s,e::number(receipt,"\"sequence\":").value_or(0));
    unsigned claims{},placements{};bool exact_scope=true;std::string placement;std::istringstream rows(product_log_since(s,mark));std::string row;
    while(std::getline(rows,row))if(row.ends_with('}')&&e::quoted(row,"\"type\":","placement")){
        ++placements;placement=row;if(e::unique_value(row,"\"native_callsite_claimed\":")=="true")++claims;
        exact_scope=exact_scope&&geometry_history_receipt(s,row,"placement")&&e::number(row,"\"generation\":")==0;
    }
    const auto actual=visible(target.hwnd),after_position=geometry_history_positioning(target.hwnd);
    const auto before_calls=e::number(prior,"\"native_callsite_claims\":"),after_calls=e::number(status,"\"native_callsite_claims\":");
    const bool positive=source_key&&e::number(receipt,"\"key\":")==source_key&&e::number(placement,"\"key\":")==source_key&&
        claims==1&&placements==1&&exact_scope&&e::unique_value(placement,"\"native_attempted\":")=="true"&&
        e::unique_value(placement,"\"native_succeeded\":")=="true"&&e::unique_value(placement,"\"identity_valid\":")=="true"&&
        e::unique_value(placement,"\"exact\":")=="true"&&placement.find("\"actual\":"+rect(expected))!=placement.npos&&
        placement.find("\"target\":"+rect(expected))!=placement.npos&&actual==expected;
    const bool negative=!claims&&!placements&&actual==before&&after_position==before_position;
    s.log.check(label,observed&&target.owns()&&other.owns()&&visible(other.hwnd)==other_before&&
        geometry_history_positioning(other.hwnd)==other_position&&GetForegroundWindow()==target.hwnd&&before_calls&&after_calls&&
        *after_calls==*before_calls+(allowed?1:0)&&(allowed?positive:negative),
        "\"slot\":"+quote(utf8(target.slot))+",\"image\":"+quote(utf8(target.image.wstring()))+",\"allowed\":"+(allowed?"true":"false")+
        ",\"action\":"+quote(utf8(name))+",\"before_visible\":"+rect(*before)+",\"before_positioning\":"+rect(*before_position)+
        ",\"expected_visible\":"+rect(allowed?expected:*before)+",\"actual_visible\":"+(actual?rect(*actual):"null")+
        ",\"source_key\":"+std::to_string(source_key)+",\"other_before_visible\":"+rect(*other_before)+
        ",\"other_after_visible\":"+(visible(other.hwnd)?rect(*visible(other.hwnd)):"null")+
        ",\"other_actual_visible_and_positioning_unchanged\":"+(visible(other.hwnd)==other_before&&geometry_history_positioning(other.hwnd)==other_position?"true":"false")+
        ",\"placement_count\":"+std::to_string(placements)+",\"native_claims\":"+std::to_string(claims)+
        ",\"actual_receipt\":"+quote(receipt)+",\"actual_placement\":"+quote(placement)+",\"actual_status\":"+quote(status)+
        ",\"single_screen_negative_source_bridge_only\":"+(before->l<0?"true":"false")+",\"negative_work_origin_or_multi_display\":\"NOT_RUN\"");
}
panebind::core::v1::Settings rules_path_save(State& s,const panebind::core::v1::Settings& wanted){
    return owned_rule_restore_rules_GUI(s,wanted);
}
void owned_rules_path_case(State& s){
    namespace v=panebind::core::v1;settings_guest_scope(s);const auto original=chain_settings(s);
    if(s.suite!="RulesPathOnly"||!original.enabled)throw std::runtime_error("rules_path_enabled_exact_suite_required");
    const auto pid=s.product_pid;const auto birth=process_birth(s.process);RulesPathTarget a{s,L"A"},b{s,L"B"};
    try{
        a.create();b.create();
        s.log.check("rules_path_actual_same_basename_distinct_full_image",a.owns()&&b.owns()&&a.pid!=b.pid&&
            a.image.filename()==b.image.filename()&&!equal(a.image.wstring(),b.image.wstring())&&
            gui_class(a.hwnd)==gui_class(b.hwnd)&&hash(a.image)==hash(b.image),
            "\"A_path\":"+quote(utf8(a.image.wstring()))+",\"B_path\":"+quote(utf8(b.image.wstring()))+
            ",\"same_hash_is_not_same_path_identity\":true,\"two_exact_new_blank_roots\":true");
        const auto initial_a=visible(a.hwnd),initial_position=geometry_history_positioning(a.hwnd);
        s.log.emit("rules_path_actual_initial_A_geometry","\"visible\":"+(initial_a?rect(*initial_a):"null")+
            ",\"positioning\":"+(initial_position?rect(*initial_position):"null")+
            ",\"negative_source_observed\":"+(initial_a&&initial_position&&initial_a->l<0&&initial_position->l<0?"true":"false")+
            ",\"single_screen_source_subset_not_negative_work_origin\":true");
        rules_path_command(b,a,L"Center",true,"rules_path_B_baseline_actual_Center");
        rules_path_command(a,b,L"Center",true,"rules_path_A_signed_source_actual_Center");
        // A is the actual last admitted foreground app before the public GUI.
        rules_path_front(a);auto gui=gui_open(s);gui_page_select(s,gui,4);
        const auto filter=gui_find(s,gui,L"Button",L"仅限制为该窗口类名");
        if(gui_read(s,gui,filter,BM_GETCHECK)!=BST_UNCHECKED)gui_click(s,gui,filter);
        s.log.check("rules_path_public_GUI_no_class_filter",gui_read(s,gui,filter,BM_GETCHECK)==BST_UNCHECKED);
        settings_guest_activate(s,gui,GetDlgItem(gui_page(s,gui),115));
        settings_guest_acknowledge(s,gui,L"已将重新核验的应用加入排除草稿");gui_save_enter(s,gui);
        const auto excluded=chain_settings(s);auto expected=original;expected.generation=excluded.generation;
        expected.rules.push_back({utf8(a.image.wstring()),"",true,0xffffffffU});
        s.log.check("rules_path_actual_GUI_added_only_A_full_path",excluded==expected&&excluded.generation>original.generation,
            "\"A_image\":"+quote(utf8(a.image.wstring()))+",\"actual_rule_count\":"+std::to_string(excluded.rules.size())+
            ",\"actual_path_fact_not_basename_substitute\":true,\"automated_guest_driver\":true");
        rules_path_command(a,b,L"Center",false,"rules_path_A_full_path_exclusion_real_rejection_zero_calls");
        rules_path_command(b,a,L"MoveRight",true,"rules_path_same_name_B_other_path_still_actual_Move");
        rules_path_save(s,original);rules_path_command(a,b,L"MoveRight",true,"rules_path_A_removed_exclusion_actual_recovery");
        auto name_rule=original;name_rule.rules.push_back({"panebind-rule-app.exe","",true,0xffffffffU});rules_path_save(s,name_rule);
        rules_path_command(a,b,L"Center",false,"rules_path_basename_rule_A_real_rejection");
        rules_path_command(b,a,L"Center",false,"rules_path_basename_rule_B_real_rejection");
        // The optional per-app command bit is exercised through the same GUI
        // and actual commands, not only the rule parser/helper.
        auto masked=original;masked.rules.push_back({utf8(a.image.wstring()),"",false,0xffffffffU&~16U});rules_path_save(s,masked);
        rules_path_command(a,b,L"MoveRight",false,"rules_path_per_app_command_mask_A_zero_calls");
        rules_path_command(b,a,L"MoveLeft",true,"rules_path_per_app_mask_does_not_disable_B");
        rules_path_save(s,original);rules_path_command(a,b,L"MoveRight",true,"rules_path_A_final_public_rule_removal_recovery");
        chain_check_product(s,pid,birth);const bool a_closed=a.close(),b_closed=b.close();
        s.log.check("rules_path_two_exact_children_normal_exit",a_closed&&b_closed);
        auto restored=chain_settings(s);auto wanted=original;wanted.generation=restored.generation;
        s.log.check("rules_path_original_configuration_restored",restored==wanted);
        s.log.emit("rules_path_component","\"result\":\"PASS\",\"scope\":\"public_from_current_app_full_path_basename_and_feature_mask\","
            "\"same_product_process\":true,\"arbitrary_apps_or_host\":false,\"whole_version_PASS\":false,\"human_UAT\":\"NOT_RUN\"");
    }catch(...){
        pause_routing_failed_case_input(s);bool restored=false;try{rules_path_save(s,original);restored=chain_settings(s).rules==original.rules;}catch(...){}
        const bool a_closed=a.close(),b_closed=b.close();
        s.log.emit("rules_path_failed_cleanup","\"original_rules_restored\":"+std::string(restored?"true":"false")+
            ",\"A_normal_exit\":"+(a_closed?"true":"false")+",\"B_normal_exit\":"+(b_closed?"true":"false")+
            ",\"original_failure_preserved\":true,\"no_user_application_or_forced_close\":true");throw;
    }
}
