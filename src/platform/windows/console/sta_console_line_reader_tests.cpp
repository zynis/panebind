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
struct ModeApi final {
    DWORD value=ENABLE_LINE_INPUT|ENABLE_ECHO_INPUT|ENABLE_QUICK_EDIT_MODE|ENABLE_PROCESSED_INPUT|ENABLE_VIRTUAL_TERMINAL_INPUT;
    unsigned sets{};bool fail_get{},fail_restore{};
    bool get(DWORD& result)noexcept{result=value;return !fail_get;}
    bool set(DWORD next)noexcept{++sets;if(fail_restore&&sets==2)return false;value=next;return true;}
};
}
int main(){
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
    for(auto abort:std::array{key(L'\x03'),key(0,VK_ESCAPE)}) {
        Input input;input.push(abort);auto result=c::detail::read_with_sta_pump(input.endpoint(),2000);
        check(result.status==c::LineStatus::Aborted&&!result.line,"Ctrl+C/Escape clean abort");
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
    {ModeApi api;const auto original=api.value;{c::detail::InputModeScope mode{api};check(mode.valid()&&!(api.value&ENABLE_PROCESSED_INPUT)&&!(api.value&ENABLE_QUICK_EDIT_MODE),"scoped raw input mode");}check(api.value==original&&api.sets==2,"normal mode restoration");}
    {ModeApi api;const auto original=api.value;try{c::detail::InputModeScope mode{api};throw std::runtime_error("test");}catch(...){}check(api.value==original,"exception mode restoration");}
    {ModeApi api;api.fail_restore=true;c::detail::InputModeScope mode{api};check(!mode.close(),"restore failure reported");}
    {ModeApi api;api.fail_get=true;c::detail::InputModeScope mode{api};check(!mode.valid()&&api.sets==0,"invalid console mode not changed");}
    UnregisterClassW(cls.lpszClassName,cls.hInstance);
    if(SUCCEEDED(com))CoUninitialize();
    std::cout<<"STA console pump failures="<<failures<<'\n';return failures?1:0;
}
