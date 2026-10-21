#pragma once

// TEST ONLY. Include after geometry-history helpers. One unchanged public
// Lower, two active exact owned roots, passive finite observations only. No
// inferred shell role, ignored unknown HWND, source activation rescue or
// alternative native z-order operation is permitted here.
std::uint64_t lower_facts_clock(){LARGE_INTEGER value{};return QueryPerformanceCounter(&value)&&value.QuadPart>0?
    static_cast<std::uint64_t>(value.QuadPart):0;}
std::string lower_facts_hwnd(HWND value){return std::to_string(reinterpret_cast<std::uintptr_t>(value));}

struct LowerZTailResult {bool complete{},links_stable{},source_seen{},other_seen{};unsigned rows{};};
LowerZTailResult lower_facts_z_tail(State& s,const char* phase,HWND source,HWND other){
    LowerZTailResult result;const auto begin=lower_facts_clock();const HWND shell_before=GetShellWindow(),desktop_before=GetDesktopWindow();
    std::array<HWND,64> seen{};HWND current=source;bool loop{},all_links_stable=true;
    while(current&&result.rows<seen.size()){
        if(std::find(seen.begin(),seen.begin()+result.rows,current)!=seen.begin()+result.rows){loop=true;break;}
        const unsigned index=result.rows++;seen[index]=current;
        const auto entry_begin=lower_facts_clock();DWORD pid_before{},pid_after{};
        const auto tid_before=GetWindowThreadProcessId(current,&pid_before);const bool live_before=IsWindow(current)!=FALSE;
        const HWND previous_before=GetWindow(current,GW_HWNDPREV),next_before=GetWindow(current,GW_HWNDNEXT);
        const HWND owner=GetWindow(current,GW_OWNER),shell_at_entry=GetShellWindow(),desktop_at_entry=GetDesktopWindow();
        std::array<wchar_t,256> class_name{};SetLastError(ERROR_SUCCESS);
        const int class_length=GetClassNameW(current,class_name.data(),static_cast<int>(class_name.size()));
        const DWORD class_error=class_length?ERROR_SUCCESS:GetLastError();
        const bool class_known=class_length>0&&class_length<static_cast<int>(class_name.size())-1;
        SetLastError(ERROR_SUCCESS);const auto style=static_cast<ULONG_PTR>(GetWindowLongPtrW(current,GWL_STYLE));
        const DWORD style_error=GetLastError();const bool style_known=style||style_error==ERROR_SUCCESS;
        SetLastError(ERROR_SUCCESS);const auto extended=static_cast<ULONG_PTR>(GetWindowLongPtrW(current,GWL_EXSTYLE));
        const DWORD extended_error=GetLastError();const bool extended_known=extended||extended_error==ERROR_SUCCESS;
        const bool visible_now=IsWindowVisible(current)!=FALSE;DWORD cloak{};
        const auto cloak_result=DwmGetWindowAttribute(current,DWMWA_CLOAKED,&cloak,sizeof(cloak));
        const bool live_after=IsWindow(current)!=FALSE;const auto tid_after=GetWindowThreadProcessId(current,&pid_after);
        const HWND previous_after=GetWindow(current,GW_HWNDPREV),next_after=GetWindow(current,GW_HWNDNEXT);
        const auto entry_end=lower_facts_clock();
        const bool identity_stable=live_before&&live_after&&pid_before&&tid_before&&pid_before==pid_after&&tid_before==tid_after;
        const bool links_stable=identity_stable&&previous_before==previous_after&&next_before==next_after;
        all_links_stable=all_links_stable&&links_stable;
        result.source_seen=result.source_seen||current==source;result.other_seen=result.other_seen||current==other;
        s.log.emit("lower_facts_Z_tail_entry","\"phase\":"+quote(phase)+",\"index\":"+std::to_string(index)+
            ",\"entry_begin_qpc\":"+std::to_string(entry_begin)+",\"entry_end_qpc\":"+std::to_string(entry_end)+
            ",\"HWND\":"+lower_facts_hwnd(current)+",\"live_before\":"+(live_before?"true":"false")+
            ",\"live_after\":"+(live_after?"true":"false")+",\"PID_before\":"+std::to_string(pid_before)+
            ",\"TID_before\":"+std::to_string(tid_before)+",\"PID_after\":"+std::to_string(pid_after)+
            ",\"TID_after\":"+std::to_string(tid_after)+",\"identity_stable\":"+(identity_stable?"true":"false")+
            ",\"previous_before\":"+lower_facts_hwnd(previous_before)+",\"next_before\":"+lower_facts_hwnd(next_before)+
            ",\"previous_after\":"+lower_facts_hwnd(previous_after)+",\"next_after\":"+lower_facts_hwnd(next_after)+
            ",\"links_stable_within_entry\":"+(links_stable?"true":"false")+",\"owner\":"+lower_facts_hwnd(owner)+
            ",\"class_known\":"+(class_known?"true":"false")+",\"class\":"+
            (class_known?quote(utf8(std::wstring(class_name.data(),static_cast<std::size_t>(class_length)))):"null")+
            ",\"class_error\":"+std::to_string(class_error)+",\"style_known\":"+(style_known?"true":"false")+
            ",\"style\":"+(style_known?std::to_string(style):"null")+",\"style_error\":"+std::to_string(style_error)+
            ",\"exstyle_known\":"+(extended_known?"true":"false")+",\"exstyle\":"+(extended_known?std::to_string(extended):"null")+
            ",\"exstyle_error\":"+std::to_string(extended_error)+",\"visible\":"+(visible_now?"true":"false")+
            ",\"cloak_hresult\":"+std::to_string(cloak_result)+",\"cloaked\":"+(SUCCEEDED(cloak_result)?std::to_string(cloak):"null")+
            ",\"GetShellWindow_at_entry\":"+lower_facts_hwnd(shell_at_entry)+",\"GetDesktopWindow_at_entry\":"+lower_facts_hwnd(desktop_at_entry)+
            ",\"equals_actual_shell_handle\":"+(shell_at_entry&&current==shell_at_entry?"true":"false")+
            ",\"equals_actual_desktop_handle\":"+(desktop_at_entry&&current==desktop_at_entry?"true":"false")+
            ",\"exact_source\":"+(current==source?"true":"false")+",\"exact_other\":"+(current==other?"true":"false")+
            ",\"title_or_content_read\":false,\"unknown_window_control\":false,\"role_not_inferred_from_class_or_number\":true");
        current=next_before;
    }
    const HWND shell_after=GetShellWindow(),desktop_after=GetDesktopWindow();const auto end=lower_facts_clock();
    result.complete=!current&&!loop;result.links_stable=all_links_stable;
    s.log.emit("lower_facts_Z_tail_summary","\"phase\":"+quote(phase)+",\"begin_qpc\":"+std::to_string(begin)+
        ",\"end_qpc\":"+std::to_string(end)+",\"row_limit\":64,\"rows\":"+std::to_string(result.rows)+
        ",\"reached_raw_NULL\":"+(result.complete?"true":"false")+",\"cycle_seen\":"+(loop?"true":"false")+
        ",\"next_unvisited_HWND\":"+lower_facts_hwnd(current)+",\"all_per_entry_links_stable\":"+(all_links_stable?"true":"false")+
        ",\"shell_before\":"+lower_facts_hwnd(shell_before)+",\"shell_after\":"+lower_facts_hwnd(shell_after)+
        ",\"desktop_before\":"+lower_facts_hwnd(desktop_before)+",\"desktop_after\":"+lower_facts_hwnd(desktop_after)+
        ",\"global_atomic_Z_snapshot\":false,\"different_phases_are_not_one_snapshot\":true,\"semantic_PASS_inferred\":false");
    return result;
}

struct LowerMessageFacts {unsigned claimed{},published{},lower_changing{},lower_changed{};bool complete{};};
LowerMessageFacts lower_facts_window_messages(State& s,Frame& frame,const char* role,DWORD owner){
    auto& buffer=frame.window_positions;buffer.enabled.store(false,std::memory_order_release);
    const unsigned claimed=buffer.claimed.load(std::memory_order_acquire);
    const unsigned count=std::min<unsigned>(claimed,static_cast<unsigned>(buffer.receipts.size()));
    const bool published=wait([&]{for(unsigned n=0;n<count;++n)if(!buffer.receipts[n].published.load(std::memory_order_acquire))return false;return true;},500);
    LowerMessageFacts result;result.claimed=claimed;result.complete=published&&claimed<=buffer.receipts.size();
    for(unsigned n=0;n<count;++n){const auto& fact=buffer.receipts[n];if(!fact.published.load(std::memory_order_acquire))continue;
        ++result.published;const auto& position=fact.position;
        const bool exact=fact.hwnd==frame.hwnd&&position.hwnd==frame.hwnd&&fact.tid==owner;
        // Windows may normalize the API's HWND_BOTTOM pseudo-handle to the
        // actual predecessor before delivering WINDOWPOS. These messages only
        // prove an exact owned z-order notification, never the API argument or
        // successful Lower; the independent native receipt remains required.
        const bool lower=exact&&(position.flags&(SWP_NOMOVE|SWP_NOSIZE))==(SWP_NOMOVE|SWP_NOSIZE)&&
            !(position.flags&SWP_NOZORDER);
        if(lower&&fact.message==WM_WINDOWPOSCHANGING)++result.lower_changing;
        if(lower&&fact.message==WM_WINDOWPOSCHANGED)++result.lower_changed;
        s.log.emit("lower_facts_owned_WINDOWPOS","\"role\":"+quote(role)+",\"slot\":"+std::to_string(n)+
            ",\"message\":"+std::to_string(fact.message)+",\"HWND\":"+lower_facts_hwnd(fact.hwnd)+
            ",\"actual_owner_TID\":"+std::to_string(fact.tid)+",\"before_DefWindowProc_qpc\":"+std::to_string(fact.before_qpc)+
            ",\"after_DefWindowProc_qpc\":"+std::to_string(fact.after_qpc)+",\"WINDOWPOS_hwnd\":"+lower_facts_hwnd(position.hwnd)+
            ",\"WINDOWPOS_insertAfter\":"+lower_facts_hwnd(position.hwndInsertAfter)+",\"WINDOWPOS_flags\":"+std::to_string(position.flags)+
            ",\"WINDOWPOS_x_y_cx_cy\":["+std::to_string(position.x)+","+std::to_string(position.y)+","+
                std::to_string(position.cx)+","+std::to_string(position.cy)+"],\"live_before\":"+(fact.live_before?"true":"false")+
            ",\"live_after\":"+(fact.live_after?"true":"false")+",\"previous_before\":"+lower_facts_hwnd(fact.previous_before)+
            ",\"next_before\":"+lower_facts_hwnd(fact.next_before)+",\"previous_after\":"+lower_facts_hwnd(fact.previous_after)+
            ",\"next_after\":"+lower_facts_hwnd(fact.next_after)+",\"exact_fixture_owner\":"+(exact?"true":"false")+
            ",\"z_order_notification_observed\":"+(lower?"true":"false")+",\"API_insertAfter_not_inferred_from_message\":true,\"parameters_modified_by_observer\":false,\"DefWindowProc_preserved\":true");
    }
    s.log.emit("lower_facts_owned_WINDOWPOS_summary","\"role\":"+quote(role)+",\"slot_limit\":16,\"claimed\":"+std::to_string(claimed)+
        ",\"published\":"+std::to_string(result.published)+",\"complete_without_overflow\":"+(result.complete?"true":"false")+
        ",\"lower_CHANGING\":"+std::to_string(result.lower_changing)+",\"lower_CHANGED\":"+std::to_string(result.lower_changed)+
        ",\"API_completion_not_inferred_from_window_message\":true");
    return result;
}

[[maybe_unused]] void owned_lower_facts_case(State& s){
    namespace e=product::guest::up_evidence_detail;
    settings_guest_scope(s);const auto original=chain_settings(s);const auto pid=s.product_pid;
    const auto birth=process_birth(s.process);const auto binary=hash(s.product);
    const auto settings_path=s.product.parent_path()/L"settings.ini";const auto saved=read(settings_path);
    if(s.suite!="LowerFactsOnly"||!original.enabled||!original.rules.empty()||s.left||s.control||
       (GetAsyncKeyState(VK_LBUTTON)&0x8000)||(GetAsyncKeyState(VK_CONTROL)&0x8000)||
       (GetAsyncKeyState(VK_SHIFT)&0x8000)||(GetAsyncKeyState(VK_MENU)&0x8000)||binary.empty()||binary!=hash(s.input_product)||saved.empty())
        throw std::runtime_error("lower_facts_exact_released_guest_product_required");
    LayoutFixtures set{s};set.create();
    try{
        const auto work=chain_work(set.frames[0].hwnd);if(work.width()<1000||work.height()<700)throw std::runtime_error("lower_facts_work_area_too_small");
        const Rect initial{work.l+200,work.t+180,work.l+660,work.t+520},other=translated(initial,80,60);
        const LayoutLimit limit{300,240,work.width(),work.height()};set.setup({initial,other},{limit,limit});
        const HWND source=set.frames[0].hwnd,other_hwnd=set.frames[1].hwnd;const auto key=geometry_history_source_key(set,initial);
        const auto source_position=geometry_history_positioning(source),other_position=geometry_history_positioning(other_hwnd);
        const auto source_visual=native_visual(source),other_visual=native_visual(other_hwnd);
        if(!source_position||!other_position||!source_visual||!other_visual||!source_visual->known||!other_visual->known||
           source_visual->topmost||other_visual->topmost||!set.owns(0)||!set.owns(1)||GetForegroundWindow()!=source||
           set.frames[0].window_positions.claimed.load()||set.frames[1].window_positions.claimed.load())
            throw std::runtime_error("lower_facts_exact_normal_nontopmost_roots_required");
        chain_check_product(s,pid,birth);const auto before=lower_facts_z_tail(s,"before_one_Lower",source,other_hwnd);
        s.log.check("lower_facts_exact_owned_relative_order_before",before.source_seen&&before.other_seen&&
            set.owns(0)&&set.owns(1)&&GetForegroundWindow()==source&&visible(source)==initial&&visible(other_hwnd)==other,
            "\"active_exact_roots\":2,\"other_fixture_roots_hidden\":6,\"source_HWND\":"+lower_facts_hwnd(source)+
            ",\"other_HWND\":"+lower_facts_hwnd(other_hwnd)+",\"source_key\":"+std::to_string(key)+
            ",\"fixture_PID\":"+std::to_string(GetCurrentProcessId())+",\"fixture_owner_TID\":"+std::to_string(set.thread));
        const auto mark=read_product(s).size();const auto before_command_qpc=lower_facts_clock();
        set.frames[0].window_positions.enabled.store(true,std::memory_order_release);
        set.frames[1].window_positions.enabled.store(true,std::memory_order_release);
        std::string receipt,command;const bool sent=launch(s,L"--command Lower",true);
        const bool observed=sent&&wait([&]{receipt=record(s,mark,"lower_result");command=record(s,mark,"command");
            return geometry_history_receipt(s,receipt,"lower_result")&&e::number(receipt,"\"key\":")==key&&
                geometry_history_receipt(s,command,"command")&&e::number(command,"\"key\":")==key&&e::quoted(command,"\"action\":","Lower");});
        const auto after_command_qpc=lower_facts_clock();
        const auto after=lower_facts_z_tail(s,"after_actual_Lower_receipt",source,other_hwnd);
        const auto source_messages=lower_facts_window_messages(s,set.frames[0],"exact_source",set.thread);
        const auto other_messages=lower_facts_window_messages(s,set.frames[1],"exact_other",set.thread);
        unsigned commands{},lower_results{};std::istringstream rows(product_log_since(s,mark));std::string row;
        while(std::getline(rows,row)){if(geometry_history_receipt(s,row,"command")&&e::quoted(row,"\"action\":","Lower"))++commands;
            if(geometry_history_receipt(s,row,"lower_result"))++lower_results;}
        const bool native_completed=observed&&e::number(receipt,"\"call_id\":").value_or(0)>0&&
            e::unique_value(receipt,"\"native_attempted\":")=="true"&&e::unique_value(receipt,"\"native_completion_known\":")=="true"&&
            e::unique_value(receipt,"\"native_succeeded\":")=="true";
        const bool bottom=e::unique_value(receipt,"\"bottom_observed\":")=="true";
        const bool lower_observed=e::unique_value(receipt,"\"lower_observed\":")=="true";
        const HWND actual_next=GetWindow(source,GW_HWNDNEXT),actual_shell=GetShellWindow();
        const bool raw_tail=bottom&&!actual_next&&after.rows==1&&
            e::quoted(receipt,"\"reason\":","lower_native_bottom_observed");
        const bool shell_tail=!bottom&&actual_shell&&actual_next==actual_shell&&actual_shell!=source&&
            IsWindow(actual_shell)&&GetAncestor(actual_shell,GA_ROOT)==actual_shell&&
            GetWindow(actual_shell,GW_HWNDPREV)==source&&!GetWindow(actual_shell,GW_HWNDNEXT)&&after.rows==2&&
            e::unique_value(receipt,"\"shell_desktop_tail_observed\":")=="true"&&
            e::quoted(receipt,"\"reason\":","lower_native_shell_desktop_tail_observed");
        const bool unchanged=set.owns(0)&&set.owns(1)&&visible(source)==initial&&visible(other_hwnd)==other&&
            geometry_history_positioning(source)==source_position&&geometry_history_positioning(other_hwnd)==other_position;
        const bool semantic=native_completed&&lower_observed&&(raw_tail||shell_tail)&&
            after.complete&&after.links_stable&&after.source_seen&&!after.other_seen&&unchanged;
        s.log.emit("lower_facts_one_command_outcome","\"actual_lower_result\":"+quote(receipt)+",\"actual_command\":"+quote(command)+
            ",\"command_before_qpc\":"+std::to_string(before_command_qpc)+",\"command_observed_after_qpc\":"+std::to_string(after_command_qpc)+
            ",\"actual_Lower_dispatches\":"+std::to_string(commands)+",\"actual_Lower_results\":"+std::to_string(lower_results)+
            ",\"native_completed_successfully\":"+(native_completed?"true":"false")+",\"production_raw_bottom_observed\":"+(bottom?"true":"false")+
            ",\"geometry_and_other_window_unchanged\":"+(unchanged?"true":"false")+",\"production_semantic_contract_passed\":"+(semantic?"true":"false")+
            ",\"diagnostic_capture_is_not_function_PASS\":true,\"between_phase_order_not_atomic_snapshot\":true,\"additional_Lower_attempted\":false");
        chain_check_product(s,pid,birth);
        s.log.check("lower_facts_original_configuration_and_binary_unchanged",chain_settings(s)==original&&read(settings_path)==saved&&hash(s.product)==binary);
        s.log.check("lower_facts_exact_owned_roots_closed",set.close());
        s.log.check("lower_facts_actual_native_call_and_passive_message_evidence",native_completed&&commands==1&&lower_results==1&&
            source_messages.complete&&other_messages.complete&&source_messages.lower_changing&&source_messages.lower_changed,
            "\"actual_lower_result\":"+quote(receipt)+",\"window_message_does_not_replace_API_completion\":true");
        // Require the corrected product postcondition and an independent
        // actual tail observation; collecting facts alone is not a PASS.
        s.log.check("lower_facts_existing_production_semantic_contract",semantic,"\"actual_lower_result\":"+quote(receipt)+
            ",\"raw_bottom_not_rewritten\":true,\"no_intervening_node_filtered\":true,\"human_UAT\":\"NOT_RUN\"");
    }catch(...){set.frames[0].window_positions.enabled=false;set.frames[1].window_positions.enabled=false;
        try{if(!set.closed)set.close();}catch(...){}throw;}
}
