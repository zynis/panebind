#pragma once

// TEST ONLY. Included after Layout/settings/product-chain helpers. The fixture
// procedure forwards this case's one owner-UI setup message and observes real
// notifications; no WM_COMMAND/WM_NOTIFY or control activation is synthesized.
constexpr UINT standard_controls_ui_command=WM_APP+0x6d;
struct StandardControlsUi {
    HWND root{},tab{},button{},edit{};HMENU menu{},popup{};Frame* frame{};bool popup_attached{},menu_attached{};
    DWORD pid{},owner{};std::wstring class_name;
    std::atomic<unsigned> operation{},completed{};std::atomic<bool> success{};
    std::array<std::atomic<unsigned>,3> downs{},ups{};
    std::atomic<unsigned> tab_changes{},button_clicks{},menu_opened{},menu_commands{};
    std::atomic<int> tab_selection{-1};POINT second_tab{};
    static constexpr UINT tab_id=21001,button_id=21002,edit_id=21003,menu_id=21004;
    bool exact_root()const{
        DWORD actual{};return root&&frame&&IsWindow(root)&&GetWindowThreadProcessId(root,&actual)==owner&&
            actual==pid&&pid==GetCurrentProcessId()&&GetAncestor(root,GA_ROOT)==root&&
            GetWindowLongPtrW(root,GWLP_USERDATA)==reinterpret_cast<LONG_PTR>(frame)&&gui_class(root)==class_name;
    }
    bool exact_child(HWND h)const{
        DWORD actual{};return exact_root()&&h&&(h==tab||h==button||h==edit)&&IsWindow(h)&&
            GetParent(h)==root&&GetWindowThreadProcessId(h,&actual)==owner&&actual==pid;
    }
};
std::atomic<std::shared_ptr<StandardControlsUi>> standard_controls_ui;
LRESULT CALLBACK standard_controls_child(HWND h,UINT m,WPARAM w,LPARAM l,UINT_PTR id,DWORD_PTR token){
    const auto state=standard_controls_ui.load(std::memory_order_acquire);
    if(state&&reinterpret_cast<DWORD_PTR>(state.get())==token&&id<3&&
       GetCurrentThreadId()==state->owner&&state->exact_child(h)&&
       h==std::array{state->tab,state->button,state->edit}[id]){
        if(m==WM_LBUTTONDOWN)++state->downs[id];
        if(m==WM_LBUTTONUP)++state->ups[id];
    }
    return DefSubclassProc(h,m,w,l);
}
bool standard_controls_ui_message(HWND h,UINT m,WPARAM w,LPARAM l){
    const auto state=standard_controls_ui.load(std::memory_order_acquire);
    if(!state||h!=state->root||GetCurrentThreadId()!=state->owner||!state->exact_root())return false;
    auto& u=*state;
    if(m==standard_controls_ui_command){
        if(reinterpret_cast<LPARAM>(state.get())!=l||w!=u.operation.load())return false;
        bool ok=true;
        if(w==1){
            INITCOMMONCONTROLSEX common{sizeof(common),ICC_TAB_CLASSES};RECT client{};
            ok=InitCommonControlsEx(&common)&&GetClientRect(h,&client)&&client.right>=600&&client.bottom>=300&&
                !u.tab&&!u.button&&!u.edit&&!u.menu&&!u.popup&&!GetMenu(h);
            if(ok){
                const auto instance=GetModuleHandleW(nullptr);
                u.tab=CreateWindowExW(0,WC_TABCONTROLW,L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP,
                    20,20,client.right-40,54,h,reinterpret_cast<HMENU>(static_cast<UINT_PTR>(StandardControlsUi::tab_id)),instance,nullptr);
                u.button=CreateWindowExW(0,L"Button",L"实际按钮",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,
                    20,100,180,40,h,reinterpret_cast<HMENU>(static_cast<UINT_PTR>(StandardControlsUi::button_id)),instance,nullptr);
                u.edit=CreateWindowExW(WS_EX_CLIENTEDGE,L"Edit",L"abcdefg",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,
                    20,170,client.right-40,40,h,reinterpret_cast<HMENU>(static_cast<UINT_PTR>(StandardControlsUi::edit_id)),instance,nullptr);
                ok=u.exact_child(u.tab)&&u.exact_child(u.button)&&u.exact_child(u.edit);
                if(ok){
                    TCITEMW item{};item.mask=TCIF_TEXT;item.pszText=const_cast<LPWSTR>(L"第一页");
                    ok=TabCtrl_InsertItem(u.tab,0,&item)==0;item.pszText=const_cast<LPWSTR>(L"第二页");
                    ok=ok&&TabCtrl_InsertItem(u.tab,1,&item)==1;
                    RECT second{};ok=ok&&TabCtrl_GetCurSel(u.tab)==0&&TabCtrl_GetItemRect(u.tab,1,&second);
                    u.second_tab={(second.left+second.right)/2,(second.top+second.bottom)/2};
                    for(std::size_t n=0;n<3;++n){const HWND child=std::array{u.tab,u.button,u.edit}[n];
                        ok=SetWindowSubclass(child,standard_controls_child,n,reinterpret_cast<DWORD_PTR>(state.get()))&&ok;}
                }
                u.menu=CreateMenu();u.popup=CreatePopupMenu();
                ok=ok&&u.menu&&u.popup&&AppendMenuW(u.popup,MF_STRING,StandardControlsUi::menu_id,L"计数一次");
                if(ok){u.popup_attached=AppendMenuW(u.menu,MF_POPUP,reinterpret_cast<UINT_PTR>(u.popup),L"原生菜单")!=FALSE;ok=u.popup_attached;}
                if(ok){u.menu_attached=SetMenu(h,u.menu)!=FALSE;ok=u.menu_attached&&DrawMenuBar(h);}
            }
        }else if(w==2){
            GUITHREADINFO gui{sizeof(gui)};
            if(GetGUIThreadInfo(u.owner,&gui)&&gui.hwndMenuOwner==h)EndMenu();
            for(std::size_t n=0;n<3;++n){HWND* slot=n==0?&u.tab:n==1?&u.button:&u.edit;
                if(!*slot)continue;
                if(!u.exact_child(*slot)){ok=false;continue;}
                (void)RemoveWindowSubclass(*slot,standard_controls_child,n);
                const HWND child=*slot;if(!DestroyWindow(child)||IsWindow(child))ok=false;else *slot=nullptr;
            }
            if(u.menu){
                const bool detached=!u.menu_attached||(GetMenu(h)==u.menu&&SetMenu(h,nullptr));
                if(!detached||!DestroyMenu(u.menu))ok=false;
                else{u.menu=nullptr;u.menu_attached=false;if(u.popup_attached){u.popup=nullptr;u.popup_attached=false;}}
            }
            if(u.popup&&!u.popup_attached){if(!DestroyMenu(u.popup))ok=false;else u.popup=nullptr;}
        }else ok=false;
        u.success.store(ok,std::memory_order_release);u.completed.store(static_cast<unsigned>(w),std::memory_order_release);return true;
    }
    if(m==WM_NOTIFY&&l){const auto* header=reinterpret_cast<const NMHDR*>(l);
        if(header->hwndFrom==u.tab&&header->idFrom==StandardControlsUi::tab_id&&
           header->code==TCN_SELCHANGE&&u.exact_child(u.tab)){
            u.tab_selection=TabCtrl_GetCurSel(u.tab);++u.tab_changes;
        }
    }
    if(m==WM_INITMENUPOPUP&&reinterpret_cast<HMENU>(w)==u.popup&&GetMenu(h)==u.menu)++u.menu_opened;
    if(m==WM_COMMAND){
        if(LOWORD(w)==StandardControlsUi::button_id&&HIWORD(w)==BN_CLICKED&&
           reinterpret_cast<HWND>(l)==u.button&&u.exact_child(u.button))++u.button_clicks;
        if(LOWORD(w)==StandardControlsUi::menu_id&&HIWORD(w)==0&&l==0&&u.menu_opened.load()&&
           GetMenu(h)==u.menu&&IsMenu(u.popup))++u.menu_commands;
    }
    return false; // Preserve the original Frame/DefWindowProc path for every native notification.
}
bool standard_controls_request(const std::shared_ptr<StandardControlsUi>& u,unsigned operation){
    if(!u->exact_root())return false;u->success=false;u->completed=0;u->operation=operation;
    return PostMessageW(u->root,standard_controls_ui_command,operation,reinterpret_cast<LPARAM>(u.get()))&&
        wait([&]{return u->completed.load(std::memory_order_acquire)==operation;},2000)&&u->success.load(std::memory_order_acquire);
}
bool standard_controls_released(){
    for(const int key:std::array{VK_LBUTTON,VK_RBUTTON,VK_MBUTTON,VK_XBUTTON1,VK_XBUTTON2,VK_CONTROL,VK_SHIFT,VK_MENU,VK_LWIN,VK_RWIN})
        if(GetAsyncKeyState(key)&0x8000)return false;
    return true;
}
bool standard_controls_idle(const StandardControlsUi& u){
    GUITHREADINFO gui{sizeof(gui)};return u.exact_root()&&GetForegroundWindow()==u.root&&
        GetGUIThreadInfo(u.owner,&gui)&&!gui.hwndCapture&&!gui.hwndMoveSize&&!gui.hwndMenuOwner&&
        !(gui.flags&(GUI_INMOVESIZE|GUI_INMENUMODE|GUI_POPUPMENUMODE|GUI_SYSTEMMENUMODE));
}
void standard_controls_click(State& s,const std::shared_ptr<StandardControlsUi>& u,HWND child,std::size_t index,
    POINT from,std::optional<POINT> to={}){
    if(index>=3||!u->exact_child(child)||s.left||s.control||!standard_controls_released()||!standard_controls_idle(*u))
        throw std::runtime_error("standard_controls_exact_released_child_required");
    const auto before_down=u->downs[index].load(),before_up=u->ups[index].load();cursor(s,from);
    if(!u->exact_child(child)||!standard_controls_idle(*u)||WindowFromPoint(from)!=child)
        throw std::runtime_error("standard_controls_fresh_actual_child_hit_required");
    button(s,true);
    if(!wait([&]{return u->downs[index].load()==before_down+1;},1000))
        throw std::runtime_error("standard_controls_actual_child_DOWN_not_observed");
    if(to)cursor(s,*to);
    GUITHREADINFO gui{sizeof(gui)};POINT actual{};
    if(!u->exact_child(child)||GetForegroundWindow()!=u->root||!GetGUIThreadInfo(u->owner,&gui)||
       (gui.hwndCapture&&gui.hwndCapture!=child)||gui.hwndMoveSize||gui.hwndMenuOwner||
       !GetCursorPos(&actual)||WindowFromPoint(actual)!=child)
        throw std::runtime_error("standard_controls_before_unique_UP_context_changed");
    button(s,false);
    s.log.check("standard_controls_real_child_DOWN_UP",wait([&]{return u->ups[index].load()==before_up+1&&
        standard_controls_idle(*u)&&standard_controls_released();},1000),
        "\"child\":"+std::to_string(reinterpret_cast<std::uintptr_t>(child))+",\"owner_TID\":"+std::to_string(u->owner)+
        ",\"actual_DOWN_count\":"+std::to_string(u->downs[index].load())+",\"actual_UP_count\":"+std::to_string(u->ups[index].load())+
        ",\"one_DOWN_one_UP_no_retransmission\":true,\"SendInput_return_alone_used_as_delivery\":false");
}
void owned_standard_controls_case(State& s){
    if(s.suite!="StandardControlsOnly"||!desktop()||!medium(GetCurrentProcess())||s.left||s.control||
       !standard_controls_released()||!chain_settings(s).enabled||standard_controls_ui.load())
        throw std::runtime_error("standard_controls_exact_Medium_suite_required");
    const auto pid=s.product_pid;const auto birth=process_birth(s.process);const auto binary=hash(s.product);const auto options=chain_settings(s);
    LayoutFixtures set{s};set.create();auto u=std::make_shared<StandardControlsUi>();
    bool published=false,cleaned=false;
    try{
        const auto work=chain_work(set.frames[0].hwnd);
        if(work.width()<1100||work.height()<760)throw std::runtime_error("standard_controls_owned_work_area_too_small");
        const Rect initial{work.l+140,work.t+140,work.l+940,work.t+660};set.setup({initial});
        // A same-root real enhanced title Move is mandatory. Mere exclusion or
        // a root which never qualified cannot make the later zero-write test PASS.
        const auto gesture=layout_down(set,0,HTCAPTION,false);
        const auto moved=translated(initial,24,17);
        layout_phase(set,0,gesture,24,17,{moved},"standard_controls_same_root_actual_title_Move");
        layout_up(set,gesture,{moved});
        u->root=set.frames[0].hwnd;u->frame=&set.frames[0];u->owner=set.thread;u->pid=GetCurrentProcessId();u->class_name=s.class_name;
        std::shared_ptr<StandardControlsUi> empty;
        if(!standard_controls_ui.compare_exchange_strong(empty,u))throw std::runtime_error("standard_controls_existing_owner_not_replaced");
        published=true;s.log.check("standard_controls_owner_created_native_controls",standard_controls_request(u,1)&&u->exact_root());
        const auto baseline=geometry_history_status(s);const auto claims=number(baseline,"\"native_callsite_claims\":");
        const auto no_enhancement=[&](std::size_t mark,const char* role){
            const auto status=geometry_history_status(s);bool zero=true;std::istringstream rows(product_log_since(s,mark));std::string row;
            while(std::getline(rows,row))if(row.find("\"type\":\"gesture_start\"")!=row.npos||
                row.find("\"type\":\"native_move_pending\"")!=row.npos||
                (row.find("\"type\":\"placement\"")!=row.npos&&row.find("\"native_attempted\":true")!=row.npos))zero=false;
            s.log.check("standard_controls_native_client_zero_enhancement",zero&&set.owns(0)&&u->exact_root()&&
                visible(u->root)==moved&&standard_controls_idle(*u)&&number(status,"\"native_callsite_claims\":")==claims&&
                status.find("\"gesture_active\":false")!=status.npos&&status.find("\"hook_owner_running\":true")!=status.npos,
                std::string("\"role\":")+quote(role)+",\"same_qualified_root\":true,\"product_enabled\":true,\"native_claims\":"+
                std::to_string(claims)+",\"new_gesture_or_native_placement\":false,\"actual_status\":"+quote(status));
        };
        POINT tab=u->second_tab;if(!ClientToScreen(u->tab,&tab))throw std::runtime_error("standard_controls_tab_point_unknown");
        auto mark=read_product(s).size();standard_controls_click(s,u,u->tab,0,tab);
        s.log.check("standard_controls_actual_TCN_SELCHANGE",wait([&]{return u->tab_changes.load()==1&&u->tab_selection.load()==1;},1000));
        no_enhancement(mark,"native_Tab_second_item");
        RECT r{};if(!GetWindowRect(u->button,&r))throw std::runtime_error("standard_controls_button_rect_unknown");
        mark=read_product(s).size();standard_controls_click(s,u,u->button,1,{(r.left+r.right)/2,(r.top+r.bottom)/2});
        s.log.check("standard_controls_actual_BN_CLICKED",wait([&]{return u->button_clicks.load()==1;},1000));
        no_enhancement(mark,"native_Button");
        DWORD_PTR first{},last{};
        if(!u->exact_child(u->edit)||!SendMessageTimeoutW(u->edit,EM_POSFROMCHAR,0,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&first)||
           !SendMessageTimeoutW(u->edit,EM_POSFROMCHAR,3,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&last)||
           static_cast<DWORD>(first)==MAXDWORD||static_cast<DWORD>(last)==MAXDWORD)throw std::runtime_error("standard_controls_Edit_char_positions_unknown");
        POINT from{static_cast<short>(LOWORD(first))+1,static_cast<short>(HIWORD(first))+3};
        POINT to{static_cast<short>(LOWORD(last))+1,static_cast<short>(HIWORD(last))+3};
        if(!ClientToScreen(u->edit,&from)||!ClientToScreen(u->edit,&to))throw std::runtime_error("standard_controls_Edit_screen_points_unknown");
        mark=read_product(s).size();standard_controls_click(s,u,u->edit,2,from,to);DWORD_PTR selection{};
        const bool selected=u->exact_child(u->edit)&&SendMessageTimeoutW(u->edit,EM_GETSEL,0,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&selection)&&
            LOWORD(selection)==0&&HIWORD(selection)==3;
        s.log.check("standard_controls_actual_Edit_selection",selected,"\"selection_begin\":"+std::to_string(LOWORD(selection))+
            ",\"selection_end\":"+std::to_string(HIWORD(selection))+",\"expected_from_real_drag\":[0,3],\"EM_SETSEL_used\":false");
        no_enhancement(mark,"native_Edit_mouse_selection");
        RECT menu{};if(!GetMenuItemRect(u->root,u->menu,0,&menu))throw std::runtime_error("standard_controls_Menu_bar_rect_unknown");
        mark=read_product(s).size();POINT bar{(menu.left+menu.right)/2,(menu.top+menu.bottom)/2};cursor(s,bar);DWORD_PTR hit{};
        if(!standard_controls_idle(*u)||WindowFromPoint(bar)!=u->root||
           !SendMessageTimeoutW(u->root,WM_NCHITTEST,0,MAKELPARAM(static_cast<short>(bar.x),static_cast<short>(bar.y)),
               SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&hit)||hit!=HTMENU)throw std::runtime_error("standard_controls_fresh_native_menu_hit_unknown");
        button(s,true);button(s,false);
        if(!wait([&]{GUITHREADINFO g{sizeof(g)};return u->menu_opened.load()==1&&GetGUIThreadInfo(u->owner,&g)&&
            g.hwndMenuOwner==u->root&&GetMenuItemRect(nullptr,u->popup,0,&menu);},1000))
            throw std::runtime_error("standard_controls_actual_owned_popup_not_opened");
        POINT item{(menu.left+menu.right)/2,(menu.top+menu.bottom)/2};cursor(s,item);const HWND popup=WindowFromPoint(item);DWORD popup_pid{};
        GUITHREADINFO gui{sizeof(gui)};
        if(!u->exact_root()||GetForegroundWindow()!=u->root||GetWindowThreadProcessId(popup,&popup_pid)!=u->owner||
           popup_pid!=u->pid||gui_class(popup)!=L"#32768"||!GetGUIThreadInfo(u->owner,&gui)||gui.hwndMenuOwner!=u->root||
           !GetMenuItemRect(nullptr,u->popup,0,&r)||!EqualRect(&r,&menu))throw std::runtime_error("standard_controls_fresh_owned_popup_identity_changed");
        button(s,true);button(s,false);
        s.log.check("standard_controls_actual_native_menu_WM_COMMAND",wait([&]{return u->menu_commands.load()==1&&
            standard_controls_idle(*u)&&standard_controls_released();},1000),"\"menu_ID\":21004,\"WM_COMMAND_injected\":false");
        no_enhancement(mark,"native_Menu_command");
        chain_check_product(s,pid,birth);
        cleaned=standard_controls_request(u,2);s.log.check("standard_controls_native_children_and_menu_destroyed",cleaned);
        auto same=u;standard_controls_ui.compare_exchange_strong(same,{});published=false;
        s.log.check("standard_controls_original_configuration_and_product_retained",chain_settings(s)==options&&hash(s.product)==binary);
        s.log.check("standard_controls_owned_root_closed",set.close());
        s.log.emit("standard_controls_component","\"result\":\"PASS\",\"real_Tab_Button_Edit_Menu\":true,"
            "\"same_root_positive_Move_before_zero_write_checks\":true,\"third_party_application_matrix\":\"NOT_RUN\",\"whole_M04_F01_PASS_inferred\":false");
    }catch(...){
        // Only release this TEST's outstanding pair, once, while its exact root
        // still owns the active context. No input is sent to a changed/unknown FG.
        if(s.left&&!s.left_up_attempted){
            GUITHREADINFO gui{sizeof(gui)};POINT point{};const bool exact=u->exact_root();
            const bool gui_known=exact&&GetGUIThreadInfo(u->owner,&gui)!=FALSE;
            const bool cursor_known=GetCursorPos(&point)!=FALSE;const HWND hit=cursor_known?WindowFromPoint(point):nullptr;
            const bool own_capture=gui_known&&gui.hwndCapture&&(gui.hwndCapture==u->root||u->exact_child(gui.hwndCapture));
            const bool own_hit=exact&&hit&&(hit==u->root||u->exact_child(hit));
            const bool allowed=exact&&GetForegroundWindow()==u->root&&gui_known&&
                (own_capture||(!gui.hwndCapture&&cursor_known&&own_hit));
            // Keep evidence IO after this one actual action, not between
            // its fresh destination check and the unique release attempt.
            if(allowed)try{button(s,false);}catch(...){}
            try{s.log.emit("standard_controls_exception_unique_UP_guard","\"actual_cursor_known\":"+std::string(cursor_known?"true":"false")+
                ",\"actual_hit\":"+std::to_string(reinterpret_cast<std::uintptr_t>(hit))+",\"GUI_known\":"+(gui_known?"true":"false")+
                ",\"actual_capture\":"+(gui_known?std::to_string(reinterpret_cast<std::uintptr_t>(gui.hwndCapture)):"null")+
                ",\"unique_cleanup_UP_allowed\":"+(allowed?"true":"false")+",\"pending_on_rejection\":"+(allowed?"false":"true")+
                ",\"unknown_destination_not_used\":true");}catch(...){}
        }
        if(published){try{cleaned=standard_controls_request(u,2);}catch(...){}
            if(cleaned){auto same=u;standard_controls_ui.compare_exchange_strong(same,{});}}
        try{if(!set.closed)set.close();}catch(...){}
        // An unconfirmed owner cleanup leaves the shared state retained, not a
        // dangling WndProc pointer or an invented cleanup PASS.
        throw;
    }
}
