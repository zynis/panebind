#pragma once

// TEST ONLY. These three finite groups use the real mapping editors and
// production input/command handlers; custom mappings are not contextual F04.
void mapping_remaining_scope(State& s){
    settings_guest_scope(s);
    if(s.suite!="MappingRemainderOnly"||s.left||s.control||!chain_settings(s).enabled)
        throw std::runtime_error("mapping_remaining_exact_enabled_released_guest_required");
    for(const int code:std::array{VK_LBUTTON,VK_RBUTTON,VK_MBUTTON,VK_XBUTTON1,VK_XBUTTON2,VK_CONTROL,VK_SHIFT,VK_MENU,VK_LWIN,VK_RWIN})
        if(GetAsyncKeyState(code)&0x8000)throw std::runtime_error("mapping_remaining_initial_input_not_released");
}
bool mapping_remaining_modifiers(unsigned wanted){
    const unsigned actual=((GetAsyncKeyState(VK_MENU)&0x8000)?1u:0u)|((GetAsyncKeyState(VK_CONTROL)&0x8000)?2u:0u)|
        ((GetAsyncKeyState(VK_SHIFT)&0x8000)?4u:0u)|(((GetAsyncKeyState(VK_LWIN)|GetAsyncKeyState(VK_RWIN))&0x8000)?8u:0u);
    return actual==wanted;
}
bool mapping_remaining_hit(LayoutFixtures& set,POINT point,int region,Rect geometry,unsigned modifiers){
    const auto h=set.frames[0].hwnd;POINT actual{};DWORD_PTR hit{};GUITHREADINFO gui{sizeof(gui)};
    return desktop()&&set.owns(0)&&visible(h)==geometry&&GetForegroundWindow()==h&&mapping_remaining_modifiers(modifiers)&&
        GetCursorPos(&actual)&&actual.x==point.x&&actual.y==point.y&&WindowFromPoint(actual)==h&&
        SendMessageTimeoutW(h,WM_NCHITTEST,0,MAKELPARAM(static_cast<short>(point.x),static_cast<short>(point.y)),
            SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&hit)&&static_cast<int>(hit)==region&&GetGUIThreadInfo(set.thread,&gui)&&
        !gui.hwndCapture&&!gui.hwndMoveSize&&!gui.hwndMenuOwner;
}
void mapping_remaining_restore(LayoutFixtures& set,const panebind::core::v1::Settings& original,const NativeVisual& visual){
    auto& s=set.state;
    if(s.left||s.control||!mapping_remaining_modifiers(0)||(GetAsyncKeyState(VK_MBUTTON)&0x8000))
        throw std::runtime_error("mapping_restore_actual_input_not_released");
    const auto mark=read_product(s).size();
    s.log.check("mapping_remaining_pause_restores_actual_visual",launch(s,L"--pause",true)&&wait([&]{
        const auto row=record(s,mark,"status");return geometry_history_receipt(s,row,"status")&&
            product::guest::up_evidence_detail::unique_value(row,"\"enabled\":")=="false"&&set.owns(0)&&native_visual(set.frames[0].hwnd)==visual;
    }));
    (void)chain_gui_save_mouse(s,original.mouse_bindings);
    // chain_gui_save intentionally does not edit the enabled toggle. Resume
    // through the same real command/status barrier as the old five-button
    // case; a saved mouse fragment alone cannot restore running authority.
    const auto resume_mark=read_product(s).size();
    s.log.check("mapping_remaining_actual_resume_restores_original_enabled",launch(s,L"--command Pause",true)&&wait([&]{
        const auto row=record(s,resume_mark,"status");return geometry_history_receipt(s,row,"status")&&
            product::guest::up_evidence_detail::unique_value(row,"\"enabled\":")==std::optional<std::string_view>(original.enabled?"true":"false");
    }));
    const auto restored=chain_gui_save(s,original);auto expected=original;
    expected.generation=restored.generation;s.log.check("mapping_remaining_original_config_and_visual_restored",restored==expected&&
        set.owns(0)&&native_visual(set.frames[0].hwnd)==visual);
}
struct MappingRemainingTap {std::uint64_t generation{},down_send_tick{};std::string down,up,command;};
MappingRemainingTap mapping_remaining_middle_pair(LayoutFixtures& set,int native_region,unsigned modifiers,
    panebind::core::v1::Action action,const NativeVisual& expected,DWORD max_prepare=3500,std::uint64_t down_deadline=0){
    namespace e=product::guest::up_evidence_detail;namespace v=panebind::core::v1;auto& s=set.state;
    const HWND h=set.frames[0].hwnd;const auto geometry=visible(h);const auto before_visual=native_visual(h);
    if(!geometry||!before_visual||!before_visual->known||s.left||s.control||(GetAsyncKeyState(VK_MBUTTON)&0x8000))
        throw std::runtime_error("mapping_middle_exact_released_context_unknown");
    chain_front(set,0);const auto point=layout_hit(set,0,native_region);if(!point)throw std::runtime_error("mapping_custom_native_region_missing");
    // A client hover makes the following preparation receipt new even when
    // repeating the same corner. It changes no geometry or product authority.
    RECT client{};POINT center{};if(!GetClientRect(h,&client))throw std::runtime_error("mapping_client_rect_unknown");
    center={(client.left+client.right)/2,(client.top+client.bottom)/2};
    if(!ClientToScreen(h,&center))throw std::runtime_error("mapping_client_point_unknown");
    const auto hover_mark=read_product(s).size();cursor(s,center);std::string client_hover;
    const bool hover_observed=wait([&]{client_hover=record(s,hover_mark,"input_preparation");return
        geometry_history_receipt(s,client_hover,"input_preparation")&&e::number(client_hover,"\"root\":")==reinterpret_cast<std::uintptr_t>(h)&&
        e::number(client_hover,"\"hit\":")==reinterpret_cast<std::uintptr_t>(h)&&e::number(client_hover,"\"actual_hit_test\":")==HTCLIENT&&
        e::unique_value(client_hover,"\"region\":")=="-1"&&
        client_hover.find("\"point\":["+std::to_string(center.x)+","+std::to_string(center.y)+"],")!=client_hover.npos;
    },max_prepare);
    if(!hover_observed)throw std::runtime_error("mapping_middle_actual_client_hover_boundary_missing");
    const auto preparation_mark=read_product(s).size();cursor(s,*point);std::string preparation;
    const bool prepared=chain_stretch_preparation(set,*point,native_region,*geometry,preparation_mark,max_prepare,preparation);
    s.log.check("mapping_custom_actual_preparation_and_native_hit",prepared&&mapping_remaining_hit(set,*point,native_region,*geometry,modifiers),
        "\"native_region\":"+std::to_string(native_region)+",\"modifiers\":"+std::to_string(modifiers)+
        ",\"actual_preparation\":"+quote(preparation)+",\"custom_mouse_mapping_not_contextual_stretch\":true");
    if(down_deadline&&GetTickCount64()>down_deadline){
        s.log.emit("mapping_double_window_not_executed","\"result\":\"NOT_RUN\",\"reason\":\"real_second_DOWN_window_expired_before_input\",\"second_DOWN_sent\":false");
        throw std::runtime_error("mapping_double_window_expired_before_second_DOWN");
    }
    const auto key=e::number(preparation,"\"key\":");const auto mark=read_product(s).size();const auto starts=set.frames[0].starts.load();
    const auto client_down=set.frames[0].client_down[2].load(),client_up=set.frames[0].client_up[2].load();
    MappingRemainingTap result;bool pending{},up_attempted{};std::uint64_t down_qpc{},up_qpc{};
    const auto send=[&](bool pressed){
        if(!desktop()||(pressed&&(!mapping_remaining_hit(set,*point,native_region,*geometry,modifiers)||pending))||(!pressed&&(!pending||up_attempted)))
            throw std::runtime_error("mapping_middle_single_input_context_or_ledger_invalid");
        INPUT packet{};packet.type=INPUT_MOUSE;packet.mi.dwFlags=pressed?MOUSEEVENTF_MIDDLEDOWN:MOUSEEVENTF_MIDDLEUP;
        LARGE_INTEGER qpc{};if(!QueryPerformanceCounter(&qpc)||qpc.QuadPart<=0)throw std::runtime_error("mapping_middle_input_clock_unknown");
        if(pressed){down_qpc=static_cast<std::uint64_t>(qpc.QuadPart);result.down_send_tick=GetTickCount64();}
        else{up_attempted=true;up_qpc=static_cast<std::uint64_t>(qpc.QuadPart);}
        UINT inserted=UINT_MAX;DWORD error{};try{inject(packet,&inserted,&error);}catch(...){
            s.log.emit("mapping_middle_input_insertion_failed","\"requested\":"+quote(pressed?"DOWN":"UP")+
                ",\"SendInput_return\":"+(inserted==UINT_MAX?"null":std::to_string(inserted))+",\"error\":"+std::to_string(error)+",\"retransmitted\":false");throw;}
        pending=pressed;
    };
    try{
        send(true);const bool mapped=wait([&]{
            result.down=record(s,mark,"input_button_observed");result.generation=e::number(result.down,"\"generation\":").value_or(0);
            result.command=record(s,mark,"mapped_command_completed",result.generation);
            return geometry_history_receipt(s,result.down,"input_button_observed")&&e::quoted(result.down,"\"kind\":","DOWN")&&
                e::quoted(result.down,"\"source\":","WH_MOUSE_LL")&&e::quoted(result.down,"\"route\":","mapped")&&
                e::quoted(result.down,"\"action\":",v::action_name(action))&&e::number(result.down,"\"button\":")==3&&
                e::number(result.down,"\"source_key\":")==key&&e::number(result.down,"\"hook_observed_qpc\":").value_or(0)>=down_qpc&&
                result.generation&&geometry_history_receipt(s,result.command,"mapped_command_completed")&&
                e::number(result.command,"\"generation\":")==result.generation&&e::quoted(result.command,"\"action\":",v::action_name(action))&&
                e::unique_value(result.command,"\"handler_invoked\":")=="true"&&e::unique_value(result.command,"\"handler_returned\":")=="true"&&
                native_visual(h)==expected;
        },1500);
        send(false);const bool released=wait([&]{result.up=actual_up(s,mark,result.generation,3);return
            geometry_history_receipt(s,result.up,"input_button_observed")&&e::quoted(result.up,"\"source\":","WH_MOUSE_LL")&&
            e::quoted(result.up,"\"kind\":","UP")&&e::number(result.up,"\"generation\":")==result.generation&&
            e::number(result.up,"\"source_key\":")==key&&e::number(result.up,"\"hook_observed_qpc\":").value_or(0)>=up_qpc;
        },1500);
        GUITHREADINFO gui{sizeof(gui)};unsigned mapped_downs{},commands{};std::string row;std::istringstream rows(product_log_since(s,mark));
        while(std::getline(rows,row)){if(geometry_history_receipt(s,row,"input_button_observed")&&e::quoted(row,"\"kind\":","DOWN"))++mapped_downs;
            if(geometry_history_receipt(s,row,"mapped_command_completed"))++commands;}
        s.log.check("mapping_custom_actual_middle_pair_and_complete_visual",mapped&&released&&mapped_downs==1&&commands==1&&set.owns(0)&&
            visible(h)==geometry&&native_visual(h)==expected&&mapping_remaining_modifiers(modifiers)&&
            set.frames[0].starts.load()==starts&&set.frames[0].client_down[2].load()==client_down&&set.frames[0].client_up[2].load()==client_up&&
            GetGUIThreadInfo(set.thread,&gui)&&!gui.hwndCapture&&!gui.hwndMoveSize&&!gui.hwndMenuOwner&&
            record(s,mark,"gesture_start").empty()&&record(s,mark,"placement").empty()&&record(s,mark,"native_move_cancel").empty(),
            "\"native_region\":"+std::to_string(native_region)+",\"modifiers\":"+std::to_string(modifiers)+",\"generation\":"+std::to_string(result.generation)+
            ",\"actual_DOWN\":"+quote(result.down)+",\"actual_matching_UP\":"+quote(result.up)+",\"actual_command\":"+quote(result.command)+
            ",\"expected_native_visual\":{\"known\":"+(expected.known?"true":"false")+",\"layered\":"+(expected.layered?"true":"false")+
            ",\"alpha\":"+std::to_string(expected.alpha)+",\"color\":"+std::to_string(expected.color)+",\"flags\":"+std::to_string(expected.flags)+
            ",\"topmost\":"+(expected.topmost?"true":"false")+"},\"complete_native_visual_tuple_exact\":"+(native_visual(h)==expected?"true":"false")+
            ",\"NormalUp_or_native_success_from_handler_inferred\":false");
        return result;
    }catch(...){
        if(pending&&!up_attempted)try{send(false);}catch(...){}
        s.log.emit("mapping_middle_failure_cleanup","\"own_middle_DOWN_pending\":"+std::string(pending?"true":"false")+
            ",\"single_UP_attempted\":"+(up_attempted?"true":"false")+",\"delivery_or_cleanup_PASS_inferred\":false");throw;
    }
}
void owned_mapping_regions_case(State& s){
    namespace v=panebind::core::v1;mapping_remaining_scope(s);const auto original=chain_settings(s);
    const auto pid=s.product_pid;const auto birth=process_birth(s.process);LayoutFixtures set{s};set.create();const auto work=chain_work(set.frames[0].hwnd);
    if(work.width()<900||work.height()<650)throw std::runtime_error("mapping_regions_actual_work_area_too_small");
    const Rect source{work.l+220,work.t+180,work.l+700,work.t+500};set.setup({source});const auto visual=native_visual(set.frames[0].hwnd);
    if(!visual||!visual->known)throw std::runtime_error("mapping_regions_original_visual_unknown");
    try{
        std::vector<v::MouseBinding> bindings;for(int region=1;region<=8;++region)bindings.push_back({region,3,1,0,v::Action::ToggleTopmost});
        chain_gui_save_mouse(s,bindings);
        constexpr std::array regions{HTLEFT,HTRIGHT,HTTOP,HTBOTTOM,HTTOPLEFT,HTTOPRIGHT,HTBOTTOMLEFT,HTBOTTOMRIGHT};
        for(const auto region:regions){const auto before=native_visual(set.frames[0].hwnd);if(!before)throw std::runtime_error("mapping_regions_visual_lost");
            auto expected=*before;expected.topmost=!expected.topmost;(void)mapping_remaining_middle_pair(set,region,0,v::Action::ToggleTopmost,expected);}
        mapping_remaining_restore(set,original,*visual);chain_check_product(s,pid,birth);s.log.check("mapping_regions_exact_resources_closed",set.close());
        s.log.emit("mapping_regions_component","\"result\":\"PASS\",\"custom_regions\":[1,2,3,4,5,6,7,8],\"button\":3,\"clicks\":1,\"modifiers\":0,\"actual_GUI_editor\":true,\"other_buttons_retested\":false");
    }catch(...){try{if(!(GetAsyncKeyState(VK_MBUTTON)&0x8000))mapping_remaining_restore(set,original,*visual);}catch(...){}
        try{if(!set.closed)set.close();}catch(...){}throw;}
}
void owned_mapping_modifier_double_case(State& s){
    namespace v=panebind::core::v1;mapping_remaining_scope(s);const auto original=chain_settings(s);
    const auto pid=s.product_pid;const auto birth=process_birth(s.process);LayoutFixtures set{s};set.create();const auto work=chain_work(set.frames[0].hwnd);
    if(work.width()<900||work.height()<650)throw std::runtime_error("mapping_modifier_actual_work_area_too_small");
    const Rect source{work.l+220,work.t+180,work.l+700,work.t+500};set.setup({source});const auto visual=native_visual(set.frames[0].hwnd);
    if(!visual||!visual->known)throw std::runtime_error("mapping_modifier_original_visual_unknown");bool shift{},shift_up_attempted{};
    try{
        chain_gui_save_mouse(s,{{8,3,1,4,v::Action::ToggleTopmost},{8,3,2,4,v::Action::OpacityDown}});
        shift=true;key(s,VK_SHIFT,true);auto expected=*visual;expected.topmost=!expected.topmost;
        const auto single=mapping_remaining_middle_pair(set,HTBOTTOMRIGHT,4,v::Action::ToggleTopmost,expected);
        const auto boundary=single.down_send_tick+GetDoubleClickTime()+1;(void)wait([&]{return GetTickCount64()>boundary;},GetDoubleClickTime()+200);
        expected.topmost=!expected.topmost;const auto first=mapping_remaining_middle_pair(set,HTBOTTOMRIGHT,4,v::Action::ToggleTopmost,expected);
        const int before_alpha=expected.layered&&(expected.flags&LWA_ALPHA)?expected.alpha:255;
        expected.layered=true;expected.known=true;expected.alpha=static_cast<BYTE>(before_alpha<=16?before_alpha:std::max(16,before_alpha-32));expected.flags|=LWA_ALPHA;
        const auto second=mapping_remaining_middle_pair(set,HTBOTTOMRIGHT,4,v::Action::OpacityDown,expected,GetDoubleClickTime(),first.down_send_tick+GetDoubleClickTime());
        shift_up_attempted=true;key(s,VK_SHIFT,false);shift=false;
        s.log.check("mapping_Shift_middle_single_and_double_distinct_actions",second.generation>first.generation&&first.generation>single.generation,
            "\"single_generation\":"+std::to_string(single.generation)+",\"double_first_generation\":"+std::to_string(first.generation)+
            ",\"double_second_generation\":"+std::to_string(second.generation)+",\"first_click_actual_mapped_UP\":true,\"only_double_tuple_has_OpacityDown\":true");
        mapping_remaining_restore(set,original,*visual);chain_check_product(s,pid,birth);s.log.check("mapping_modifier_exact_resources_closed",set.close());
        s.log.emit("mapping_modifier_double_component","\"result\":\"PASS\",\"region\":8,\"button\":3,\"modifiers\":4,\"single_action\":\"ToggleTopmost\",\"double_action\":\"OpacityDown\",\"NormalUp_inferred\":false");
    }catch(...){if(shift&&!shift_up_attempted)try{shift_up_attempted=true;key(s,VK_SHIFT,false);shift=false;}catch(...){}
        try{if(!(GetAsyncKeyState(VK_MBUTTON)&0x8000))mapping_remaining_restore(set,original,*visual);}catch(...){}
        try{if(!set.closed)set.close();}catch(...){}throw;}
}
void owned_mapping_hotkey_edits_case(State& s){
    namespace e=product::guest::up_evidence_detail;namespace v=panebind::core::v1;mapping_remaining_scope(s);const auto original=chain_settings(s);
    const auto pid=s.product_pid;const auto birth=process_birth(s.process);LayoutFixtures set{s};set.create();const auto work=chain_work(set.frames[0].hwnd);
    if(work.width()<900||work.height()<650)throw std::runtime_error("mapping_hotkey_actual_work_area_too_small");
    const Rect source{work.l+220,work.t+180,work.l+700,work.t+500};set.setup({source});WORD test_key{};bool registered{};constexpr int registration=0x50b2;
    for(const WORD candidate:std::array<WORD,3>{VK_F9,VK_F10,VK_F8})if(std::none_of(original.hotkeys.begin(),original.hotkeys.end(),[&](const auto& h){return h.modifiers==3&&h.key==candidate;})){test_key=candidate;break;}
    if(!test_key)throw std::runtime_error("mapping_edit_unused_nonreserved_key_missing");
    const auto release=[&]{if(!registered)return true;const bool returned=UnregisterHotKey(nullptr,registration)!=FALSE;if(returned)registered=false;return returned;};
    const auto send_hotkey=[&]{
        const std::array<WORD,3> codes{VK_CONTROL,VK_MENU,test_key};std::array<bool,3> down_attempted{},up_attempted{};
        const auto released=[&]{return !s.control&&mapping_remaining_modifiers(0)&&!(GetAsyncKeyState(test_key)&0x8000);};
        const auto up_once=[&](std::size_t index){if(down_attempted[index]&&!up_attempted[index]){
            up_attempted[index]=true;key(s,codes[index],false);
        }};
        if(!released())throw std::runtime_error("mapping_hotkey_actual_keys_not_initially_released");
        try{
            for(std::size_t n=0;n<codes.size();++n){
                if(n==2&&(!set.owns(0)||GetForegroundWindow()!=set.frames[0].hwnd||!mapping_remaining_modifiers(3)))
                    throw std::runtime_error("mapping_hotkey_chord_or_exact_source_changed");
                down_attempted[n]=true;key(s,codes[n],true);
            }
            for(std::size_t n=codes.size();n>0;--n)up_once(n-1);
            const bool actual_released=released();s.log.check("mapping_hotkey_single_chord_actual_release",actual_released,
                "\"key\":"+std::to_string(test_key)+",\"modifiers\":3,\"each_UP_attempted_at_most_once\":true,\"actual_keys_released\":"+(actual_released?"true":"false"));
        }catch(...){
            for(std::size_t n=codes.size();n>0;--n)try{up_once(n-1);}catch(...){}
            s.log.emit("mapping_hotkey_failure_single_release_ledger","\"actual_keys_released\":"+std::string(released()?"true":"false")+
                ",\"Ctrl_UP_attempted\":"+(up_attempted[0]?"true":"false")+",\"Alt_UP_attempted\":"+(up_attempted[1]?"true":"false")+
                ",\"test_key_UP_attempted\":"+(up_attempted[2]?"true":"false")+",\"UP_retransmitted\":false,\"cleanup_PASS_inferred\":false");throw;
        }
    };
    try{
        auto configured=original;configured.hotkeys.push_back({3,test_key,v::Action::MoveRight});configured=chain_gui_save(s,configured);
        const UINT dpi=GetDpiForWindow(set.frames[0].hwnd);if(!dpi)throw std::runtime_error("mapping_hotkey_source_DPI_unknown");
        const LONG step=MulDiv(configured.step_dip,static_cast<int>(dpi),96);if(step<=0)throw std::runtime_error("mapping_hotkey_nonpositive_step");
        const auto invoke=[&](const char* action,Rect expected){chain_front(set,0);const auto mark=read_product(s).size();send_hotkey();
            std::string command,placement;const bool done=wait([&]{command=record(s,mark,"command");placement=chain_any_placement(s,mark,expected);
                return geometry_history_receipt(s,command,"command")&&e::quoted(command,"\"action\":",action)&&!placement.empty()&&visible(set.frames[0].hwnd)==expected;});
            (void)geometry_history_status(s);unsigned commands{},placements{};std::istringstream rows(product_log_since(s,mark));std::string row;
            while(std::getline(rows,row)){if(geometry_history_receipt(s,row,"command"))++commands;if(geometry_history_receipt(s,row,"placement"))++placements;}
            s.log.check("mapping_same_hotkey_one_press_one_actual_command",done&&commands==1&&placements==1&&set.owns(0),
                "\"key\":"+std::to_string(test_key)+",\"modifiers\":3,\"action\":"+quote(action)+",\"actual_command\":"+quote(command)+",\"actual_placement\":"+quote(placement));};
        const auto right=translated(source,step,0);invoke("MoveRight",right);
        configured.hotkeys.back().action=v::Action::MoveDown;configured=chain_gui_save(s,configured);const auto lower=translated(right,0,step);invoke("MoveDown",lower);
        configured.hotkeys.pop_back();configured=chain_gui_save(s,configured);
        SetLastError(0);registered=RegisterHotKey(nullptr,registration,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,test_key)!=FALSE;const DWORD register_error=registered?0:GetLastError();
        s.log.check("mapping_deleted_tuple_really_released_OS_registration",registered,
            "\"key\":"+std::to_string(test_key)+",\"modifiers\":3,\"actual_RegisterHotKey_return\":"+(registered?"true":"false")+",\"error\":"+std::to_string(register_error)+",\"registration_id\":"+std::to_string(registration));
        s.log.check("mapping_test_registration_removed",release());
        const auto fragment=[](const v::Settings& value,bool mouse){if(mouse)return chain_mouse_mapping_fragment(value);std::wstring out;const auto serialized=v::serialize(value);
            for(const auto row:v::settings_detail::split(serialized,'\n'))if(row.starts_with("hotkey_count=")||row.starts_with("hotkey.")){
                for(const char c:row)out+=static_cast<wchar_t>(c);out+=L"\r\n";}return out;};
        for(const bool mouse:{false,true}){
            const auto before=chain_settings(s);const auto path=s.product.parent_path()/L"settings.ini";const auto saved=read(path),saved_hash=hash(path);
            auto duplicate=before;if(mouse){duplicate.mouse_bindings={{8,3,1,4,v::Action::ToggleTopmost},{8,3,1,4,v::Action::OpacityDown}};}
            else{duplicate.hotkeys.push_back({3,test_key,v::Action::MoveRight});duplicate.hotkeys.push_back({3,test_key,v::Action::MoveDown});}
            const auto draft=fragment(duplicate,mouse);auto gui=gui_open(s);gui_page_select(s,gui,3);HWND edit{};unsigned editors{};
            for(HWND child=GetWindow(gui_page(s,gui),GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT))
                if(gui_identity(s,gui,child)&&gui_class(child)==L"Edit"&&chain_edit_text(s,gui,child).starts_with(mouse?L"mouse_count=":L"hotkey_count=")){edit=child;++editors;}
            if(editors!=1)throw std::runtime_error("mapping_duplicate_exact_public_editor_missing");
            chain_type_fragment(s,gui,edit,draft);settings_guest_activate(s,gui,GetDlgItem(gui.root,IDOK));
            settings_guest_acknowledge(s,gui,mouse?L"鼠标映射冲突":L"快捷键映射冲突",L"PaneBind：设置未更改");
            s.log.check("mapping_duplicate_tuple_Chinese_rejection_preserves_runtime",chain_settings(s)==before&&read(path)==saved&&hash(path)==saved_hash&&
                gui_identity(s,gui,edit)&&chain_edit_text(s,gui,edit)==draft,
                "\"tuple\":"+quote(mouse?"mouse_region_button_click_modifiers":"hotkey_modifiers_key")+",\"actual_Chinese_modal\":true,\"running_generation_unchanged\":true");
            // Repair only our rejected draft through the same actual editor.
            chain_type_fragment(s,gui,edit,fragment(before,mouse));gui_save_enter(s,gui);const auto restored=chain_settings(s);auto expected=before;expected.generation=restored.generation;
            s.log.check("mapping_rejected_draft_repaired_by_actual_GUI",restored==expected);
        }
        const auto restored=chain_gui_save(s,original);auto expected=original;expected.generation=restored.generation;
        s.log.check("mapping_hotkey_original_configuration_restored",restored==expected&&visible(set.frames[0].hwnd)==lower);chain_check_product(s,pid,birth);
        s.log.check("mapping_hotkey_exact_owned_resources_closed",set.close());
        s.log.emit("mapping_hotkey_edits_component","\"result\":\"PASS\",\"same_combination_action_changed\":true,\"delete_OS_release_verified\":true,\"duplicate_hotkey_and_mouse_Chinese_rejection\":true,\"old_OS_conflict_case_repeated\":false");
    }catch(...){try{release();}catch(...){}try{if(!s.left&&!s.control&&mapping_remaining_modifiers(0)&&!(GetAsyncKeyState(test_key)&0x8000))chain_gui_save(s,original);}catch(...){}
        try{if(!set.closed)set.close();}catch(...){}throw;}
}
