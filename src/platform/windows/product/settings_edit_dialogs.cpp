#include "settings_edit_dialogs.h"

#include <commdlg.h>
#include <array>
#include <string>
#include <vector>

namespace panebind::platform::windows::product {
namespace {
namespace v=core::v1;
namespace ed=v::settings_editor;
constexpr wchar_t editor_class[]=L"PaneBindSettingsMappingEditor";
std::wstring wide(std::string_view s) {
    if(s.empty())return {};
    const int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0);
    if(n<=0)return {};
    std::wstring out(static_cast<std::size_t>(n),0);
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),out.data(),n);return out;
}
std::optional<std::string> utf8(std::wstring_view s) {
    if(s.empty())return std::string{};
    const int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0,nullptr,nullptr);
    if(n<=0)return std::nullopt;
    std::string out(static_cast<std::size_t>(n),0);
    WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),out.data(),n,nullptr,nullptr);return out;
}
std::wstring value(HWND h){std::wstring s(static_cast<std::size_t>(GetWindowTextLengthW(h))+1,0);s.resize(GetWindowTextW(h,s.data(),static_cast<int>(s.size())));return s;}
struct Editor {
    HWND owner{},window{},key{},action{},region{},button{},clicks{},exe{},cls{},mode{};
    HFONT font{}; UINT dpi{96}; int kind{},y{16}; bool accepted{};
    std::array<HWND,4> modifiers{}; std::array<HWND,6> features{};
    v::Settings& draft; std::optional<std::size_t> index; v::Rule original;
    explicit Editor(v::Settings& d):draft(d){}
    int px(int n)const{return MulDiv(n,static_cast<int>(dpi),96);}
    HWND add(const wchar_t* c,const wchar_t* label,DWORD style,int x,int yy,int width,int height,int id=0) {
        const HWND h=CreateWindowExW(std::wstring_view(c)==L"EDIT"?WS_EX_CLIENTEDGE:0,c,label,
            WS_CHILD|WS_VISIBLE|style,px(x),px(yy),px(width),px(height),window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
        SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(font),FALSE);return h;
    }
    HWND combo(const wchar_t* label,int id) {
        add(L"STATIC",label,SS_LEFT,16,y,470,23);
        auto h=add(L"COMBOBOX",L"",WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,16,y+26,470,260,id);
        y+=66;return h;
    }
    static void item(HWND combo,std::wstring_view label,int data) {
        const auto i=SendMessageW(combo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(std::wstring(label).c_str()));
        SendMessageW(combo,CB_SETITEMDATA,static_cast<WPARAM>(i),data);
    }
    static void select(HWND combo,int data) {
        for(LRESULT i=0;i<SendMessageW(combo,CB_GETCOUNT,0,0);++i)
            if(SendMessageW(combo,CB_GETITEMDATA,static_cast<WPARAM>(i),0)==data){SendMessageW(combo,CB_SETCURSEL,static_cast<WPARAM>(i),0);break;}
    }
    static int selected(HWND combo){const auto i=SendMessageW(combo,CB_GETCURSEL,0,0);return i==CB_ERR?-1:static_cast<int>(SendMessageW(combo,CB_GETITEMDATA,static_cast<WPARAM>(i),0));}
    void create() {
        font=CreateFontW(-MulDiv(10,static_cast<int>(dpi),72),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
        if(kind<2){
            unsigned mods{};v::Action chosen=v::Action::Center;
            if(kind==0){
                const auto h=index?draft.hotkeys[*index]:v::Hotkey{3,'J',v::Action::Center};mods=h.modifiers;chosen=h.action;
                key=combo(L"主键",1300);
                // Every schema-legal key remains representable, including
                // uncommon keys imported from an existing configuration.
                for(unsigned k=1;k<=254;++k)item(key,settings_key_label(k),static_cast<int>(k));select(key,static_cast<int>(h.key));
            }else{
                const auto m=index?draft.mouse_bindings[*index]:v::MouseBinding{0,3,1,2,v::Action::Center};mods=m.modifiers;chosen=m.action;
                region=combo(L"触发区域",1310);for(int i=0;i<9;++i)item(region,ed::region_label(i),i);select(region,m.region);
                button=combo(L"按键",1311);for(int i=1;i<=5;++i)item(button,ed::button_label(i),i);select(button,m.button);
                clicks=combo(L"点击方式",1312);item(clicks,L"单击",1);item(clicks,L"双击",2);select(clicks,m.clicks);
            }
            add(L"STATIC",L"修饰键",SS_LEFT,16,y,470,24);y+=28;
            constexpr std::array labels{L"Ctrl",L"Alt",L"Shift",L"Win"};constexpr std::array bits{2U,1U,4U,8U};
            for(std::size_t i=0;i<4;++i){modifiers[i]=add(L"BUTTON",labels[i],WS_TABSTOP|BS_AUTOCHECKBOX,16+static_cast<int>(i)*116,y,108,28,1320+static_cast<int>(i));SendMessageW(modifiers[i],BM_SETCHECK,(mods&bits[i])?BST_CHECKED:BST_UNCHECKED,0);}y+=44;
            action=combo(L"执行动作",1330);for(std::size_t i=1;i<ed::action_labels.size();++i)item(action,ed::action_labels[i],static_cast<int>(i));select(action,static_cast<int>(chosen));
            add(L"STATIC",L"确定仅修改设置草稿。保存并关闭后才应用；取消不改变草稿。",SS_LEFT,16,y,470,44);y+=52;
        }else{
            add(L"STATIC",L"程序名或完整路径",SS_LEFT,16,y,470,24);y+=28;
            exe=add(L"EDIT",wide(original.exe).c_str(),WS_TABSTOP|ES_AUTOHSCROLL,16,y,360,30,1400);
            SendMessageW(exe,EM_SETLIMITTEXT,32767,0);
            add(L"BUTTON",L"选择 .exe…",WS_TABSTOP|BS_PUSHBUTTON,384,y,102,30,1401);y+=42;
            add(L"STATIC",L"窗口类（可选，精确匹配）",SS_LEFT,16,y,470,24);y+=28;
            cls=add(L"EDIT",wide(original.window_class).c_str(),WS_TABSTOP|ES_AUTOHSCROLL,16,y,470,30,1402);y+=44;
            mode=combo(L"规则",1403);item(mode,L"排除此应用",0);item(mode,L"自定义功能",1);select(mode,original.excluded?0:1);
            constexpr std::array labels{L"磁吸",L"联动",L"停靠",L"拉伸",L"窗口命令",L"视觉效果"};
            for(std::size_t i=0;i<6;++i){features[i]=add(L"BUTTON",labels[i],WS_TABSTOP|BS_AUTOCHECKBOX,16+static_cast<int>(i%2)*230,y+static_cast<int>(i/2)*34,220,30,1410+static_cast<int>(i));SendMessageW(features[i],BM_SETCHECK,(original.feature_mask&(1U<<i))?BST_CHECKED:BST_UNCHECKED,0);}y+=112;
            add(L"STATIC",L"各功能仍受全局开关控制；停靠、拉伸等快捷命令还需要“窗口命令”许可。模式切换保留当前勾选，排除模式不清除原功能位。",SS_LEFT,16,y,470,60);y+=68;update_mode();
        }
        add(L"BUTTON",L"确定",WS_TABSTOP|BS_DEFPUSHBUTTON,266,y,104,32,IDOK);
        add(L"BUTTON",L"取消",WS_TABSTOP|BS_PUSHBUTTON,382,y,104,32,IDCANCEL);y+=52;
        RECT r{0,0,px(506),px(y)};AdjustWindowRectEx(&r,WS_CAPTION|WS_SYSMENU,FALSE,WS_EX_DLGMODALFRAME|WS_EX_CONTROLPARENT);
        RECT p{};GetWindowRect(owner,&p);MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(owner,MONITOR_DEFAULTTONEAREST),&monitor);const auto width=r.right-r.left,height=r.bottom-r.top;const auto x=std::clamp<LONG>(p.left+((p.right-p.left)-width)/2,monitor.rcWork.left,std::max(monitor.rcWork.left,monitor.rcWork.right-width));const auto top=std::clamp<LONG>(p.top+((p.bottom-p.top)-height)/2,monitor.rcWork.top,std::max(monitor.rcWork.top,monitor.rcWork.bottom-height));SetWindowPos(window,nullptr,x,top,width,height,SWP_NOZORDER|SWP_NOACTIVATE);
    }
    void update_mode(){const bool custom=selected(mode)==1;for(const auto h:features)EnableWindow(h,custom);}
    void browse(){std::array<wchar_t,32768> path{};OPENFILENAMEW f{sizeof(f)};f.hwndOwner=window;f.lpstrFilter=L"应用程序 (*.exe)\0*.exe\0\0";f.lpstrFile=path.data();f.nMaxFile=static_cast<DWORD>(path.size());f.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;if(GetOpenFileNameW(&f))SetWindowTextW(exe,path.data());}
    void accept(){
        std::string failure;
        if(kind<2){unsigned mods{};constexpr std::array bits{2U,1U,4U,8U};for(std::size_t i=0;i<4;++i)if(SendMessageW(modifiers[i],BM_GETCHECK,0,0)==BST_CHECKED)mods|=bits[i];
            const auto a=static_cast<v::Action>(selected(action));
            if(kind==0)failure=ed::put_hotkey(draft,index,{mods,static_cast<unsigned>(selected(key)),a});
            else failure=ed::put_mouse(draft,index,{selected(region),selected(button),selected(clicks),mods,a});
        }else{
            unsigned known{};for(std::size_t i=0;i<6;++i)if(SendMessageW(features[i],BM_GETCHECK,0,0)==BST_CHECKED)known|=1U<<i;
            auto rule=ed::rule_features(original,selected(mode)==0,known);const auto program=utf8(value(exe)),window_class=utf8(value(cls));if(!program||!window_class){MessageBoxW(window,L"程序或窗口类包含无效的文字编码，草稿未更改。",L"PaneBind",MB_OK|MB_ICONWARNING);return;}rule.exe=*program;rule.window_class=*window_class;failure=ed::put_rule(draft,index,std::move(rule));
        }
        if(!failure.empty()){MessageBoxW(window,wide(failure).c_str(),L"PaneBind：请检查此条目",MB_OK|MB_ICONWARNING);return;}
        accepted=true;DestroyWindow(window);
    }
    static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){
        auto* self=reinterpret_cast<Editor*>(GetWindowLongPtrW(h,GWLP_USERDATA));
        if(m==WM_NCCREATE){self=static_cast<Editor*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);self->window=h;SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
        if(!self)return DefWindowProcW(h,m,w,l);
        if(m==WM_CREATE){self->create();return 0;}
        if(m==WM_COMMAND){if(LOWORD(w)==IDOK){self->accept();return 0;}if(LOWORD(w)==IDCANCEL){DestroyWindow(h);return 0;}if(LOWORD(w)==1401){self->browse();return 0;}if(LOWORD(w)==1403&&HIWORD(w)==CBN_SELCHANGE){self->update_mode();return 0;}}
        if(m==WM_CLOSE){DestroyWindow(h);return 0;}
        if(m==WM_NCDESTROY){self->window=nullptr;SetWindowLongPtrW(h,GWLP_USERDATA,0);}
        return DefWindowProcW(h,m,w,l);
    }
};
}
std::wstring settings_key_label(unsigned key){
    if(key>='A'&&key<='Z')return std::wstring(1,static_cast<wchar_t>(key));
    if(key>='0'&&key<='9')return std::wstring(1,static_cast<wchar_t>(key));
    if(key>=VK_F1&&key<=VK_F24)return L"F"+std::to_wstring(key-VK_F1+1);
    UINT scan=MapVirtualKeyW(key,MAPVK_VK_TO_VSC);
    if(key==VK_LEFT||key==VK_RIGHT||key==VK_UP||key==VK_DOWN||key==VK_HOME||key==VK_END||key==VK_PRIOR||key==VK_NEXT||key==VK_INSERT||key==VK_DELETE||key==VK_DIVIDE||key==VK_NUMLOCK)scan|=0x100;
    std::array<wchar_t,100> label{};if(GetKeyNameTextW(static_cast<LONG>(scan<<16),label.data(),static_cast<int>(label.size()))>0)return label.data();
    return L"扩展键（保留 "+std::to_wstring(key)+L"）";
}
bool edit_settings_mapping(HWND owner,v::Settings& draft,int kind,std::optional<std::size_t> index,std::optional<v::Rule> suggested){
    Editor editor(draft);editor.owner=owner;editor.kind=kind;editor.index=index;editor.dpi=GetDpiForWindow(owner);
    if(kind==2){editor.original=index?draft.rules[*index]:suggested.value_or(v::Rule{"","",true,0xffffffffU});}
    WNDCLASSW wc{};wc.lpfnWndProc=Editor::proc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=editor_class;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1);RegisterClassW(&wc);
    EnableWindow(owner,FALSE);
    const auto h=CreateWindowExW(WS_EX_DLGMODALFRAME|WS_EX_CONTROLPARENT,editor_class,kind==0?L"编辑键盘快捷键":kind==1?L"编辑鼠标操作":L"编辑应用规则",WS_CAPTION|WS_SYSMENU|WS_POPUP,0,0,500,500,owner,nullptr,GetModuleHandleW(nullptr),&editor);
    if(h){ShowWindow(h,SW_SHOW);SetFocus(GetNextDlgTabItem(h,nullptr,FALSE));MSG msg{};while(editor.window&&IsWindow(owner)){const auto n=GetMessageW(&msg,nullptr,0,0);if(n<=0){if(n==0)PostQuitMessage(static_cast<int>(msg.wParam));break;}if(!IsDialogMessageW(h,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}if(editor.window)DestroyWindow(editor.window);}
    if(IsWindow(owner)){EnableWindow(owner,TRUE);SetActiveWindow(owner);}if(editor.font)DeleteObject(editor.font);return editor.accepted;
}
} // namespace panebind::platform::windows::product
