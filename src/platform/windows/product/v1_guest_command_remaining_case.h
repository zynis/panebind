#pragma once

// Guest TEST ONLY. Include after the existing settings, product-chain and
// geometry-history helpers. This is one finite public-command chain, not a
// second command implementation, a tray substitute or a host input entry.
[[maybe_unused]] void owned_command_remaining_case(State& s){
    namespace e=product::guest::up_evidence_detail;
    settings_guest_scope(s);const auto original=chain_settings(s);
    const auto pid=s.product_pid;const auto birth=process_birth(s.process);const auto binary=hash(s.product);
    const auto settings_path=s.product.parent_path()/L"settings.ini";const auto saved_bytes=read(settings_path);
    const bool geometry_only=s.suite=="CommandsGeometryOnly";
    if((s.suite!="CommandsRemainderOnly"&&!geometry_only)||!original.enabled||!original.rules.empty()||original.step_dip!=10||
       s.left||s.control||(GetAsyncKeyState(VK_LBUTTON)&0x8000)||(GetAsyncKeyState(VK_CONTROL)&0x8000)||
       (GetAsyncKeyState(VK_SHIFT)&0x8000)||(GetAsyncKeyState(VK_MENU)&0x8000)||
       binary.empty()||binary!=hash(s.input_product)||saved_bytes.empty())
        throw std::runtime_error("command_remaining_exact_idle_default_step_guest_required");
    LayoutFixtures set{s};set.create();
    try{
        const auto work=chain_work(set.frames[0].hwnd);const HWND source=set.frames[0].hwnd;
        const auto dpi=GetDpiForWindow(source);const auto monitor=MonitorFromWindow(source,MONITOR_DEFAULTTONULL);
        const LONG step=static_cast<LONG>((static_cast<std::uint64_t>(original.step_dip)*dpi+48)/96);
        if(work.width()<1200||work.height()<850||!dpi||!monitor||step<2||step>64)
            throw std::runtime_error("command_remaining_declared_single_display_geometry_unavailable");
        const LayoutLimit limit{300,240,work.width(),work.height()};
        const Rect initial{work.l+200,work.t+180,work.l+660,work.t+520};
        set.setup({initial},{limit});const auto key=geometry_history_source_key(set,initial);
        const auto native=geometry_history_positioning(source);
        WINDOWPLACEMENT normal{sizeof(normal)};
        if(!native||!GetWindowPlacement(source,&normal)||IsZoomed(source)||IsIconic(source))
            throw std::runtime_error("command_remaining_actual_normal_placement_unknown");

        if(!geometry_only){
        const auto placement_json=[&]{
            WINDOWPLACEMENT value{sizeof(value)};const bool known=set.owns(0)&&GetWindowPlacement(source,&value);
            const auto current=visible(source),positioning=geometry_history_positioning(source);
            return "\"window_placement_known\":"+std::string(known?"true":"false")+
                ",\"show_cmd\":"+(known?std::to_string(value.showCmd):"null")+
                ",\"normal_position\":"+(known?rect({value.rcNormalPosition.left,value.rcNormalPosition.top,
                    value.rcNormalPosition.right,value.rcNormalPosition.bottom}):"null")+
                ",\"actual_visible\":"+(current?rect(*current):"null")+
                ",\"actual_positioning\":"+(positioning?rect(*positioning):"null")+
                ",\"actual_iconic\":"+(IsIconic(source)?"true":"false")+
                ",\"actual_zoomed\":"+(IsZoomed(source)?"true":"false");
        };
        // A state command logs dispatch, not a WindowWriter placement. Require
        // actual native state as well, and never label ShowWindowAsync's
        // unlogged API return as an observed native call receipt.
        const auto state_command=[&](const wchar_t* action,const char* label,const auto& state){
            chain_check_product(s,pid,birth);settings_guest_scope(s);
            if(!set.owns(0)||s.left||s.control)throw std::runtime_error("command_remaining_state_identity_lost");
            const auto mark=read_product(s).size();std::string command;
            const auto name=utf8(action);const bool sent=launch(s,L"--command "+std::wstring(action),true);
            const bool observed=sent&&wait([&]{command=record(s,mark,"command");
                return geometry_history_receipt(s,command,"command")&&e::quoted(command,"\"action\":",name)&&
                    e::number(command,"\"key\":")==key&&set.owns(0)&&state();});
            s.log.check(label,observed,"\"action\":"+quote(name)+",\"source_HWND\":"+
                std::to_string(reinterpret_cast<std::uintptr_t>(source))+",\"source_key\":"+std::to_string(key)+
                ",\"actual_command\":"+quote(command)+","+placement_json()+
                ",\"ShowWindowAsync_API_return_observed\":false,\"test_ShowWindow_or_SetFocus_used\":false");
            const auto sequence=e::number(command,"\"sequence\":");
            if(!sequence)throw std::runtime_error("command_remaining_actual_command_sequence_unknown");
            (void)geometry_history_status(s,*sequence);
            unsigned commands{},placements{};std::istringstream rows(product_log_since(s,mark));std::string row;
            while(std::getline(rows,row)){
                if(geometry_history_receipt(s,row,"command"))++commands;
                if(geometry_history_receipt(s,row,"placement"))++placements;
            }
            s.log.check(std::string(label)+"_single_dispatch_not_fake_placement",commands==1&&!placements&&set.owns(0)&&state(),
                "\"actual_commands\":"+std::to_string(commands)+",\"placement_rows\":"+std::to_string(placements)+
                ",\"state_API_not_counted_as_WindowWriter_placement\":true");
        };
        const auto restored=[&]{WINDOWPLACEMENT current{sizeof(current)};
            return set.owns(0)&&!IsZoomed(source)&&!IsIconic(source)&&visible(source)==initial&&
                geometry_history_positioning(source)==native&&GetWindowPlacement(source,&current)&&
                EqualRect(&current.rcNormalPosition,&normal.rcNormalPosition)&&
                GetDpiForWindow(source)==dpi&&MonitorFromWindow(source,MONITOR_DEFAULTTONULL)==monitor;};

        chain_front(set,0);
        state_command(L"Maximize","command_remaining_actual_Maximize",[&]{
            WINDOWPLACEMENT value{sizeof(value)};return IsZoomed(source)&&!IsIconic(source)&&GetWindowPlacement(source,&value);});
        state_command(L"Restore","command_remaining_Maximize_Restore_same_identity",restored);
        chain_front(set,0); // Normal test setup BEFORE minimize, never a rescue.
        state_command(L"Minimize","command_remaining_actual_Minimize",[&]{
            WINDOWPLACEMENT value{sizeof(value)};return IsIconic(source)&&GetWindowPlacement(source,&value);});

        // Do not activate/restore the minimized source from the test. A direct
        // Restore otherwise prefers another eligible foreground application.
        // Use the PUBLIC settings request to make the product's own UI the
        // foreground; its normal command handler then revalidates last_target.
        // No gui_open activation fallback, SetFocus, ShowWindow or private
        // target/WM_COMMAND is used to manufacture this precondition.
        const auto public_settings=[&](HWND expected_root){
            const auto mark=read_product(s).size();SettingsGui gui;std::string receipt;
            const bool sent=launch(s,L"--settings",true);
            const bool ready=sent&&wait([&]{
                receipt=record(s,mark,"settings_open");
                if(!geometry_history_receipt(s,receipt,"settings_open")||e::unique_value(receipt,"\"created\":")!="true")return false;
                struct Search {DWORD pid{};HWND root{};DWORD tid{};unsigned count{};} query{pid};
                EnumWindows([](HWND h,LPARAM data)->BOOL{auto& q=*reinterpret_cast<Search*>(data);DWORD actual{};
                    const auto tid=GetWindowThreadProcessId(h,&actual);
                    if(actual==q.pid&&gui_class(h)==L"PaneBindV1SettingsWindow"&&IsWindowVisible(h)){
                        q.root=h;q.tid=tid;++q.count;}return TRUE;},reinterpret_cast<LPARAM>(&query));
                gui={query.root,query.root?GetDlgItem(query.root,100):nullptr,query.tid};
                return query.count==1&&(!expected_root||gui.root==expected_root)&&gui.root&&gui.tab&&
                    gui_identity(s,gui,gui.tab)&&GetForegroundWindow()==gui.root&&gui_identity(s,gui,gui_focus(gui));
            });
            s.log.check("command_remaining_public_settings_natural_foreground",ready,
                "\"actual_product_receipt\":"+quote(receipt)+",\"settings_HWND\":"+
                std::to_string(reinterpret_cast<std::uintptr_t>(gui.root))+
                ",\"driver_activation_fallback\":false,\"private_target_or_test_restore\":false");
            return gui;
        };
        const auto restore_gui=public_settings(nullptr);
        s.log.check("command_remaining_Restore_uses_real_own_UI_last_target_precondition",
            IsIconic(source)&&set.owns(0)&&GetForegroundWindow()==restore_gui.root&&
            read(settings_path)==saved_bytes&&chain_settings(s)==original,
            "\"source_still_minimized\":true,\"expected_source_key\":"+std::to_string(key)+
            ",\"test_source_activation_between_Minimize_and_Restore\":false");
        state_command(L"Restore","command_remaining_Minimize_Restore_same_last_target",restored);
        const auto close_gui=public_settings(restore_gui.root);
        gui_press(s,close_gui,VK_ESCAPE);
        s.log.check("command_remaining_public_settings_Esc_leaves_configuration_unchanged",
            wait([&]{return !IsWindow(close_gui.root);})&&chain_settings(s)==original&&read(settings_path)==saved_bytes&&restored());

        // Only exact new test roots participate in this z-order observation.
        // Traversing HWND links reads no title/content and controls no unknown
        // window. The product's own stronger bottom-observed receipt is still
        // mandatory; relative order alone cannot replace it.
        const Rect other=translated(initial,80,60);set.setup({initial,other},{limit,limit});
        chain_front(set,0);const auto before_positioning=geometry_history_positioning(source);
        const auto other_positioning=geometry_history_positioning(set.frames[1].hwnd);
        const auto below=[](HWND lower,HWND upper){HWND cursor=GetWindow(upper,GW_HWNDNEXT);
            for(unsigned n=0;cursor&&n<256;++n,cursor=GetWindow(cursor,GW_HWNDNEXT))if(cursor==lower)return true;
            return false;};
        const auto source_visual=native_visual(source),other_visual=native_visual(set.frames[1].hwnd);
        s.log.check("command_remaining_Lower_exact_nontopmost_order_before",set.owns(0)&&set.owns(1)&&
            before_positioning&&other_positioning&&visible(source)==initial&&visible(set.frames[1].hwnd)==other&&
            source_visual&&other_visual&&source_visual->known&&other_visual->known&&
            !source_visual->topmost&&!other_visual->topmost&&below(set.frames[1].hwnd,source));
        const auto lower_mark=read_product(s).size();std::string lower,lower_command;
        const bool sent_lower=launch(s,L"--command Lower",true);
        const bool lowered=sent_lower&&wait([&]{lower=record(s,lower_mark,"lower_result");lower_command=record(s,lower_mark,"command");
            return geometry_history_receipt(s,lower,"lower_result")&&e::number(lower,"\"key\":")==key&&
                e::number(lower,"\"call_id\":").value_or(0)>0&&e::unique_value(lower,"\"native_attempted\":")=="true"&&
                e::unique_value(lower,"\"native_completion_known\":")=="true"&&e::unique_value(lower,"\"native_succeeded\":")=="true"&&
                e::unique_value(lower,"\"bottom_observed\":")=="true"&&e::quoted(lower,"\"reason\":","lower_native_bottom_observed")&&
                geometry_history_receipt(s,lower_command,"command")&&
                e::number(lower_command,"\"key\":")==key&&e::quoted(lower_command,"\"action\":","Lower")&&
                set.owns(0)&&set.owns(1)&&GetWindow(source,GW_HWNDNEXT)==nullptr&&below(source,set.frames[1].hwnd);});
        s.log.check("command_remaining_Lower_actual_native_and_zorder",lowered&&visible(source)==initial&&
            visible(set.frames[1].hwnd)==other&&geometry_history_positioning(source)==before_positioning&&
            geometry_history_positioning(set.frames[1].hwnd)==other_positioning,
            "\"actual_lower_result\":"+quote(lower)+",\"actual_command\":"+quote(lower_command)+
            ",\"actual_source_next_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(GetWindow(source,GW_HWNDNEXT)))+
            ",\"actual_relative_owned_order_reversed\":"+(below(source,set.frames[1].hwnd)?"true":"false")+
            ",\"geometry_and_other_window_unchanged\":true");
        chain_check_product(s,pid,birth);
        }

        // The existing actual-command helper distinguishes a real clamp call
        // from an already-exact no-op and checks before/after native counters.
        const LONG inset=std::max<LONG>(1,step/2);
        const Rect near_min{work.l+200,work.t+180,work.l+500+inset,work.t+420+inset};
        set.setup({near_min},{limit});const auto size_key=geometry_history_source_key(set,near_min);
        if(size_key!=key)throw std::runtime_error("command_remaining_existing_source_identity_changed_after_states");
        const Rect width_min{near_min.l,near_min.t,near_min.l+300,near_min.b};
        const Rect both_min{near_min.l,near_min.t,near_min.l+300,near_min.t+240};
        geometry_history_command(set,key,L"WidthLess",width_min);
        geometry_history_command(set,key,L"HeightLess",both_min);
        geometry_history_command(set,key,L"WidthLess",both_min,true);
        geometry_history_command(set,key,L"HeightLess",both_min,true);

        // MicroMove has a translation policy, NOT a work-area clamp. Cross
        // the work edge only slightly while this exact window still belongs
        // to the same physical display; do not claim a NextDisplay test.
        const LONG edge_inset=std::max<LONG>(1,step/3);
        const Rect edge{work.l+edge_inset,work.t+150,work.l+edge_inset+400,work.t+470};
        set.setup({edge},{limit});const auto edge_key=geometry_history_source_key(set,edge);
        if(edge_key!=key)throw std::runtime_error("command_remaining_source_identity_changed_before_edge_step");
        const Rect outside=translated(edge,-step,0);
        geometry_history_command(set,key,L"MoveLeft",outside);
        s.log.check("command_remaining_actual_same_display_microMove_work_edge_policy",
            outside.l<work.l&&outside.r>work.l&&set.owns(0)&&visible(source)==outside&&
            MonitorFromWindow(source,MONITOR_DEFAULTTONULL)==monitor&&GetDpiForWindow(source)==dpi,
            "\"work\":"+rect(work)+",\"actual_DPI\":"+std::to_string(dpi)+
            ",\"step_DIP\":"+std::to_string(original.step_dip)+",\"actual_step_pixels\":"+std::to_string(step)+
            ",\"before\":"+rect(edge)+",\"after\":"+rect(outside)+
            ",\"policy\":\"same_display_absolute_step_not_work_area_clamped\",\"cross_display_PASS_inferred\":false");
        geometry_history_command(set,key,L"MoveRight",edge);
        chain_check_product(s,pid,birth);
        s.log.check("command_remaining_original_configuration_and_binary_unchanged",chain_settings(s)==original&&
            read(settings_path)==saved_bytes&&hash(s.product)==binary&&binary==hash(s.input_product));
        s.log.check("command_remaining_exact_test_roots_closed",set.close());
        s.log.emit("command_remaining_component","\"result\":\"PASS_SCOPED_SINGLE_DISPLAY_COMMANDS\",\"suite\":"+quote(s.suite)+
            ",\"Maximize_Minimize_Restore_same_identity\":"+std::string(geometry_only?"\"NOT_RUN_IN_THIS_SUITE\"":"true")+
            ",\"Lower_actual_native_zorder\":"+std::string(geometry_only?"\"NOT_RUN_IN_THIS_SUITE\"":"true")+","+
            "\"Width_Height_minimum_and_noop\":true,\"microMove_same_display_work_edge\":true,"
            "\"tray_menu\":\"NOT_RUN\",\"NextDisplay\":\"NOT_RUN\",\"Full_suite\":\"NOT_RUN\","
            "\"human_UAT\":\"NOT_RUN\",\"whole_M09_F03_or_version_PASS_inferred\":false");
    }catch(...){
        // Only this test's blank roots are destroyed. Do not manufacture a
        // Restore or activate a source after a failed state/identity assertion.
        try{if(!set.closed)set.close();}catch(...){}throw;
    }
}
