#include "platform/windows/console/sta_console_line_reader.h"

namespace panebind::platform::windows::console_input {
namespace {
using ReadNowait=BOOL (WINAPI *)(HANDLE,PINPUT_RECORD,DWORD,LPDWORD,USHORT);
constexpr USHORT read_nowait_flag=0x0002; // documented CONSOLE_READ_NOWAIT
ReadNowait resolve_nowait() noexcept {
    // Kernel32 is already loaded by this Win32 executable. No DLL search path
    // or external library is consulted and no import-library fallback exists.
    const auto module=GetModuleHandleW(L"kernel32.dll");
    return module?reinterpret_cast<ReadNowait>(GetProcAddress(module,"ReadConsoleInputExW")):nullptr;
}
bool high(wchar_t c) noexcept{return c>=0xD800&&c<=0xDBFF;}
bool low(wchar_t c) noexcept{return c>=0xDC00&&c<=0xDFFF;}
struct Editor final {
    std::array<wchar_t,StaConsoleLineReader::capacity> text{};
    std::size_t size{};wchar_t pending_high{};
    std::wstring_view view()const noexcept{return {text.data(),size};}
};
struct NativeConsole final {
    HANDLE input,output;ReadNowait read;
    std::array<COORD,StaConsoleLineReader::capacity> anchors{};
    std::array<SHORT,StaConsoleLineReader::capacity> widths{};
    std::size_t glyphs{};
    static detail::RecordRead read_record(void* raw,INPUT_RECORD& record) noexcept {
        auto& self=*static_cast<NativeConsole*>(raw);DWORD count{};
        if(!self.read(self.input,&record,1,&count,read_nowait_flag))return detail::RecordRead::Failed;
        return count==1?detail::RecordRead::Record:detail::RecordRead::Empty;
    }
    bool write(std::wstring_view text)noexcept {
        DWORD written{};
        return WriteConsoleW(output,text.data(),static_cast<DWORD>(text.size()),&written,nullptr)&&written==text.size();
    }
    static bool echo(void* raw,std::wstring_view before,std::wstring_view after,bool complete)noexcept {
        auto& self=*static_cast<NativeConsole*>(raw);
        if(complete)return self.write(L"\r\n");
        if(before.size()==after.size())return true;
        CONSOLE_SCREEN_BUFFER_INFO info{};
        if(!GetConsoleScreenBufferInfo(self.output,&info))return false;
        if(after.size()>before.size()) {
            if(self.glyphs==self.anchors.size())return false;
            self.anchors[self.glyphs]=info.dwCursorPosition;
            self.widths[self.glyphs++]=info.dwSize.X;
            return self.write(after.substr(before.size()));
        }
        if(!self.glyphs)return false;
        const auto index=--self.glyphs;const auto start=self.anchors[index];
        const auto cells=(static_cast<int>(info.dwCursorPosition.Y)-start.Y)*info.dwSize.X+
            info.dwCursorPosition.X-start.X;
        // Native cursor distances support wide Unicode cells. A resized or
        // scrolled console invalidates editing coordinates: fail, don't erase
        // unrelated output. This is deliberately not a full terminal editor.
        if(self.widths[index]!=info.dwSize.X || cells<0 || cells>512)return false;
        DWORD erased{};
        return FillConsoleOutputCharacterW(self.output,L' ',static_cast<DWORD>(cells),start,&erased)&&
            erased==static_cast<DWORD>(cells)&&SetConsoleCursorPosition(self.output,start);
    }
};
thread_local bool console_read_active{};
}
const char* line_status_name(LineStatus status) noexcept {
    switch(status) {
    case LineStatus::Complete:return "complete";
    case LineStatus::Aborted:return "aborted";
    case LineStatus::Quit:return "wm_quit";
    case LineStatus::TimedOut:return "timed_out";
    case LineStatus::InvalidConsole:return "invalid_console";
    case LineStatus::ApiUnavailable:return "nowait_api_unavailable";
    case LineStatus::WaitFailed:return "wait_failed";
    case LineStatus::ReadFailed:return "read_failed";
    case LineStatus::EchoFailed:return "echo_failed";
    case LineStatus::TooLong:return "line_capacity_exceeded";
    case LineStatus::InvalidUnicode:return "invalid_unicode";
    case LineStatus::WrongThread:return "wrong_thread";
    case LineStatus::Reentrant:return "reentrant_read";
    case LineStatus::ModeQueryFailed:return "console_mode_query_failed";
    case LineStatus::OwnerWorkFailed:return "owner_work_failed";
    }
    return "unknown";
}
bool detail::nowait_api_available() noexcept{return resolve_nowait()!=nullptr;}
LineResult detail::read_with_sta_pump(const InputEndpoint& endpoint,DWORD timeout_ms,StaOwnerWork work) {
    LineResult result;result.owner_thread=GetCurrentThreadId();
    if(!endpoint.ready || endpoint.ready==INVALID_HANDLE_VALUE || !endpoint.read_nowait || !endpoint.echo)return result;
    Editor editor;
    const auto started=GetTickCount64();
    const auto finish=[&](LineStatus status,DWORD error=ERROR_SUCCESS){result.status=status;result.error=error;return result;};
    for(;;) {
        DWORD remaining=INFINITE;
        if(timeout_ms!=INFINITE) {
            const auto elapsed=GetTickCount64()-started;
            if(elapsed>=timeout_ms)return finish(LineStatus::TimedOut);
            remaining=timeout_ms-static_cast<DWORD>(elapsed);
        }
        ++result.wait_count;
        const DWORD wake=MsgWaitForMultipleObjectsEx(1,&endpoint.ready,remaining,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
        if(wake==WAIT_FAILED)return finish(LineStatus::WaitFailed,GetLastError());
        if(wake==WAIT_TIMEOUT)return finish(LineStatus::TimedOut);
        if(wake!=WAIT_OBJECT_0 && wake!=WAIT_OBJECT_0+1)return finish(LineStatus::WaitFailed,ERROR_INVALID_DATA);
        // Pump even when the console won the wait. No HWND/message filtering:
        // OleMainThreadWndClass and nonqueued calls must also make progress.
        ++result.pump_count;
        for(std::size_t i=0;i<(work.run?8U:64U);++i) {
            MSG message{};
            if(!PeekMessageW(&message,nullptr,0,0,PM_REMOVE))break;
            if(message.message==WM_QUIT) {
                PostQuitMessage(static_cast<int>(message.wParam));
                return finish(LineStatus::Quit);
            }
            TranslateMessage(&message);DispatchMessageW(&message);
            ++result.message_dispatch_count;
        }
        if(work.run&&!work.run(work.context))return finish(LineStatus::OwnerWorkFailed);
        INPUT_RECORD record{};
        const auto read=endpoint.read_nowait(endpoint.context,record);
        if(read==RecordRead::Failed)return finish(LineStatus::ReadFailed,GetLastError());
        if(read==RecordRead::Empty)continue; // next step is the blocking wait
        ++result.console_input_event_count;
        if(record.EventType!=KEY_EVENT || !record.Event.KeyEvent.bKeyDown)continue;
        const auto& key=record.Event.KeyEvent;
        const wchar_t c=key.uChar.UnicodeChar;
        if(key.wVirtualKeyCode==VK_ESCAPE || c==0x1B)return finish(LineStatus::Aborted);
        // Ctrl+C belongs to the host (copy or its normal processed-input
        // behavior). An in-band copy-related record is not a command/abort.
        if(c==3 || (key.wVirtualKeyCode==L'C' &&
            (key.dwControlKeyState&(LEFT_CTRL_PRESSED|RIGHT_CTRL_PRESSED))))continue;
        if(!key.wRepeatCount)continue;
        if(key.wVirtualKeyCode==VK_RETURN || c==L'\r') {
            if(editor.pending_high)return finish(LineStatus::InvalidUnicode);
            if(!endpoint.echo(endpoint.context,editor.view(),editor.view(),true))return finish(LineStatus::EchoFailed,GetLastError());
            result.line=std::wstring(editor.view());return finish(LineStatus::Complete);
        }
        for(WORD repeat=0;repeat<key.wRepeatCount;++repeat) {
            const auto before=editor.view();
            // VT input represents Backspace as DEL; accept its documented
            // control encoding without turning off the host's VT input mode.
            if(key.wVirtualKeyCode==VK_BACK || c==L'\b' || c==0x7F) {
                if(editor.pending_high){editor.pending_high=0;continue;}
                if(!editor.size)break;
                --editor.size;
                if(low(editor.text[editor.size]) && editor.size && high(editor.text[editor.size-1]))--editor.size;
            } else if(c>=L' ' && c!=0x7F) {
                if(high(c)) {
                    if(editor.pending_high)return finish(LineStatus::InvalidUnicode);
                    editor.pending_high=c;continue;
                }
                if(low(c)) {
                    if(!editor.pending_high)return finish(LineStatus::InvalidUnicode);
                    if(editor.size+2>editor.text.size())return finish(LineStatus::TooLong);
                    editor.text[editor.size++]=editor.pending_high;editor.text[editor.size++]=c;editor.pending_high=0;
                } else {
                    if(editor.pending_high)return finish(LineStatus::InvalidUnicode);
                    if(editor.size==editor.text.size())return finish(LineStatus::TooLong);
                    editor.text[editor.size++]=c;
                }
            } else break; // modifiers/non-text controls aren't command text
            if(!endpoint.echo(endpoint.context,before,editor.view(),false))return finish(LineStatus::EchoFailed,GetLastError());
        }
    }
}
LineResult StaConsoleLineReader::read(StaOwnerWork work) {
    LineResult result;result.owner_thread=GetCurrentThreadId();
    if(result.owner_thread!=owner_){result.status=LineStatus::WrongThread;return result;}
    if(console_read_active){result.status=LineStatus::Reentrant;return result;}
    console_read_active=true;
    struct ActiveGuard {~ActiveGuard(){console_read_active=false;}} active_guard;
    DWORD input_mode{},output_mode{};
    if(GetFileType(input_)!=FILE_TYPE_CHAR || GetFileType(output_)!=FILE_TYPE_CHAR ||
       !GetConsoleMode(input_,&input_mode) || !GetConsoleMode(output_,&output_mode) ||
       !(output_mode&ENABLE_PROCESSED_OUTPUT)) {result.error=GetLastError();return result;}
    const auto read=resolve_nowait();
    if(!read){result.status=LineStatus::ApiUnavailable;result.error=ERROR_PROC_NOT_FOUND;return result;}
    NativeConsole native{input_,output_,read};
    result=detail::read_with_sta_pump({input_,&native,&NativeConsole::read_record,&NativeConsole::echo},INFINITE,work);
    result.input_mode_before=input_mode;
    result.modes_observed=GetConsoleMode(input_,&result.input_mode_after)!=FALSE;
    if(!result.modes_observed){result.status=LineStatus::ModeQueryFailed;result.error=GetLastError();result.line.reset();}
    // Observation only. A host-driven change is reported, never undone and
    // never treated as an instruction to reserve a shortcut or abort the line.
    result.mode_changed=result.modes_observed&&result.input_mode_before!=result.input_mode_after;
    return result;
}
} // namespace panebind::platform::windows::console_input
