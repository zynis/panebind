#pragma once

// TEST ONLY. One semantic, read-only observation at the existing exact UID1
// rectangle. No tree search, ancestor fallback, focus, Invoke or synthetic
// callback: only a freshly identified PaneBind button may supply a physical
// clickable point for the existing guarded real mouse pair.
template<class Guard,class Emit>
POINT medium_tray_semantic_point(RECT icon,DWORD shell_pid,Guard&& guard,Emit&& emit){
    struct Client {IUIAutomation2* value{};~Client(){if(value)value->Release();}} client;
    struct Element {IUIAutomationElement* value{};~Element(){if(value)value->Release();}} element;
    struct Name {BSTR value{};~Name(){if(value)SysFreeString(value);}} name;
    const auto began=GetTickCount64();const HWND foreground=GetForegroundWindow();
    const auto active=[&]{return GetTickCount64()-began<1500&&guard()&&GetForegroundWindow()==foreground;};
    if(!shell_pid||!active())throw std::runtime_error("Medium_UI_semantic_context_not_current");
    const HRESULT created=CoCreateInstance(__uuidof(CUIAutomation8),nullptr,CLSCTX_INPROC_SERVER,
        __uuidof(IUIAutomation2),reinterpret_cast<void**>(&client.value));
    HRESULT connection=E_PENDING,transaction=E_PENDING,focus=E_PENDING,located=E_PENDING;
    HRESULT process=E_PENDING,label=E_PENDING,kind=E_PENDING,bounds=E_PENDING,offscreen=E_PENDING,enabled=E_PENDING,clickable=E_PENDING;
    int pid{};CONTROLTYPEID control{};RECT actual{};BOOL off{},available{},got{};POINT point{};
    const POINT center{icon.left+(icon.right-icon.left)/2,icon.top+(icon.bottom-icon.top)/2};
    if(SUCCEEDED(created)&&client.value&&active()){
        connection=client.value->put_ConnectionTimeout(150);
        transaction=client.value->put_TransactionTimeout(150);
        // This case performs no pattern action at all. Keep implicit focus
        // disabled as well; an API return never substitutes for the guards.
        focus=client.value->put_AutoSetFocus(FALSE);
        if(SUCCEEDED(connection)&&SUCCEEDED(transaction)&&SUCCEEDED(focus)&&active())
            located=client.value->ElementFromPoint(center,&element.value);
        if(SUCCEEDED(located)&&element.value&&active())process=element.value->get_CurrentProcessId(&pid);
        if(SUCCEEDED(process)&&pid>0&&static_cast<DWORD>(pid)==shell_pid&&active())label=element.value->get_CurrentName(&name.value);
        if(SUCCEEDED(label)&&name.value&&SysStringLen(name.value)<=128&&active())kind=element.value->get_CurrentControlType(&control);
        if(SUCCEEDED(kind)&&control==UIA_ButtonControlTypeId&&active())bounds=element.value->get_CurrentBoundingRectangle(&actual);
        if(SUCCEEDED(bounds)&&active())offscreen=element.value->get_CurrentIsOffscreen(&off);
        if(SUCCEEDED(offscreen)&&!off&&active())enabled=element.value->get_CurrentIsEnabled(&available);
        if(SUCCEEDED(enabled)&&available&&active())clickable=element.value->GetClickablePoint(&point,&got);
    }
    const std::wstring_view text=name.value&&SysStringLen(name.value)<=128?std::wstring_view(name.value,SysStringLen(name.value)):std::wstring_view{};
    const bool own_name=text==L"PaneBind"||text==L"PaneBind — 运行中；Shift 起手为原生操作"||
        text==L"PaneBind — 已暂停"||text==L"PaneBind — 输入故障";
    const bool exact=SUCCEEDED(process)&&static_cast<DWORD>(pid)==shell_pid&&SUCCEEDED(label)&&own_name&&
        SUCCEEDED(kind)&&control==UIA_ButtonControlTypeId&&SUCCEEDED(bounds)&&actual.left<actual.right&&actual.top<actual.bottom&&
        SUCCEEDED(offscreen)&&!off&&SUCCEEDED(enabled)&&available&&SUCCEEDED(clickable)&&got&&
        PtInRect(&icon,point)&&PtInRect(&actual,point)&&active();
    const auto hr=[](HRESULT value){return std::to_string(static_cast<long>(value));};
    emit("medium_desktop_ui_semantic_icon","\"CoCreateInstance_hresult\":"+hr(created)+
        ",\"connection_timeout_hresult\":"+hr(connection)+",\"transaction_timeout_hresult\":"+hr(transaction)+
        ",\"timeout_ms\":150,\"AutoSetFocus_false_hresult\":"+hr(focus)+",\"ElementFromPoint_hresult\":"+hr(located)+
        ",\"process_hresult\":"+hr(process)+",\"actual_provider_PID\":"+(SUCCEEDED(process)?std::to_string(pid):"null")+
        ",\"name_hresult\":"+hr(label)+",\"exact_PaneBind_name\":"+(SUCCEEDED(label)?(own_name?"true":"false"):"null")+
        ",\"control_type_hresult\":"+hr(kind)+",\"actual_control_type\":"+(SUCCEEDED(kind)?std::to_string(control):"null")+
        ",\"bounds_hresult\":"+hr(bounds)+",\"actual_UIA_rect\":"+(SUCCEEDED(bounds)?rect({actual.left,actual.top,actual.right,actual.bottom}):"null")+
        ",\"offscreen_hresult\":"+hr(offscreen)+",\"offscreen\":"+(SUCCEEDED(offscreen)?(off?"true":"false"):"null")+
        ",\"enabled_hresult\":"+hr(enabled)+",\"enabled\":"+(SUCCEEDED(enabled)?(available?"true":"false"):"null")+
        ",\"GetClickablePoint_hresult\":"+hr(clickable)+",\"got_point\":"+(SUCCEEDED(clickable)?(got?"true":"false"):"null")+
        ",\"physical_point\":"+(SUCCEEDED(clickable)&&got?("["+std::to_string(point.x)+","+std::to_string(point.y)+"]"):"null")+
        ",\"semantic_guard_accepted\":"+(exact?"true":"false")+
        ",\"before_tick\":"+std::to_string(began)+",\"after_tick\":"+std::to_string(GetTickCount64())+
        ",\"read_only_UIA\":true,\"tree_or_parent_fallback\":false,\"Invoke_or_ShowContextMenu_or_SetFocus_called\":false,\"nonmatching_name_redacted\":true");
    if(!exact)throw std::runtime_error("Medium_UI_exact_semantic_product_button_not_confirmed_no_click");
    return point;
}
