#include "settings_window.h"
#include "settings_edit_dialogs.h"
#include "core/v1/settings_editor.h"

#include <commctrl.h>
#include <commdlg.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace panebind::platform::windows::product {
namespace {
namespace v=core::v1;
constexpr wchar_t main_class[]=L"PaneBindV1SettingsWindow";
constexpr wchar_t page_class[]=L"PaneBindV1SettingsPage";
constexpr int id_tab=100,id_page_defaults=110,id_all_defaults=111,id_import=112,id_export=113,id_diagnostic=114;
constexpr int id_current_app_rule=115;
constexpr std::array<const wchar_t*,6> page_names{L"常规",L"磁吸／联动",L"停靠／拉伸",L"快捷键／鼠标",L"应用规则",L"诊断／外观"};
namespace ed=v::settings_editor;
constexpr auto& action_labels=ed::action_labels;
constexpr int id_hotkeys=1200,id_hotkey_add=1201,id_hotkey_edit=1202,id_hotkey_delete=1203;
constexpr int id_mouse=1210,id_mouse_add=1211,id_mouse_edit=1212,id_mouse_delete=1213;
constexpr int id_rules=1220,id_rule_add=1221,id_rule_edit=1222,id_rule_delete=1223;
constexpr int id_color=1230,id_choose_color=1231,id_pause=1240,id_help=1241,id_exit=1242;
std::wstring widen(std::string_view value) {
    if (value.empty()) return {};
    const int length=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),nullptr,0);
    if (length<=0) return L"文字编码无效";
    std::wstring out(static_cast<std::size_t>(length),L'\0');
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),out.data(),length);
    return out;
}
std::optional<std::string> narrow(std::wstring_view value) {
    if (value.empty()) return std::string{};
    const int length=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),nullptr,0,nullptr,nullptr);
    if (length<=0) return std::nullopt;
    std::string out(static_cast<std::size_t>(length),'\0');
    WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),out.data(),length,nullptr,nullptr);
    return out;
}
std::wstring text(HWND window) {
    const int length=GetWindowTextLengthW(window);
    std::wstring result(static_cast<std::size_t>(length)+1,L'\0');
    const int copied=GetWindowTextW(window,result.data(),length+1);
    result.resize(static_cast<std::size_t>(std::max(0,copied)));
    return result;
}
std::optional<std::wstring> select_file(HWND owner,bool save) {
    std::array<wchar_t,32768> path{};
    if (save) wcscpy_s(path.data(),path.size(),L"PaneBind-settings.conf");
    OPENFILENAMEW dialog{sizeof(dialog)};
    dialog.hwndOwner=owner;
    dialog.lpstrFilter=L"PaneBind 配置 (*.conf)\0*.conf\0所有文件\0*.*\0\0";
    dialog.lpstrFile=path.data(); dialog.nMaxFile=static_cast<DWORD>(path.size());
    dialog.lpstrDefExt=L"conf";
    dialog.lpstrTitle=save?L"导出当前设置草稿":L"导入 PaneBind 配置";
    dialog.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|
                 (save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);
    const BOOL accepted=save?GetSaveFileNameW(&dialog):GetOpenFileNameW(&dialog);
    if (!accepted) {
        if (const DWORD error=CommDlgExtendedError()) {
            const auto message=L"文件选择失败，错误码 "+std::to_wstring(error);
            MessageBoxW(owner,message.c_str(),L"PaneBind",MB_OK|MB_ICONERROR);
        }
        return std::nullopt;
    }
    return std::wstring(path.data());
}
}

struct SettingsWindow::Impl {
    struct Field { HWND control{}; int page{}; bool v::Settings::* boolean{}; int v::Settings::* number{}; };
    struct Control { HWND hwnd{}; int x{},y{},width{},height{}; bool right_stretch{},pair_label{},pair_edit{},flow_text{}; };
    struct Page {
        Impl* owner{}; HWND hwnd{}; int index{},next_y{12},scroll{},extent{};
        std::vector<Control> controls;
    };
    HWND window{},tab{};
    HFONT font{};
    UINT dpi{96};
    v::Settings draft,baseline;
    bool runtime_enabled{true},runtime_healthy{true};
    std::function<void()> toggle_pause,help,exit;
    std::function<std::wstring()> save_error;
    std::function<bool(const v::Settings&)> save;
    std::function<void()> diagnostic;
    std::function<std::optional<v::Rule>()> current_application;
    std::array<Page,6> pages;
    std::vector<Field> fields;
    std::array<HWND,8> dock_actions{};
    HWND hotkeys{},mouse{},rules{},hotkey_empty{},mouse_empty{},rules_empty{},metadata{};
    HWND color_text{},color_swatch{},run_status{},pause_button{},help_button{},exit_button{};
    std::array<HWND,6> row_actions{};
    std::array<COLORREF,16> custom_colors{};
    std::array<HWND,7> footer{};
    int selected{};
    int px(int value) const {return MulDiv(value,static_cast<int>(dpi),96);}
    bool high_contrast() const {
        HIGHCONTRASTW state{sizeof(state)};
        const bool queried=SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(state),&state,0)!=FALSE;
        const bool enabled=queried&&(state.dwFlags&HCF_HIGHCONTRASTON)!=0;
        if(queried&&state.lpszDefaultScheme)LocalFree(state.lpszDefaultScheme);
        return enabled;
    }
    bool animate() const {
        BOOL enabled=FALSE;
        return draft.animations&&!high_contrast()&&SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&enabled,0)!=FALSE&&enabled;
    }
    ~Impl() {close(); if(font) DeleteObject(font);}
    void error(std::wstring_view message) const {MessageBoxW(window,std::wstring(message).c_str(),L"PaneBind：设置未更改",MB_OK|MB_ICONWARNING);}
    HWND add(Page& page,const wchar_t* cls,const wchar_t* label,DWORD style,int x,int y,int width,int height,bool stretch_width=false,int id=0) {
        const auto hwnd=CreateWindowExW(cls==std::wstring_view(L"EDIT")?WS_EX_CLIENTEDGE:0,
            cls,label,WS_CHILD|WS_VISIBLE|style,0,0,0,0,page.hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
        SendMessageW(hwnd,WM_SETFONT,reinterpret_cast<WPARAM>(font),FALSE);
        page.controls.push_back({hwnd,x,y,width,height,stretch_width});
        SetWindowSubclass(hwnd,control_proc,1,reinterpret_cast<DWORD_PTR>(&page));
        return hwnd;
    }
    // An integer control ID must never bind silently to the stretch flag.
    HWND add(Page&,const wchar_t*,const wchar_t*,DWORD,int,int,int,int,int)=delete;
    void note(Page& page,const wchar_t* label,int height=52) {
        add(page,L"STATIC",label,SS_LEFT,12,page.next_y,0,height,true);
        page.controls.back().flow_text=true;
        page.next_y+=height+8;
    }
    void checkbox(Page& page,const wchar_t* label,bool v::Settings::* member) {
        const auto hwnd=add(page,L"BUTTON",label,WS_TABSTOP|BS_AUTOCHECKBOX|BS_MULTILINE,12,page.next_y,0,44,true);
        fields.push_back({hwnd,page.index,member,nullptr}); page.next_y+=48;
    }
    void number(Page& page,const wchar_t* label,int v::Settings::* member) {
        add(page,L"STATIC",label,SS_LEFT,12,page.next_y+5,270,22);
        page.controls.back().pair_label=true;
        const auto hwnd=add(page,L"EDIT",L"",WS_TABSTOP|ES_AUTOHSCROLL,290,page.next_y,140,28);
        page.controls.back().pair_edit=true;
        SendMessageW(hwnd,EM_SETLIMITTEXT,20,0);
        fields.push_back({hwnd,page.index,nullptr,member}); page.next_y+=36;
    }
    HWND multiline(Page& page,const wchar_t* label,int height) {
        note(page,label,40);
        const auto hwnd=add(page,L"EDIT",L"",WS_TABSTOP|WS_VSCROLL|WS_HSCROLL|ES_MULTILINE|ES_WANTRETURN|ES_AUTOVSCROLL|ES_AUTOHSCROLL,
            12,page.next_y,0,height,true);
        SendMessageW(hwnd,EM_SETLIMITTEXT,v::settings_detail::max_file_bytes,0);
        page.next_y+=height+12; return hwnd;
    }
    HWND note_control(Page& page,const wchar_t* label,int height) {
        const auto h=add(page,L"STATIC",label,SS_LEFT,12,page.next_y,0,height,true);
        page.controls.back().flow_text=true;page.next_y+=height+6;return h;
    }
    HWND list(Page& page,const wchar_t* label,int id,std::initializer_list<const wchar_t*> columns,int height) {
        note(page,label,26);
        const auto h=add(page,WC_LISTVIEWW,L"",WS_TABSTOP|WS_BORDER|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS,
                         12,page.next_y,0,height,true,id);
        ListView_SetExtendedListViewStyle(h,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_LABELTIP);
        int i{};for(const auto name:columns){LVCOLUMNW c{LVCF_TEXT|LVCF_WIDTH};c.pszText=const_cast<wchar_t*>(name);c.cx=px(columns.size()==2?250:columns.size()==3?(i==0?300:180):135);ListView_InsertColumn(h,i++,&c);}
        page.next_y+=height+8;return h;
    }
    void list_buttons(Page& page,int add_id,int edit_id,int delete_id,std::size_t offset) {
        add(page,L"BUTTON",L"添加",WS_TABSTOP|BS_PUSHBUTTON,12,page.next_y,120,32,false,add_id);
        row_actions[offset]=add(page,L"BUTTON",L"编辑",WS_TABSTOP|BS_PUSHBUTTON,144,page.next_y,120,32,false,edit_id);
        row_actions[offset+1]=add(page,L"BUTTON",L"删除",WS_TABSTOP|BS_PUSHBUTTON,276,page.next_y,120,32,false,delete_id);
        page.next_y+=48;
    }
    static void row(HWND list,int n,std::initializer_list<std::wstring> cells) {
        int column{};for(const auto& cell:cells){
            if(column==0){LVITEMW i{LVIF_TEXT};i.iItem=n;i.pszText=const_cast<wchar_t*>(cell.c_str());ListView_InsertItem(list,&i);}
            else ListView_SetItemText(list,n,column,const_cast<wchar_t*>(cell.c_str()));++column;
        }
    }
    void enable_row_actions() {
        for(std::size_t i=0;i<3;++i){const HWND h=i==0?hotkeys:i==1?mouse:rules;const bool chosen=ListView_GetNextItem(h,-1,LVNI_SELECTED)>=0;EnableWindow(row_actions[i*2],chosen);EnableWindow(row_actions[i*2+1],chosen);}
    }
    void refresh_lists() {
        ListView_DeleteAllItems(hotkeys);ListView_DeleteAllItems(mouse);ListView_DeleteAllItems(rules);
        for(std::size_t i=0;i<draft.hotkeys.size();++i){const auto& h=draft.hotkeys[i];row(hotkeys,static_cast<int>(i),{(h.modifiers?ed::modifier_label(h.modifiers)+L"+":L"")+settings_key_label(h.key),std::wstring(ed::action_label(h.action))});}
        for(std::size_t i=0;i<draft.mouse_bindings.size();++i){const auto& m=draft.mouse_bindings[i];row(mouse,static_cast<int>(i),{std::wstring(ed::region_label(m.region)),std::wstring(ed::button_label(m.button)),m.clicks==1?L"单击":L"双击",ed::modifier_label(m.modifiers),std::wstring(ed::action_label(m.action))});}
        constexpr std::array names{L"磁吸",L"联动",L"停靠",L"拉伸",L"窗口命令",L"视觉效果"};
        for(std::size_t i=0;i<draft.rules.size();++i){const auto& r=draft.rules[i];std::wstring description=r.excluded?L"排除此应用":L"自定义：";if(!r.excluded){for(std::size_t b=0;b<6;++b)if(r.feature_mask&(1U<<b)){if(description.back()!=L'：')description+=L"、";description+=names[b];}if((r.feature_mask&63)==0)description+=L"不允许增强";}row(rules,static_cast<int>(i),{widen(r.exe),r.window_class.empty()?L"全部窗口类":widen(r.window_class),description});}
        ShowWindow(hotkey_empty,draft.hotkeys.empty()?SW_SHOW:SW_HIDE);ShowWindow(mouse_empty,draft.mouse_bindings.empty()?SW_SHOW:SW_HIDE);ShowWindow(rules_empty,draft.rules.empty()?SW_SHOW:SW_HIDE);enable_row_actions();
    }
    void mapping_command(int id) {
        const int kind=id>=id_rule_add?2:id>=id_mouse_add?1:0;
        const int first=kind==0?id_hotkey_add:kind==1?id_mouse_add:id_rule_add;
        const HWND list=kind==0?hotkeys:kind==1?mouse:rules;
        const int operation=id-first;const int n=ListView_GetNextItem(list,-1,LVNI_SELECTED);
        if(operation!=0&&n<0)return;
        if(operation==2){if(kind==0)ed::erase_hotkey(draft,static_cast<std::size_t>(n));else if(kind==1)ed::erase_mouse(draft,static_cast<std::size_t>(n));else ed::erase_rule(draft,static_cast<std::size_t>(n));refresh_lists();return;}
        if(edit_settings_mapping(window,draft,kind,operation==0?std::nullopt:std::optional<std::size_t>(n)))refresh_lists();
    }
    bool list_notification(const NMHDR* header) {
        if(header->hwndFrom!=hotkeys&&header->hwndFrom!=mouse&&header->hwndFrom!=rules)return false;
        if(header->code==LVN_ITEMCHANGED)enable_row_actions();
        if(header->code==NM_DBLCLK){mapping_command(header->hwndFrom==hotkeys?id_hotkey_edit:header->hwndFrom==mouse?id_mouse_edit:id_rule_edit);}
        return true;
    }
    void choose_color() {
        const auto raw=narrow(text(color_text));const auto color=raw?ed::parse_color_hex(*raw):std::nullopt;
        if(!color){error(L"请先输入有效的 #RRGGBB 颜色。");return;}
        CHOOSECOLORW choice{sizeof(choice)};choice.hwndOwner=window;choice.rgbResult=static_cast<COLORREF>(*color);choice.lpCustColors=custom_colors.data();choice.Flags=CC_FULLOPEN|CC_RGBINIT;
        if(ChooseColorW(&choice)){SetWindowTextW(color_text,widen(ed::color_hex(static_cast<int>(choice.rgbResult))).c_str());InvalidateRect(color_swatch,nullptr,TRUE);}
        else if(const auto failure=CommDlgExtendedError())error(L"颜色选择器未完成，错误码 "+std::to_wstring(failure));
    }
    void refresh_runtime() {
        SetWindowTextW(run_status,!runtime_healthy?L"输入故障":runtime_enabled?L"运行中":L"已暂停");
        SetWindowTextW(pause_button,runtime_enabled?L"立即暂停":L"立即恢复");
        EnableWindow(pause_button,static_cast<bool>(toggle_pause));EnableWindow(help_button,static_cast<bool>(help));EnableWindow(exit_button,static_cast<bool>(exit));
    }
    void sync_runtime(bool enabled,bool healthy,bool acknowledge_control=false) {
        if(!acknowledge_control&&runtime_enabled==enabled&&runtime_healthy==healthy)return;
        if(acknowledge_control||runtime_enabled!=enabled){draft.enabled=enabled;baseline.enabled=enabled;for(const auto& f:fields)if(f.boolean==&v::Settings::enabled)SendMessageW(f.control,BM_SETCHECK,enabled?BST_CHECKED:BST_UNCHECKED,0);}
        runtime_enabled=enabled;runtime_healthy=healthy;if(window)refresh_runtime();
    }
    void request_exit() {
        const auto candidate=gather(false);const bool dirty=!candidate||*candidate!=baseline;
        if(dirty){
            constexpr TASKDIALOG_BUTTON buttons[]{{IDYES,L"放弃修改并退出"},{IDCANCEL,L"返回设置"}};
            TASKDIALOGCONFIG dialog{sizeof(dialog)};dialog.hwndParent=window;dialog.dwFlags=TDF_ALLOW_DIALOG_CANCELLATION|TDF_POSITION_RELATIVE_TO_WINDOW;dialog.pszWindowTitle=L"PaneBind";dialog.pszMainInstruction=L"设置还有未保存的修改";dialog.pszContent=L"退出将放弃草稿；已执行的即时暂停／恢复不会被取消。";dialog.cButtons=2;dialog.pButtons=buttons;dialog.nDefaultButton=IDCANCEL;
            int selected_button=IDCANCEL;if(FAILED(TaskDialogIndirect(&dialog,&selected_button,nullptr,nullptr))||selected_button!=IDYES)return;
        }
        if(exit)exit();
    }
    void create() {
        INITCOMMONCONTROLSEX common{sizeof(common),ICC_TAB_CLASSES|ICC_LISTVIEW_CLASSES}; InitCommonControlsEx(&common);
        dpi=GetDpiForWindow(window); if(!dpi)dpi=96;
        font=CreateFontW(-MulDiv(10,static_cast<int>(dpi),72),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
            DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Microsoft YaHei UI");
        run_status=CreateWindowExW(0,L"STATIC",L"",WS_CHILD|WS_VISIBLE|SS_LEFT,0,0,0,0,window,nullptr,GetModuleHandleW(nullptr),nullptr);
        pause_button=CreateWindowExW(0,L"BUTTON",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP,0,0,0,0,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id_pause)),GetModuleHandleW(nullptr),nullptr);
        help_button=CreateWindowExW(0,L"BUTTON",L"帮助",WS_CHILD|WS_VISIBLE|WS_TABSTOP,0,0,0,0,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id_help)),GetModuleHandleW(nullptr),nullptr);
        exit_button=CreateWindowExW(0,L"BUTTON",L"退出程序",WS_CHILD|WS_VISIBLE|WS_TABSTOP,0,0,0,0,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id_exit)),GetModuleHandleW(nullptr),nullptr);
        for(auto h:{run_status,pause_button,help_button,exit_button})SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(font),FALSE);
        tab=CreateWindowExW(0,WC_TABCONTROLW,L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_CLIPSIBLINGS,
            0,0,0,0,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id_tab)),GetModuleHandleW(nullptr),nullptr);
        SendMessageW(tab,WM_SETFONT,reinterpret_cast<WPARAM>(font),FALSE);
        for(std::size_t i=0;i<pages.size();++i) {
            TCITEMW item{TCIF_TEXT}; item.pszText=const_cast<wchar_t*>(page_names[i]);
            SendMessageW(tab,TCM_INSERTITEMW,static_cast<WPARAM>(i),reinterpret_cast<LPARAM>(&item));
            auto& page=pages[i]; page.owner=this; page.index=static_cast<int>(i);
            page.hwnd=CreateWindowExW(WS_EX_CONTROLPARENT,page_class,L"",WS_CHILD|WS_VSCROLL|WS_CLIPCHILDREN,
                0,0,0,0,window,nullptr,GetModuleHandleW(nullptr),&page);
        }
        auto& general=pages[0];
        note(general,L"设置仅在点击“保存并关闭”且保存成功后生效。取消或关闭只放弃草稿，不退出程序；即时暂停／恢复会保留。\r\n紧急暂停：Ctrl+Shift+F11；系统 Win+方向键保持原有行为。",58);
        checkbox(general,L"启用 PaneBind 窗口增强",&v::Settings::enabled);
        checkbox(general,L"用户登录后启动（默认关闭；仅保存明确选择）",&v::Settings::startup);
        checkbox(general,L"设置窗口打开时淡入（100 毫秒；不插值窗口几何）",&v::Settings::animations);
        note(general,L"动画只作用于 PaneBind 设置窗口。关闭、系统减少动画或高对比度模式时直接显示；增强手势不会等待此动画。",50);
        note(general,L"普通产品只操作同用户、同会话、普通权限的合格窗口。被排除应用、高权限窗口和系统桌面不会进入增强手势。",64);
        auto& magnet=pages[1];
        checkbox(magnet,L"移动时磁吸其他窗口与工作区",&v::Settings::magnet_move);
        checkbox(magnet,L"缩放时对活动边／角磁吸",&v::Settings::magnet_resize);
        checkbox(magnet,L"Ctrl 起手联动连通窗口（按下时锁定）",&v::Settings::glue);
        number(magnet,L"吸引距离（DIP，0–100）",&v::Settings::attraction_dip);
        number(magnet,L"释放距离（DIP，1–200）",&v::Settings::release_dip);
        number(magnet,L"快移抑制（DIP/秒，0–20000）",&v::Settings::speed_dip);
        number(magnet,L"邻接容差（DIP，0–20）",&v::Settings::adjacency_dip);
        note(magnet,L"释放距离必须严格大于吸引距离。Shift 起手临时绕过磁吸、停靠与联动。参数按目标 DPI 换算；当前手势固定旧配置，下次起手使用新设置。",64);
        auto& dock=pages[2];
        checkbox(dock,L"启用边角停靠",&v::Settings::docking);
        checkbox(dock,L"停靠时参考已有窗口分割线",&v::Settings::smart_docking);
        checkbox(dock,L"启用边／角双击拉伸",&v::Settings::stretch);
        checkbox(dock,L"智能拉伸：止于最近相交邻窗",&v::Settings::smart_stretch);
        checkbox(dock,L"显示非激活停靠／拉伸预览",&v::Settings::preview);
        number(dock,L"预览不透明度（1–255）",&v::Settings::preview_alpha);
        note(dock,L"预览颜色：使用 #RRGGBB 或系统颜色选择器；保存后用于下一次预览。",32);
        color_swatch=add(dock,L"STATIC",L"",SS_OWNERDRAW,12,dock.next_y,48,30);
        color_text=add(dock,L"EDIT",L"",WS_TABSTOP|ES_AUTOHSCROLL,72,dock.next_y,132,30,false,id_color);
        SendMessageW(color_text,EM_SETLIMITTEXT,7,0);
        add(dock,L"BUTTON",L"选择颜色…",WS_TABSTOP|BS_PUSHBUTTON,218,dock.next_y,150,30,false,id_choose_color);dock.next_y+=44;
        number(dock,L"显示器接缝停留（毫秒，100–3000）",&v::Settings::seam_dwell_ms);
        note(dock,L"按方向设置停靠动作；“最大化”与“上半屏”不同。移出预览区撤销，正常抬键才提交。",44);
        constexpr std::array<const wchar_t*,8> direction{L"左边",L"右边",L"顶部",L"底部",L"左上角",L"右上角",L"左下角",L"右下角"};
        for(std::size_t i=0;i<dock_actions.size();++i) {
            add(dock,L"STATIC",direction[i],SS_LEFT,12,dock.next_y+5,270,22);
            dock.controls.back().pair_label=true;
            dock_actions[i]=add(dock,L"COMBOBOX",L"",WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,290,dock.next_y,240,280,true);
            dock.controls.back().pair_edit=true;
            for (std::size_t a=0;a<action_labels.size();++a) {
                const auto action=static_cast<v::Action>(a);
                if (action!=v::Action::None && action!=v::Action::Maximize &&
                    !(action>=v::Action::DockLeft && action<=v::Action::DockBottomRight)) continue;
                const auto index=SendMessageW(dock_actions[i],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(action_labels[a].data()));
                SendMessageW(dock_actions[i],CB_SETITEMDATA,static_cast<WPARAM>(index),static_cast<LPARAM>(a));
            }
            dock.next_y+=36;
        }
        auto& shortcuts=pages[3];
        number(shortcuts,L"键盘微移／调尺寸步长（DIP，1–1000）",&v::Settings::step_dip);
        note(shortcuts,L"添加或编辑一行即可设置组合。确定只改草稿，保存并关闭后应用。F12 与 Ctrl+Shift+F11 为保留组合。",48);
        hotkeys=list(shortcuts,L"键盘快捷键",id_hotkeys,{L"快捷键",L"执行动作"},180);
        hotkey_empty=note_control(shortcuts,L"尚无键盘快捷键，点击“添加”创建。",26);
        list_buttons(shortcuts,id_hotkey_add,id_hotkey_edit,id_hotkey_delete,0);
        mouse=list(shortcuts,L"鼠标操作",id_mouse,{L"触发区域",L"按键",L"单击／双击",L"修饰键",L"执行动作"},210);
        mouse_empty=note_control(shortcuts,L"尚无鼠标映射；未映射的操作保持原生行为。",26);
        list_buttons(shortcuts,id_mouse_add,id_mouse_edit,id_mouse_delete,2);
        note(shortcuts,L"双击选中行可编辑。未映射按钮与客户区默认透传，系统按钮不作为标题栏。",44);
        auto& rule_page=pages[4];
        note(rule_page,L"按程序名或完整路径匹配，可选精确窗口类名。规则只改草稿，保存后生效；不匹配窗口标题。",52);
        rules=list(rule_page,L"应用规则",id_rules,{L"程序",L"窗口类（可选）",L"规则"},250);
        rules_empty=note_control(rule_page,L"尚无应用规则。可以添加程序，或添加打开设置前已核验的应用。",40);
        add(rule_page,L"BUTTON",L"添加程序…",WS_TABSTOP|BS_PUSHBUTTON,12,rule_page.next_y,138,32,false,id_rule_add);
        add(rule_page,L"BUTTON",L"添加当前应用",WS_TABSTOP|BS_PUSHBUTTON,160,rule_page.next_y,150,32,false,id_current_app_rule);
        row_actions[4]=add(rule_page,L"BUTTON",L"编辑",WS_TABSTOP|BS_PUSHBUTTON,320,rule_page.next_y,90,32,false,id_rule_edit);
        row_actions[5]=add(rule_page,L"BUTTON",L"删除",WS_TABSTOP|BS_PUSHBUTTON,420,rule_page.next_y,90,32,false,id_rule_delete);rule_page.next_y+=46;
        note(rule_page,L"“添加当前应用”仅使用打开设置前已核验的身份。新规则默认排除；自定义功能仍受全局开关及“窗口命令”许可控制。",64);
        auto& visual=pages[5];
        checkbox(visual,L"显示实际吸附／联动关系提示",&v::Settings::feedback);
        checkbox(visual,L"拖动期间临时透明（结束恢复原状态）",&v::Settings::drag_transparency);
        checkbox(visual,L"Move 中摇动切换置顶与透明",&v::Settings::shake);
        number(visual,L"拖动不透明度（16–255）",&v::Settings::drag_alpha);
        number(visual,L"摇动不透明度（16–255）",&v::Settings::shake_alpha);
        number(visual,L"摇动幅度（DIP，4–300）",&v::Settings::shake_amplitude_dip);
        number(visual,L"摇动反向次数（2–20）",&v::Settings::shake_reversals);
        number(visual,L"摇动时间窗口（毫秒，100–5000）",&v::Settings::shake_window_ms);
        metadata=add(visual,L"STATIC",L"",SS_LEFT,12,visual.next_y,0,60,true); visual.next_y+=72;
        note(visual,L"点击底部“诊断”执行产品诊断入口，不保存本页草稿。诊断只记录必要状态；不会收集文档内容或键盘文本。未执行的兼容及真人验收不因打开此页变成通过。",76);
        constexpr std::array<const wchar_t*,7> labels{L"保存并关闭 (&S)",L"取消 (&C)",L"此页默认 (&P)",L"全部默认 (&R)",L"导入… (&I)",L"导出… (&E)",L"诊断 (&D)"};
        constexpr std::array<int,7> ids{IDOK,IDCANCEL,id_page_defaults,id_all_defaults,id_import,id_export,id_diagnostic};
        for(std::size_t i=0;i<footer.size();++i) {
            footer[i]=CreateWindowExW(0,L"BUTTON",labels[i],WS_CHILD|WS_VISIBLE|WS_TABSTOP|
                (i==0?BS_DEFPUSHBUTTON:BS_PUSHBUTTON),0,0,0,0,window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(ids[i])),GetModuleHandleW(nullptr),nullptr);
            SendMessageW(footer[i],WM_SETFONT,reinterpret_cast<WPARAM>(font),FALSE);
        }
        for(auto& page:pages) page.extent=page.next_y+12;
        load(); layout();
    }
    void load() {
        for(const auto& f:fields) {
            if(f.boolean) SendMessageW(f.control,BM_SETCHECK,draft.*(f.boolean)?BST_CHECKED:BST_UNCHECKED,0);
            if(f.number) SetWindowTextW(f.control,std::to_wstring(draft.*(f.number)).c_str());
        }
        for(std::size_t i=0;i<dock_actions.size();++i) {
            const auto count=SendMessageW(dock_actions[i],CB_GETCOUNT,0,0);
            for(LRESULT index=0;index<count;++index) if(SendMessageW(dock_actions[i],CB_GETITEMDATA,static_cast<WPARAM>(index),0)==static_cast<LRESULT>(draft.dock_actions[i])) {
                SendMessageW(dock_actions[i],CB_SETCURSEL,static_cast<WPARAM>(index),0);break;
            }
        }
        SetWindowTextW(color_text,widen(ed::color_hex(draft.preview_color)).c_str());
        refresh_lists();refresh_runtime();
        const std::wstring version=L"设置仅在“保存并关闭”成功后生效。\r\n取消或关闭设置窗口不会更改正在使用的选项。";
        SetWindowTextW(metadata,version.c_str());
    }
    std::optional<v::Settings> gather(bool report=true) {
        auto candidate=draft;
        for(const auto& f:fields) {
            if(f.boolean) candidate.*(f.boolean)=SendMessageW(f.control,BM_GETCHECK,0,0)==BST_CHECKED;
            if(f.number) {
                const auto raw=narrow(text(f.control)); int number{};
                if(!raw || !v::settings_detail::number(*raw,number)) {
                    if(report) {selected=f.page;TabCtrl_SetCurSel(tab,selected);layout();SetFocus(f.control);error(L"数字格式无效，请输入范围内的十进制整数。");}
                    return std::nullopt;
                }
                candidate.*(f.number)=number;
            }
        }
        for(std::size_t i=0;i<dock_actions.size();++i) {
            const auto index=SendMessageW(dock_actions[i],CB_GETCURSEL,0,0);
            if(index==CB_ERR)return std::nullopt;
            candidate.dock_actions[i]=static_cast<v::Action>(SendMessageW(dock_actions[i],CB_GETITEMDATA,static_cast<WPARAM>(index),0));
        }
        if(const auto failure=v::validate(candidate);!failure.empty()) {if(report)error(widen(failure));return std::nullopt;}
        const auto raw_color=narrow(text(color_text));
        const auto parsed_color=raw_color?ed::parse_color_hex(*raw_color):std::nullopt;
        if(!parsed_color){if(report){selected=2;TabCtrl_SetCurSel(tab,selected);layout();SetFocus(color_text);error(L"颜色格式无效，请输入 #RRGGBB，例如 #0078D7。");}return std::nullopt;}
        candidate.preview_color=*parsed_color;
        return candidate;
    }
    void defaults(bool all) {
        const auto defaults=v::default_settings();
        if(all) {const auto schema=draft.schema;const auto generation=draft.generation;draft=defaults;draft.schema=schema;draft.generation=generation;load();return;}
        // Preserve all other pages, including invalid draft text. A page reset
        // cannot silently discard another page's not-yet-saved edits.
        for(const auto& f:fields) if(f.page==selected) {
            if(f.boolean) SendMessageW(f.control,BM_SETCHECK,defaults.*(f.boolean)?BST_CHECKED:BST_UNCHECKED,0);
            if(f.number) SetWindowTextW(f.control,std::to_wstring(defaults.*(f.number)).c_str());
        }
        if(selected==2) for(std::size_t i=0;i<dock_actions.size();++i) {
            const auto count=SendMessageW(dock_actions[i],CB_GETCOUNT,0,0);
            for(LRESULT n=0;n<count;++n) if(SendMessageW(dock_actions[i],CB_GETITEMDATA,static_cast<WPARAM>(n),0)==static_cast<LRESULT>(defaults.dock_actions[i])) SendMessageW(dock_actions[i],CB_SETCURSEL,static_cast<WPARAM>(n),0);
        }
        if(selected==2){draft.preview_color=defaults.preview_color;SetWindowTextW(color_text,widen(ed::color_hex(defaults.preview_color)).c_str());}
        if(selected==3){draft.hotkeys=defaults.hotkeys;draft.mouse_bindings=defaults.mouse_bindings;refresh_lists();}
        if(selected==4){draft.rules=defaults.rules;refresh_lists();}
    }
    void import_file() {
        const auto path=select_file(window,false);if(!path)return;
        const HANDLE file=CreateFileW(path->c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file==INVALID_HANDLE_VALUE){error(L"无法读取选择的配置文件；当前草稿保留。");return;}
        LARGE_INTEGER size{};std::string bytes;DWORD read{};
        bool okay=GetFileSizeEx(file,&size) && size.QuadPart>0 && size.QuadPart<=static_cast<LONGLONG>(v::settings_detail::max_file_bytes);
        if(okay) {bytes.resize(static_cast<std::size_t>(size.QuadPart));okay=ReadFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&read,nullptr) && read==bytes.size();}
        CloseHandle(file);
        if(!okay){error(L"读取失败、文件为空或超过 256 KiB；当前草稿保留。");return;}
        auto parsed=v::parse(bytes);
        if(!parsed.value){error(widen(parsed.error));return;}
        const auto generation=draft.generation;draft=std::move(*parsed.value);draft.generation=generation;load();
        MessageBoxW(window,L"已导入到草稿。请检查各页，再点击“保存并关闭”应用；取消不会更改运行配置。",L"PaneBind",MB_OK|MB_ICONINFORMATION);
    }
    void export_file() {
        const auto candidate=gather();if(!candidate)return;
        const auto path=select_file(window,true);if(!path)return;
        const auto bytes=v::serialize(*candidate);
        if(bytes.empty()){error(L"配置无法序列化，未写出文件。");return;}
        const auto temporary=*path+L".panebind-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64())+L".tmp";
        const HANDLE file=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file==INVALID_HANDLE_VALUE){error(L"无法创建导出临时文件；目标文件未更改。");return;}
        DWORD written{};const bool write_ok=WriteFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr) && written==bytes.size() && FlushFileBuffers(file);
        const bool closed=CloseHandle(file)!=FALSE;
        if(!write_ok || !closed || !MoveFileExW(temporary.c_str(),path->c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
            DeleteFileW(temporary.c_str());error(L"导出写入／替换失败；未报告保存成功。请检查目录权限及磁盘空间。");return;
        }
        MessageBoxW(window,L"已导出经过校验的设置草稿。导出不改变当前运行配置。",L"PaneBind",MB_OK|MB_ICONINFORMATION);
    }
    void add_current_rule() {
        if(!current_application){error(L"当前没有已核验的前台应用来源。请先切换到要排除的普通应用，再重新打开设置。");return;}
        std::optional<v::Rule> observed;
        try{observed=current_application();}catch(...){error(L"无法重新核验前台应用，未添加任何规则。");return;}
        if(!observed||observed->exe.empty()){error(L"打开设置前的应用身份已失效或不符合权限规则，未添加规则。");return;}
        observed->excluded=true;
        if(edit_settings_mapping(window,draft,2,std::nullopt,*observed))refresh_lists();
    }
    void reveal(Page& page,HWND child) {
        if(!child||!page.hwnd||page.index!=selected)return;
        const auto known=std::find_if(page.controls.begin(),page.controls.end(),[&](const Control& c){return c.hwnd==child;});
        if(known==page.controls.end())return;
        RECT bounds{},client{};if(!GetWindowRect(child,&bounds)||!GetClientRect(page.hwnd,&client))return;
        MapWindowPoints(nullptr,page.hwnd,reinterpret_cast<POINT*>(&bounds),2);
        const int bottom=bounds.top+std::min<int>(bounds.bottom-bounds.top,px(48));
        if(bounds.top<0)page.scroll+=bounds.top;
        else if(bottom>client.bottom)page.scroll+=bottom-client.bottom;
        else return;
        arrange_page(page);
    }
    static LRESULT CALLBACK control_proc(HWND child,UINT message,WPARAM w,LPARAM l,UINT_PTR id,DWORD_PTR data) {
        auto* page=reinterpret_cast<Page*>(data);
        if(message==WM_SETFOCUS&&page&&page->owner)page->owner->reveal(*page,child);
        if(message==WM_NCDESTROY)RemoveWindowSubclass(child,control_proc,id);
        return DefSubclassProc(child,message,w,l);
    }
    void arrange_page(Page& page) {
        if(!page.hwnd)return;
        RECT client{};GetClientRect(page.hwnd,&client);
        const bool narrow=client.right<px(550);
        std::vector<RECT> positions;positions.reserve(page.controls.size());
        HDC dc=GetDC(page.hwnd);HGDIOBJ old_font=dc?SelectObject(dc,font):nullptr;
        int extra{};
        for(const auto& c:page.controls) {
            int x=c.x,y=px(c.y)+extra,width=c.width,height=px(c.height);
            bool stretch=c.right_stretch;
            if(narrow&&c.pair_label){y-=px(5);width=0;stretch=true;height=px(36);}
            if(narrow&&c.pair_edit){x=12;y+=px(38);stretch=c.right_stretch;}
            const int actual_width=stretch?std::max(px(80),static_cast<int>(client.right)-px(x)-px(12)):
                std::min(px(width),std::max(px(24),static_cast<int>(client.right)-px(x)-px(12)));
            if(c.flow_text&&dc){RECT needed{0,0,actual_width,0};const auto label=text(c.hwnd);
                DrawTextW(dc,label.c_str(),static_cast<int>(label.size()),&needed,DT_CALCRECT|DT_WORDBREAK|DT_NOPREFIX);
                const int measured=needed.bottom-needed.top+px(4);
                if(measured>height){extra+=measured-height;height=measured;}}
            positions.push_back({px(x),y,px(x)+actual_width,y+height});
            if(narrow&&c.pair_edit)extra+=px(42);
        }
        if(dc){if(old_font)SelectObject(dc,old_font);ReleaseDC(page.hwnd,dc);}
        SCROLLINFO scroll{sizeof(scroll),SIF_RANGE|SIF_PAGE|SIF_POS|SIF_DISABLENOSCROLL};
        scroll.nMin=0;scroll.nMax=px(page.extent)+extra-1;scroll.nPage=static_cast<UINT>(std::max<LONG>(0,client.bottom));
        page.scroll=std::clamp(page.scroll,0,std::max(0,scroll.nMax-static_cast<int>(scroll.nPage)+1));scroll.nPos=page.scroll;
        SetScrollInfo(page.hwnd,SB_VERT,&scroll,TRUE);
        for(std::size_t i=0;i<page.controls.size();++i){const auto& bounds=positions[i];
            MoveWindow(page.controls[i].hwnd,bounds.left,bounds.top-page.scroll,bounds.right-bounds.left,bounds.bottom-bounds.top,TRUE);
        }
    }
    void layout() {
        if(!window || !tab)return;
        RECT r{};GetClientRect(window,&r);
        const int margin=px(8),width=static_cast<int>(r.right);
        const int columns=std::clamp((width-margin)/px(150),1,4);
        const int rows=(static_cast<int>(footer.size())+columns-1)/columns;
        const int footer_height=px(rows*36+12);
        MoveWindow(run_status,margin,px(14),px(150),px(26),TRUE);
        MoveWindow(pause_button,px(170),px(8),px(126),px(34),TRUE);
        MoveWindow(help_button,px(306),px(8),px(90),px(34),TRUE);
        MoveWindow(exit_button,px(406),px(8),px(136),px(34),TRUE);
        MoveWindow(tab,margin,px(52),std::max(px(150),width-margin*2),std::max(px(120),static_cast<int>(r.bottom)-footer_height-px(52)-margin),TRUE);
        RECT inner{};GetClientRect(tab,&inner);TabCtrl_AdjustRect(tab,FALSE,&inner);
        MapWindowPoints(tab,window,reinterpret_cast<POINT*>(&inner),2);
        for(auto& page:pages) {
            MoveWindow(page.hwnd,inner.left,inner.top,std::max<LONG>(1,inner.right-inner.left),std::max<LONG>(1,inner.bottom-inner.top),TRUE);
            ShowWindow(page.hwnd,page.index==selected?SW_SHOW:SW_HIDE);arrange_page(page);
            // Tab and page are siblings. The tab spans the display area as
            // well as its header; keep the selected page ABOVE it for actual
            // mouse hit testing, without activating either window.
            if(page.index==selected)SetWindowPos(page.hwnd,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
        }
        for(std::size_t i=0;i<footer.size();++i) {
            const int row=static_cast<int>(i)/columns,column=static_cast<int>(i)%columns;
            const int button_width=std::max(px(36),(width-margin*(columns+1))/columns);
            MoveWindow(footer[i],margin+column*(button_width+margin),static_cast<int>(r.bottom)-footer_height+row*px(36),button_width,px(30),TRUE);
        }
    }
    void change_dpi(UINT value) {
        dpi=value?value:96;
        if(font)DeleteObject(font);
        font=CreateFontW(-MulDiv(10,static_cast<int>(dpi),72),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Microsoft YaHei UI");
        SendMessageW(tab,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
        for(auto& page:pages) for(const auto& c:page.controls) SendMessageW(c.hwnd,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
        for(const auto button:footer)SendMessageW(button,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
        for(auto h:{run_status,pause_button,help_button,exit_button})SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
        layout();
    }
    void close(){if(window)DestroyWindow(window);}
    static LRESULT CALLBACK page_proc(HWND hwnd,UINT message,WPARAM w,LPARAM l) {
        auto* page=reinterpret_cast<Page*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
        if(message==WM_NCCREATE){page=static_cast<Page*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(page));page->hwnd=hwnd;}
        if(!page)return DefWindowProcW(hwnd,message,w,l);
        if(message==WM_CTLCOLORSTATIC){SetTextColor(reinterpret_cast<HDC>(w),GetSysColor(COLOR_WINDOWTEXT));SetBkColor(reinterpret_cast<HDC>(w),GetSysColor(COLOR_WINDOW));return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));}
        if(message==WM_CTLCOLORBTN)return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
        if(message==WM_SIZE){page->owner->arrange_page(*page);return 0;}
        if(message==WM_VSCROLL || message==WM_MOUSEWHEEL) {
            RECT r{};GetClientRect(hwnd,&r);int next=page->scroll;
            if(message==WM_MOUSEWHEEL)next-=GET_WHEEL_DELTA_WPARAM(w)/WHEEL_DELTA*page->owner->px(72);
            else switch(LOWORD(w)) {
                case SB_LINEUP:next-=page->owner->px(32);break;case SB_LINEDOWN:next+=page->owner->px(32);break;
                case SB_PAGEUP:next-=r.bottom;break;case SB_PAGEDOWN:next+=r.bottom;break;
                case SB_TOP:next=0;break;
                case SB_BOTTOM:{SCROLLINFO s{sizeof(s),SIF_RANGE|SIF_PAGE};GetScrollInfo(hwnd,SB_VERT,&s);next=s.nMax-static_cast<int>(s.nPage)+1;break;}
                case SB_THUMBTRACK:case SB_THUMBPOSITION:{SCROLLINFO s{sizeof(s),SIF_TRACKPOS};GetScrollInfo(hwnd,SB_VERT,&s);next=s.nTrackPos;break;}
                default:break;
            }
            page->scroll=next;page->owner->arrange_page(*page);return 0;
        }
        if(message==WM_NOTIFY||message==WM_DRAWITEM)return SendMessageW(page->owner->window,message,w,l);
        if(message==WM_COMMAND) {
            // Keyboard navigation must make the newly focused control visible
            // even on a small/high-DPI work area. Text areas keep their own
            // internal scrolling; the page only exposes the focused row.
            const auto child=reinterpret_cast<HWND>(l);
            if(child && GetFocus()==child)page->owner->reveal(*page,child);
            return SendMessageW(page->owner->window,message,w,l);
        }
        return DefWindowProcW(hwnd,message,w,l);
    }
    static LRESULT CALLBACK proc(HWND hwnd,UINT message,WPARAM w,LPARAM l) {
        auto* self=reinterpret_cast<Impl*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
        if(message==WM_NCCREATE){self=static_cast<Impl*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));self->window=hwnd;}
        if(!self)return DefWindowProcW(hwnd,message,w,l);
        switch(message) {
        case WM_CREATE:self->create();return 0;
        case WM_SIZE:self->layout();return 0;
        case WM_DPICHANGED:{self->change_dpi(HIWORD(w));const auto r=reinterpret_cast<RECT*>(l);SetWindowPos(hwnd,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);return 0;}
        case WM_GETMINMAXINFO:{auto* limits=reinterpret_cast<MINMAXINFO*>(l);MONITORINFO monitor{sizeof(monitor)};
            if(GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&monitor))
                limits->ptMinTrackSize={std::min<LONG>(self->px(570),monitor.rcWork.right-monitor.rcWork.left),
                    std::min<LONG>(self->px(360),monitor.rcWork.bottom-monitor.rcWork.top)};
            else limits->ptMinTrackSize={self->px(400),self->px(300)};
            return 0;}
        case WM_NOTIFY:if(self->list_notification(reinterpret_cast<NMHDR*>(l)))return 0;if(reinterpret_cast<NMHDR*>(l)->hwndFrom==self->tab && reinterpret_cast<NMHDR*>(l)->code==TCN_SELCHANGE){self->selected=TabCtrl_GetCurSel(self->tab);self->layout();return 0;}break;
        case WM_COMMAND:
            switch(LOWORD(w)) {
            case IDOK:if(const auto candidate=self->gather()) {
                if(!self->save || !self->save(*candidate)){const auto why=self->save_error?self->save_error():std::wstring{};self->error(why.empty()?L"保存或应用失败，原有效配置应保持不变。请检查快捷键冲突、目录权限或诊断记录；草稿仍保留。":why);return 0;}
                self->draft=*candidate;self->close();
            }return 0;
            case IDCANCEL:self->close();return 0;
            case id_page_defaults:self->defaults(false);return 0;
            case id_all_defaults:if(MessageBoxW(hwnd,L"将所有设置草稿恢复默认？只有保存成功才会应用。",L"PaneBind",MB_YESNO|MB_ICONQUESTION)==IDYES)self->defaults(true);return 0;
            case id_import:self->import_file();return 0;
            case id_export:self->export_file();return 0;
            case id_diagnostic:if(self->diagnostic)self->diagnostic();else self->error(L"诊断入口尚不可用，未导出文件。");return 0;
            case id_current_app_rule:self->add_current_rule();return 0;
            case id_hotkey_add:case id_hotkey_edit:case id_hotkey_delete:
            case id_mouse_add:case id_mouse_edit:case id_mouse_delete:
            case id_rule_add:case id_rule_edit:case id_rule_delete:self->mapping_command(LOWORD(w));return 0;
            case id_choose_color:self->choose_color();return 0;
            case id_color:if(HIWORD(w)==EN_CHANGE)InvalidateRect(self->color_swatch,nullptr,TRUE);return 0;
            case id_pause:if(self->toggle_pause){
                self->toggle_pause();
                // The immediate command may be rejected (e.g. input restart
                // or persistence failure). Acknowledge the actual state even
                // when it stayed unchanged; do not retain a contrary enabled
                // checkbox, and do not reload any other draft field.
                self->sync_runtime(self->runtime_enabled,self->runtime_healthy,true);
            }return 0;
            case id_help:if(self->help)self->help();return 0;
            case id_exit:self->request_exit();return 0;
            default:break;
            }break;
        case WM_DRAWITEM:{const auto* item=reinterpret_cast<DRAWITEMSTRUCT*>(l);if(item->hwndItem==self->color_swatch){const auto raw=narrow(text(self->color_text));const auto color=raw?ed::parse_color_hex(*raw):std::nullopt;const auto brush=CreateSolidBrush(color?static_cast<COLORREF>(*color):GetSysColor(COLOR_BTNFACE));FillRect(item->hDC,&item->rcItem,brush);DeleteObject(brush);FrameRect(item->hDC,&item->rcItem,GetSysColorBrush(COLOR_WINDOWTEXT));return TRUE;}break;}
        case WM_MOUSEWHEEL:return SendMessageW(self->pages[static_cast<std::size_t>(self->selected)].hwnd,message,w,l);
        case WM_SETTINGCHANGE:case WM_SYSCOLORCHANGE:case WM_THEMECHANGED:
            InvalidateRect(hwnd,nullptr,TRUE);for(auto& page:self->pages)InvalidateRect(page.hwnd,nullptr,TRUE);return 0;
        case WM_CLOSE:self->close();return 0;
        case WM_NCDESTROY:self->window=nullptr;self->tab=nullptr;self->fields.clear();for(auto& page:self->pages){page.hwnd=nullptr;page.controls.clear();page.next_y=12;page.scroll=0;}if(self->font){DeleteObject(self->font);self->font=nullptr;}SetWindowLongPtrW(hwnd,GWLP_USERDATA,0);break;
        default:break;
        }
        return DefWindowProcW(hwnd,message,w,l);
    }
    void show(HWND owner,const v::Settings& settings,std::function<bool(const v::Settings&)> callback,std::function<void()> diagnosis,
              std::function<std::optional<v::Rule>()> application,std::function<void()> pause_callback,std::function<void()> help_callback,std::function<void()> exit_callback,bool healthy,std::function<std::wstring()> failure_callback) {
        if(window){ShowWindow(window,SW_RESTORE);SetForegroundWindow(window);return;}
        save_error=std::move(failure_callback);draft=settings;baseline=settings;runtime_enabled=settings.enabled;runtime_healthy=healthy;toggle_pause=std::move(pause_callback);help=std::move(help_callback);exit=std::move(exit_callback);save=std::move(callback);diagnostic=std::move(diagnosis);current_application=std::move(application);selected=0;
        WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=proc;wc.lpszClassName=main_class;wc.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1);RegisterClassW(&wc);
        wc.lpfnWndProc=page_proc;wc.lpszClassName=page_class;wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);RegisterClassW(&wc);
        const auto monitor=MonitorFromWindow(owner,MONITOR_DEFAULTTONEAREST);MONITORINFO info{sizeof(info)};GetMonitorInfoW(monitor,&info);
        dpi=owner?GetDpiForWindow(owner):GetDpiForSystem();if(!dpi)dpi=96;
        const int width=std::min(px(790),static_cast<int>(info.rcWork.right-info.rcWork.left));
        const int height=std::min(px(720),static_cast<int>(info.rcWork.bottom-info.rcWork.top));
        const auto created=CreateWindowExW(WS_EX_CONTROLPARENT,main_class,L"PaneBind 设置",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,
            info.rcWork.left+(info.rcWork.right-info.rcWork.left-width)/2,info.rcWork.top+(info.rcWork.bottom-info.rcWork.top-height)/2,
            width,height,owner,nullptr,GetModuleHandleW(nullptr),this);
        if(!created){MessageBoxW(owner,L"无法创建设置窗口，请查看系统资源和诊断记录。",L"PaneBind",MB_OK|MB_ICONERROR);return;}
        if(!animate()||!AnimateWindow(created,100,AW_BLEND|AW_ACTIVATE))ShowWindow(created,SW_SHOWNORMAL);
        // This is an explicit user request to open our settings, never a
        // gesture/foreign-window recovery. A second instance may transfer its
        // foreground permission to this process; Windows still decides.
        UpdateWindow(created);SetForegroundWindow(created);SetFocus(tab);
    }
};

SettingsWindow::SettingsWindow():impl_(std::make_unique<Impl>()){}
SettingsWindow::~SettingsWindow()=default;
void SettingsWindow::show(HWND owner,const core::v1::Settings& settings,std::function<bool(const core::v1::Settings&)> save,std::function<void()> diagnostic,
                          std::function<std::optional<core::v1::Rule>()> application,std::function<void()> pause,std::function<void()> help,std::function<void()> exit,bool healthy,std::function<std::wstring()> failure){impl_->show(owner,settings,std::move(save),std::move(diagnostic),std::move(application),std::move(pause),std::move(help),std::move(exit),healthy,std::move(failure));}
void SettingsWindow::sync_runtime(bool enabled,bool healthy){impl_->sync_runtime(enabled,healthy);}
void SettingsWindow::close(){impl_->close();}
HWND SettingsWindow::handle()const{return impl_->window;}
bool SettingsWindow::translate(MSG& message){
    if(!impl_->window)return false;
    const bool system_modifier=(GetKeyState(VK_MENU)&0x8000)||(GetKeyState(VK_LWIN)&0x8000)||(GetKeyState(VK_RWIN)&0x8000);
    if(message.message==WM_KEYDOWN&&message.wParam==VK_TAB&&(GetKeyState(VK_CONTROL)&0x8000)&&!system_modifier&&
       (message.hwnd==impl_->window||IsChild(impl_->window,message.hwnd))){
        const int direction=(GetKeyState(VK_SHIFT)&0x8000)?-1:1;
        impl_->selected=(impl_->selected+static_cast<int>(impl_->pages.size())+direction)%static_cast<int>(impl_->pages.size());
        TabCtrl_SetCurSel(impl_->tab,impl_->selected);impl_->layout();SetFocus(impl_->tab);return true;
    }
    return IsDialogMessageW(impl_->window,&message)!=FALSE;
}

} // namespace panebind::platform::windows::product
