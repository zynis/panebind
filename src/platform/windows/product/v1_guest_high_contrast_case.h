#pragma once

// TEST ONLY. A one-use configuration lease in the exact disposable guest.
// Neither the product nor an ordinary host entry includes a system setter.
struct GuestHighContrastState {
    bool known{},scheme_known{},scheme_freed{};DWORD flags{},error{};
    std::wstring scheme;std::array<COLORREF,6> colors{};
    static GuestHighContrastState read(){
        GuestHighContrastState out;HIGHCONTRASTW actual{sizeof(actual)};
        SetLastError(ERROR_SUCCESS);out.known=SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(actual),&actual,0)!=FALSE;
        out.error=out.known?0:GetLastError();out.flags=actual.dwFlags;
        // SPI_GET owns this allocation. Do not retain its pointer through SET.
        struct Scheme {LPWSTR value{};~Scheme(){if(value)LocalFree(value);}} allocation{actual.lpszDefaultScheme};
        if(out.known&&allocation.value){const auto size=wcsnlen_s(allocation.value,4096);
            out.scheme_known=size<4096;if(out.scheme_known)out.scheme.assign(allocation.value,size);}
        out.scheme_freed=!allocation.value||LocalFree(allocation.value)==nullptr;allocation.value=nullptr;
        constexpr std::array<int,6> indices{COLOR_WINDOW,COLOR_WINDOWTEXT,COLOR_BTNFACE,COLOR_BTNTEXT,COLOR_INFOBK,COLOR_INFOTEXT};
        for(std::size_t i=0;i<indices.size();++i)out.colors[i]=GetSysColor(indices[i]);
        return out;
    }
    std::string json()const{
        std::string palette="[";for(const auto color:colors){if(palette.size()>1)palette+=',';palette+=std::to_string(color);}palette+=']';
        return "{\"GET_return\":"+std::string(known?"true":"false")+",\"GET_error\":"+std::to_string(error)+
            ",\"flags\":"+std::to_string(flags)+",\"HC_ON\":"+((flags&HCF_HIGHCONTRASTON)?"true":"false")+
            ",\"available\":"+((flags&HCF_AVAILABLE)?"true":"false")+",\"scheme_known\":"+(scheme_known?"true":"false")+
            ",\"scheme\":"+(scheme_known?quote(utf8(scheme)):"null")+",\"system_scheme_allocation_freed\":"+(scheme_freed?"true":"false")+
            ",\"colors_WINDOW_WINDOWTEXT_BTNFACE_BTNTEXT_INFOBK_INFOTEXT\":"+palette+"}";
    }
};
class GuestHighContrastLease {
    State& state_;bool attempted_{},finished_{};
public:
    const GuestHighContrastState original=GuestHighContrastState::read();
    explicit GuestHighContrastLease(State& state):state_(state){}
    bool enable(){
        if(original.flags&HCF_HIGHCONTRASTON)return true;
        HIGHCONTRASTW request{sizeof(request),original.flags|HCF_HIGHCONTRASTON,const_cast<LPWSTR>(original.scheme.c_str())};
        attempted_=true;SetLastError(ERROR_SUCCESS);
        const bool returned=SystemParametersInfoW(SPI_SETHIGHCONTRAST,sizeof(request),&request,SPIF_SENDCHANGE)!=FALSE;
        const DWORD error=returned?0:GetLastError();
        state_.log.emit("high_contrast_guest_SET","\"requested_ON\":true,\"native_return\":"+std::string(returned?"true":"false")+
            ",\"native_error\":"+std::to_string(error)+",\"requested_flags\":"+std::to_string(request.dwFlags)+
            ",\"fWinIni\":"+std::to_string(SPIF_SENDCHANGE)+",\"profile_update_requested\":false,\"host_API_called\":false");
        return returned;
    }
    bool restore(){
        if(finished_)return false;finished_=true;
        if(!attempted_&&(!original.known||!original.scheme_known||!original.scheme_freed)){
            state_.log.emit("high_contrast_guest_actual_restore","\"SET_attempted\":false,\"restore_required\":false,"
                "\"native_return\":null,\"reason\":\"no_SET_after_unknown_original\",\"host_API_called\":false");
            return true;
        }
        bool returned=true;DWORD error{};
        if(attempted_){HIGHCONTRASTW request{sizeof(request),original.flags,const_cast<LPWSTR>(original.scheme.c_str())};
            SetLastError(ERROR_SUCCESS);returned=SystemParametersInfoW(SPI_SETHIGHCONTRAST,sizeof(request),&request,SPIF_SENDCHANGE)!=FALSE;
            error=returned?0:GetLastError();}
        GuestHighContrastState actual;
        const bool exact=returned&&wait([&]{actual=GuestHighContrastState::read();return actual.known&&actual.scheme_known&&actual.scheme_freed&&
            actual.flags==original.flags&&actual.scheme==original.scheme&&actual.colors==original.colors;},1000);
        state_.log.emit("high_contrast_guest_actual_restore","\"SET_attempted\":"+std::string(attempted_?"true":"false")+
            ",\"native_return\":"+(attempted_?(returned?"true":"false"):"null")+",\"native_error\":"+std::to_string(error)+
            ",\"original\":"+original.json()+",\"actual_after\":"+actual.json()+",\"exact_original_flags_scheme_colors\":"+(exact?"true":"false")+
            ",\"profile_update_requested\":false,\"guest_destruction_used_as_restore\":false,\"not_product_resource_cleanup_receipt\":true");
        if(!exact)state_.log.failed=true;return exact;
    }
    ~GuestHighContrastLease(){if(!finished_)try{restore();}catch(...){state_.log.failed=true;}}
};

void guest_high_contrast_tab_to(State& s,const SettingsGui& gui,HWND wanted){
    for(unsigned step=0;step<48&&gui_focus(gui)!=wanted;++step){
        const auto before=gui_focus(gui);gui_press(s,gui,VK_TAB);
        if(!wait([&]{return gui_focus(gui)&&gui_focus(gui)!=before;}))throw std::runtime_error("high_contrast_actual_Tab_did_not_advance");
    }
    if(gui_focus(gui)!=wanted||!gui_identity(s,gui,wanted))throw std::runtime_error("high_contrast_Tab_target_not_reached");
}
void guest_high_contrast_focus(State& s,const SettingsGui& gui,HWND wanted,const char* role,bool caret){
    GUITHREADINFO info{sizeof(info)};RECT control{},page{};
    const bool native=GetGUIThreadInfo(gui.tid,&info)&&gui_identity(s,gui,wanted)&&info.hwndFocus==wanted&&
        GetForegroundWindow()==gui.root&&IsWindowVisible(wanted)&&IsWindowEnabled(wanted)&&
        GetWindowRect(wanted,&control)&&GetWindowRect(GetParent(wanted),&page);
    const auto ui_state=native?gui_read(s,gui,wanted,WM_QUERYUISTATE):DWORD_PTR{UISF_HIDEFOCUS};
    const bool contained=native&&control.left>=page.left&&control.top>=page.top&&control.right<=page.right&&control.bottom<=page.bottom;
    const bool actual_caret=!caret||(info.hwndCaret==wanted&&info.rcCaret.right>info.rcCaret.left&&info.rcCaret.bottom>info.rcCaret.top);
    s.log.check("high_contrast_actual_keyboard_focus",native&&contained&&actual_caret&&!(ui_state&UISF_HIDEFOCUS),
        std::string("\"role\":")+quote(role)+",\"focus\":"+std::to_string(reinterpret_cast<std::uintptr_t>(info.hwndFocus))+
        ",\"actual_caret\":"+std::to_string(reinterpret_cast<std::uintptr_t>(info.hwndCaret))+
        ",\"WM_QUERYUISTATE\":"+std::to_string(ui_state)+",\"focus_rect_not_hidden\":"+((ui_state&UISF_HIDEFOCUS)?"false":"true")+
        ",\"control_fully_in_parent\":"+(contained?"true":"false")+",\"screenshot_visual_review\":\"NOT_RUN\",\"pixels_not_inferred_from_focus_API\":true");
}

void owned_high_contrast_case(State& s){
    namespace e=product::guest::up_evidence_detail;
    settings_guest_scope(s);
    if(s.suite!="HighContrastOnly"||s.left||s.control||!chain_settings(s).enabled)
        throw std::runtime_error("high_contrast_exact_released_enabled_guest_required");
    const auto original=chain_settings(s);const auto pid=s.product_pid;const auto birth=process_birth(s.process);
    const auto binary=hash(s.product);const auto initial_status=geometry_history_status(s);
    if(e::unique_value(initial_status,"\"gesture_active\":")!="false"||
       e::unique_value(initial_status,"\"native_move_handoff_pending\":")!="false"||
       e::unique_value(initial_status,"\"native_operation_pending\":")!="false")
        throw std::runtime_error("high_contrast_SET_requires_actual_idle_product");
    GuestHighContrastLease lease{s};
    s.log.emit("high_contrast_guest_original","\"actual\":"+lease.original.json()+
        ",\"ordinary_host_entry\":false,\"real_200_percent_DPI\":\"NOT_RUN\",\"product_EXE_rebuilt_for_TEST\":false");
    const auto blocked=[&](const char* why){s.log.emit("high_contrast_environment_blocked",
        std::string("\"environment_blocked\":true,\"reason\":")+quote(why)+
        ",\"theme_registry_fallback_used\":false,\"whole_M10_F05_PASS\":false");};
    if(!lease.original.known||!lease.original.scheme_known||!lease.original.scheme_freed||
       !(lease.original.flags&HCF_AVAILABLE)||(lease.original.flags&HCF_OPTION_NOTHEMECHANGE)){
        blocked("actual_HC_availability_scheme_or_safe_restore_unknown");return;
    }
    SettingsGui gui;std::unique_ptr<LayoutFixtures> source;
    try{
        if(!lease.enable()){blocked("SPI_SETHIGHCONTRAST_failed_no_alternate_theme_route");
            if(!lease.restore())throw std::runtime_error("high_contrast_restore_after_SET_failure_failed");return;}
        GuestHighContrastState active;
        const bool enabled=wait([&]{active=GuestHighContrastState::read();return active.known&&active.scheme_known&&active.scheme_freed&&
            (active.flags&HCF_HIGHCONTRASTON);},1000);
        s.log.check("high_contrast_actual_GET_ON",enabled&&active.colors[0]!=active.colors[1]&&active.colors[2]!=active.colors[3],
            "\"actual\":"+active.json()+",\"palette_changed\":"+(active.colors!=lease.original.colors?"true":"false")+
            ",\"mode_and_color_scheme_are_distinct\":true,\"visual_readability_inferred\":false");
        gui=gui_open(s);gui_page_select(s,gui,0);
        const auto enabled_control=gui_find(s,gui,L"Button",L"启用 PaneBind");
        guest_high_contrast_tab_to(s,gui,enabled_control);guest_high_contrast_focus(s,gui,enabled_control,"general_enable_checkbox",false);
        guest_capture_settings_page(s,gui,0);
        gui_page_select(s,gui,3);const auto mappings=gui_find(s,gui,L"Edit",L"hotkey_count=");
        guest_high_contrast_tab_to(s,gui,mappings);guest_high_contrast_focus(s,gui,mappings,"shortcuts_mapping_edit",true);
        guest_capture_settings_page(s,gui,3);gui_save_enter(s,gui);
        auto live=chain_settings(s);auto expected=original;expected.generation=live.generation;
        s.log.check("high_contrast_real_Enter_preserves_logical_configuration",live==expected);
        const auto before_cancel=read(s.product.parent_path()/L"settings.ini");gui=gui_open(s);gui_press(s,gui,VK_ESCAPE);
        s.log.check("high_contrast_real_Esc_closes_without_write",wait([&]{return !IsWindow(gui.root);})&&
            read(s.product.parent_path()/L"settings.ini")==before_cancel);

        // A public no-history Undo produces a real rejection, not a fake fault.
        source=std::make_unique<LayoutFixtures>(s);source->create();const auto work=chain_work(source->frames[0].hwnd);
        if(work.width()<600||work.height()<500)throw std::runtime_error("high_contrast_notice_owned_work_area_too_small");
        const Rect actual{work.l+100,work.t+100,work.l+460,work.t+400};source->setup({actual});chain_front(*source,0);
        const HWND foreground_before=GetForegroundWindow();const auto mark=read_product(s).size();std::string notice;
        const bool requested=launch(s,L"--command Undo",true);
        const bool observed=wait([&]{notice=record(s,mark,"notice");return e::quoted(notice,"\"source_sha\":",s.product_source)&&
            e::number(notice,"\"process_id\":")==s.product_pid&&notice.find("没有仍归属本窗口的布局可撤销")!=notice.npos;});
        const HWND foreground_after=GetForegroundWindow();drain_foreground_observer();
        s.log.check("high_contrast_notice_actual_foreground_snapshots",requested&&observed&&source->owns(0)&&
            foreground_before==source->frames[0].hwnd&&foreground_after==foreground_before&&visible(foreground_before)==actual&&
            record(s,mark,"placement").empty(),
            "\"actual_notice\":"+quote(notice)+",\"foreground_before\":"+std::to_string(reinterpret_cast<std::uintptr_t>(foreground_before))+
            ",\"foreground_after\":"+std::to_string(reinterpret_cast<std::uintptr_t>(foreground_after))+
            ",\"continuous_foreground_proof\":false,\"existing_foreground_events_retained\":true,\"visible_notice_review\":\"NOT_RUN\"");
        std::string presentation;std::optional<std::uint64_t> serial;
        const bool presented=wait([&]{presentation=record(s,mark,"notice_presentation");serial=e::number(presentation,"\"serial\":");
            return e::quoted(presentation,"\"source_sha\":",s.product_source)&&e::number(presentation,"\"process_id\":")==s.product_pid&&
                e::quoted(presentation,"\"phase\":","show")&&serial&&*serial;},2000);
        s.log.check("high_contrast_real_rejection_has_actual_idle_notice",presented&&
            e::unique_value(presentation,"\"SetWindowPos_succeeded\":")=="true"&&
            e::unique_value(presentation,"\"actual_visible\":")=="true"&&
            e::unique_value(presentation,"\"foreground_unchanged\":")=="true",
            "\"actual_presentation\":"+quote(presentation));
        guest_actual_product_notice_strip(*source,0,actual,
            L"当前窗口已改变，或没有仍归属本窗口的布局可撤销。",L"high-contrast-notice");
        const HWND hint=chain_preview(s);cursor(s,{actual.l+60,actual.t+120});
        (void)geometry_history_status(s);
        s.log.check("high_contrast_idle_hover_does_not_immediately_erase_notice",hint&&chain_preview(s)==hint&&
            IsWindowVisible(hint)&&GetForegroundWindow()==foreground_before&&source->owns(0)&&visible(foreground_before)==actual);
        std::string hidden;
        const bool expired=wait([&]{hidden=record(s,mark,"notice_presentation");return
            e::number(hidden,"\"serial\":")==serial&&e::unique_value(hidden,"\"phase\":")=="\"hidden\""&&
            e::unique_value(hidden,"\"reason\":")=="\"notice_deadline\"";},5000);
        s.log.check("high_contrast_notice_real_one_shot_expiry_hides",expired&&
            e::unique_value(hidden,"\"timer_cancelled\":")=="true"&&
            e::unique_value(hidden,"\"title_cleared\":")=="true"&&!IsWindowVisible(hint)&&GetForegroundWindow()==foreground_before,
            "\"actual_hide\":"+quote(hidden));
        const auto next_mark=read_product(s).size();
        s.log.check("high_contrast_second_real_Undo_notice_requested",launch(s,L"--command Undo",true));
        std::string second;std::optional<std::uint64_t> next_serial;
        const bool second_shown=wait([&]{
            second=record(s,next_mark,"notice_presentation");next_serial=e::number(second,"\"serial\":");
            return e::quoted(second,"\"source_sha\":",s.product_source)&&e::number(second,"\"process_id\":")==s.product_pid&&
                e::quoted(second,"\"phase\":","show")&&next_serial&&*next_serial&&next_serial!=serial&&
                chain_preview(s)==hint&&IsWindowVisible(hint);},2000);
        s.log.check("high_contrast_second_real_Undo_notice_visible",second_shown,"\"actual_second_show\":"+quote(second));
        const auto g=layout_down(*source,0,HTCAPTION,false);
        const Rect moved=translated(actual,24,17);layout_phase(*source,0,g,24,17,{moved},"high_contrast_after_notice_real_Move");
        layout_up(*source,g,{moved});chain_preview_hidden(s,"high_contrast_new_gesture_does_not_inherit_old_notice");
        const auto retired=record(s,next_mark,"notice_presentation");
        const auto retire_reason=e::unique_value(retired,"\"reason\":");
        s.log.check("high_contrast_new_gesture_retires_actual_notice_timer",
            next_serial&&*next_serial&&e::number(retired,"\"serial\":")==next_serial&&e::unique_value(retired,"\"phase\":")=="\"hidden\""&&
            (retire_reason=="\"new_native_handoff\""||retire_reason=="\"new_gesture\"")&&
            e::unique_value(retired,"\"timer_cancelled\":")=="true"&&e::unique_value(retired,"\"title_cleared\":")=="true",
            "\"actual_notice_retirement\":"+quote(retired));
        s.log.check("high_contrast_notice_exact_owned_window_closed",source->close());
        chain_check_product(s,pid,birth);s.log.check("high_contrast_same_product_EXE",hash(s.product)==binary);
        s.log.check("high_contrast_original_system_state_restored",lease.restore());
        s.log.emit("high_contrast_component","\"result\":\"PASS_AUTOMATED_HC_MODE_AND_KEYBOARD_SUBSET\","
            "\"screen_BMP_visual_review\":\"NOT_RUN\",\"real_DPI\":\"ACTUAL_GUEST_NOT_FORCED\","
            "\"200_percent_DPI\":\"NOT_RUN\",\"specific_notice_visible_exit\":\"ACTUAL_WINDOW_PIXELS_AND_HIDE_REQUIRE_VISUAL_REVIEW\","
            "\"animation_native_API_trace\":\"NOT_RUN_SOURCE_BRANCH_ONLY\",\"whole_M10_F05_PASS\":false,\"human_UAT\":\"NOT_RUN\"");
    }catch(...){
        if(gui.root&&IsWindow(gui.root)&&gui_identity(s,gui,gui_focus(gui))&&GetForegroundWindow()==gui.root)
            try{gui_press(s,gui,VK_ESCAPE);}catch(...){}
        if(source&&!source->closed)try{source->close();}catch(...){}
        try{lease.restore();}catch(...){s.log.failed=true;}throw;
    }
}
