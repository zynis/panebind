#pragma once

// TEST ONLY. Include after the existing rule, interruption and layout helpers.
// These two finite cases use real owned roots and the unchanged product. A
// test cleanup UP after member loss is not a normal completion of that drag.
bool remaining_group_receipt(State& s,std::string_view row,std::string_view type){
    namespace e=product::guest::up_evidence_detail;
    return !row.empty()&&row.front()=='{'&&row.back()=='}'&&
        e::quoted(row,"\"schema\":","panebind-v1/1")&&e::quoted(row,"\"source_sha\":",s.product_source)&&
        e::quoted(row,"\"type\":",type)&&e::number(row,"\"process_id\":")==s.product_pid;
}
std::uint64_t remaining_group_start(State& s,const Gesture& g,std::size_t members,const char* label){
    namespace e=product::guest::up_evidence_detail;const auto row=record(s,g.offset,"gesture_start",g.generation);
    const auto key=e::number(row,"\"key\":");
    s.log.check(label,remaining_group_receipt(s,row,"gesture_start")&&key&&*key&&
        e::number(row,"\"generation\":")==g.generation&&e::unique_value(row,"\"ctrl\":")=="true"&&
        e::unique_value(row,"\"resize\":")=="false"&&e::number(row,"\"group_members\":")==members,
        "\"generation\":"+std::to_string(g.generation)+",\"actual_START\":"+quote(row));return key.value_or(0);
}
std::uint64_t remaining_group_actual_key(State& s,const Gesture& g,Rect actual){
    namespace e=product::guest::up_evidence_detail;std::uint64_t key{};bool ambiguous=false;
    std::istringstream rows(product_log_since(s,g.offset));std::string row;
    while(std::getline(rows,row))if(remaining_group_receipt(s,row,"placement")&&
        e::number(row,"\"generation\":")==g.generation&&e::unique_value(row,"\"native_callsite_claimed\":")=="true"&&
        e::unique_value(row,"\"native_attempted\":")=="true"&&e::unique_value(row,"\"identity_valid\":")=="true"&&
        e::unique_value(row,"\"exact\":")=="true"&&row.find("\"actual\":"+rect(actual)+",")!=row.npos){
        const auto value=e::number(row,"\"key\":");if(!value||!*value||(key&&key!=*value))ambiguous=true;else key=*value;
    }
    if(!key||ambiguous)throw std::runtime_error("remaining_group_actual_member_receipt_key_unknown");return key;
}
void remaining_group_native_scope(State& s,const Gesture& g,const std::vector<std::uint64_t>& allowed,const char* label){
    namespace e=product::guest::up_evidence_detail;std::vector<unsigned> calls(allowed.size());unsigned foreign{},invalid{};
    std::istringstream rows(product_log_since(s,g.offset));std::string row;
    while(std::getline(rows,row))if(row.find("\"type\":\"placement\"")!=row.npos&&
        e::number(row,"\"generation\":")==g.generation&&e::unique_value(row,"\"native_callsite_claimed\":")=="true"){
        const auto key=e::number(row,"\"key\":");const auto found=key?std::find(allowed.begin(),allowed.end(),*key):allowed.end();
        if(found==allowed.end())++foreign;else ++calls[static_cast<std::size_t>(found-allowed.begin())];
        if(!remaining_group_receipt(s,row,"placement")||e::unique_value(row,"\"native_attempted\":")!="true"||
           e::unique_value(row,"\"identity_valid\":")!="true"||e::unique_value(row,"\"exact\":")!="true")++invalid;
    }
    s.log.check(label,!allowed.empty()&&!foreign&&!invalid&&std::all_of(calls.begin(),calls.end(),[](unsigned n){return n>0;}),
        "\"generation\":"+std::to_string(g.generation)+",\"actual_allowed_member_count\":"+std::to_string(allowed.size())+
        ",\"foreign_native_claims\":"+std::to_string(foreign)+",\"invalid_native_receipts\":"+std::to_string(invalid));
}
void owned_remaining_group_membership_case(State& s){
    namespace v=panebind::core::v1;namespace e=product::guest::up_evidence_detail;
    if(s.suite!="RemainingMoveContractsOnly"||!desktop()||s.left||s.control)
        throw std::runtime_error("remaining_group_exact_released_guest_suite_required");
    settings_guest_scope(s);const auto original=chain_settings(s);const auto pid=s.product_pid;
    const auto birth=process_birth(s.process);const auto binary=hash(s.product);
    if(!original.enabled)throw std::runtime_error("remaining_group_enabled_product_required");
    auto config=original;config.glue=true;config.magnet_move=false;config.magnet_resize=false;config.docking=false;
    config.preview=false;config.feedback=false;config.drag_transparency=false;config.shake=false;
    const auto baseline_config=chain_gui_save(s,config);std::vector<v::Rule> added_rules;
    try{
        {
            LayoutFixtures set{s};set.create();try{
                const auto work=chain_work(set.frames[0].hwnd);const auto initial=pause_routing_initial(work);
                const LayoutLimit limit{120,100,work.width(),work.height()};set.setup(initial,{limit,limit,limit});
                chain_check_product(s,pid,birth);const auto g=layout_down(set,0,HTCAPTION,true);
                const auto source_key=remaining_group_start(s,g,3,"remaining_group_three_actual_members_before_close");
                auto moved=initial;for(auto& r:moved)r=translated(r,40,25);
                layout_phase(set,0,g,40,25,moved,"remaining_group_three_actual_writes_before_close");
                const auto follower_key=remaining_group_actual_key(s,g,moved[2]);const HWND source=set.frames[0].hwnd,closed=set.frames[2].hwnd;
                s.log.check("remaining_group_close_only_live_exact_follower",source_key&&follower_key!=source_key&&
                    set.owns(0)&&set.owns(1)&&set.owns(2)&&GetForegroundWindow()==source&&s.left&&s.control&&
                    !s.left_up_attempted&&actual_up(s,g.offset,g.generation).empty());
                LARGE_INTEGER trigger{};if(!QueryPerformanceCounter(&trigger)||trigger.QuadPart<=0)
                    throw std::runtime_error("remaining_group_member_close_clock_unknown");
                const bool destroyed=set.destroy(2);const bool gone=destroyed&&!IsWindow(closed);set.active=2;
                s.log.check("remaining_group_actual_follower_DestroyWindow",gone&&set.owns(0)&&set.owns(1),
                    "\"generation\":"+std::to_string(g.generation)+",\"closed_member\":2,\"closed_HWND\":"+
                    std::to_string(reinterpret_cast<std::uintptr_t>(closed))+",\"closed_key\":"+std::to_string(follower_key)+
                    ",\"actual_PID\":"+std::to_string(GetCurrentProcessId())+",\"actual_owner_TID\":"+std::to_string(set.thread)+
                    ",\"trigger_before_qpc\":"+std::to_string(trigger.QuadPart)+",\"source_closed\":false");
                // A catalog notification is not a new writer. If no end has
                // yet been observed, one real continuation must encounter the
                // invalid frozen member at the unchanged batch preflight.
                const bool already_ended=!record(s,g.offset,"gesture_end",g.generation).empty();
                if(!already_ended){
                    if(!set.owns(0)||!set.owns(1)||GetForegroundWindow()!=source||!s.left||!s.control||
                       visible(source)!=moved[0]||visible(set.frames[1].hwnd)!=moved[1]||s.left_up_attempted)
                        throw std::runtime_error("remaining_group_close_continuation_context_changed");
                    cursor(s,{g.anchor.x+70,g.anchor.y+45});
                }
                s.log.emit("remaining_group_member_loss_continuation","\"generation\":"+std::to_string(g.generation)+
                    ",\"test_cursor_sent\":"+(already_ended?"false":"true")+",\"already_observed_end\":"+(already_ended?"true":"false")+
                    ",\"old_member_rebased_or_replaced\":false");
                const auto ended=interruption_end(s,g,v::End::SourceLost,false,source);std::string resource;
                const bool stopped=wait([&]{std::istringstream rows(product_log_since(s,g.offset));std::string row;
                    while(std::getline(rows,row))if(remaining_group_receipt(s,row,"gesture_input_resource")&&
                        e::number(row,"\"generation\":")==g.generation&&e::quoted(row,"\"fact\":","isolation_gone"))resource=row;
                    const auto overlay=e::number(resource,"\"overlay\":");GUITHREADINFO gui{sizeof(gui)};
                    return overlay&&*overlay&&!IsWindow(reinterpret_cast<HWND>(static_cast<std::uintptr_t>(*overlay)))&&
                        e::number(resource,"\"source\":")==reinterpret_cast<std::uintptr_t>(source)&&
                        e::unique_value(resource,"\"overlay_destroyed\":")=="true"&&e::unique_value(resource,"\"hotkey_unregistered\":")=="true"&&
                        e::unique_value(resource,"\"detach_succeeded\":")=="true"&&GetGUIThreadInfo(set.thread,&gui)&&
                        !gui.hwndCapture&&!gui.hwndMoveSize&&!gui.hwndMenuOwner;},1500);
                s.log.check("remaining_group_close_aborts_before_single_cleanup_UP",stopped&&s.left&&!s.left_up_attempted&&
                    remaining_group_receipt(s,ended,"gesture_end")&&visible(source)==moved[0]&&visible(set.frames[1].hwnd)==moved[1],
                    "\"actual_end\":"+quote(ended)+",\"actual_resource_gone\":"+quote(resource)+",\"NormalUp_inferred\":false");
                button(s,false);if(s.control)key(s,VK_CONTROL,false);
                const auto cleanup=abort_cleanup_up_receipt(s,g.offset,g.generation);
                s.log.check("remaining_group_close_unique_actual_abort_cleanup_UP",cleanup.matched&&!s.left,cleanup.json());
                interruption_audit_calls(s,g,static_cast<std::uint64_t>(trigger.QuadPart),std::nullopt);interruption_idle(s,set,true);
                auto actual=pause_routing_actual(set);const auto next=layout_down(set,0,HTCAPTION,true);
                const auto next_key=remaining_group_start(s,next,2,"remaining_group_after_close_actual_surviving_pair");
                s.log.check("remaining_group_close_following_generation_is_new",next.generation>g.generation&&!IsWindow(closed));
                for(auto& r:actual)r=translated(r,25,15);
                layout_phase(set,0,next,25,15,actual,"remaining_group_after_close_actual_pair_Move");
                const auto other_key=remaining_group_actual_key(s,next,actual[1]);layout_up(set,next,actual);
                remaining_group_native_scope(s,next,{next_key,other_key},"remaining_group_no_closed_member_in_new_native_batch");
                chain_check_product(s,pid,birth);s.log.check("remaining_group_close_exact_owned_resources_closed",set.close());
            }catch(...){pause_routing_failed_case_input(s);throw;}
        }
        {
            LayoutFixtures set{s};set.create(true);try{
                const auto work=chain_work(set.frames[0].hwnd);
                if(work.width()<1000||work.height()<700)throw std::runtime_error("remaining_group_rule_work_area_too_small");
                const std::vector<Rect> initial{{work.l+200,work.t+160,work.l+500,work.t+480},
                    {work.l+500,work.t+160,work.l+780,work.t+480}};
                const LayoutLimit limit{120,100,work.width(),work.height()};set.setup(initial,{limit,limit});
                const HWND source=set.frames[0].hwnd,target=set.frames[1].hwnd;const auto alias=set.storage->target1_alias;
                s.log.check("remaining_group_rule_exact_distinct_classes",set.owns(0)&&set.owns(1)&&!alias.empty()&&
                    gui_class(source)==s.class_name&&gui_class(target)==alias);
                auto g=layout_down(set,0,HTCAPTION,true);remaining_group_start(s,g,2,"remaining_group_rule_baseline_actual_pair");
                auto actual=initial;for(auto& r:actual)r=translated(r,35,25);
                layout_phase(set,0,g,35,25,actual,"remaining_group_rule_baseline_actual_pair_Move");
                const auto baseline_generation=g.generation;const auto before_rule=actual;
                // Exercise the reachable user path while the group is still
                // active. show_settings() conservatively retires the gesture
                // before creating/activating its UI; saving a rule is later.
                std::string active_capture;std::istringstream capture_rows(product_log_since(s,g.offset));std::string capture_row;
                while(std::getline(capture_rows,capture_row))if(remaining_group_receipt(s,capture_row,"gesture_input_resource")&&
                    e::number(capture_row,"\"generation\":")==g.generation&&e::quoted(capture_row,"\"fact\":","capture_ready"))active_capture=capture_row;
                const auto overlay_value=e::number(active_capture,"\"overlay\":");
                const HWND overlay=overlay_value?reinterpret_cast<HWND>(static_cast<std::uintptr_t>(*overlay_value)):nullptr;
                DWORD overlay_pid{},source_pid{};const auto overlay_tid=overlay?GetWindowThreadProcessId(overlay,&overlay_pid):0;
                const auto source_tid=GetWindowThreadProcessId(source,&source_pid);
                GUITHREADINFO source_gui{sizeof(source_gui)},owner_gui{sizeof(owner_gui)};
                const bool source_gui_known=GetGUIThreadInfo(source_tid,&source_gui)!=FALSE;
                const bool owner_gui_known=overlay_tid&&GetGUIThreadInfo(overlay_tid,&owner_gui)!=FALSE;
                const SHORT left_before_settings=GetAsyncKeyState(VK_LBUTTON),ctrl_before_settings=GetAsyncKeyState(VK_CONTROL);
                s.log.check("remaining_group_rule_active_exact_capture_before_settings",set.owns(0)&&set.owns(1)&&
                    source_pid==GetCurrentProcessId()&&source_tid==set.thread&&overlay&&IsWindow(overlay)&&overlay_pid==pid&&
                    e::number(active_capture,"\"source\":")==reinterpret_cast<std::uintptr_t>(source)&&
                    e::number(active_capture,"\"source_pid\":")==source_pid&&e::number(active_capture,"\"source_tid\":")==source_tid&&
                    e::number(active_capture,"\"capture_actual_after\":")==overlay_value&&
                    e::unique_value(active_capture,"\"attach_succeeded\":")=="true"&&source_gui_known&&owner_gui_known&&
                    source_gui.hwndCapture==overlay&&owner_gui.hwndCapture==overlay&&GetForegroundWindow()==source&&
                    s.left&&s.control&&(left_before_settings&0x8000)&&(ctrl_before_settings&0x8000)&&
                    !s.left_up_attempted&&actual_up(s,g.offset,g.generation).empty()&&pause_routing_actual(set)==actual,
                    "\"generation\":"+std::to_string(g.generation)+",\"actual_capture_ready\":"+quote(active_capture)+
                    ",\"source_GUI_known\":"+(source_gui_known?"true":"false")+",\"owner_GUI_known\":"+(owner_gui_known?"true":"false")+
                    ",\"actual_source_capture\":"+std::to_string(reinterpret_cast<std::uintptr_t>(source_gui.hwndCapture))+
                    ",\"actual_owner_capture\":"+std::to_string(reinterpret_cast<std::uintptr_t>(owner_gui.hwndCapture))+
                    ",\"actual_foreground\":"+std::to_string(reinterpret_cast<std::uintptr_t>(GetForegroundWindow()))+
                    ",\"async_left_SHORT\":"+std::to_string(left_before_settings)+",\"async_Ctrl_SHORT\":"+std::to_string(ctrl_before_settings)+
                    ",\"driver_left_outstanding\":"+(s.left?"true":"false")+",\"single_cleanup_UP_not_yet_attempted\":"+(!s.left_up_attempted?"true":"false"));
                const auto open_mark=read_product(s).size();LARGE_INTEGER settings_trigger{};
                if(!QueryPerformanceCounter(&settings_trigger)||settings_trigger.QuadPart<=0)
                    throw std::runtime_error("remaining_group_active_settings_trigger_clock_unknown");
                const bool requested=launch(s,L"--settings",true);std::string settings_open,settings_end,settings_gone;
                const bool settings_consumed=requested&&wait([&]{
                    settings_end=record(s,g.offset,"gesture_end",g.generation);settings_open=record(s,open_mark,"settings_open");
                    const auto end_sequence=e::number(settings_end,"\"sequence\":");const auto open_sequence=e::number(settings_open,"\"sequence\":");
                    return remaining_group_receipt(s,settings_end,"gesture_end")&&e::number(settings_end,"\"generation\":")==g.generation&&
                        e::number(settings_end,"\"end\":")==static_cast<unsigned>(v::End::Paused)&&
                        e::unique_value(settings_end,"\"normal_up\":")=="false"&&e::unique_value(settings_end,"\"pending_retired\":")=="true"&&
                        remaining_group_receipt(s,settings_open,"settings_open")&&e::unique_value(settings_open,"\"created\":")=="true"&&
                        end_sequence&&*end_sequence&&open_sequence&&*open_sequence>*end_sequence;},1500);
                s.log.check("remaining_group_active_settings_entry_actual_Paused_before_UI",settings_consumed&&s.left&&!s.left_up_attempted,
                    "\"generation\":"+std::to_string(g.generation)+",\"public_settings_command_requested\":"+(requested?"true":"false")+
                    ",\"actual_end\":"+quote(settings_end)+",\"actual_settings_open\":"+quote(settings_open)+
                    ",\"settings_entry_termination_not_rule_save_causality\":true,\"NormalUp_inferred\":false");
                const bool settings_resources_gone=wait([&]{std::istringstream rows(product_log_since(s,g.offset));std::string row;
                    while(std::getline(rows,row))if(remaining_group_receipt(s,row,"gesture_input_resource")&&
                        e::number(row,"\"generation\":")==g.generation&&e::quoted(row,"\"fact\":","isolation_gone"))settings_gone=row;
                    GUITHREADINFO now{sizeof(now)};return remaining_group_receipt(s,settings_gone,"gesture_input_resource")&&
                        e::number(settings_gone,"\"source\":")==reinterpret_cast<std::uintptr_t>(source)&&
                        e::number(settings_gone,"\"overlay\":")==overlay_value&&overlay&&!IsWindow(overlay)&&
                        e::unique_value(settings_gone,"\"overlay_destroyed\":")=="true"&&e::unique_value(settings_gone,"\"hotkey_unregistered\":")=="true"&&
                        e::unique_value(settings_gone,"\"detach_succeeded\":")=="true"&&GetGUIThreadInfo(set.thread,&now)&&
                        !now.hwndCapture&&!now.hwndMoveSize&&!now.hwndMenuOwner;},1500);
                POINT cleanup_cursor{};const bool cleanup_cursor_known=GetCursorPos(&cleanup_cursor)!=FALSE;
                s.log.check("remaining_group_active_settings_resources_gone_before_single_cleanup_UP",settings_resources_gone&&
                    s.left&&!s.left_up_attempted&&set.owns(0)&&set.owns(1)&&pause_routing_actual(set)==actual,
                    "\"actual_resource_gone\":"+quote(settings_gone)+",\"cleanup_cursor_known\":"+(cleanup_cursor_known?"true":"false")+
                    ",\"actual_cleanup_cursor\":"+(cleanup_cursor_known?"["+std::to_string(cleanup_cursor.x)+","+std::to_string(cleanup_cursor.y)+"]":"null")+
                    ",\"actual_foreground\":"+std::to_string(reinterpret_cast<std::uintptr_t>(GetForegroundWindow()))+
                    ",\"extra_cursor_or_UP_used\":false,\"actual_legacy_UP_or_NormalUp_not_inferred\":true");
                button(s,false);if(s.control)key(s,VK_CONTROL,false);
                const auto settings_cleanup=abort_cleanup_up_receipt(s,g.offset,g.generation);
                s.log.check("remaining_group_active_settings_actual_unique_abort_cleanup_UP",settings_cleanup.matched&&!s.left,settings_cleanup.json());
                interruption_audit_calls(s,g,static_cast<std::uint64_t>(settings_trigger.QuadPart),std::nullopt);interruption_idle(s,set,true);
                // Do not activate a window or send a second --settings while
                // held. Attach read-only to the exact UI just opened. Cancel
                // its unchanged draft before selecting the alias target for
                // the existing Add Revalidated Foreground/Class workflow.
                struct SettingsSearch{DWORD pid{};HWND root{};DWORD tid{};unsigned count{};} search{pid};
                EnumWindows([](HWND h,LPARAM data)->BOOL{auto& q=*reinterpret_cast<SettingsSearch*>(data);DWORD p{};
                    const auto tid=GetWindowThreadProcessId(h,&p);if(p==q.pid&&gui_class(h)==L"PaneBindV1SettingsWindow"&&IsWindowVisible(h)){
                        q.root=h;q.tid=tid;++q.count;}return TRUE;},reinterpret_cast<LPARAM>(&search));
                SettingsGui opened{search.root,search.root?GetDlgItem(search.root,100):nullptr,search.tid};
                s.log.check("remaining_group_active_settings_exact_UI_after_abort",search.count==1&&opened.root&&opened.tab&&
                    gui_identity(s,opened,opened.tab)&&GetForegroundWindow()==opened.root&&chain_settings(s)==baseline_config&&
                    pause_routing_actual(set)==before_rule,
                    "\"actual_settings_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(opened.root))+
                    ",\"actual_settings_TID\":"+std::to_string(opened.tid)+",\"driver_activation_fallback_used\":false");
                gui_press(s,opened,VK_ESCAPE);
                s.log.check("remaining_group_active_settings_unchanged_draft_cancelled",wait([&]{return !IsWindow(opened.root);})&&
                    chain_settings(s)==baseline_config&&pause_routing_actual(set)==before_rule,
                    "\"actual_Escape_input\":true,\"configuration_saved_by_first_settings_entry\":false");
                chain_front(set,1);const auto caption=layout_hit(set,1,HTCAPTION);
                if(!caption)throw std::runtime_error("remaining_group_rule_target_caption_unknown");
                cursor(s,*caption);(void)layout_wait_sent([]{return false;},100);
                s.log.check("remaining_group_rule_exact_target_before_GUI",set.owns(1)&&GetForegroundWindow()==target&&
                    WindowFromPoint(*caption)==target&&visible(target)==actual[1]&&IsWindowVisible(target));
                auto gui=gui_open(s);gui_page_select(s,gui,4);const auto filter=gui_find(s,gui,L"Button",L"仅限制为该窗口类名");
                if(gui_read(s,gui,filter,BM_GETCHECK)!=BST_CHECKED)gui_click(s,gui,filter);
                s.log.check("remaining_group_rule_actual_class_filter_checked",wait([&]{return gui_read(s,gui,filter,BM_GETCHECK)==BST_CHECKED;}));
                settings_guest_activate(s,gui,GetDlgItem(gui_page(s,gui),115));
                settings_guest_acknowledge(s,gui,L"已将重新核验的应用加入排除草稿");gui_save_enter(s,gui);
                const auto excluded=settings_guest_configuration(s);std::array<wchar_t,32768> image{};
                const auto length=GetModuleFileNameW(nullptr,image.data(),static_cast<DWORD>(image.size()));
                if(!length||length>=image.size())throw std::runtime_error("remaining_group_actual_driver_image_unknown");
                const auto path=utf8(std::wstring(image.data(),length));
                const bool exact_rule=excluded.rules.size()==baseline_config.rules.size()+1&&
                    std::equal(baseline_config.rules.begin(),baseline_config.rules.end(),excluded.rules.begin())&&
                    excluded.rules.back().excluded&&excluded.rules.back().window_class==utf8(alias)&&
                    v::settings_detail::ascii_fold(excluded.rules.back().exe)==v::settings_detail::ascii_fold(path)&&
                    excluded.rules.back().feature_mask==0xffffffffU;
                auto expected=baseline_config;expected.rules=excluded.rules;expected.generation=excluded.generation;
                s.log.check("remaining_group_GUI_exact_alias_rule_not_whole_exe",exact_rule&&excluded==expected&&
                    set.owns(0)&&set.owns(1)&&pause_routing_actual(set)==before_rule&&IsWindowVisible(target),
                    "\"actual_path\":"+quote(path)+",\"actual_target_class\":"+quote(utf8(alias))+
                    ",\"GUI_foreground_change_attributed_to_rule_invalidation\":false");added_rules=excluded.rules;
                g=layout_down(set,0,HTCAPTION,true);const auto source_key=remaining_group_start(s,g,1,"remaining_group_rule_excluded_target_not_selected");
                s.log.check("remaining_group_rule_new_generation_after_save",g.generation>baseline_generation);
                actual[0]=translated(before_rule[0],-60,20);
                layout_phase(set,0,g,-60,20,actual,"remaining_group_rule_only_eligible_source_moves");
                actual=before_rule;layout_phase(set,0,g,0,0,actual,"remaining_group_rule_original_DOWN_return_restores_real_adjacency");layout_up(set,g,actual);
                remaining_group_native_scope(s,g,{source_key},"remaining_group_rule_zero_excluded_target_native_claims");
                s.log.check("remaining_group_rule_target_visible_unchanged_not_recreated",set.frames[0].hwnd==source&&set.frames[1].hwnd==target&&
                    set.owns(0)&&set.owns(1)&&IsWindowVisible(target)&&pause_routing_actual(set)==before_rule);
                const auto excluded_generation=g.generation;const auto removed=owned_rule_restore_rules_GUI(s,original);
                s.log.check("remaining_group_actual_GUI_rule_removed_same_exact_roots",removed.rules==original.rules&&
                    set.frames[0].hwnd==source&&set.frames[1].hwnd==target&&set.owns(0)&&set.owns(1)&&pause_routing_actual(set)==before_rule);
                g=layout_down(set,0,HTCAPTION,true);const auto restored_source_key=remaining_group_start(s,g,2,"remaining_group_rule_removed_actual_pair_reselected");
                s.log.check("remaining_group_rule_restored_generation_is_new",g.generation>excluded_generation);
                for(auto& r:actual)r=translated(r,25,15);
                layout_phase(set,0,g,25,15,actual,"remaining_group_rule_removed_both_actual_members_move");
                const auto restored_target_key=remaining_group_actual_key(s,g,actual[1]);layout_up(set,g,actual);
                remaining_group_native_scope(s,g,{restored_source_key,restored_target_key},"remaining_group_removed_rule_actual_pair_native_scope");
                interruption_idle(s,set,true);chain_check_product(s,pid,birth);
                s.log.check("remaining_group_rule_exact_roots_and_alias_closed",set.close()&&set.storage->alias_ever_registered&&
                    set.storage->alias_unregister_attempted&&set.storage->alias_unregistered&&set.storage->alias_unregistration_tid==set.thread);
            }catch(...){pause_routing_failed_case_input(s);throw;}
        }
        const auto restored=chain_gui_save(s,original);auto expected=original;expected.generation=restored.generation;
        chain_check_product(s,pid,birth);s.log.check("remaining_group_original_configuration_and_binary_retained",restored==expected&&hash(s.product)==binary);
        s.log.emit("remaining_group_membership_component","\"result\":\"PASS\",\"scope\":\"active_follower_close_and_active_group_settings_entry_then_GUI_class_rule_next_group_selection\","
            "\"same_product_process\":true,\"active_group_settings_entry_aborts_before_rule_save\":true,\"close_abort_cleanup_UP_is_NormalUp\":false,\"GUI_focus_loss_is_rule_causality\":false,"
            "\"old_failed_evidence_preserved\":true,\"whole_M06_or_version_PASS\":false,\"real_applications\":\"NOT_RUN\",\"human_uat\":\"NOT_RUN\"");
    }catch(...){
        pause_routing_failed_case_input(s);bool restored=false;std::string reason;
        try{const auto current=settings_guest_configuration(s);
            if(current.rules!=original.rules&&(added_rules.empty()||current.rules!=added_rules))
                throw std::runtime_error("remaining_group_unknown_rules_not_overwritten");
            if(current.rules!=original.rules)owned_rule_restore_rules_GUI(s,original);
            const auto actual=chain_gui_save(s,original);auto expected=original;expected.generation=actual.generation;restored=actual==expected;
        }catch(const std::exception& error){reason=error.what();}
        s.log.emit("remaining_group_failure_configuration_cleanup","\"original_configuration_restored\":"+std::string(restored?"true":"false")+
            ",\"reason\":"+quote(reason)+",\"original_failure_preserved\":true,\"function_PASS_inferred\":false");throw;
    }
}
