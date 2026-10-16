#include "settings_window.h"

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
constexpr std::array<const wchar_t*,37> action_labels{
    L"不执行",L"最小化",L"最大化",L"恢复",L"居中",L"置后",
    L"向左微移",L"向右微移",L"向上微移",L"向下微移",L"减小宽度",L"增加宽度",L"减小高度",L"增加高度",
    L"移到最近相邻显示器",L"切换置顶",L"降低不透明度",L"提高不透明度",
    L"左半屏",L"右半屏",L"上半屏",L"下半屏",L"左上四分屏",L"右上四分屏",L"左下四分屏",L"右下四分屏",
    L"向左拉伸",L"向右拉伸",L"向上拉伸",L"向下拉伸",L"水平双向拉伸",L"垂直双向拉伸",L"双轴拉伸",
    L"撤销布局",L"暂停／恢复",L"打开设置",L"退出"};
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
std::wstring lines_for(const v::Settings& settings,std::string_view prefix,std::string_view count) {
    const auto all=v::serialize(settings);
    std::string result;
    for (const auto row:v::settings_detail::split(all,'\n')) {
        if (row.starts_with(prefix) || row.starts_with(count)) {result+=row;result+="\r\n";}
    }
    return widen(result);
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
    v::Settings draft;
    std::function<bool(const v::Settings&)> save;
    std::function<void()> diagnostic;
    std::function<std::optional<v::Rule>()> current_application;
    std::array<Page,6> pages;
    std::vector<Field> fields;
    std::array<HWND,8> dock_actions{};
    HWND hotkey_text{},mouse_text{},rules_text{},legend_text{},metadata{},rule_class_filter{};
    std::array<HWND,7> footer{};
    int selected{};
    int px(int value) const {return MulDiv(value,static_cast<int>(dpi),96);}
    bool high_contrast() const {
        HIGHCONTRASTW state{sizeof(state)};
        return SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(state),&state,0)!=FALSE && (state.dwFlags&HCF_HIGHCONTRASTON)!=0;
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
    void create() {
        INITCOMMONCONTROLSEX common{sizeof(common),ICC_TAB_CLASSES}; InitCommonControlsEx(&common);
        dpi=GetDpiForWindow(window); if(!dpi)dpi=96;
        font=CreateFontW(-MulDiv(10,static_cast<int>(dpi),72),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
            DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Microsoft YaHei UI");
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
        note(general,L"设置仅在点击“保存并关闭”且保存成功后生效。取消或关闭窗口不会改变运行配置。\r\n紧急暂停：Ctrl+Shift+F11；系统 Win+方向键保持原有行为。",58);
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
        number(dock,L"预览颜色（十进制，0–16777215）",&v::Settings::preview_color);
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
                const auto index=SendMessageW(dock_actions[i],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(action_labels[a]));
                SendMessageW(dock_actions[i],CB_SETITEMDATA,static_cast<WPARAM>(index),static_cast<LPARAM>(a));
            }
            dock.next_y+=36;
        }
        auto& shortcuts=pages[3];
        number(shortcuts,L"键盘微移／调尺寸步长（DIP，1–1000）",&v::Settings::step_dip);
        note(shortcuts,L"映射编辑使用下方配置片段；删除条目须同时修改 count 并重新连续编号。修饰键为 Alt=1、Ctrl=2、Shift=4、Win=8，相加组合。F12 及 Ctrl+Shift+F11 不可占用。",64);
        hotkey_text=multiline(shortcuts,L"键盘：hotkey.编号=修饰键数值,虚拟键十进制,动作标识\r\n例：hotkey.0=3,67,Center 表示 Ctrl+Alt+C 居中。",146);
        mouse_text=multiline(shortcuts,L"鼠标：mouse.编号=区域,按键,点击次数,修饰键,动作标识\r\n区域：标题0、左1、右2、上3、下4、左上5、右上6、左下7、右下8。",146);
        note(shortcuts,L"鼠标按键：左1、右2、中3、X1=4、X2=5；点击次数1或2。未映射的按钮／客户区默认透传，系统按钮不作为标题栏。",48);
        std::wstring actions=L"动作标识 → 中文行为：\r\n";
        for(std::size_t i=1;i<action_labels.size();++i) actions+=widen(v::action_name(static_cast<v::Action>(i)))+L" → "+action_labels[i]+L"\r\n";
        legend_text=add(shortcuts,L"EDIT",actions.c_str(),WS_TABSTOP|WS_VSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL,12,shortcuts.next_y,0,200,true);
        shortcuts.next_y+=212;
        auto& rules=pages[4];
        note(rules,L"可以从打开设置前的合格前台应用添加排除规则。产品会重新核验其身份；此操作只修改草稿，保存后才应用。",56);
        add(rules,L"BUTTON",L"添加已核验前台应用的排除规则 (&A)",WS_TABSTOP|BS_PUSHBUTTON,12,rules.next_y,0,32,true,id_current_app_rule);
        rules.next_y+=40;
        rule_class_filter=add(rules,L"BUTTON",L"仅限制为该窗口类名（不勾选时排除此程序全部窗口）",WS_TABSTOP|BS_AUTOCHECKBOX|BS_MULTILINE,12,rules.next_y,0,44,true);
        rules.next_y+=52;
        note(rules,L"按实际 exe 名称或完整路径匹配，可选精确窗口类名；不按标题猜应用，不支持通配符。编辑完成后保存才生效。true 表示排除；4294967295 表示允许全部功能位。",70);
        rules_text=multiline(rules,L"rule.编号=程序|可选类名|排除true/false|功能掩码\r\n例：rule_count=1 及 rule.0=notepad.exe||true|4294967295",260);
        note(rules,L"路径中的空格、%、|、=、# 按 UTF-8 字节写成 %HH：空格=%20，百分号=%25。导入／导出使用同一严格格式；重复规则或无效编码会拒绝保存。功能掩码按设置文件说明使用。",76);
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
        SetWindowTextW(hotkey_text,lines_for(draft,"hotkey.","hotkey_count=").c_str());
        SetWindowTextW(mouse_text,lines_for(draft,"mouse.","mouse_count=").c_str());
        SetWindowTextW(rules_text,lines_for(draft,"rule.","rule_count=").c_str());
        const auto version=L"配置版本："+std::to_wstring(draft.schema)+L"；当前代际："+std::to_wstring(draft.generation)+L"\r\n内部版本字段只读；保存后由产品统一推进配置代际。";
        SetWindowTextW(metadata,version.c_str());
    }
    std::optional<v::Settings> gather(bool report=true) {
        auto candidate=draft;
        for(const auto& f:fields) {
            if(f.boolean) candidate.*(f.boolean)=SendMessageW(f.control,BM_GETCHECK,0,0)==BST_CHECKED;
            if(f.number) {
                const auto raw=narrow(text(f.control)); int number{};
                if(!raw || !v::settings_detail::number(*raw,number)) {
                    if(report) {selected=f.page;TabCtrl_SetCurSel(tab,selected);layout();SetFocus(f.control);error(L"数字格式无效，请输入范围内的十进制整数。预览颜色也使用十进制数值。");}
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
        std::string combined;
        const auto serialized=v::serialize(candidate);
        for(const auto row:v::settings_detail::split(serialized,'\n')) {
            if(row.starts_with("hotkey.") || row.starts_with("hotkey_count=") || row.starts_with("mouse.") ||
               row.starts_with("mouse_count=") || row.starts_with("rule.") || row.starts_with("rule_count=")) continue;
            combined+=row;combined+='\n';
        }
        for(const auto edit:{hotkey_text,mouse_text,rules_text}) {
            const auto fragment=narrow(text(edit));
            if(!fragment) {if(report)error(L"映射或规则包含无效 Unicode 字符。");return std::nullopt;}
            combined+=*fragment;combined+='\n';
        }
        auto parsed=v::parse(combined);
        if(!parsed.value) {if(report)error(widen(parsed.error));return std::nullopt;}
        return parsed.value;
    }
    void defaults(bool all) {
        const auto baseline=v::default_settings();
        if(all) {const auto schema=draft.schema;const auto generation=draft.generation;draft=baseline;draft.schema=schema;draft.generation=generation;load();return;}
        // Preserve all other pages, including invalid draft text. A page reset
        // cannot silently discard another page's not-yet-saved edits.
        for(const auto& f:fields) if(f.page==selected) {
            if(f.boolean) SendMessageW(f.control,BM_SETCHECK,baseline.*(f.boolean)?BST_CHECKED:BST_UNCHECKED,0);
            if(f.number) SetWindowTextW(f.control,std::to_wstring(baseline.*(f.number)).c_str());
        }
        if(selected==2) for(std::size_t i=0;i<dock_actions.size();++i) {
            const auto count=SendMessageW(dock_actions[i],CB_GETCOUNT,0,0);
            for(LRESULT n=0;n<count;++n) if(SendMessageW(dock_actions[i],CB_GETITEMDATA,static_cast<WPARAM>(n),0)==static_cast<LRESULT>(baseline.dock_actions[i])) SendMessageW(dock_actions[i],CB_SETCURSEL,static_cast<WPARAM>(n),0);
        }
        if(selected==3) {SetWindowTextW(hotkey_text,lines_for(baseline,"hotkey.","hotkey_count=").c_str());SetWindowTextW(mouse_text,lines_for(baseline,"mouse.","mouse_count=").c_str());}
        if(selected==4) SetWindowTextW(rules_text,lines_for(baseline,"rule.","rule_count=").c_str());
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
        auto candidate=gather();if(!candidate)return;
        observed->excluded=true;
        if(SendMessageW(rule_class_filter,BM_GETCHECK,0,0)!=BST_CHECKED)observed->window_class.clear();
        const auto same=[&](const v::Rule& item){
            return v::settings_detail::ascii_fold(item.exe)==v::settings_detail::ascii_fold(observed->exe)&&
                v::settings_detail::ascii_case_fold(item.window_class)==v::settings_detail::ascii_case_fold(observed->window_class);};
        const auto existing=std::find_if(candidate->rules.begin(),candidate->rules.end(),same);
        if(existing!=candidate->rules.end())existing->excluded=true;
        else candidate->rules.push_back(*observed);
        if(const auto failure=v::validate(*candidate);!failure.empty()){error(widen(failure));return;}
        // Update only the rules editor. Other pages keep their in-progress
        // unsaved text, and the normal save callback remains the sole commit.
        SetWindowTextW(rules_text,lines_for(*candidate,"rule.","rule_count=").c_str());
        SetFocus(rules_text);
        MessageBoxW(window,L"已将重新核验的应用加入排除草稿。点击“保存并关闭”后生效；取消仍会保留原运行配置。",L"PaneBind",MB_OK|MB_ICONINFORMATION);
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
        if(message==WM_GETDLGCODE&&page&&page->owner&&
           (child==page->owner->hotkey_text||child==page->owner->mouse_text||child==page->owner->rules_text||child==page->owner->legend_text)) {
            const auto code=DefSubclassProc(child,message,w,l);
            // Multiline EDIT claims all keys. Its immediate parent is our
            // scrolling page, not a dialog procedure, so its own TAB handling
            // cannot navigate the root and ESC is otherwise retained by EDIT.
            // Delegate only these two actual keys to the root IsDialogMessage
            // path; keep ENTER/newline, selection and editing behavior intact.
            if(w==VK_TAB||w==VK_ESCAPE)
                return code&~static_cast<LRESULT>(DLGC_WANTALLKEYS|DLGC_WANTTAB|DLGC_WANTCHARS);
            return code;
        }
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
        MoveWindow(tab,margin,margin,std::max(px(150),width-margin*2),std::max(px(120),static_cast<int>(r.bottom)-footer_height-margin*2),TRUE);
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
        case WM_NOTIFY:if(reinterpret_cast<NMHDR*>(l)->hwndFrom==self->tab && reinterpret_cast<NMHDR*>(l)->code==TCN_SELCHANGE){self->selected=TabCtrl_GetCurSel(self->tab);self->layout();return 0;}break;
        case WM_COMMAND:
            switch(LOWORD(w)) {
            case IDOK:if(const auto candidate=self->gather()) {
                if(!self->save || !self->save(*candidate)){self->error(L"保存或应用失败，原有效配置应保持不变。请检查快捷键冲突、目录权限或诊断记录；草稿仍保留。");return 0;}
                self->draft=*candidate;self->close();
            }return 0;
            case IDCANCEL:self->close();return 0;
            case id_page_defaults:self->defaults(false);return 0;
            case id_all_defaults:if(MessageBoxW(hwnd,L"将所有设置草稿恢复默认？只有保存成功才会应用。",L"PaneBind",MB_YESNO|MB_ICONQUESTION)==IDYES)self->defaults(true);return 0;
            case id_import:self->import_file();return 0;
            case id_export:self->export_file();return 0;
            case id_diagnostic:if(self->diagnostic)self->diagnostic();else self->error(L"诊断入口尚不可用，未导出文件。");return 0;
            case id_current_app_rule:self->add_current_rule();return 0;
            default:break;
            }break;
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
              std::function<std::optional<v::Rule>()> application) {
        if(window){ShowWindow(window,SW_RESTORE);SetForegroundWindow(window);return;}
        draft=settings;save=std::move(callback);diagnostic=std::move(diagnosis);current_application=std::move(application);selected=0;
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
                          std::function<std::optional<core::v1::Rule>()> application){impl_->show(owner,settings,std::move(save),std::move(diagnostic),std::move(application));}
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
    const auto focus=GetFocus();
    const bool own_multiline=focus&&(focus==impl_->hotkey_text||focus==impl_->mouse_text||focus==impl_->rules_text||focus==impl_->legend_text)&&
        message.hwnd==focus&&IsChild(impl_->window,focus)&&GetForegroundWindow()==impl_->window&&
        GetWindowThreadProcessId(focus,nullptr)==GetCurrentThreadId()&&GetActiveWindow()==impl_->window&&
        IsWindowEnabled(impl_->window)&&IsWindowVisible(focus)&&IsWindowEnabled(focus);
    if(message.message==WM_KEYDOWN&&own_multiline&&!(GetKeyState(VK_CONTROL)&0x8000)&&!system_modifier){
        // Navigate the root's actual tab order, not EDIT's immediate scrolling
        // page. Do not rely solely on EDIT's WM_GETDLGCODE negotiation after
        // multiline input, and never focus another application's window.
        if(message.wParam==VK_TAB){
            const auto next=GetNextDlgTabItem(impl_->window,focus,(GetKeyState(VK_SHIFT)&0x8000)!=0);
            if(next&&next!=focus&&IsChild(impl_->window,next)&&IsWindowVisible(next)&&IsWindowEnabled(next)&&
               GetWindowThreadProcessId(next,nullptr)==GetCurrentThreadId()){
                SetFocus(next);return true;
            }
        }
        if(message.wParam==VK_ESCAPE){impl_->close();return true;}
    }
    return IsDialogMessageW(impl_->window,&message)!=FALSE;
}

} // namespace panebind::platform::windows::product
