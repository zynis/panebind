#include "platform/windows/console/sta_console_line_reader.h"
#include <objbase.h>
#include <deque>
#include <iostream>
#include <stdexcept>

namespace c=panebind::platform::windows::console_input;
namespace {
int failures{};
void check(bool condition,const char* name){if(!condition){++failures;std::cerr<<"FAIL "<<name<<'\n';}}
INPUT_RECORD key(wchar_t character,WORD vk=0,WORD repeat=1,bool down=true) {
    INPUT_RECORD r{};r.EventType=KEY_EVENT;r.Event.KeyEvent.bKeyDown=down;
    r.Event.KeyEvent.uChar.UnicodeChar=character;r.Event.KeyEvent.wVirtualKeyCode=vk;
    r.Event.KeyEvent.wRepeatCount=repeat;return r;
}
struct Input final {
    HANDLE ready=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    HWND window{};
    std::deque<INPUT_RECORD> records;
    unsigned messages{},reads{},echoes{};
    bool message_after_partial{},empty_once{},fail_read{},fail_echo{},done{};
    std::wstring rendered;
    ~Input(){if(ready)CloseHandle(ready);}
    void push(INPUT_RECORD r){records.push_back(r);check(SetEvent(ready)!=FALSE,"signal fake input");}
    c::detail::InputEndpoint endpoint(){return {ready,this,&read,&echo};}
    static c::detail::RecordRead read(void* context,INPUT_RECORD& record)noexcept {
        auto& s=*static_cast<Input*>(context);++s.reads;
        if(s.fail_read){SetLastError(ERROR_READ_FAULT);return c::detail::RecordRead::Failed;}
        if(s.empty_once){s.empty_once=false;ResetEvent(s.ready);return c::detail::RecordRead::Empty;}
        if(s.records.empty()){ResetEvent(s.ready);return c::detail::RecordRead::Empty;}
        record=s.records.front();s.records.pop_front();
        if(s.records.empty())ResetEvent(s.ready);
        if(s.message_after_partial){s.message_after_partial=false;PostMessageW(s.window,WM_APP+17,1,0);}
        return c::detail::RecordRead::Record;
    }
    static bool echo(void* context,std::wstring_view,std::wstring_view after,bool complete)noexcept {
        auto& s=*static_cast<Input*>(context);++s.echoes;s.rendered=after;s.done=complete;
        return !s.fail_echo;
    }
};
LRESULT CALLBACK window_proc(HWND window,UINT message,WPARAM wp,LPARAM lp) {
    if(message==WM_NCCREATE){const auto* cs=reinterpret_cast<CREATESTRUCTW*>(lp);SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(cs->lpCreateParams));}
    auto* input=reinterpret_cast<Input*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(message==WM_APP+17 && input){
        ++input->messages;
        if(wp>1)PostMessageW(window,WM_APP+17,wp-1,0);
        else input->push(key(L'\r',VK_RETURN));
        return 0;
    }
    return DefWindowProcW(window,message,wp,lp);
}
struct Window final {
    HWND value{};
    explicit Window(Input& input){
        value=CreateWindowExW(0,L"PaneBind.STA.Test.ForeignMessageWindow",L"owned hidden pump probe",
            0,0,0,0,0,HWND_MESSAGE,nullptr,GetModuleHandleW(nullptr),&input);
        input.window=value;check(value!=nullptr,"create owned hidden STA window");
    }
    ~Window(){if(value)DestroyWindow(value);}
};
struct ModeProbe final {
    HANDLE input{};DWORD during{};bool observed{},input_written{},escape{};
};
LRESULT CALLBACK mode_window_proc(HWND window,UINT message,WPARAM wp,LPARAM lp) {
    if(message==WM_NCCREATE){const auto* cs=reinterpret_cast<CREATESTRUCTW*>(lp);SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(cs->lpCreateParams));}
    auto* probe=reinterpret_cast<ModeProbe*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(message==WM_APP+19 && probe) {
        probe->observed=GetConsoleMode(probe->input,&probe->during)!=FALSE;
        const std::array input{key(L'Y'),key(L'\b',VK_BACK),key(L'Q'),key(L'\r',VK_RETURN)};
        DWORD written{};
        if(probe->escape) {
            const auto esc=key(0x1B,VK_ESCAPE);
            probe->input_written=WriteConsoleInputW(probe->input,&esc,1,&written)&&written==1;
        } else probe->input_written=WriteConsoleInputW(probe->input,input.data(),static_cast<DWORD>(input.size()),&written)&&written==input.size();
        return 0;
    }
    return DefWindowProcW(window,message,wp,lp);
}
int private_console_probe() {
    // Automated fixture only: never attach to the user's console, never open
    // Explorer/clipboard, and never change the harness's process group.
    DWORD clients[4]{};
    if(GetConsoleProcessList(clients,4)!=1 || clients[0]!=GetCurrentProcessId())return 20;
    if(FAILED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)))return 21;
    const auto input=GetStdHandle(STD_INPUT_HANDLE),output=GetStdHandle(STD_OUTPUT_HANDLE);
    WNDCLASSW cls{};cls.lpfnWndProc=&mode_window_proc;cls.hInstance=GetModuleHandleW(nullptr);
    cls.lpszClassName=L"PaneBind.STA.PrivateConsoleModeProbe";
    if(!RegisterClassW(&cls))return 22;
    ModeProbe probe{input};
    const auto window=CreateWindowExW(0,cls.lpszClassName,L"owned mode probe",0,0,0,0,0,
        HWND_MESSAGE,nullptr,cls.hInstance,&probe);
    if(!window)return 23;
    const std::array<DWORD,3> modes{
        ENABLE_EXTENDED_FLAGS|ENABLE_LINE_INPUT|ENABLE_ECHO_INPUT|ENABLE_QUICK_EDIT_MODE|ENABLE_PROCESSED_INPUT|ENABLE_VIRTUAL_TERMINAL_INPUT,
        ENABLE_EXTENDED_FLAGS|ENABLE_LINE_INPUT|ENABLE_ECHO_INPUT|ENABLE_PROCESSED_INPUT,
        ENABLE_EXTENDED_FLAGS|ENABLE_VIRTUAL_TERMINAL_INPUT};
    int failure{};
    for(std::size_t i=0;i<modes.size()&&!failure;++i) {
        // This SET is test-fixture preparation in a verified PRIVATE console,
        // before invoking the production reader, not runtime mode mutation.
        if(!SetConsoleMode(input,modes[i])){failure=30+static_cast<int>(i);break;}
        DWORD before{};
        if(!GetConsoleMode(input,&before)||before!=modes[i]){failure=40+static_cast<int>(i);break;}
        probe.observed=false;probe.input_written=false;probe.escape=false;
        if(!PostMessageW(window,WM_APP+19,0,0)){failure=50;break;}
        c::StaConsoleLineReader reader{input,output};
        const auto result=reader.read();
        DWORD after{};
        if(!GetConsoleMode(input,&after))failure=61;
        else if(!probe.observed)failure=62;
        else if(!probe.input_written)failure=63;
        else if(probe.during!=before)failure=64;
        else if(after!=before)failure=65;
        else if(result.input_mode_before!=before)failure=66;
        else if(result.input_mode_after!=before)failure=67;
        else if(!result.modes_observed)failure=68;
        else if(result.mode_changed)failure=69;
        else if(result.status!=c::LineStatus::Complete)failure=100+static_cast<int>(result.status);
        else if(result.line!=L"Q")failure=71;
        else if(result.message_dispatch_count==0)failure=70;
        if(!failure) {
            probe.escape=true;probe.observed=false;probe.input_written=false;
            if(!PostMessageW(window,WM_APP+19,0,0)){failure=72;break;}
            const auto cancelled=reader.read();
            if(cancelled.status!=c::LineStatus::Aborted||cancelled.line||!probe.observed||
               !probe.input_written||probe.during!=before||!cancelled.modes_observed||
               cancelled.input_mode_before!=before||cancelled.input_mode_after!=before||cancelled.mode_changed)
                failure=73;
        }
    }
    DestroyWindow(window);UnregisterClassW(cls.lpszClassName,cls.hInstance);CoUninitialize();
    return failure;
}
bool run_private_console_probe() {
    std::wstring executable(32768,L'\0');
    const auto count=GetModuleFileNameW(nullptr,executable.data(),static_cast<DWORD>(executable.size()));
    if(!count||count>=executable.size())return false;
    executable.resize(count);
    auto command=L"\""+executable+L"\" --private-console-mode-probe";
    STARTUPINFOW startup{};startup.cb=sizeof(startup);
    startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;
    PROCESS_INFORMATION process{};
    if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NEW_CONSOLE,
        nullptr,nullptr,&startup,&process))return false;
    CloseHandle(process.hThread);
    // Parent has not initialized COM or created windows yet. A finite wait is
    // safe here; the CHILD's owner STA uses the actual message-pumping reader.
    const auto wait=WaitForSingleObject(process.hProcess,10000);
    DWORD code=99;
    if(wait==WAIT_OBJECT_0)GetExitCodeProcess(process.hProcess,&code);
    else {
        // Only this newly created, synthetic test child can be terminated.
        // It cannot contain user work or any Explorer/capability object.
        TerminateProcess(process.hProcess,99);WaitForSingleObject(process.hProcess,1000);
    }
    CloseHandle(process.hProcess);
    std::cout<<"private console modes A/B/C probe exit="<<code<<'\n';
    return code==0;
}
}
int wmain(int argc,wchar_t** argv){
    if(argc==2&&std::wstring_view(argv[1])==L"--private-console-mode-probe")return private_console_probe();
    check(run_private_console_probe(),"native modes before/during/after preserved in private console");
    const auto com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    check(SUCCEEDED(com),"probe owner STA initialized");
    WNDCLASSW cls{};cls.lpfnWndProc=&window_proc;cls.hInstance=GetModuleHandleW(nullptr);
    cls.lpszClassName=L"PaneBind.STA.Test.ForeignMessageWindow";
    check(RegisterClassW(&cls)!=0,"register owned message class");
    check(c::detail::nowait_api_available(),"documented Kernel32 NOWAIT export available");
    for(WPARAM count:{1U,3U}){ // A/B: real MSG wait + real owned-window dispatch.
        Input input;Window window(input);
        check(PostMessageW(window.value,WM_APP+17,count,0)!=FALSE,"post before completion");
        auto result=c::detail::read_with_sta_pump(input.endpoint(),2000);
        check(result.status==c::LineStatus::Complete&&result.line==L"","line returned after messages");
        check(input.messages==count&&result.message_dispatch_count>=count&&input.done,"all messages dispatched before input returned");
        check(result.pump_count>0&&result.console_input_event_count==1,"bounded aggregate counters");
    }
    { // Partial input cannot turn into a cooked blocking read.
        Input input;Window window(input);input.message_after_partial=true;input.push(key(L'Y'));
        auto result=c::detail::read_with_sta_pump(input.endpoint(),2000);
        check(result.line==L"Y"&&input.messages==1&&result.wait_count>=2,"message pumped between character and later Enter");
    }
    { // C: console input wins, but the STA still gets a dispatch turn.
        Input input;input.push(key(L'\r',VK_RETURN));
        auto result=c::detail::read_with_sta_pump(input.endpoint(),2000);
        check(result.status==c::LineStatus::Complete&&result.pump_count>=1,"immediate Enter without hang");
    }
    { // D: preserve WM_QUIT for the host; don't consume queued consent.
        Input input;input.push(key(L'Y'));PostQuitMessage(73);
        auto result=c::detail::read_with_sta_pump(input.endpoint(),2000);
        check(result.status==c::LineStatus::Quit&&!result.line&&input.reads==0,"quit abort before reading input");
        MSG msg{};check(PeekMessageW(&msg,nullptr,WM_QUIT,WM_QUIT,PM_REMOVE)&&msg.wParam==73,"quit propagated");
    }
    for(auto abort:std::array{key(0,VK_ESCAPE),key(0x1B)}) {
        Input input;input.push(abort);auto result=c::detail::read_with_sta_pump(input.endpoint(),2000);
        check(result.status==c::LineStatus::Aborted&&!result.line,"Escape clean abort");
    }
    {
        Input input;input.push(key(L'Y'));input.push(key(L'\x03'));
        auto copy=key(L'C',L'C');copy.Event.KeyEvent.dwControlKeyState=LEFT_CTRL_PRESSED;
        input.push(copy);input.push(key(L'\r',VK_RETURN));
        const auto result=c::detail::read_with_sta_pump(input.endpoint(),2000);
        check(result.line==L"Y"&&input.rendered==L"Y"&&input.echoes==2,"copy-related Ctrl+C neither aborts nor enters/echoes command");
    }
    {
        Input input;input.push(key(L'Y'));input.push(key(0x7F));input.push(key(L'Q'));input.push(key(L'\r',VK_RETURN));
        check(c::detail::read_with_sta_pump(input.endpoint(),2000).line==L"Q","VT DEL Backspace without mode mutation");
    }
    { // E: an empty signaled read returns to a nonzero event/message wait.
        Input input;input.empty_once=true;SetEvent(input.ready);
        const auto begin=GetTickCount64();auto result=c::detail::read_with_sta_pump(input.endpoint(),30);
        check(result.status==c::LineStatus::TimedOut&&!result.line&&result.console_input_event_count==0,"no input completion invented");
        check(GetTickCount64()-begin>=15&&result.wait_count<8,"idle waits block instead of polling");
    }
    {
        Input input;
        input.push(key(L'你'));input.push(key(L'\b',VK_BACK));
        input.push(key(0xD83D));input.push(key(0xDE00));input.push(key(L'\b',VK_BACK));
        input.push(key(L'界'));input.push(key(L'\r',VK_RETURN));
        auto result=c::detail::read_with_sta_pump(input.endpoint(),2000);
        check(result.line==L"界"&&input.rendered==L"界","Unicode scalar input and Backspace");
    }
    {
        Input input;INPUT_RECORD noise{};noise.EventType=MOUSE_EVENT;input.push(noise);
        input.push(key(L'Z',0,1,false));input.push(key(L'A',0,3));input.push(key(L'\r',VK_RETURN));
        auto result=c::detail::read_with_sta_pump(input.endpoint(),2000);
        check(result.line==L"AAA","ignore mouse/key-up, honor bounded repeat");
    }
    for(bool overflow:{false,true}){
        Input input;input.push(key(L'X',0,overflow?256:255));input.push(key(L'\r',VK_RETURN));
        auto result=c::detail::read_with_sta_pump(input.endpoint(),2000);
        check(overflow?result.status==c::LineStatus::TooLong:result.line&&result.line->size()==255,"bounded line capacity");
    }
    for(auto record:std::array{key(0xDC00),key(0xD800)}) {
        Input input;input.push(record);input.push(key(L'\r',VK_RETURN));
        check(c::detail::read_with_sta_pump(input.endpoint(),2000).status==c::LineStatus::InvalidUnicode,"reject malformed UTF16");
    }
    {
        Input input;input.fail_read=true;SetEvent(input.ready);
        auto r=c::detail::read_with_sta_pump(input.endpoint(),2000);
        check(r.status==c::LineStatus::ReadFailed&&r.error==ERROR_READ_FAULT,"read failure abort");
    }
    {
        Input input;input.fail_echo=true;input.push(key(L'Y'));
        check(c::detail::read_with_sta_pump(input.endpoint(),2000).status==c::LineStatus::EchoFailed,"echo failure abort");
    }
    { // C4B optional work consumes the SAME owner queue in bounded quanta.
        Input input;Window window(input);
        struct Work {Input* input;unsigned previous{},calls{};DWORD owner;bool bounded{true};} work{&input,0,0,GetCurrentThreadId(),true};
        PostMessageW(window.value,WM_APP+17,25,0);
        const auto dispatch=[](void* raw)noexcept {
            auto& w=*static_cast<Work*>(raw);++w.calls;
            w.bounded=w.bounded&&GetCurrentThreadId()==w.owner&&w.input->messages-w.previous<=8;
            w.previous=w.input->messages;return true;
        };
        const auto result=c::detail::read_with_sta_pump(input.endpoint(),2000,{&work,dispatch});
        check(result.status==c::LineStatus::Complete&&input.messages==25&&work.calls>=4&&work.bounded,"optional live work is owner-affine and eight-message bounded");
    }
    {
        Input input;input.push(key(L'Y'));
        const auto failed=[](void*)noexcept{return false;};
        const auto result=c::detail::read_with_sta_pump(input.endpoint(),2000,{nullptr,failed});
        check(result.status==c::LineStatus::OwnerWorkFailed&&!result.line&&input.reads==0,"failed live work cannot accept console consent");
    }
    UnregisterClassW(cls.lpszClassName,cls.hInstance);
    if(SUCCEEDED(com))CoUninitialize();
    std::cout<<"STA console pump failures="<<failures<<'\n';return failures?1:0;
}
