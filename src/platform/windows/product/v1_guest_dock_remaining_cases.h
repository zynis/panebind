#pragma once

// TEST ONLY. Include inside the guarded fixture translation unit after the
// existing GUI, layout and product-chain helpers. Expected docking geometry
// comes from physical work-area halves/quarters, never the product solver.
panebind::core::v1::Settings dock_remaining_public_top_action(
    State& s,panebind::core::v1::Action wanted){
    namespace v=panebind::core::v1;
    settings_guest_scope(s);const auto before=chain_settings(s);
    if(s.suite!="DockRemainderOnly"||s.left||s.control||
       (wanted!=v::Action::DockTop&&wanted!=v::Action::Maximize))
        throw std::runtime_error("dock_remaining_top_public_edit_scope_required");
    const auto gui=gui_open(s);gui_page_select(s,gui,2);
    const auto label=gui_find(s,gui,L"Static",L"顶部");
    const auto combo=GetWindow(label,GW_HWNDNEXT);
    if(gui_text(s,gui,label)!=L"顶部"||gui_class(combo)!=L"ComboBox"||
       GetParent(combo)!=GetParent(label)||!gui_identity(s,gui,combo)||
       !IsWindowVisible(combo)||!IsWindowEnabled(combo))
        throw std::runtime_error("dock_remaining_exact_top_direction_combo_required");
    const auto count=gui_read(s,gui,combo,CB_GETCOUNT);
    const auto previous=gui_read(s,gui,combo,CB_GETCURSEL);
    if(!count||count>16||previous>=count||
       gui_read(s,gui,combo,CB_GETITEMDATA,previous)!=static_cast<DWORD_PTR>(before.dock_actions[2]))
        throw std::runtime_error("dock_remaining_top_combo_does_not_match_saved_configuration");
    DWORD_PTR target=count;
    for(DWORD_PTR n=0;n<count;++n)
        if(gui_read(s,gui,combo,CB_GETITEMDATA,n)==static_cast<DWORD_PTR>(wanted)){
            if(target!=count)throw std::runtime_error("dock_remaining_duplicate_top_action_item");target=n;
        }
    if(target==count)throw std::runtime_error("dock_remaining_requested_top_action_missing");
    unsigned tabs{};
    for(;tabs<64&&gui_focus(gui)!=combo;++tabs){
        const auto focus=gui_focus(gui);gui_press(s,gui,VK_TAB);
        if(!wait([&]{return gui_focus(gui)&&gui_focus(gui)!=focus;}))
            throw std::runtime_error("dock_remaining_actual_Tab_did_not_advance");
    }
    s.log.check("dock_remaining_top_actual_public_Tab_focus",gui_identity(s,gui,combo)&&
        GetForegroundWindow()==gui.root&&gui_focus(gui)==combo,
        "\"actual_Tab_pairs\":"+std::to_string(tabs)+",\"actual_combo\":"+
        std::to_string(reinterpret_cast<std::uintptr_t>(combo))+",\"selection_write_message\":false");
    // Open the real dropdown first: Enter then closes that dropdown rather
    // than accidentally submitting the dialog's default Save button.
    gui_press(s,gui,VK_F4);
    s.log.check("dock_remaining_top_actual_dropdown_open",wait([&]{return
        gui_identity(s,gui,combo)&&gui_focus(gui)==combo&&gui_read(s,gui,combo,CB_GETDROPPEDSTATE)!=0;}));
    gui_press(s,gui,VK_HOME);
    s.log.check("dock_remaining_top_actual_Home_selection",wait([&]{return gui_read(s,gui,combo,CB_GETCURSEL)==0;}));
    for(DWORD_PTR n=0;n<target;++n){
        gui_press(s,gui,VK_DOWN);
        s.log.check("dock_remaining_top_actual_Down_selection",wait([&]{return gui_read(s,gui,combo,CB_GETCURSEL)==n+1;}));
    }
    gui_press(s,gui,VK_RETURN);
    s.log.check("dock_remaining_top_actual_public_selection",wait([&]{return
        gui_identity(s,gui,combo)&&gui_read(s,gui,combo,CB_GETDROPPEDSTATE)==0&&
        gui_read(s,gui,combo,CB_GETCURSEL)==target&&
        gui_read(s,gui,combo,CB_GETITEMDATA,target)==static_cast<DWORD_PTR>(wanted);}),
        "\"previous_action\":"+quote(std::string(v::action_name(before.dock_actions[2])))+
        ",\"requested_action\":"+quote(std::string(v::action_name(wanted)))+
        ",\"actual_item_index\":"+std::to_string(target)+
        ",\"input_kind\":\"actual_Tab_F4_Home_Down_Enter\",\"CB_SETCURSEL_or_WM_COMMAND\":false");
    gui_save_enter(s,gui);const auto actual=chain_settings(s);auto expected=before;
    expected.dock_actions[2]=wanted;expected.generation=actual.generation;
    s.log.check("dock_remaining_top_saved_only_requested_direction",actual==expected&&actual.generation>before.generation,
        "\"actual_generation\":"+std::to_string(actual.generation)+",\"other_options_match_expected\":"+
        (actual==expected?"true":"false"));
    return actual;
}

std::vector<std::string> dock_remaining_commit_receipts(State& s,const Gesture& g){
    namespace e=product::guest::up_evidence_detail;
    std::vector<std::string> commits;std::istringstream rows(product_log_since(s,g.offset));std::string row;
    while(std::getline(rows,row)){
        if(!e::quoted(row,"\"type\":","placement")||e::number(row,"\"generation\":")!=g.generation||
           !e::quoted(row,"\"intent_reason\":","normal_up_dock"))continue;
        if(row.empty()||row.back()!='}'||e::number(row,"\"process_id\":")!=s.product_pid||
           !e::quoted(row,"\"source_sha\":",s.product_source))
            throw std::runtime_error("dock_remaining_commit_receipt_identity_invalid");
        commits.push_back(std::move(row));
    }
    return commits;
}

[[maybe_unused]] void owned_dock_remaining_case(State& s){
    namespace v=panebind::core::v1;
    namespace e=product::guest::up_evidence_detail;
    settings_guest_scope(s);const auto original=chain_settings(s);
    const auto pid=s.product_pid;const auto birth=process_birth(s.process);const auto binary=hash(s.product);
    if(s.suite!="DockRemainderOnly"||!original.enabled||!original.rules.empty()||s.left||s.control||
       s.explorer[0]||s.applications[0]||binary.empty()||binary!=hash(s.input_product)||
       original.dock_actions!=v::default_settings().dock_actions||
       (GetAsyncKeyState(VK_LBUTTON)&0x8000)||(GetAsyncKeyState(VK_CONTROL)&0x8000)||
       (GetAsyncKeyState(VK_SHIFT)&0x8000)||(GetAsyncKeyState(VK_MENU)&0x8000)||
       (GetAsyncKeyState(VK_LWIN)&0x8000)||(GetAsyncKeyState(VK_RWIN)&0x8000))
        throw std::runtime_error("dock_remaining_exact_default_idle_owned_guest_required");
    auto config=original;config.magnet_move=false;config.magnet_resize=false;config.glue=false;
    config.docking=true;config.smart_docking=false;config.stretch=false;config.smart_stretch=false;
    config.preview=true;config.feedback=false;config.drag_transparency=false;config.shake=false;
    chain_gui_save(s,config);LayoutFixtures set{s};set.create();
    try{
        product::Catalog catalog;const auto displays=catalog.displays();
        const auto work=chain_work(set.frames[0].hwnd);
        if(displays.size()!=1||work.width()<1200||work.height()<850)
            throw std::runtime_error("dock_remaining_declared_single_monitor_work_area_unavailable");
        const LONG midx=work.l+work.width()/2,midy=work.t+work.height()/2;
        const Rect initial{work.l+200,work.t+160,work.l+600,work.t+460};
        const LayoutLimit limit{100,100,work.width(),work.height()};
        struct Target {const char* name;POINT cursor;Rect expected;};
        const std::array<Target,5> targets{{
            {"DockTop",{midx,work.t+1},{work.l,work.t,work.r,midy}},
            {"DockBottom",{midx,work.b-1},{work.l,midy,work.r,work.b}},
            {"DockTopRight",{work.r-1,work.t+1},{midx,work.t,work.r,midy}},
            {"DockBottomLeft",{work.l+1,work.b-1},{work.l,midy,midx,work.b}},
            {"DockBottomRight",{work.r-1,work.b-1},{midx,midy,work.r,work.b}}
        }};
        (void)dock_remaining_public_top_action(s,v::Action::DockTop);
        std::uint64_t previous_generation{};
        for(const auto& target:targets){
            chain_check_product(s,pid,birth);set.setup({initial},{limit});
            chain_preview_hidden(s,"dock_remaining_before_gesture_preview_hidden");
            const auto g=layout_down(set,0,HTCAPTION,false);
            const auto start=record(s,g.offset,"gesture_start",g.generation);
            const auto key=e::number(start,"\"key\":");
            s.log.check("dock_remaining_fresh_plain_Move_generation",g.generation>previous_generation&&key&&*key&&
                e::number(start,"\"process_id\":")==pid&&e::quoted(start,"\"source_sha\":",s.product_source)&&
                e::unique_value(start,"\"ctrl\":")=="false"&&e::unique_value(start,"\"resize\":")=="false",
                "\"target\":"+quote(target.name)+",\"actual_start\":"+quote(start));
            const LONG dx=target.cursor.x-g.anchor.x,dy=target.cursor.y-g.anchor.y;
            const Rect raw=translated(initial,dx,dy);
            if(raw==target.expected)throw std::runtime_error("dock_remaining_raw_and_discrete_oracles_must_differ");
            layout_phase(set,0,g,dx,dy,{raw},std::string("dock_remaining_")+target.name+"_actual_free_Move_to_zone");
            chain_preview_exact(s,set.frames[0].hwnd,target.expected);
            const auto early_commits=dock_remaining_commit_receipts(s,g);
            s.log.check("dock_remaining_actual_preview_not_early_commit",set.owns(0)&&s.left&&!s.left_up_attempted&&
                (GetAsyncKeyState(VK_LBUTTON)&0x8000)&&visible(set.frames[0].hwnd)==raw&&
                early_commits.empty()&&placement(s,g.offset,g.generation,target.expected,true).empty()&&
                actual_up(s,g.offset,g.generation).empty(),
                "\"target\":"+quote(target.name)+",\"generation\":"+std::to_string(g.generation)+
                ",\"physical_work_area\":"+rect(work)+",\"independent_preview\":"+rect(target.expected)+
                ",\"UP_before_raw_visible\":"+rect(raw)+",\"normal_up_dock_rows_before_UP\":"+
                std::to_string(early_commits.size()));
            layout_up(set,g,{target.expected});chain_preview_hidden(s,"dock_remaining_actual_UP_hides_preview");
            const auto committed=placement(s,g.offset,g.generation,target.expected,true);
            const auto commits=dock_remaining_commit_receipts(s,g);
            s.log.check("dock_remaining_actual_NormalUp_discrete_exact_commit",commits.size()==1&&commits.front()==committed&&key&&
                e::number(committed,"\"process_id\":")==pid&&e::quoted(committed,"\"source_sha\":",s.product_source)&&
                e::number(committed,"\"generation\":")==g.generation&&e::number(committed,"\"key\":")==*key&&
                e::quoted(committed,"\"intent_reason\":","normal_up_dock")&&
                e::unique_value(committed,"\"native_attempted\":")=="true"&&
                e::unique_value(committed,"\"native_succeeded\":")=="true"&&e::unique_value(committed,"\"exact\":")=="true"&&
                !IsZoomed(set.frames[0].hwnd)&&chain_work(set.frames[0].hwnd)==work,
                "\"target\":"+quote(target.name)+",\"receipt\":"+quote(committed)+
                ",\"expected_visible\":"+rect(target.expected)+",\"normal_up_dock_records\":"+
                std::to_string(commits.size())+",\"actual_IsZoomed\":"+(IsZoomed(set.frames[0].hwnd)?"true":"false"));
            // The production discrete command remembers its then-current
            // actual rectangle, i.e. the last free Move BEFORE this UP. It is
            // not the gesture's older original-DOWN rectangle.
            chain_command(set,0,L"Undo",raw);
            s.log.check("dock_remaining_public_Undo_restores_UP_before_raw_not_DOWN",set.owns(0)&&
                visible(set.frames[0].hwnd)==raw&&!IsZoomed(set.frames[0].hwnd)&&!s.left&&!s.control,
                "\"target\":"+quote(target.name)+",\"original_DOWN_visible\":"+rect(initial)+
                ",\"expected_Undo_visible\":"+rect(raw)+",\"actual\":"+rect(*visible(set.frames[0].hwnd)));
            chain_check_product(s,pid,birth);previous_generation=g.generation;
        }
        (void)dock_remaining_public_top_action(s,original.dock_actions[2]);
        const auto restored=chain_gui_save(s,original);auto expected=original;expected.generation=restored.generation;
        s.log.check("dock_remaining_original_GUI_configuration_restored",restored==expected&&hash(s.product)==binary);
        chain_check_product(s,pid,birth);s.log.check("dock_remaining_exact_owned_windows_closed",set.close());
        s.log.emit("dock_remaining_component",std::string("\"result\":\"PASS_SCOPED_FIVE_REMAINING_TARGETS\",\"same_product_process\":true,")+
            "\"top_action_publicly_changed_and_restored\":true,\"independent_physical_work_area\":"+rect(work)+
            ",\"negative_work_origin_exercised\":"+(work.l<0||work.t<0?"true":"false")+
            ",\"prior_left_topLeft_right_replayed\":false,\"additional_monitors\":\"NOT_RUN\","+
            "\"Full_suite\":\"NOT_RUN\",\"human_UAT\":\"NOT_RUN\",\"whole_version_PASS\":false");
    }catch(...){
        pause_routing_failed_case_input(s);
        try{
            if(s.process&&WaitForSingleObject(s.process,0)==WAIT_TIMEOUT&&!s.left&&!s.control){
                const auto current=chain_settings(s);
                if(current.dock_actions[2]!=original.dock_actions[2])
                    (void)dock_remaining_public_top_action(s,original.dock_actions[2]);
                (void)chain_gui_save(s,original);
            }
        }catch(...){}
        try{if(!set.closed)(void)set.close();}catch(...){}throw;
    }
}
