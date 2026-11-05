#pragma once

// TEST ONLY, in the exact guarded Medium disposable guest. All state-changing
// UI actions below are real paired input. Native messages only read actual
// controls; no BM_CLICK/WM_COMMAND/WM_SETTEXT or private mutation API is used.
struct ReviewEditor {HWND root{};DWORD tid{};};
struct ReviewReachSample {
    RECT target{},page{};HWND focus{};
};
ReviewReachSample review_reach_sample(State& s,const SettingsGui& gui,HWND panel,HWND control){
    ReviewReachSample sample;
    if(!desktop()||!gui_identity(s,gui,control)||!gui_identity(s,gui,panel)||GetForegroundWindow()!=gui.root||
       GetParent(control)!=panel||!GetWindowRect(control,&sample.target)||!GetWindowRect(panel,&sample.page))
        throw std::runtime_error("settings_review_narrow_focus_bounds_or_identity_unknown");
    sample.focus=gui_focus(gui);return sample;
}
struct ReviewComboFacts {
    bool queried{},info_known{},dropped_known{};DWORD info_error{},dropped_error{};DWORD_PTR dropped{};
    COMBOBOXINFO info{sizeof(info)};std::uint64_t elapsed_ms{};
};
ReviewComboFacts review_combo_facts(State& s,const SettingsGui& gui,HWND control){
    ReviewComboFacts facts;
    if(gui_class(control)!=L"ComboBox")return facts;
    if(!gui_identity(s,gui,control))throw std::runtime_error("settings_review_combo_identity_changed_before_read");
    const auto begin=GetTickCount64();facts.queried=true;
    SetLastError(0);facts.info_known=GetComboBoxInfo(control,&facts.info)!=FALSE;facts.info_error=facts.info_known?0:GetLastError();
    SetLastError(0);facts.dropped_known=SendMessageTimeoutW(control,CB_GETDROPPEDSTATE,0,0,
        SMTO_ABORTIFHUNG|SMTO_BLOCK,200,&facts.dropped)!=FALSE;facts.dropped_error=facts.dropped_known?0:GetLastError();
    facts.elapsed_ms=GetTickCount64()-begin;
    if(!gui_identity(s,gui,control))throw std::runtime_error("settings_review_combo_identity_changed_during_read");
    return facts;
}
std::string review_reach_json(const ReviewReachSample& sample){
    return "{\"rect\":"+rect({sample.target.left,sample.target.top,sample.target.right,sample.target.bottom})+
        ",\"page_rect\":"+rect({sample.page.left,sample.page.top,sample.page.right,sample.page.bottom})+
        ",\"focus\":"+std::to_string(reinterpret_cast<std::uintptr_t>(sample.focus))+"}";
}
std::string review_combo_json(const ReviewComboFacts& facts){
    if(!facts.queried)return "null";
    return "{\"GetComboBoxInfo_called\":true,\"info_known\":"+std::string(facts.info_known?"true":"false")+
        ",\"info_error\":"+std::to_string(facts.info_error)+",\"hwndCombo\":"+
        (facts.info_known?std::to_string(reinterpret_cast<std::uintptr_t>(facts.info.hwndCombo)):"null")+
        ",\"hwndItem\":"+(facts.info_known?std::to_string(reinterpret_cast<std::uintptr_t>(facts.info.hwndItem)):"null")+
        ",\"hwndList\":"+(facts.info_known?std::to_string(reinterpret_cast<std::uintptr_t>(facts.info.hwndList)):"null")+
        ",\"rcItem_raw\":"+(facts.info_known?rect({facts.info.rcItem.left,facts.info.rcItem.top,facts.info.rcItem.right,facts.info.rcItem.bottom}):"null")+
        ",\"rcButton_raw\":"+(facts.info_known?rect({facts.info.rcButton.left,facts.info.rcButton.top,facts.info.rcButton.right,facts.info.rcButton.bottom}):"null")+
        ",\"button_state\":"+(facts.info_known?std::to_string(facts.info.stateButton):"null")+
        ",\"rc_coordinate_space\":\"not_explicit_in_official_COMBOBOXINFO_page_raw_diagnostic_only\",\"rc_used_for_reachability\":false"+
        ",\"CB_GETDROPPEDSTATE_known\":"+(facts.dropped_known?"true":"false")+",\"dropped_error\":"+std::to_string(facts.dropped_error)+
        ",\"drop_list_visible\":"+(facts.dropped_known?(facts.dropped?"true":"false"):"null")+
        ",\"query_elapsed_ms\":"+std::to_string(facts.elapsed_ms)+"}";
}
void owned_settings_ui_narrow_case(State& s){
    settings_guest_scope(s);const auto bytes=read(s.product.parent_path()/L"settings.ini");const auto gui=gui_open(s);
    RECT original{};if(!GetWindowRect(gui.root,&original))throw std::runtime_error("settings_review_narrow_original_rect_unknown");
    const UINT dpi=GetDpiForWindow(gui.root);if(!dpi)throw std::runtime_error("settings_review_narrow_DPI_unknown");
    const int width=MulDiv(570,static_cast<int>(dpi),96),height=MulDiv(560,static_cast<int>(dpi),96);
    if(!gui_identity(s,gui,gui.root)||GetForegroundWindow()!=gui.root||!SetWindowPos(gui.root,nullptr,0,0,width,height,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE))
        throw std::runtime_error("settings_review_narrow_exact_test_UI_resize_failed");
    RECT actual{};const bool narrow=wait([&]{return gui_identity(s,gui,gui.root)&&GetWindowRect(gui.root,&actual)&&
        actual.right-actual.left==width&&actual.bottom-actual.top==height;});
    s.log.check("T01_actual_narrow_settings_native_rectangle",narrow,
        "\"actual_rect\":"+rect({actual.left,actual.top,actual.right,actual.bottom})+",\"dpi\":"+std::to_string(dpi)+
        ",\"test_setup_SetWindowPos_only_exact_product_settings\":true,\"WM_SIZE_not_faked\":true,\"user_resize_gesture_PASS_inferred\":false");
    for(unsigned page=0;page<6;++page){
        gui_page_select(s,gui,page);const auto panel=gui_page(s,gui);std::vector<HWND> stops;
        const std::vector<int> required=page==2?std::vector<int>{1230,1231}:page==3?std::vector<int>{1200,1201,1202,1203,1210,1211,1212,1213}:
            page==4?std::vector<int>{1220,1221,1222,1223,115}:std::vector<int>{};
        for(const int id:required){const auto control=GetDlgItem(panel,id);
            s.log.check("T01_actual_production_control_ID_is_wired",control&&GetDlgCtrlID(control)==id&&gui_identity(s,gui,control)&&GetParent(control)==panel,
                "\"page\":"+std::to_string(page)+",\"expected_control_id\":"+std::to_string(id)+",\"actual_control_id\":"+(control?std::to_string(GetDlgCtrlID(control)):"null")+
                ",\"actual_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(control))+",\"old_integer_ID_to_bool_regression_not_bypassed\":true");}
        for(HWND child=GetWindow(panel,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT))
            if(IsWindowVisible(child)&&IsWindowEnabled(child)&&(GetWindowLongPtrW(child,GWL_STYLE)&WS_TABSTOP))stops.push_back(child);
        if(stops.empty())throw std::runtime_error("settings_review_narrow_no_page_tabstops");
        review_tab_to(s,gui,stops.front());review_capture_settings(s,gui,page,"narrow-top");
        for(const auto h:stops){review_tab_to(s,gui,h);
            const auto initial=review_reach_sample(s,gui,panel,h);const auto initial_combo=review_combo_facts(s,gui,h);
            auto final=initial,previous=initial;unsigned observations=1,stable=1;
            const auto wait_begin=GetTickCount64();
            const auto reached=[&](const ReviewReachSample& sample){
                const auto& target=sample.target;const auto& container=sample.page;
                return target.left>=container.left&&target.right<=container.right&&target.top>=container.top&&
                    target.top+std::min<LONG>(target.bottom-target.top,MulDiv(48,static_cast<int>(dpi),96))<=container.bottom&&sample.focus==h;
            };
            // GetGUIThreadInfo can publish focus before WM_SETFOCUS/reveal
            // and native ComboBox layout have returned. Observe completion
            // using the unchanged reachability predicate, without new input.
            const bool settled=wait([&]{
                final=review_reach_sample(s,gui,panel,h);++observations;
                const bool same=EqualRect(&previous.target,&final.target)&&EqualRect(&previous.page,&final.page)&&previous.focus==final.focus;
                stable=same?stable+1:1;previous=final;return stable>=2&&reached(final);
            },500);
            const auto wait_elapsed=GetTickCount64()-wait_begin;const auto final_combo=review_combo_facts(s,gui,h);
            const auto& target=final.target;const auto& container=final.page;
            s.log.check("T01_narrow_actual_focused_control_reachable",settled&&reached(final),
                "\"page\":"+std::to_string(page)+",\"id\":"+std::to_string(GetDlgCtrlID(h))+
                ",\"class\":"+quote(utf8(gui_class(h)))+",\"rect\":"+rect({target.left,target.top,target.right,target.bottom})+
                ",\"page_rect\":"+rect({container.left,container.top,container.right,container.bottom})+
                ",\"initial_sample\":"+review_reach_json(initial)+",\"final_sample\":"+review_reach_json(final)+
                ",\"initial_combo\":"+review_combo_json(initial_combo)+",\"final_combo\":"+review_combo_json(final_combo)+
                ",\"readiness_wait_budget_ms\":500,\"readiness_wait_elapsed_ms\":"+std::to_string(wait_elapsed)+
                ",\"native_bounds_observations\":"+std::to_string(observations)+",\"same_bounds_focus_streak\":"+std::to_string(stable)+
                ",\"unchanged_reachability_predicate\":true,\"extra_input_or_layout_mutation\":false,\"visual_text_readability_separate\":true");
        }
        const auto before=gui_focus(gui);key(s,VK_SHIFT,true);gui_press(s,gui,VK_TAB);key(s,VK_SHIFT,false);
        s.log.check("T01_narrow_actual_ShiftTab_visible_focus",wait([&]{return gui_focus(gui)&&gui_focus(gui)!=before&&gui_identity(s,gui,gui_focus(gui));},500));
        review_tab_to(s,gui,stops.back());review_capture_settings(s,gui,page,"narrow-bottom");review_page_bounds(s,gui,page);
    }
    if(!gui_identity(s,gui,gui.root)||!SetWindowPos(gui.root,nullptr,original.left,original.top,original.right-original.left,original.bottom-original.top,SWP_NOZORDER|SWP_NOACTIVATE))
        throw std::runtime_error("settings_review_narrow_original_own_UI_restore_failed");
    gui_press(s,gui,VK_ESCAPE);s.log.check("T01_narrow_cancel_preserves_configuration",wait([&]{return !IsWindow(gui.root);})&&read(s.product.parent_path()/L"settings.ini")==bytes);
}
bool review_editor_identity(State& s,const SettingsGui& gui,const ReviewEditor& editor,HWND child){
    DWORD pid{},child_pid{};
    return s.process&&WaitForSingleObject(s.process,0)==WAIT_TIMEOUT&&editor.root&&
        GetWindowThreadProcessId(editor.root,&pid)==gui.tid&&pid==s.product_pid&&editor.tid==gui.tid&&
        gui_class(editor.root)==L"PaneBindSettingsMappingEditor"&&GetWindow(editor.root,GW_OWNER)==gui.root&&
        IsWindowVisible(editor.root)&&GetWindowThreadProcessId(child,&child_pid)==gui.tid&&child_pid==s.product_pid&&
        (child==editor.root||IsChild(editor.root,child));
}
ReviewEditor review_editor(State& s,const SettingsGui& gui){
    ReviewEditor found;
    s.log.check("settings_review_exact_product_editor",wait([&]{
        struct Query {State* s;const SettingsGui* gui;ReviewEditor value;unsigned matches{};} q{&s,&gui};
        EnumWindows([](HWND h,LPARAM data)->BOOL{auto& q=*reinterpret_cast<Query*>(data);DWORD pid{};
            const auto tid=GetWindowThreadProcessId(h,&pid);
            if(pid==q.s->product_pid&&tid==q.gui->tid&&gui_class(h)==L"PaneBindSettingsMappingEditor"&&
               IsWindowVisible(h)&&GetWindow(h,GW_OWNER)==q.gui->root){q.value={h,tid};++q.matches;}return TRUE;
        },reinterpret_cast<LPARAM>(&q));found=q.value;
        return q.matches==1&&GetForegroundWindow()==found.root&&review_editor_identity(s,gui,found,found.root);
    }));return found;
}
HWND review_editor_focus(const ReviewEditor& editor){GUITHREADINFO info{sizeof(info)};
    return GetGUIThreadInfo(editor.tid,&info)?info.hwndFocus:nullptr;}
DWORD_PTR review_editor_read(State& s,const SettingsGui& gui,const ReviewEditor& editor,HWND h,UINT msg,WPARAM w=0,LPARAM l=0){
    DWORD_PTR result{};
    if(!review_editor_identity(s,gui,editor,h)||!SendMessageTimeoutW(h,msg,w,l,SMTO_ABORTIFHUNG|SMTO_BLOCK,200,&result))
        throw std::runtime_error("settings_review_exact_editor_read_failed");return result;
}
void review_editor_key(State& s,const SettingsGui& gui,const ReviewEditor& editor,WORD code){
    if(!review_editor_identity(s,gui,editor,review_editor_focus(editor))||GetForegroundWindow()!=editor.root)
        throw std::runtime_error("settings_review_editor_keyboard_context_changed");
    key(s,code,true);key(s,code,false);
}
void review_editor_tab(State& s,const SettingsGui& gui,const ReviewEditor& editor,HWND h){
    if(!review_editor_identity(s,gui,editor,h)||!IsWindowVisible(h)||!IsWindowEnabled(h))
        throw std::runtime_error("settings_review_editor_target_unavailable");
    unsigned count{};for(;count<64&&review_editor_focus(editor)!=h;++count){
        const auto previous=review_editor_focus(editor);review_editor_key(s,gui,editor,VK_TAB);
        if(!wait([&]{return review_editor_focus(editor)&&review_editor_focus(editor)!=previous;},500))
            throw std::runtime_error("settings_review_editor_Tab_failed");
    }
    s.log.check("settings_review_actual_editor_Tab",review_editor_focus(editor)==h,
        "\"id\":"+std::to_string(GetDlgCtrlID(h))+",\"actual_key_pairs\":"+std::to_string(count));
}
void review_editor_combo(State& s,const SettingsGui& gui,const ReviewEditor& editor,int id,LRESULT data){
    const auto h=GetDlgItem(editor.root,id);review_editor_tab(s,gui,editor,h);
    const auto count=review_editor_read(s,gui,editor,h,CB_GETCOUNT);LRESULT wanted=-1;
    if(count>512)throw std::runtime_error("settings_review_combo_count_over_bound");
    for(DWORD_PTR i=0;i<count;++i)if(static_cast<LRESULT>(review_editor_read(s,gui,editor,h,CB_GETITEMDATA,i))==data){wanted=static_cast<LRESULT>(i);break;}
    if(wanted<0)throw std::runtime_error("settings_review_required_combo_item_not_present");
    unsigned home_pairs{},down_pairs{},pair_attempts{};bool progressed{};
    std::string stage="HOME",failure;
    try{
        ++pair_attempts;review_editor_key(s,gui,editor,VK_HOME);++home_pairs;
        progressed=wait([&]{return review_editor_focus(editor)==h&&GetForegroundWindow()==editor.root&&
            review_editor_read(s,gui,editor,h,CB_GETCURSEL)==0;},1000);
        for(LRESULT i=1;progressed&&i<=wanted;++i){
            stage="DOWN_to_"+std::to_string(i);
            ++pair_attempts;review_editor_key(s,gui,editor,VK_DOWN);++down_pairs;
            progressed=wait([&]{return review_editor_focus(editor)==h&&GetForegroundWindow()==editor.root&&
                review_editor_read(s,gui,editor,h,CB_GETCURSEL)==static_cast<DWORD_PTR>(i);},1000);
        }
        if(!progressed)failure="actual_selection_did_not_reach_exact_next_index";
    }catch(const std::exception& error){failure=error.what();}
    DWORD_PTR selection{},item_data{};bool selection_known{},item_known{};
    if(review_editor_identity(s,gui,editor,h)){
        selection_known=SendMessageTimeoutW(h,CB_GETCURSEL,0,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,200,&selection)!=FALSE;
        if(selection_known&&static_cast<LRESULT>(selection)>=0&&selection<count)
            item_known=SendMessageTimeoutW(h,CB_GETITEMDATA,selection,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,200,&item_data)!=FALSE;
    }
    const auto focus=review_editor_focus(editor),foreground=GetForegroundWindow();
    const bool exact=review_editor_identity(s,gui,editor,h);
    s.log.check("settings_review_combo_actual_keys",failure.empty()&&progressed&&exact&&focus==h&&foreground==editor.root&&
        selection_known&&static_cast<LRESULT>(selection)==wanted&&item_known&&static_cast<LRESULT>(item_data)==data,
        "\"id\":"+std::to_string(id)+",\"item_data\":"+std::to_string(data)+",\"expected_selection\":"+std::to_string(wanted)+
        ",\"final_selection\":"+(selection_known?std::to_string(static_cast<LRESULT>(selection)):"null")+
        ",\"final_item_data\":"+(item_known?std::to_string(static_cast<LRESULT>(item_data)):"null")+
        ",\"focus\":"+std::to_string(reinterpret_cast<std::uintptr_t>(focus))+",\"expected_focus\":"+std::to_string(reinterpret_cast<std::uintptr_t>(h))+
        ",\"foreground\":"+std::to_string(reinterpret_cast<std::uintptr_t>(foreground))+",\"expected_foreground\":"+std::to_string(reinterpret_cast<std::uintptr_t>(editor.root))+
        ",\"exact_control_identity\":"+(exact?"true":"false")+",\"completed_HOME_key_pairs\":"+std::to_string(home_pairs)+
        ",\"completed_DOWN_key_pairs\":"+std::to_string(down_pairs)+",\"key_pair_attempts\":"+std::to_string(pair_attempts)+
        ",\"last_step\":"+quote(stage)+",\"failure\":"+(failure.empty()?"null":quote(failure))+
        ",\"extra_or_repeated_key_pairs\":false,\"CB_SETCURSEL_used\":false");
}
void review_editor_check(State& s,const SettingsGui& gui,const ReviewEditor& editor,int id,bool desired){
    const auto h=GetDlgItem(editor.root,id);review_editor_tab(s,gui,editor,h);
    if((review_editor_read(s,gui,editor,h,BM_GETCHECK)==BST_CHECKED)!=desired)review_editor_key(s,gui,editor,VK_SPACE);
    s.log.check("settings_review_checkbox_actual_keys",wait([&]{return (review_editor_read(s,gui,editor,h,BM_GETCHECK)==BST_CHECKED)==desired;}),
        "\"id\":"+std::to_string(id)+",\"checked\":"+(desired?"true":"false"));
}
void review_editor_replace(State& s,const SettingsGui& gui,const ReviewEditor& editor,int id,std::wstring_view desired){
    const auto h=GetDlgItem(editor.root,id);review_editor_tab(s,gui,editor,h);
    key(s,VK_CONTROL,true);review_editor_key(s,gui,editor,'A');key(s,VK_CONTROL,false);
    const auto exact=[&]{return review_editor_identity(s,gui,editor,h)&&GetForegroundWindow()==editor.root&&
        review_editor_focus(editor)==h&&gui_class(h)==L"Edit"&&IsWindowEnabled(h)&&IsWindowVisible(h);};
    settings_guest_unicode(s,desired,exact);std::array<wchar_t,32768> buffer{};
    s.log.check("settings_review_editor_actual_text",wait([&]{const auto length=review_editor_read(s,gui,editor,h,WM_GETTEXT,buffer.size(),reinterpret_cast<LPARAM>(buffer.data()));
        return length<buffer.size()&&std::wstring_view(buffer.data(),length)==desired;}),
        "\"id\":"+std::to_string(id)+",\"characters\":"+std::to_string(desired.size())+",\"WM_SETTEXT_used\":false");
}
void review_editor_finish(State& s,const SettingsGui& gui,const ReviewEditor& editor,bool accept){
    const auto h=GetDlgItem(editor.root,accept?IDOK:IDCANCEL);review_editor_tab(s,gui,editor,h);
    review_editor_key(s,gui,editor,VK_RETURN);
    const bool returned=wait([&]{const auto focus=gui_focus(gui);return !IsWindow(editor.root)&&GetForegroundWindow()==gui.root&&
        gui_identity(s,gui,gui.tab)&&gui_identity(s,gui,focus)&&IsWindowVisible(focus)&&IsWindowEnabled(focus);});
    const auto focus=gui_focus(gui),foreground=GetForegroundWindow();DWORD focus_pid{};const auto focus_tid=GetWindowThreadProcessId(focus,&focus_pid);
    s.log.check("settings_review_actual_editor_accept_or_cancel",returned,
        std::string("\"accepted\":")+(accept?"true":"false")+",\"editor_destroyed\":"+(!IsWindow(editor.root)?"true":"false")+
        ",\"root\":"+std::to_string(reinterpret_cast<std::uintptr_t>(gui.root))+",\"root_exact\":"+(gui_identity(s,gui,gui.root)?"true":"false")+
        ",\"tab_exact\":"+(gui_identity(s,gui,gui.tab)?"true":"false")+",\"foreground\":"+std::to_string(reinterpret_cast<std::uintptr_t>(foreground))+
        ",\"foreground_exact\":"+(foreground==gui.root?"true":"false")+",\"focus\":"+std::to_string(reinterpret_cast<std::uintptr_t>(focus))+
        ",\"focus_exact\":"+(gui_identity(s,gui,focus)?"true":"false")+",\"focus_pid\":"+std::to_string(focus_pid)+",\"focus_tid\":"+std::to_string(focus_tid)+
        ",\"focus_class\":"+quote(utf8(gui_class(focus)))+",\"focus_id\":"+std::to_string(GetDlgCtrlID(focus))+
        ",\"focus_visible\":"+(IsWindowVisible(focus)?"true":"false")+",\"focus_enabled\":"+(IsWindowEnabled(focus)?"true":"false"));
}
HWND review_page_control(State& s,const SettingsGui& gui,int id){
    const auto h=GetDlgItem(gui_page(s,gui),id);
    if(!h||!gui_identity(s,gui,h))throw std::runtime_error("settings_review_expected_production_control_missing");return h;
}
void review_list_select(State& s,const SettingsGui& gui,int id,std::size_t row){
    const auto h=review_page_control(s,gui,id);review_tab_to(s,gui,h);
    if(gui_read(s,gui,h,LVM_GETITEMCOUNT)<=row)throw std::runtime_error("settings_review_list_row_missing");
    gui_press(s,gui,VK_HOME);for(std::size_t n=0;n<row;++n)gui_press(s,gui,VK_DOWN);
    s.log.check("settings_review_list_actual_keyboard_selection",wait([&]{return gui_read(s,gui,h,LVM_GETNEXTITEM,static_cast<WPARAM>(-1),LVNI_SELECTED)==row;}),
        "\"list_id\":"+std::to_string(id)+",\"row\":"+std::to_string(row+1));
}
ReviewEditor review_open_editor(State& s,const SettingsGui& gui,int button){
    const auto h=review_page_control(s,gui,button);review_tab_to(s,gui,h);
    gui_press(s,gui,VK_SPACE);return review_editor(s,gui);
}
void review_saved_hotkey(State& s,const SettingsGui& gui,std::size_t row,unsigned code,bool save){
    gui_page_select(s,gui,3);review_list_select(s,gui,1200,row);const auto editor=review_open_editor(s,gui,1202);
    review_editor_combo(s,gui,editor,1300,code);review_editor_finish(s,gui,editor,save);
}
void review_chord(State& s,WORD code){
    const auto h=s.frames[0].hwnd;foreground(s,h);const auto point=hit_point(h,HTCAPTION);
    if(!point)throw std::runtime_error("settings_review_hotkey_fixture_caption_unknown");cursor(s,*point);
    if(!desktop()||!own(s,h)||GetForegroundWindow()!=h)throw std::runtime_error("settings_review_hotkey_source_changed");
    key(s,VK_CONTROL,true);key(s,VK_MENU,true);key(s,code,true);key(s,code,false);key(s,VK_MENU,false);key(s,VK_CONTROL,false);
}
void review_require_saved_baseline(State& s){
    settings_guest_scope(s);
    if(s.suite!="SettingsEditorsOnly")throw std::runtime_error("settings_review_saved_baseline_wrong_suite");
    const auto path=s.product.parent_path()/L"settings.ini";
    if(!plain_directory(path.parent_path()))throw std::runtime_error("settings_review_baseline_parent_not_plain");
    SetLastError(0);const DWORD before_attributes=GetFileAttributesW(path.c_str());
    const DWORD before_error=before_attributes==INVALID_FILE_ATTRIBUTES?GetLastError():0;
    if(before_attributes!=INVALID_FILE_ATTRIBUTES){
        if(before_attributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))throw std::runtime_error("settings_review_existing_baseline_not_regular");
        (void)chain_settings(s); // Existing malformed/inaccessible data never falls back.
        return;
    }
    if(before_error!=ERROR_FILE_NOT_FOUND)throw std::runtime_error("settings_review_baseline_absence_not_confirmed");
    const auto driver_mark=s.log.mark();const auto gui=gui_open(s);
    // A fresh product deliberately uses in-memory defaults before its first
    // save. Establish the test's persisted baseline through the public GUI,
    // without editing a control or writing a configuration file in the driver.
    gui_save_enter(s,gui);
    const DWORD after_attributes=GetFileAttributesW(path.c_str());
    if(after_attributes==INVALID_FILE_ATTRIBUTES||(after_attributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)))
        throw std::runtime_error("settings_review_baseline_actual_GUI_Save_did_not_create_regular_file");
    const auto saved=chain_settings(s);auto expected=v1::default_settings();expected.generation=saved.generation;
    std::string save_receipt,line;std::istringstream rows(s.log.since(driver_mark));
    while(std::getline(rows,line))if(line.find("\"case\":\"GUI_actual_Enter_saved_and_closed\",\"result\":\"PASS\"")!=line.npos)save_receipt=line;
    s.log.check("settings_review_missing_baseline_created_by_actual_unedited_GUI_Save",saved==expected&&saved.generation>1&&!save_receipt.empty(),
        "\"product_source_sha\":"+quote(s.product_source)+",\"product_EXE_sha256\":"+quote(hash(s.product))+
        ",\"product_PID\":"+std::to_string(s.product_pid)+",\"before_GetFileAttributes_error\":"+std::to_string(before_error)+
        ",\"actual_saved_file_sha256\":"+quote(hash(path))+",\"actual_saved_generation\":"+std::to_string(saved.generation)+
        ",\"actual_driver_GUI_input_receipt\":"+quote(save_receipt)+",\"product_save_callback_return_separately_instrumented\":false"+
        ",\"direct_test_configuration_write\":false,\"parse_failure_fallback\":false");
}
void owned_settings_ui_hotkey_case(State& s){
    review_require_saved_baseline(s);const auto original=chain_settings(s);const auto bytes=read(s.product.parent_path()/L"settings.ini");
    const auto center=std::find_if(original.hotkeys.begin(),original.hotkeys.end(),[](const auto& h){return h.modifiers==3&&h.key=='C'&&h.action==v1::Action::Center;});
    if(center==original.hotkeys.end())throw std::runtime_error("settings_review_T02_requires_actual_default_Center_mapping");
    const auto row=static_cast<std::size_t>(center-original.hotkeys.begin());
    auto gui=gui_open(s);review_saved_hotkey(s,gui,row,'J',false);
    s.log.check("T02_editor_Cancel_does_not_change_saved_config",read(s.product.parent_path()/L"settings.ini")==bytes);
    gui_page_select(s,gui,3);review_list_select(s,gui,1200,row);auto reopened=review_open_editor(s,gui,1202);
    const auto primary=GetDlgItem(reopened.root,1300);const auto current=review_editor_read(s,gui,reopened,primary,CB_GETCURSEL);
    s.log.check("T02_Cancel_preserves_the_structured_draft_not_only_disk",review_editor_read(s,gui,reopened,primary,CB_GETITEMDATA,current)=='C',
        "\"actual_reopened_combo_item_data\":"+std::to_string(review_editor_read(s,gui,reopened,primary,CB_GETITEMDATA,current)));
    review_editor_finish(s,gui,reopened,false);
    review_saved_hotkey(s,gui,row,'J',true);gui_save_enter(s,gui);
    const auto saved=chain_settings(s);auto wanted=original;wanted.hotkeys[row].key='J';wanted.generation=saved.generation;
    s.log.check("T02_structured_editor_Save_same_schema",saved==wanted&&saved.generation>original.generation);
    const auto initial=visible(s.frames[0].hwnd);MONITORINFO monitor{sizeof(monitor)};
    if(!initial||!GetMonitorInfoW(MonitorFromWindow(s.frames[0].hwnd,MONITOR_DEFAULTTONULL),&monitor))throw std::runtime_error("settings_review_T02_monitor_unknown");
    const Rect centered{monitor.rcWork.left+(monitor.rcWork.right-monitor.rcWork.left-initial->width())/2,
        monitor.rcWork.top+(monitor.rcWork.bottom-monitor.rcWork.top-initial->height())/2,0,0};
    const Rect expected{centered.l,centered.t,centered.l+initial->width(),centered.t+initial->height()};
    const auto mark=read_product(s).size();review_chord(s,'J');
    std::string receipt;const bool centered_now=wait([&]{receipt=chain_any_placement(s,mark,expected);
        return own(s,s.frames[0].hwnd)&&visible(s.frames[0].hwnd)==expected&&!receipt.empty();});
    const auto after=visible(s.frames[0].hwnd);
    s.log.check("T02_actual_CtrlAltJ_centers_exact_fixture",centered_now&&own(s,s.frames[0].hwnd)&&after==expected,
        "\"expected\":"+rect(expected)+",\"actual\":"+(after?rect(*after):"null")+",\"actual_sampled_after_wait\":true,\"exact_native_postverify_receipt\":"+quote(receipt));
    const Rect offset=translated(expected,-75,-45);if(!own(s,s.frames[0].hwnd)||!fixture_rect(s.frames[0].hwnd,offset))throw std::runtime_error("settings_review_T02_exact_fixture_setup_failed");
    const auto old_mark=read_product(s).size();review_chord(s,'C');
    const auto status=settings_guest_single_instance(s,s.product_pid,process_birth(s.process),"T02_old_key_after_actual_input");
    s.log.check("T02_actual_CtrlAltC_no_longer_centers",visible(s.frames[0].hwnd)==offset&&record(s,old_mark,"placement").empty()&&!status.empty());
    // Retain J for the requested subsequent demonstration; preserve the
    // original configuration in the raw receipt, never rebind old evidence.
    s.log.emit("settings_ui_T02_result","\"result\":\"PASS\",\"new_key\":\"Ctrl+Alt+J\",\"old_key\":\"Ctrl+Alt+C\",\"human_UAT\":\"NOT_RUN\"");
}
void review_editor_reject(State& s,const SettingsGui& gui,const ReviewEditor& editor,std::wstring_view expected){
    review_editor_tab(s,gui,editor,GetDlgItem(editor.root,IDOK));review_editor_key(s,gui,editor,VK_RETURN);
    const auto message=settings_guest_modal(s,gui,L"PaneBind：请检查此条目");
    struct Query {std::wstring text;unsigned count{};} query;
    EnumChildWindows(message.root,[](HWND h,LPARAM p)->BOOL{auto& q=*reinterpret_cast<Query*>(p);
        if(gui_class(h)!=L"Static")return TRUE;std::array<wchar_t,2048> value{};DWORD_PTR size{};
        if(SendMessageTimeoutW(h,WM_GETTEXT,value.size(),reinterpret_cast<LPARAM>(value.data()),SMTO_ABORTIFHUNG|SMTO_BLOCK,200,&size)&&size<value.size()){
            q.text.append(value.data(),size);++q.count;}return TRUE;
    },reinterpret_cast<LPARAM>(&query));
    s.log.check("settings_review_editor_actual_Chinese_rejection",query.count&&query.text.find(expected)!=query.text.npos,
        "\"expected_text\":"+quote(utf8(std::wstring(expected)))+",\"actual_message\":"+quote(utf8(query.text)));
    settings_guest_modal_press(s,gui,message,VK_RETURN);
    s.log.check("settings_review_rejection_returns_same_editor",wait([&]{return !IsWindow(message.root)&&
        review_editor_identity(s,gui,editor,editor.root)&&GetForegroundWindow()==editor.root;}));
}
void review_new_key(State& s,const SettingsGui& gui,const ReviewEditor& editor,unsigned key,unsigned modifiers,v1::Action action){
    review_editor_combo(s,gui,editor,1300,key);
    constexpr std::array<unsigned,4> bits{2,1,4,8};
    for(int i=0;i<4;++i)review_editor_check(s,gui,editor,1320+i,(modifiers&bits[static_cast<std::size_t>(i)])!=0);
    review_editor_combo(s,gui,editor,1330,static_cast<LRESULT>(action));
}
void owned_settings_ui_conflicts_case(State& s){
    settings_guest_scope(s);const auto before=chain_settings(s);const auto bytes=read(s.product.parent_path()/L"settings.ini");
    const auto duplicate=std::find_if(before.hotkeys.begin(),before.hotkeys.end(),[](const auto& h){return h.modifiers==3&&h.key=='J';});
    if(duplicate==before.hotkeys.end())throw std::runtime_error("settings_review_T03_requires_T02_saved_J");
    auto gui=gui_open(s);gui_page_select(s,gui,3);auto editor=review_open_editor(s,gui,1201);
    for(const auto& rejected:std::array<std::tuple<unsigned,unsigned,const wchar_t*>,3>{{
        {'J',3,L"重复"},{VK_F12,3,L"F12"},{VK_F11,6,L"Ctrl+Shift+F11"}}}){
        review_new_key(s,gui,editor,std::get<0>(rejected),std::get<1>(rejected),v1::Action::Center);
        review_editor_reject(s,gui,editor,std::get<2>(rejected));
        s.log.check("T03_rejected_form_preserves_live_configuration",read(s.product.parent_path()/L"settings.ini")==bytes&&chain_settings(s)==before);
    }
    review_editor_finish(s,gui,editor,false);
    WORD candidate{};
    for(const WORD code:std::array<WORD,3>{VK_F9,VK_F10,VK_F8})if(std::none_of(before.hotkeys.begin(),before.hotkeys.end(),[&](const auto& h){return h.modifiers==3&&h.key==code;})){candidate=code;break;}
    if(!candidate)throw std::runtime_error("settings_review_T03_no_unmapped_fixture_key");
    constexpr int registration=0x5042;
    SetLastError(0);const bool registered=RegisterHotKey(nullptr,registration,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,candidate)!=FALSE;
    const DWORD registration_error=registered?0:GetLastError();
    s.log.emit("settings_review_own_fixture_hotkey_registration","\"PID\":"+std::to_string(GetCurrentProcessId())+
        ",\"TID\":"+std::to_string(GetCurrentThreadId())+",\"HWND\":null,\"id\":"+std::to_string(registration)+
        ",\"key\":"+std::to_string(candidate)+",\"modifiers\":3,\"actual_API_result\":"+(registered?"true":"false")+
        ",\"win32_error\":"+std::to_string(registration_error)+",\"occupied_chord_will_not_be_sent\":true");
    if(!registered)throw std::runtime_error("settings_review_T03_fixture_RegisterHotKey_failed");
    bool released{};try{
        editor=review_open_editor(s,gui,1201);review_new_key(s,gui,editor,candidate,3,v1::Action::MoveRight);review_editor_finish(s,gui,editor,true);
        settings_guest_activate(s,gui,GetDlgItem(gui.root,IDOK));
        const v1::Hotkey occupied{3,candidate,v1::Action::MoveRight};
        const auto expected_conflict=L"快捷键注册冲突："+v1::settings_editor::hotkey_label(occupied)+L"（"+
            std::wstring(v1::settings_editor::action_label(occupied.action))+L"）。原有效映射保留，配置未提交。";
        settings_guest_acknowledge(s,gui,expected_conflict,L"PaneBind：设置未更改");
        s.log.check("T03_real_RegisterHotKey_conflict_preserves_previous_file",chain_settings(s)==before&&read(s.product.parent_path()/L"settings.ini")==bytes&&IsWindow(gui.root),
            "\"fixture_registered_actual_chord\":\"Ctrl+Alt+F"+std::to_string(candidate-VK_F1+1)+"\",\"real_OS_registration_conflict\":true");
        gui_press(s,gui,VK_ESCAPE);
        released=UnregisterHotKey(nullptr,registration)!=FALSE;
        s.log.check("T03_fixture_own_hotkey_unregistered",released);
        const auto before_rect=visible(s.frames[0].hwnd);if(!before_rect)throw std::runtime_error("settings_review_T03_fixture_geometry_unknown");
        MONITORINFO monitor{sizeof(monitor)};if(!GetMonitorInfoW(MonitorFromWindow(s.frames[0].hwnd,MONITOR_DEFAULTTONULL),&monitor))throw std::runtime_error("settings_review_T03_monitor_unknown");
        const Rect expected{monitor.rcWork.left+(monitor.rcWork.right-monitor.rcWork.left-before_rect->width())/2,
            monitor.rcWork.top+(monitor.rcWork.bottom-monitor.rcWork.top-before_rect->height())/2,0,0};
        const Rect centered{expected.l,expected.t,expected.l+before_rect->width(),expected.t+before_rect->height()};
        review_chord(s,'J');s.log.check("T03_old_valid_mapping_still_works_after_failed_Save",wait([&]{return visible(s.frames[0].hwnd)==centered;}));
    }catch(...){if(!released){released=UnregisterHotKey(nullptr,registration)!=FALSE;
        s.log.emit("settings_review_fixture_hotkey_failure_cleanup","\"own_registration_released\":"+std::string(released?"true":"false")+",\"functional_PASS_inferred\":false");}throw;}
}
void review_middle_center(State& s,bool mapped){
    settings_guest_effect_layout(s);const auto h=s.frames[0].hwnd;foreground(s,h);
    const auto point=hit_point(h,HTCAPTION);const auto before=visible(h);MONITORINFO monitor{sizeof(monitor)};
    if(!point||!before||!GetMonitorInfoW(MonitorFromWindow(h,MONITOR_DEFAULTTONULL),&monitor))throw std::runtime_error("settings_review_middle_fixture_geometry_unknown");
    cursor(s,*point);key(s,VK_CONTROL,true);const auto mark=read_product(s).size();
    const Rect center{monitor.rcWork.left+(monitor.rcWork.right-monitor.rcWork.left-before->width())/2,
        monitor.rcWork.top+(monitor.rcWork.bottom-monitor.rcWork.top-before->height())/2,0,0};
    const Rect expected{center.l,center.t,center.l+before->width(),center.t+before->height()};
    const auto prepared=wait([&]{const auto row=record(s,mark,"input_preparation");return row.find("\"actual_hit_test\":2")!=row.npos&&
        number(row,"\"root\":")==reinterpret_cast<std::uintptr_t>(h);},1500);
    // The existing status command pumps the ordinary app without granting a
    // private readiness lease. DOWN still requires the exact actual hit.
    if(!prepared){const auto q=read_product(s).size();if(!launch(s,L"--status",true)||!wait([&]{return !record(s,q,"status").empty();}))throw std::runtime_error("settings_review_middle_STA_status_missing");}
    DWORD_PTR hit{};GUITHREADINFO gui{sizeof(gui)};
    if(!desktop()||!own(s,h)||GetForegroundWindow()!=h||WindowFromPoint(*point)!=h||visible(h)!=before||
       !GetGUIThreadInfo(s.owner,&gui)||gui.hwndCapture||gui.hwndMoveSize||
       !SendMessageTimeoutW(h,WM_NCHITTEST,0,MAKELPARAM(static_cast<short>(point->x),static_cast<short>(point->y)),SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&hit)||hit!=HTCAPTION)
        throw std::runtime_error("settings_review_middle_fresh_caption_not_exact");
    bool pending=false,up_attempted=false;
    const auto send=[&](bool down){INPUT input{};input.type=INPUT_MOUSE;input.mi.dwFlags=down?MOUSEEVENTF_MIDDLEDOWN:MOUSEEVENTF_MIDDLEUP;input.mi.dwExtraInfo=0x50425631;
        if(!desktop()||(!down&&(!pending||up_attempted)))throw std::runtime_error("settings_review_middle_pair_ledger_invalid");
        if(!down)up_attempted=true;inject(input);pending=down;};
    try{send(true);send(false);key(s,VK_CONTROL,false);
        if(mapped){s.log.check("T04_actual_Ctrl_middle_Center",wait([&]{return visible(h)==expected&&!record(s,mark,"mapped_command_completed").empty();}),
            "\"expected\":"+rect(expected)+",\"same_actual_mapping_editor_and_command_service\":true");}
        else{const auto status=settings_guest_single_instance(s,s.product_pid,process_birth(s.process),"T04_deleted_mapping_after_actual_input");
            s.log.check("T04_deleted_mapping_no_longer_invokes_Center",visible(h)==before&&record(s,mark,"mapped_command_completed").empty()&&record(s,mark,"placement").empty()&&!status.empty());}
    }catch(...){if(pending&&!up_attempted)try{send(false);}catch(...){}try{if(s.control)key(s,VK_CONTROL,false);}catch(...){}throw;}
}
void owned_settings_ui_mouse_case(State& s){
    settings_guest_scope(s);const auto original=chain_settings(s);
    if(std::any_of(original.mouse_bindings.begin(),original.mouse_bindings.end(),[](const auto& m){return m.region==0&&m.button==3&&m.clicks==1&&m.modifiers==2;}))
        throw std::runtime_error("settings_review_T04_requires_unmapped_Ctrl_middle_single_caption");
    auto gui=gui_open(s);gui_page_select(s,gui,3);auto editor=review_open_editor(s,gui,1211);
    review_editor_combo(s,gui,editor,1310,0);review_editor_combo(s,gui,editor,1311,3);review_editor_combo(s,gui,editor,1312,1);
    for(int i=0;i<4;++i)review_editor_check(s,gui,editor,1320+i,i==0);
    review_editor_combo(s,gui,editor,1330,static_cast<LRESULT>(v1::Action::Center));review_editor_finish(s,gui,editor,true);gui_save_enter(s,gui);
    const auto configured=chain_settings(s);auto wanted=original;wanted.mouse_bindings.push_back({0,3,1,2,v1::Action::Center});wanted.generation=configured.generation;
    s.log.check("T04_real_UI_saved_structured_mouse_mapping",configured==wanted&&configured.generation>original.generation);
    review_middle_center(s,true);
    gui=gui_open(s);gui_page_select(s,gui,3);review_list_select(s,gui,1210,original.mouse_bindings.size());
    const auto remove=review_page_control(s,gui,1213);review_tab_to(s,gui,remove);gui_press(s,gui,VK_SPACE);gui_save_enter(s,gui);
    const auto deleted=chain_settings(s);wanted=original;wanted.generation=deleted.generation;
    s.log.check("T04_real_UI_delete_restores_previous_mouse_list",deleted==wanted);review_middle_center(s,false);
}
void review_write_config(const fs::path& path,const v1::Settings& value){
    if(fs::exists(path)||!plain_directory(path.parent_path()))throw std::runtime_error("settings_review_new_test_configuration_path_required");
    const auto bytes=v1::serialize(value);Handle file(CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr));
    DWORD written{};if(file.value==INVALID_HANDLE_VALUE||bytes.size()>v1::settings_detail::max_file_bytes||
       !WriteFile(file.value,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr)||written!=bytes.size()||!FlushFileBuffers(file.value))
        throw std::runtime_error("settings_review_test_input_configuration_write_failed");
}
void owned_settings_ui_rules_case(State& s){
    settings_guest_scope(s);const auto before=chain_settings(s);
    std::array<wchar_t,32768> path{};const auto length=GetModuleFileNameW(nullptr,path.data(),static_cast<DWORD>(path.size()));
    if(!length||length>=path.size())throw std::runtime_error("settings_review_actual_fixture_exe_unknown");
    const std::wstring exe(path.data(),length);
    auto gui=gui_open(s);gui_page_select(s,gui,4);auto editor=review_open_editor(s,gui,1221);
    review_editor_replace(s,gui,editor,1400,exe);review_editor_replace(s,gui,editor,1402,s.class_name);
    const auto mode=GetDlgItem(editor.root,1403);const auto selected=review_editor_read(s,gui,editor,mode,CB_GETCURSEL);
    s.log.check("T05_new_application_rule_defaults_to_exclusion",review_editor_read(s,gui,editor,mode,CB_GETITEMDATA,selected)==0);
    review_editor_finish(s,gui,editor,true);gui_save_enter(s,gui);
    const auto excluded=chain_settings(s);auto wanted=before;wanted.rules.push_back({utf8(exe),utf8(s.class_name),true,0xffffffffU});wanted.generation=excluded.generation;
    s.log.check("T05_saved_exact_owned_exe_and_class_exclusion",excluded==wanted);
    settings_guest_effect_layout(s);const auto mark=read_product(s).size();settings_guest_native_move(s,36,21,"T05_excluded_exact_fixture_Move");
    s.log.check("T05_exclusion_has_no_enhanced_geometry_write",record(s,mark,"placement").empty()&&record(s,mark,"gesture_start").empty());
    gui=gui_open(s);gui_page_select(s,gui,4);review_list_select(s,gui,1220,before.rules.size());
    const auto remove=review_page_control(s,gui,1223);review_tab_to(s,gui,remove);gui_press(s,gui,VK_SPACE);gui_save_enter(s,gui);
    s.log.check("T05_UI_delete_retains_unmodified_rules",chain_settings(s).rules==before.rules);
    settings_guest_move_effect(s,true,"T05_rule_deleted_new_actual_snap_and_UP");
}
void owned_settings_ui_roundtrip_case(State& s){
    settings_guest_scope(s);const auto original=chain_settings(s);const auto original_bytes=read(s.product.parent_path()/L"settings.ini");
    auto imported=original;
    imported.rules.push_back({"C:\\PaneBind 测试\\含 空格\\测试程序.exe","PaneBindFixtureClass",false,0xa000003fU});
    const auto input=s.output/(s.run+L"-UI-中文 空格-input.conf");const auto output=s.output/(s.run+L"-UI-中文 空格-export.conf");
    review_write_config(input,imported);
    auto gui=gui_open(s);settings_guest_file_path(s,gui,false,input);
    s.log.check("T05_T06_import_changes_draft_only",read(s.product.parent_path()/L"settings.ini")==original_bytes&&chain_settings(s)==original);
    gui_page_select(s,gui,4);review_list_select(s,gui,1220,original.rules.size());auto editor=review_open_editor(s,gui,1222);
    review_editor_check(s,gui,editor,1410,false);review_editor_finish(s,gui,editor,true);
    imported.rules.back().feature_mask=0xa000003eU;
    settings_guest_file_path(s,gui,true,output);
    const auto exported=v1::parse(read(output));
    s.log.check("T05_Chinese_space_path_old_schema_unknown_high_bits_and_untouched_rules_roundtrip",exported.value&&*exported.value==imported&&
        imported.schema==1&&read(s.product.parent_path()/L"settings.ini")==original_bytes,
        "\"known_bits_edit\":\"0x3f_to_0x3e\",\"unknown_high_bits\":\"0xa0000000_preserved\",\"native_file_dialog_used\":true");
    gui_press(s,gui,VK_ESCAPE);s.log.check("T06_import_then_Cancel_preserves_running_configuration",wait([&]{return !IsWindow(gui.root);})&&chain_settings(s)==original&&
        read(s.product.parent_path()/L"settings.ini")==original_bytes);
    gui=gui_open(s);gui_page_select(s,gui,1);const auto changed=original.attraction_dip==7?8:7;
    gui_edit_number(s,gui,gui_number(s,gui,L"吸引距离"),static_cast<unsigned>(changed));
    gui_page_select(s,gui,3);const auto defaults=GetDlgItem(gui.root,110);review_tab_to(s,gui,defaults);gui_press(s,gui,VK_SPACE);
    gui_page_select(s,gui,1);s.log.check("T06_page_defaults_do_not_discard_other_page_draft",gui_text(s,gui,gui_number(s,gui,L"吸引距离"))==std::to_wstring(changed)&&
        read(s.product.parent_path()/L"settings.ini")==original_bytes);
    gui_press(s,gui,VK_ESCAPE);
    // Persist the imported schema through the actual UI, then same-binary
    // normal restart reads it. This test input is unrelated to real apps.
    gui=gui_open(s);settings_guest_file_path(s,gui,false,output);gui_save_enter(s,gui);
    const auto saved=chain_settings(s);imported.generation=saved.generation;
    s.log.check("T06_structured_rules_and_mapping_actual_Save",saved==imported&&saved.generation>original.generation);
    const auto old_pid=s.product_pid;const auto old_birth=process_birth(s.process);const auto binary=hash(s.product);cleanup(s);const auto restart_mark=read_product(s).size();
    s.log.check("T06_normal_exit_before_same_EXE_restart",!s.process);
    s.log.check("T06_same_EXE_restart_actual",launch(s,L"--diagnostic",false)&&wait([&]{return number(record(s,restart_mark,"startup"),"\"process_id\":")==s.product_pid;},8000));
    s.log.check("T06_restart_preserves_same_mappings_rules_and_unknown_bits",s.product_pid!=old_pid&&process_birth(s.process)!=old_birth&&hash(s.product)==binary&&chain_settings(s)==saved);
    // Remove only the explicitly imported temporary rule through the UI.
    gui=gui_open(s);gui_page_select(s,gui,4);review_list_select(s,gui,1220,original.rules.size());
    const auto remove=review_page_control(s,gui,1223);review_tab_to(s,gui,remove);gui_press(s,gui,VK_SPACE);gui_save_enter(s,gui);
    s.log.check("T06_owned_test_rule_removed_other_config_retained",chain_settings(s).rules==original.rules&&chain_settings(s).hotkeys==original.hotkeys);
}
void review_root_replace(State& s,const SettingsGui& gui,HWND edit,std::wstring_view wanted){
    review_tab_to(s,gui,edit);key(s,VK_CONTROL,true);gui_press(s,gui,'A');key(s,VK_CONTROL,false);
    const auto exact=[&]{return gui_identity(s,gui,edit)&&GetForegroundWindow()==gui.root&&gui_focus(gui)==edit&&
        gui_class(edit)==L"Edit"&&IsWindowVisible(edit)&&IsWindowEnabled(edit)&&!(GetWindowLongPtrW(edit,GWL_STYLE)&ES_READONLY);};
    settings_guest_unicode(s,wanted,exact);
    s.log.check("settings_review_root_single_line_actual_keys",wait([&]{return exact()&&gui_text(s,gui,edit)==wanted;}),
        "\"control_id\":"+std::to_string(GetDlgCtrlID(edit))+",\"actual_text\":"+quote(utf8(std::wstring(wanted)))+",\"WM_SETTEXT_used\":false");
}
std::wstring review_hex(int color){
    std::array<wchar_t,8> value{};swprintf_s(value.data(),value.size(),L"#%02X%02X%02X",GetRValue(color),GetGValue(color),GetBValue(color));
    return value.data();
}
void review_preview_fill(State& s,COLORREF expected,std::string_view label){
    LayoutFixtures set{s};set.create();
    try{
        const auto work=chain_work(set.frames[0].hwnd);const LayoutLimit limit{120,100,work.width(),work.height()};
        if(work.width()<1100||work.height()<760)throw std::runtime_error("settings_review_preview_work_area_too_small");
        const Rect initial{work.l+260,work.t+160,work.l+660,work.t+460};set.setup({initial},{limit});
        const auto g=layout_down(set,0,HTCAPTION,false);const POINT edge{work.l+1,work.t+work.height()/2};
        const auto raw=translated(initial,edge.x-g.anchor.x,edge.y-g.anchor.y);
        layout_phase(set,0,g,edge.x-g.anchor.x,edge.y-g.anchor.y,{raw},std::string(label)+".actual_left_preview");
        const Rect half{work.l,work.t,work.l+work.width()/2,work.b};chain_preview_exact(s,set.frames[0].hwnd,half);
        const auto overlay=chain_preview(s);const auto source=set.frames[0].hwnd;RECT client{};
        if(!overlay||!GetClientRect(source,&client))throw std::runtime_error("settings_review_preview_exact_overlay_or_client_unknown");
        POINT tl{client.left,client.top},br{client.right,client.bottom};if(!ClientToScreen(source,&tl)||!ClientToScreen(source,&br))throw std::runtime_error("settings_review_preview_client_screen_unknown");
        const LONG left=std::max(tl.x,half.l+5),right=std::min(br.x,half.r-5),top=std::max(tl.y,half.t+5),bottom=std::min(br.y,half.b-5);
        if(right<=left||bottom<=top)throw std::runtime_error("settings_review_preview_no_owned_client_intersection");
        const POINT point{left+(right-left)/2,top+(bottom-top)/2};const auto flushed=DwmFlush();
        HDC screen=GetDC(nullptr);COLORREF actual=CLR_INVALID;BYTE alpha{};DWORD flags{};COLORREF keycolor{};
        bool read=false;try{read=screen&&SUCCEEDED(flushed)&&wait([&]{if(!s.left||!set.owns(0)||GetForegroundWindow()!=source||
            visible(source)!=raw||chain_preview(s)!=overlay||!IsWindowVisible(overlay))return false;
            actual=GetPixel(screen,point.x,point.y);return GetLayeredWindowAttributes(overlay,&keycolor,&alpha,&flags)&&alpha==255&&flags==LWA_ALPHA&&actual==expected;},1000);
        }catch(...){if(screen)ReleaseDC(nullptr,screen);throw;}
        const bool released=screen&&ReleaseDC(nullptr,screen)==1;
        s.log.check(std::string(label)+".actual_opaque_preview_pixel",read&&released&&actual==expected,
            "\"actual_screen_COLORREF\":"+(actual==CLR_INVALID?"null":std::to_string(actual))+",\"expected_COLORREF\":"+std::to_string(expected)+
            ",\"actual_DPI\":"+std::to_string(GetDpiForWindow(source))+",\"sample\":["+std::to_string(point.x)+","+std::to_string(point.y)+"],\"actual_alpha\":"+std::to_string(alpha));
        const Rect away=translated(initial,24,17);layout_phase(set,0,g,24,17,{away},std::string(label)+".leave_preview");
        chain_preview_hidden(s,(std::string(label)+".cancel_preview").c_str());layout_up(set,g,{away});
        s.log.check(std::string(label)+".owned_fixture_closed",set.close());
    }catch(...){pause_routing_failed_case_input(s);try{if(!set.closed)set.close();}catch(...){}throw;}
}
void owned_settings_ui_color_case(State& s){
    settings_guest_scope(s);const auto before=chain_settings(s);const auto bytes=read(s.product.parent_path()/L"settings.ini");
    auto gui=gui_open(s);gui_page_select(s,gui,2);const auto color=review_page_control(s,gui,1230);
    const auto initial=gui_text(s,gui,color);
    s.log.check("T07_existing_COLORREF_uses_correct_RGB_order",initial==review_hex(before.preview_color));
    const auto choose=review_page_control(s,gui,1231);review_tab_to(s,gui,choose);gui_press(s,gui,VK_SPACE);
    const auto dialog=settings_guest_modal(s,gui,L"颜色");settings_guest_modal_press(s,gui,dialog,VK_ESCAPE);
    s.log.check("T07_actual_ChooseColor_Cancel_preserves_draft",wait([&]{return !IsWindow(dialog.root)&&GetForegroundWindow()==gui.root;})&&gui_text(s,gui,color)==initial&&read(s.product.parent_path()/L"settings.ini")==bytes);
    review_root_replace(s,gui,color,L"#12ZZ00");settings_guest_activate(s,gui,GetDlgItem(gui.root,IDOK));
    settings_guest_acknowledge(s,gui,L"颜色格式无效",L"PaneBind：设置未更改");
    s.log.check("T07_invalid_RGB_does_not_Save",read(s.product.parent_path()/L"settings.ini")==bytes&&gui_text(s,gui,color)==L"#12ZZ00");
    review_root_replace(s,gui,color,L"#0078D7");
    gui_page_select(s,gui,1);chain_checkbox(s,gui,L"移动时磁吸",false);
    gui_page_select(s,gui,2);chain_checkbox(s,gui,L"启用边角停靠",true);chain_checkbox(s,gui,L"停靠时参考",false);chain_checkbox(s,gui,L"显示非激活",true);
    gui_edit_number(s,gui,gui_number(s,gui,L"预览不透明度"),255);gui_save_enter(s,gui);
    s.log.check("T07_default_blue_saved_without_red_blue_swap",chain_settings(s).preview_color==0xd77800&&chain_settings(s).preview_alpha==255);
    review_preview_fill(s,RGB(0,120,215),"T07_blue");
    gui=gui_open(s);gui_page_select(s,gui,2);const auto second_color=review_page_control(s,gui,1230);
    const auto open_picker=review_page_control(s,gui,1231);review_tab_to(s,gui,open_picker);gui_press(s,gui,VK_SPACE);
    const auto picker=settings_guest_modal(s,gui,L"颜色");
    const auto focus_picker=[&](HWND target,int target_id){
        GUITHREADINFO initial{sizeof(initial)};const bool initial_known=GetGUIThreadInfo(picker.tid,&initial)!=FALSE;
        HWND before=initial.hwndFocus;unsigned tabs{};bool reached{};std::string failure;
        for(;tabs<64;){
            GUITHREADINFO context{sizeof(context)};
            if(!settings_guest_modal_identity(s,gui,picker)||GetForegroundWindow()!=picker.root||
               !GetGUIThreadInfo(picker.tid,&context)){failure="exact_modal_focus_context_unknown";break;}
            if(context.hwndFocus==target){reached=true;break;}
            before=context.hwndFocus;settings_guest_modal_press(s,gui,picker,VK_TAB);++tabs;
            const bool changed=wait([&]{GUITHREADINFO after{sizeof(after)};return settings_guest_modal_identity(s,gui,picker)&&
                GetForegroundWindow()==picker.root&&GetGUIThreadInfo(picker.tid,&after)&&after.hwndFocus&&
                after.hwndFocus!=before&&IsChild(picker.root,after.hwndFocus);},500);
            if(!changed){failure="actual_Tab_focus_change_not_observed";break;}
        }
        GUITHREADINFO final{sizeof(final)};const bool final_known=GetGUIThreadInfo(picker.tid,&final)!=FALSE;
        DWORD pid{};const auto target_tid=GetWindowThreadProcessId(target,&pid);const auto foreground=GetForegroundWindow();
        reached=reached||(tabs==64&&final_known&&final.hwndFocus==target);
        s.log.check("T07_ChooseColor_actual_Tab_reaches_target",reached&&final_known&&final.hwndFocus==target&&
            settings_guest_modal_identity(s,gui,picker)&&foreground==picker.root&&target_tid==picker.tid&&pid==s.product_pid&&
            GetDlgItem(picker.root,target_id)==target&&IsWindowVisible(target)&&IsWindowEnabled(target),
            "\"target_id\":"+std::to_string(target_id)+",\"target\":"+std::to_string(reinterpret_cast<std::uintptr_t>(target))+
            ",\"initial_focus_known\":"+(initial_known?"true":"false")+",\"initial_focus\":"+std::to_string(reinterpret_cast<std::uintptr_t>(initial.hwndFocus))+
            ",\"before_last_Tab_focus\":"+std::to_string(reinterpret_cast<std::uintptr_t>(before))+",\"final_focus_known\":"+(final_known?"true":"false")+
            ",\"final_focus\":"+std::to_string(reinterpret_cast<std::uintptr_t>(final.hwndFocus))+",\"final_focus_id\":"+std::to_string(GetDlgCtrlID(final.hwndFocus))+
            ",\"actual_Tab_key_pairs\":"+std::to_string(tabs)+",\"foreground\":"+std::to_string(reinterpret_cast<std::uintptr_t>(foreground))+
            ",\"expected_foreground\":"+std::to_string(reinterpret_cast<std::uintptr_t>(picker.root))+",\"failure\":"+(failure.empty()?"null":quote(failure))+
            ",\"extra_Tab_or_SetFocus_used\":false");
    };
    // Actual SDK 10.0.26100.0/um/ColorDlg.h identifiers, not guessed IDs.
    // The dialog is also checked as the exact product-owned native modal.
    for(const auto& channel:std::array<std::pair<int,unsigned>,3>{{{COLOR_RED,215},{COLOR_GREEN,40},{COLOR_BLUE,70}}}){
        const auto edit=GetDlgItem(picker.root,channel.first);
        if(!edit||gui_class(edit)!=L"Edit"||!IsWindowVisible(edit)||!IsWindowEnabled(edit)||!IsChild(picker.root,edit))
            throw std::runtime_error("settings_review_ChooseColor_actual_SDK_RGB_edit_missing");
        focus_picker(edit,channel.first);
        const auto focused=[&]{GUITHREADINFO context{sizeof(context)};DWORD pid{};
            return settings_guest_modal_identity(s,gui,picker)&&GetForegroundWindow()==picker.root&&GetGUIThreadInfo(picker.tid,&context)&&
                context.hwndFocus==edit&&GetWindowThreadProcessId(edit,&pid)==picker.tid&&pid==s.product_pid&&IsChild(picker.root,edit)&&IsWindowEnabled(edit);};
        if(!focused())throw std::runtime_error("settings_review_ChooseColor_RGB_target_not_reached");
        key(s,VK_CONTROL,true);settings_guest_modal_press(s,gui,picker,'A');key(s,VK_CONTROL,false);
        const auto desired=std::to_wstring(channel.second);settings_guest_unicode(s,desired,focused);
        std::array<wchar_t,16> value{};DWORD_PTR copied{};
        const bool matched=wait([&]{return focused()&&SendMessageTimeoutW(edit,WM_GETTEXT,value.size(),reinterpret_cast<LPARAM>(value.data()),SMTO_ABORTIFHUNG|SMTO_BLOCK,200,&copied)&&
            copied<value.size()&&std::wstring_view(value.data(),copied)==desired;});
        const bool read_known=settings_guest_modal_identity(s,gui,picker)&&SendMessageTimeoutW(edit,WM_GETTEXT,value.size(),
            reinterpret_cast<LPARAM>(value.data()),SMTO_ABORTIFHUNG|SMTO_BLOCK,200,&copied)&&copied<value.size();
        s.log.check("T07_ChooseColor_actual_RGB_keys",matched&&read_known&&std::wstring_view(value.data(),copied)==desired,
            "\"SDK_control_id\":"+std::to_string(channel.first)+",\"expected_channel_value\":"+std::to_string(channel.second)+
            ",\"actual_text\":"+(read_known?quote(utf8(std::wstring(value.data(),copied))):"null")+
            ",\"actual_text_sampled_after_wait\":true,\"private_mutation_message_used\":false");
    }
    const auto picker_ok=GetDlgItem(picker.root,IDOK);focus_picker(picker_ok,IDOK);
    GUITHREADINFO confirm{sizeof(confirm)};
    if(!GetGUIThreadInfo(picker.tid,&confirm)||confirm.hwndFocus!=picker_ok)throw std::runtime_error("settings_review_ChooseColor_OK_not_reached");
    settings_guest_modal_press(s,gui,picker,VK_RETURN);
    s.log.check("T07_ChooseColor_actual_OK_changes_only_draft",wait([&]{return !IsWindow(picker.root)&&GetForegroundWindow()==gui.root&&gui_text(s,gui,second_color)==L"#D72846";})&&
        chain_settings(s).preview_color==0xd77800, "\"actual_native_RGB_selection\":\"215,40,70\",\"saved_blue_unchanged_until_Save\":true");
    gui_save_enter(s,gui);
    s.log.check("T07_second_RGB_saved_in_COLORREF_order",chain_settings(s).preview_color==static_cast<int>(RGB(215,40,70)));
    review_preview_fill(s,RGB(215,40,70),"T07_red");
    gui=gui_open(s);gui_page_select(s,gui,2);const auto old_hex=review_hex(before.preview_color);
    review_root_replace(s,gui,review_page_control(s,gui,1230),old_hex);
    gui_edit_number(s,gui,gui_number(s,gui,L"预览不透明度"),static_cast<unsigned>(before.preview_alpha));
    chain_checkbox(s,gui,L"启用边角停靠",before.docking);chain_checkbox(s,gui,L"停靠时参考",before.smart_docking);chain_checkbox(s,gui,L"显示非激活",before.preview);
    gui_page_select(s,gui,1);chain_checkbox(s,gui,L"移动时磁吸",before.magnet_move);gui_save_enter(s,gui);
    const auto restored=chain_settings(s);auto wanted=before;wanted.generation=restored.generation;
    s.log.check("T07_old_COLORREF_and_other_options_roundtrip_restored",restored==wanted);
}
