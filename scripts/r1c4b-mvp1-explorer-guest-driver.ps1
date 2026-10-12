[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('debug', 'release')][string]$Configuration,
    [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{32}$')][string]$RunId,
    [ValidateSet('AutomatedValidation', 'BindOnlyIndependentReview',
        'CommandStopValidation', 'CommandDisconnectValidation')]
    [string]$Mode = 'AutomatedValidation'
)

# Disposable Windows Sandbox test driver only. Product binaries never call
# SendInput, and this script refuses the ordinary host before creating a GUI
# process, Explorer window, evidence file, or synthetic input.
$ErrorActionPreference = 'Stop'
$inputDir = 'C:\PaneBindMVP1\Input'
$outputDir = 'C:\PaneBindMVP1\Output'
$product = if ($Configuration -ceq 'debug') {
    Join-Path $inputDir 'Debug\panebind-explorer-mvp1.exe'
} else { Join-Path $inputDir 'panebind-explorer-mvp1.exe' }
$productLog = Join-Path $outputDir ($RunId + $(if ($Configuration -ceq 'debug') {
    '-explorer-mvp1-debug.jsonl'
} else { '-explorer-mvp1.jsonl' }))
$driverLog = Join-Path $outputDir ($RunId + '-explorer-driver-' + $Configuration + '.jsonl')
$script:sequence = 0
$script:writer = $null
$script:consoleInput = [IntPtr]::Zero
$script:commandPipe = $null
$script:usesGuestPipe = $Mode -cne 'BindOnlyIndependentReview'
$script:pipeName = 'PaneBindMVP1-' + $RunId + '-' + $Configuration
$script:pipePath = '\\.\pipe\' + $script:pipeName
$script:pressed = $false
$script:ctrlPressed = $false
$script:productPid = 0
$script:productStarted = $false
$script:attachedConsole = $null
$script:bound = @()
$script:confirmationRows = @()
$script:targets = @()
$script:gestureGenerations = [Collections.Generic.HashSet[long]]::new()

function Test-DriverModeManifest($Manifest) {
    if ($Manifest.explorer_guest_mode -cne $Mode -or
        $Manifest.independent_review_configuration -cne 'release') {
        throw 'exact_guest_driver_mode_manifest_required'
    }
    if ($Mode -ceq 'BindOnlyIndependentReview' -and $Configuration -cne 'release') {
        throw 'independent_review_requires_exact_release_configuration'
    }
}

function Test-ExactGuest {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent().Name
    $guestAccount = $identity -ieq 'WDAGUtilityAccount' -or
        $identity.EndsWith('\WDAGUtilityAccount', [StringComparison]::OrdinalIgnoreCase)
    if (-not $guestAccount -or (Get-Process -Id $PID).SessionId -le 0 -or
        -not [Environment]::UserInteractive) { throw 'guest_interactive_identity_required' }
    if (-not (Test-Path -LiteralPath $inputDir -PathType Container) -or
        -not (Test-Path -LiteralPath $outputDir -PathType Container)) {
        throw 'exact_mappings_required'
    }
    $inputMarker = [IO.File]::ReadAllBytes((Join-Path $inputDir 'run-id.txt'))
    $outputMarker = [IO.File]::ReadAllBytes((Join-Path $outputDir 'run-id.txt'))
    if ($inputMarker.Length -ne 32 -or $outputMarker.Length -ne 32 -or
        [Text.Encoding]::ASCII.GetString($inputMarker) -cne $RunId -or
        [Text.Encoding]::ASCII.GetString($outputMarker) -cne $RunId) {
        throw 'run_id_mapping_mismatch'
    }
    $manifest = Get-Content -LiteralPath (Join-Path $inputDir 'manifest.json') -Raw |
        ConvertFrom-Json
    if ($manifest.schema -cne 'r1c4b-mvp1-shield-sandbox-package/v3' -or
        $manifest.run_id -cne $RunId -or
        $manifest.source_commit -cnotmatch '^[0-9a-f]{40}$' -or
        $manifest.explorer_test_exception -cne 'exact_sandbox_same_sid_session_high' -or
        $manifest.explorer_driver_sha256 -cne
            (Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash) {
        throw 'driver_manifest_mismatch'
    }
    $channel = if ($script:usesGuestPipe) { 'guest_named_pipe' } else { 'console' }
    if ($manifest.explorer_command_channel -cne $channel) {
        throw 'exact_guest_command_channel_manifest_required'
    }
    $expected = if ($Configuration -ceq 'debug') {
        $manifest.debug_explorer_sha256
    } else { $manifest.binaries_sha256.'panebind-explorer-mvp1.exe' }
    if ($expected -cne (Get-FileHash -LiteralPath $product -Algorithm SHA256).Hash) {
        throw 'product_binary_mismatch'
    }
    Test-DriverModeManifest $manifest
    if (Test-Path -LiteralPath $productLog) { throw 'product_evidence_already_exists' }
    if (Test-Path -LiteralPath $driverLog) { throw 'driver_evidence_already_exists' }
    return $manifest
}

$manifest = Test-ExactGuest

# Win32 calls are encapsulated in the guest-only test driver. The product's
# owned resource and input authority remain separate and unchanged.
Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Collections.Generic;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Threading;
public static class PaneBindMvpGuestInput {
    [StructLayout(LayoutKind.Sequential)] public struct Rect {
        public int Left, Top, Right, Bottom;
    }
    [StructLayout(LayoutKind.Sequential)] public struct Point {
        public int X, Y;
    }
    [StructLayout(LayoutKind.Sequential)] public struct GuiThreadInfo {
        public uint size, flags;
        public IntPtr active, focus, capture, menuOwner, moveSize, caret;
        public Rect caretRect;
    }
    [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)] public struct StartupInfo {
        public uint cb; public string reserved; public string desktop; public string title;
        public uint x,y,xSize,ySize,xCountChars,yCountChars,fillAttribute,flags;
        public ushort showWindow, reserved2; public IntPtr reserved2Ptr, stdInput, stdOutput, stdError;
    }
    [StructLayout(LayoutKind.Sequential)] public struct ProcessInfo {
        public IntPtr process, thread; public uint processId, threadId;
    }
    [StructLayout(LayoutKind.Explicit, Size=20)] public struct InputRecord {
        [FieldOffset(0)] public ushort eventType;
        [FieldOffset(4)] public int keyDown;
        [FieldOffset(8)] public ushort repeat;
        [FieldOffset(10)] public ushort virtualKey;
        [FieldOffset(12)] public ushort scan;
        [FieldOffset(14)] public char character;
        [FieldOffset(16)] public uint controlState;
    }
    [StructLayout(LayoutKind.Sequential)] public struct MouseInput {
        public int dx,dy; public uint mouseData, flags, time; public UIntPtr extra;
    }
    [StructLayout(LayoutKind.Sequential)] public struct KeyInput {
        public ushort key, scan; public uint flags,time; public UIntPtr extra;
    }
    [StructLayout(LayoutKind.Explicit, Size=40)] public struct Input {
        [FieldOffset(0)] public uint type;
        [FieldOffset(8)] public MouseInput mouse;
        [FieldOffset(8)] public KeyInput key;
    }
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    static extern bool CreateProcessW(string application, StringBuilder command,
        IntPtr processAttributes, IntPtr threadAttributes, bool inherit,
        uint flags, IntPtr environment, string directory,
        ref StartupInfo startup, out ProcessInfo process);
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool FreeConsole();
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool AttachConsole(uint processId);
    [DllImport("kernel32.dll")] static extern IntPtr GetConsoleWindow();
    [DllImport("kernel32.dll", SetLastError=true)] static extern uint GetConsoleProcessList(
        [Out] uint[] processes,uint capacity);
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    static extern IntPtr CreateFileW(string name,uint access,uint share,IntPtr security,
        uint creation,uint attributes,IntPtr template);
    [DllImport("kernel32.dll", SetLastError=true)] public static extern bool CloseHandle(IntPtr handle);
    [DllImport("kernel32.dll")] static extern void SetLastError(uint error);
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool WriteConsoleInputW(
        IntPtr input,[In] InputRecord[] records,uint length,out uint written);
    [DllImport("user32.dll")] static extern IntPtr GetAncestor(IntPtr window,uint flags);
    [DllImport("user32.dll")] static extern IntPtr WindowFromPoint(Point point);
    [DllImport("user32.dll")] static extern bool GetCursorPos(out Point point);
    [DllImport("user32.dll",SetLastError=true)] static extern IntPtr OpenInputDesktop(uint flags,bool inherit,uint access);
    [DllImport("user32.dll",SetLastError=true)] static extern IntPtr GetThreadDesktop(uint thread);
    [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool GetUserObjectInformationW(
        IntPtr handle,int index,StringBuilder value,int length,out int needed);
    [DllImport("user32.dll",EntryPoint="GetUserObjectInformationW",SetLastError=true)]
    static extern bool GetUserObjectInput(IntPtr handle,int index,out int value,int length,out int needed);
    [DllImport("user32.dll")] static extern bool CloseDesktop(IntPtr desktop);
    [DllImport("kernel32.dll")] static extern uint GetCurrentThreadId();
    [DllImport("kernel32.dll")] static extern uint WTSGetActiveConsoleSessionId();
    [DllImport("kernel32.dll")] static extern uint GetCurrentProcessId();
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool ProcessIdToSessionId(uint pid,out uint session);
    [DllImport("wtsapi32.dll",CharSet=CharSet.Unicode,SetLastError=true)]
    static extern bool WTSQuerySessionInformationW(IntPtr server,uint session,int infoClass,
        out IntPtr buffer,out uint bytes);
    [DllImport("wtsapi32.dll")] static extern void WTSFreeMemory(IntPtr buffer);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern int GetClassNameW(
        IntPtr window,StringBuilder name,int length);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(
        IntPtr window,out uint pid);
    [DllImport("user32.dll")] static extern bool IsWindow(IntPtr window);
    [DllImport("user32.dll")] static extern bool IsIconic(IntPtr window);
    [DllImport("user32.dll")] static extern bool IsZoomed(IntPtr window);
    [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] static extern uint GetDpiForWindow(IntPtr window);
    [DllImport("user32.dll")] static extern IntPtr GetWindowDpiAwarenessContext(IntPtr window);
    [DllImport("user32.dll",SetLastError=true)] static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
    [DllImport("user32.dll")] static extern IntPtr GetThreadDpiAwarenessContext();
    [DllImport("user32.dll")] static extern bool AreDpiAwarenessContextsEqual(IntPtr first,IntPtr second);
    [DllImport("user32.dll",SetLastError=true)] static extern IntPtr GetWindowLongPtrW(IntPtr window,int index);
    [DllImport("user32.dll",SetLastError=true)] static extern bool GetGUIThreadInfo(uint thread,ref GuiThreadInfo info);
    [DllImport("user32.dll",SetLastError=true)] static extern bool GetWindowRect(IntPtr window,out Rect rect);
    [DllImport("dwmapi.dll")] static extern int DwmGetWindowAttribute(
        IntPtr window,uint attribute,out Rect rect,uint size);
    [DllImport("user32.dll", SetLastError=true)] static extern bool SetWindowPos(
        IntPtr window,IntPtr after,int x,int y,int width,int height,uint flags);
    [DllImport("user32.dll")] static extern bool ShowWindow(IntPtr window,int command);
    [DllImport("user32.dll")] static extern bool SetForegroundWindow(IntPtr window);
    [DllImport("user32.dll")] static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll", SetLastError=true)] static extern IntPtr SendMessageTimeoutW(
        IntPtr window,uint message,IntPtr wp,IntPtr lp,uint flags,uint timeout,out IntPtr result);
    [DllImport("user32.dll", SetLastError=true)] static extern uint SendInput(
        uint count,[In] Input[] inputs,int size);
    [DllImport("user32.dll")] static extern short GetAsyncKeyState(int key);
    [DllImport("user32.dll")] static extern int GetSystemMetrics(int metric);
    const uint MOUSE_MOVE=0x0001, LEFTDOWN=0x0002, LEFTUP=0x0004,
        ABSOLUTE=0x8000, VIRTUALDESK=0x4000, KEYUP=0x0002;
    static bool callerDpiEstablished;
    public sealed class CallerDpiSnapshot {
        public uint thread_id;
        public long previous_context, current_context;
        public int win32_error;
        public bool set_returned_previous, current_per_monitor_v2, established;
    }
    public static CallerDpiSnapshot EstablishCallerPhysicalCoordinates() {
        // Runs on the ACTUAL PowerShell caller, not an Add-Type worker or a
        // presumed persistent host thread. The exact guest guard precedes it.
        var facts=new CallerDpiSnapshot();facts.thread_id=GetCurrentThreadId();
        SetLastError(0);
        var previous=SetThreadDpiAwarenessContext(new IntPtr(-4));
        facts.win32_error=Marshal.GetLastWin32Error();
        facts.previous_context=previous.ToInt64();
        facts.set_returned_previous=previous!=IntPtr.Zero;
        var current=GetThreadDpiAwarenessContext();
        facts.current_context=current.ToInt64();
        facts.current_per_monitor_v2=current!=IntPtr.Zero&&
            AreDpiAwarenessContextsEqual(current,new IntPtr(-4));
        facts.established=facts.set_returned_previous&&facts.current_per_monitor_v2;
        callerDpiEstablished=facts.established;
        return facts;
    }
    static void RequirePhysicalCaller() {
        // Recheck on EACH actual geometry/input call. If PowerShell changes
        // threads or the context changes, refuse rather than assume/add loops.
        var current=GetThreadDpiAwarenessContext();
        if(!callerDpiEstablished||current==IntPtr.Zero||
            !AreDpiAwarenessContextsEqual(current,new IntPtr(-4)))
            throw new InvalidOperationException("guest driver caller must be actual PMV2; thread="+
                GetCurrentThreadId()+" context="+current.ToInt64());
    }
    public static StartupInfo ProductStartup(bool pipe) {
        var startup=new StartupInfo();startup.cb=(uint)Marshal.SizeOf(typeof(StartupInfo));
        startup.flags=pipe?0u:0x00000001u;
        startup.showWindow=(ushort)(pipe?0:5);
        return startup;
    }
    public sealed class ProductStartSnapshot {
        public uint pid, tid, creation_flags, startup_flags;
        public ushort show_window;
        public bool started;
    }
    public static uint ProductCreationFlags(bool pipe) {
        return pipe?0x08000000u:0x00000010u;
    }
    public static ProductStartSnapshot StartProduct(string path,string arguments,bool pipe) {
        RequirePhysicalCaller();
        var startup=ProductStartup(pipe);
        ProcessInfo process;
        var facts=new ProductStartSnapshot();
        // CREATE_NO_WINDOW is not SW_HIDE. The original independent-review
        // console remains a separate, explicit entry; no unknown HWND control.
        facts.creation_flags=ProductCreationFlags(pipe);
        facts.startup_flags=startup.flags;facts.show_window=startup.showWindow;
        var command=new StringBuilder("\""+path+"\" "+arguments);
        if(!CreateProcessW(path,command,IntPtr.Zero,IntPtr.Zero,false,facts.creation_flags,
            IntPtr.Zero,null,ref startup,out process))
            throw new Win32Exception(Marshal.GetLastWin32Error(),"CreateProcessW");
        facts.pid=process.processId;facts.tid=process.threadId;facts.started=true;
        CloseHandle(process.thread);CloseHandle(process.process);
        return facts;
    }
    public static long DriverConsoleWindow() { return GetConsoleWindow().ToInt64(); }
    public static IntPtr AttachInput(int pid) {
        RequirePhysicalCaller();
        FreeConsole();
        for(int i=0;i<30;i++) {
            if(AttachConsole((uint)pid)) {
                var input=CreateFileW("CONIN$",0xC0000000,3,IntPtr.Zero,3,0,IntPtr.Zero);
                if(input!=new IntPtr(-1)) return input;
                throw new Win32Exception(Marshal.GetLastWin32Error(),"CONIN$");
            }
            Thread.Sleep(100);
        }
        throw new Win32Exception(Marshal.GetLastWin32Error(),"AttachConsole");
    }
    public sealed class ConsoleSnapshot {
        public long hwnd, root;
        public uint owner_pid, owner_tid, caller_pid, expected_product_pid;
        public uint caller_session, owner_session, product_session;
        public bool caller_session_known, owner_session_known, product_session_known;
        public bool visible, process_list_complete, strict_dedicated;
        public string window_class="";
        public uint process_count;
        public int process_list_error;
        public uint[] clients=new uint[0];
    }
    public static ConsoleSnapshot ObserveAttachedConsole(int productPid) {
        RequirePhysicalCaller();
        var facts=new ConsoleSnapshot();var window=GetConsoleWindow();
        facts.hwnd=window.ToInt64();facts.root=GetAncestor(window,2).ToInt64();
        facts.visible=window!=IntPtr.Zero&&IsWindowVisible(window);
        facts.window_class=ClassOf(window);facts.caller_pid=GetCurrentProcessId();
        facts.expected_product_pid=productPid>0?(uint)productPid:0;
        uint ownerPid;facts.owner_tid=GetWindowThreadProcessId(window,out ownerPid);facts.owner_pid=ownerPid;
        uint session;facts.caller_session_known=ProcessIdToSessionId(facts.caller_pid,out session);
        facts.caller_session=session;facts.owner_session_known=ownerPid!=0&&ProcessIdToSessionId(ownerPid,out session);
        facts.owner_session=facts.owner_session_known?session:0;
        facts.product_session_known=productPid>0&&ProcessIdToSessionId((uint)productPid,out session);
        facts.product_session=facts.product_session_known?session:0;
        var clients=new uint[8];SetLastError(0);
        facts.process_count=GetConsoleProcessList(clients,(uint)clients.Length);
        facts.process_list_error=Marshal.GetLastWin32Error();
        facts.process_list_complete=facts.process_count>0&&facts.process_count<=clients.Length;
        if(facts.process_list_complete) {
            facts.clients=new uint[facts.process_count];Array.Copy(clients,facts.clients,facts.clients.Length);
        }
        facts.strict_dedicated=DedicatedConsoleFacts(facts);
        return facts;
    }
    public static bool DedicatedConsoleFacts(ConsoleSnapshot facts) {
        if(facts==null||facts.hwnd==0||facts.root!=facts.hwnd||facts.owner_pid==0||facts.owner_tid==0||
            facts.window_class!="ConsoleWindowClass"||facts.caller_pid==0||facts.expected_product_pid==0||
            facts.caller_pid==facts.expected_product_pid||!facts.process_list_complete||
            facts.process_count!=2||facts.clients==null||facts.clients.Length!=2||
            !facts.caller_session_known||!facts.owner_session_known||!facts.product_session_known||
            facts.caller_session==0||facts.owner_session!=facts.caller_session||
            facts.product_session!=facts.caller_session) return false;
        return (facts.clients[0]==facts.caller_pid&&facts.clients[1]==facts.expected_product_pid)||
            (facts.clients[1]==facts.caller_pid&&facts.clients[0]==facts.expected_product_pid);
    }
    public static void ConsoleLine(IntPtr input,string line) {
        RequirePhysicalCaller();
        var records=new List<InputRecord>();
        foreach(var ch in line) records.Add(new InputRecord {
            eventType=1,keyDown=1,repeat=1,character=ch
        });
        records.Add(new InputRecord {eventType=1,keyDown=1,repeat=1,
            virtualKey=13,character='\r'});
        uint written;
        if(!WriteConsoleInputW(input,records.ToArray(),(uint)records.Count,out written)
            ||written!=records.Count)
            throw new Win32Exception(Marshal.GetLastWin32Error(),"WriteConsoleInputW");
    }
    public static bool ExplorerRoot(long value,out int pid,out int tid) {
        RequirePhysicalCaller();
        var window=new IntPtr(value);uint nativePid;
        uint nativeTid=GetWindowThreadProcessId(window,out nativePid);
        pid=(int)nativePid;tid=(int)nativeTid;
        var name=new StringBuilder(128);
        return IsWindow(window)&&GetAncestor(window,2)==window&&
            GetClassNameW(window,name,name.Capacity)>0&&
            (name.ToString()=="CabinetWClass"||name.ToString()=="ExploreWClass")&&
            nativePid!=0&&nativeTid!=0;
    }
    public sealed class RootSnapshot {
        public long hwnd, foreground, style, extended_style;
        public int pid, tid;
        public bool exact_explorer_root, minimized, maximized, visible;
        public uint dpi;
        public bool per_monitor_v2;
        public bool style_available, extended_style_available, outer_rect_available, visible_rect_available;
        public int style_error, extended_style_error, outer_rect_error, visible_rect_hresult;
        public int[] outer_rect, visible_rect;
        public bool gui_available;
        public int gui_error;
        public uint gui_flags;
        public long active, focus, capture, move_size, menu_owner;
    }
    public static RootSnapshot ObserveRoot(long value) {
        RequirePhysicalCaller();
        var facts=new RootSnapshot();facts.hwnd=value;
        int pid,tid;
        facts.exact_explorer_root=ExplorerRoot(value,out pid,out tid);
        facts.pid=pid;facts.tid=tid;
        if(!facts.exact_explorer_root) return facts;
        var window=new IntPtr(value);
        facts.minimized=IsIconic(window);facts.maximized=IsZoomed(window);
        facts.visible=IsWindowVisible(window);facts.foreground=GetForegroundWindow().ToInt64();
        facts.dpi=GetDpiForWindow(window);
        facts.per_monitor_v2=AreDpiAwarenessContextsEqual(GetWindowDpiAwarenessContext(window),new IntPtr(-4));
        SetLastError(0);facts.style=GetWindowLongPtrW(window,-16).ToInt64();
        facts.style_error=Marshal.GetLastWin32Error();
        facts.style_available=facts.style!=0||facts.style_error==0;
        SetLastError(0);facts.extended_style=GetWindowLongPtrW(window,-20).ToInt64();
        facts.extended_style_error=Marshal.GetLastWin32Error();
        facts.extended_style_available=facts.extended_style!=0||facts.extended_style_error==0;
        Rect rect;
        facts.outer_rect_available=GetWindowRect(window,out rect);
        facts.outer_rect_error=facts.outer_rect_available?0:Marshal.GetLastWin32Error();
        if(facts.outer_rect_available) facts.outer_rect=new[]{rect.Left,rect.Top,rect.Right,rect.Bottom};
        facts.visible_rect_hresult=DwmGetWindowAttribute(window,9,out rect,(uint)Marshal.SizeOf(typeof(Rect)));
        facts.visible_rect_available=facts.visible_rect_hresult==0;
        if(facts.visible_rect_available) facts.visible_rect=new[]{rect.Left,rect.Top,rect.Right,rect.Bottom};
        var gui=new GuiThreadInfo();gui.size=(uint)Marshal.SizeOf(typeof(GuiThreadInfo));
        facts.gui_available=GetGUIThreadInfo((uint)tid,ref gui);
        facts.gui_error=facts.gui_available?0:Marshal.GetLastWin32Error();
        if(facts.gui_available) {
            facts.gui_flags=gui.flags;facts.active=gui.active.ToInt64();facts.focus=gui.focus.ToInt64();
            facts.capture=gui.capture.ToInt64();facts.move_size=gui.moveSize.ToInt64();
            facts.menu_owner=gui.menuOwner.ToInt64();
        }
        return facts;
    }
    public static int[] Rectangle(long value,bool visible) {
        RequirePhysicalCaller();
        Rect rect;
        if(visible) {
            if(DwmGetWindowAttribute(new IntPtr(value),9,out rect,(uint)Marshal.SizeOf(typeof(Rect)))!=0)
                throw new Win32Exception("DwmGetWindowAttribute");
        } else if(!GetWindowRect(new IntPtr(value),out rect))
            throw new Win32Exception(Marshal.GetLastWin32Error(),"GetWindowRect");
        return new[]{rect.Left,rect.Top,rect.Right,rect.Bottom};
    }
    public static void Place(long value,int x,int y,int width,int height) {
        RequirePhysicalCaller();
        ShowWindow(new IntPtr(value),9);
        if(!SetWindowPos(new IntPtr(value),IntPtr.Zero,x,y,width,height,0x0014))
            throw new Win32Exception(Marshal.GetLastWin32Error(),"SetWindowPos(test-created root)");
    }
    public sealed class ForegroundSnapshot {
        public long requested, before, after;
        public bool api_returned, exact_after;
    }
    public static ForegroundSnapshot Foreground(long value) {
        RequirePhysicalCaller();
        var facts=new ForegroundSnapshot();facts.requested=value;
        facts.before=GetForegroundWindow().ToInt64();
        facts.api_returned=SetForegroundWindow(new IntPtr(value));
        facts.after=GetForegroundWindow().ToInt64();facts.exact_after=facts.after==value;
        return facts;
    }
    public sealed class DesktopSnapshot {
        public bool ready;
        public uint session_id, active_console_session_id, thread_id;
        public bool session_query_ok, connection_query_ok, connection_buffer_valid;
        public int session_error, connection_error, connection_state=-1;
        public bool input_desktop_available, thread_desktop_available;
        public int input_desktop_error, thread_desktop_error;
        public bool input_name_query_ok, thread_name_query_ok;
        public string input_desktop_name="", thread_desktop_name="";
        public int input_name_error, thread_name_error;
        public bool input_io_query_ok, thread_io_query_ok, input_receives_input, thread_receives_input;
        public int input_io_error, thread_io_error;
    }
    public static DesktopSnapshot InputDesktopSnapshot() {
        var facts=new DesktopSnapshot();
        facts.active_console_session_id=WTSGetActiveConsoleSessionId(); // Diagnostic, not guest readiness.
        facts.thread_id=GetCurrentThreadId();
        uint session;
        facts.session_query_ok=ProcessIdToSessionId(GetCurrentProcessId(),out session);
        facts.session_error=facts.session_query_ok?0:Marshal.GetLastWin32Error();
        facts.session_id=session;
        if(!facts.session_query_ok||session==0) return facts;
        IntPtr buffer=IntPtr.Zero;uint bytes;
        try {
            // WTSConnectState is the documented enum (index 8); WTSActive is 0.
            // Sandbox may use an active remote session rather than the physical console.
            facts.connection_query_ok=WTSQuerySessionInformationW(IntPtr.Zero,session,8,out buffer,out bytes);
            facts.connection_error=facts.connection_query_ok?0:Marshal.GetLastWin32Error();
            facts.connection_buffer_valid=facts.connection_query_ok&&buffer!=IntPtr.Zero&&bytes==4;
            if(facts.connection_buffer_valid) facts.connection_state=Marshal.ReadInt32(buffer);
        } finally { if(buffer!=IntPtr.Zero) WTSFreeMemory(buffer); }
        var input=OpenInputDesktop(0,false,0x0001);
        facts.input_desktop_available=input!=IntPtr.Zero;
        facts.input_desktop_error=facts.input_desktop_available?0:Marshal.GetLastWin32Error();
        var current=GetThreadDesktop(facts.thread_id);
        facts.thread_desktop_available=current!=IntPtr.Zero;
        facts.thread_desktop_error=facts.thread_desktop_available?0:Marshal.GetLastWin32Error();
        try {
            int needed,receives;
            if(facts.input_desktop_available) {
                var name=new StringBuilder(256);
                facts.input_name_query_ok=GetUserObjectInformationW(input,2,name,512,out needed);
                facts.input_name_error=facts.input_name_query_ok?0:Marshal.GetLastWin32Error();
                if(facts.input_name_query_ok) facts.input_desktop_name=name.ToString();
                facts.input_io_query_ok=GetUserObjectInput(input,6,out receives,4,out needed);
                facts.input_io_error=facts.input_io_query_ok?0:Marshal.GetLastWin32Error();
                facts.input_receives_input=facts.input_io_query_ok&&needed==4&&receives!=0;
            }
            if(facts.thread_desktop_available) {
                var name=new StringBuilder(256);
                facts.thread_name_query_ok=GetUserObjectInformationW(current,2,name,512,out needed);
                facts.thread_name_error=facts.thread_name_query_ok?0:Marshal.GetLastWin32Error();
                if(facts.thread_name_query_ok) facts.thread_desktop_name=name.ToString();
                facts.thread_io_query_ok=GetUserObjectInput(current,6,out receives,4,out needed);
                facts.thread_io_error=facts.thread_io_query_ok?0:Marshal.GetLastWin32Error();
                facts.thread_receives_input=facts.thread_io_query_ok&&needed==4&&receives!=0;
            }
            facts.ready=facts.connection_buffer_valid&&facts.connection_state==0&&
                facts.input_name_query_ok&&facts.thread_name_query_ok&&
                facts.input_desktop_name=="Default"&&facts.thread_desktop_name=="Default"&&
                facts.input_receives_input&&facts.thread_receives_input;
            return facts;
        } finally { if(input!=IntPtr.Zero) CloseDesktop(input); }
    }
    public static bool InputDesktopReady() { return InputDesktopSnapshot().ready; }
    public static int[] Cursor() {
        RequirePhysicalCaller();
        Point point;
        if(!GetCursorPos(out point)) throw new Win32Exception(Marshal.GetLastWin32Error(),"GetCursorPos");
        return new[]{point.X,point.Y};
    }
    public static long CursorRoot() {
        RequirePhysicalCaller();
        Point point;
        if(!GetCursorPos(out point)) throw new Win32Exception(Marshal.GetLastWin32Error(),"GetCursorPos");
        return GetAncestor(WindowFromPoint(point),2).ToInt64();
    }
    public static int Hit(long value,int x,int y) {
        RequirePhysicalCaller();
        IntPtr result;
        var point=new IntPtr((y<<16)|(x&0xffff));
        if(SendMessageTimeoutW(new IntPtr(value),0x84,IntPtr.Zero,point,0x0002,500,out result)==IntPtr.Zero)
            return int.MinValue;
        return result.ToInt32();
    }
    public sealed class HitSnapshot {
        public long expected_root, message_lparam, root_api_return;
        public int root_result, root_error;
        public bool root_call_succeeded;
        public long hit_hwnd, hit_root;
        public uint hit_pid, hit_tid;
        public string hit_class="", hit_root_class="";
        public bool exact_hit_root, identity_after_confirmed, child_queried, child_call_succeeded;
        public long child_api_return;
        public int child_result, child_error;
    }
    static string ClassOf(IntPtr window) {
        if(window==IntPtr.Zero) return "";
        var name=new StringBuilder(256);
        return GetClassNameW(window,name,name.Capacity)>0?name.ToString():"";
    }
    public static HitSnapshot ObserveHit(long value,int x,int y) {
        RequirePhysicalCaller();
        var facts=new HitSnapshot();facts.expected_root=value;
        var root=new IntPtr(value);var point=new Point{X=x,Y=y};
        var parameter=new IntPtr((y<<16)|(x&0xffff));facts.message_lparam=parameter.ToInt64();
        IntPtr result;SetLastError(0);
        var returned=SendMessageTimeoutW(root,0x84,IntPtr.Zero,parameter,0x0002,500,out result);
        facts.root_api_return=returned.ToInt64();facts.root_call_succeeded=returned!=IntPtr.Zero;
        facts.root_error=Marshal.GetLastWin32Error();facts.root_result=result.ToInt32();
        var hit=WindowFromPoint(point);var hitRoot=GetAncestor(hit,2);
        facts.hit_hwnd=hit.ToInt64();facts.hit_root=hitRoot.ToInt64();
        facts.hit_class=ClassOf(hit);facts.hit_root_class=ClassOf(hitRoot);
        facts.exact_hit_root=hitRoot==root;
        uint hitPid;facts.hit_tid=GetWindowThreadProcessId(hit,out hitPid);facts.hit_pid=hitPid;
        uint expectedPid;var expectedTid=GetWindowThreadProcessId(root,out expectedPid);
        // Query the ACTUAL endpoint under this point, only inside the exact
        // root and its same PID/TID. A class name or HTCLIENT is no permission.
        if(hit!=IntPtr.Zero&&hit!=root&&hitRoot==root&&expectedPid!=0&&expectedTid!=0&&
            hitPid==expectedPid&&facts.hit_tid==expectedTid) {
            facts.child_queried=true;SetLastError(0);
            returned=SendMessageTimeoutW(hit,0x84,IntPtr.Zero,parameter,0x0002,500,out result);
            facts.child_api_return=returned.ToInt64();facts.child_call_succeeded=returned!=IntPtr.Zero;
            facts.child_error=Marshal.GetLastWin32Error();facts.child_result=result.ToInt32();
        }
        uint afterPid;var afterTid=GetWindowThreadProcessId(hit,out afterPid);
        facts.identity_after_confirmed=hit!=IntPtr.Zero&&WindowFromPoint(point)==hit&&
            GetAncestor(hit,2)==root&&afterPid==hitPid&&afterTid==facts.hit_tid;
        return facts;
    }
    public static int ExactEndpointHit(HitSnapshot facts,long root,int pid,int tid) {
        if(facts==null||pid<=0||tid<=0||facts.expected_root!=root||
            facts.hit_hwnd==0||facts.hit_root!=root||!facts.exact_hit_root||
            !facts.identity_after_confirmed||facts.hit_pid!=(uint)pid||facts.hit_tid!=(uint)tid)
            return int.MinValue;
        if(facts.hit_hwnd==root) return facts.root_call_succeeded?facts.root_result:int.MinValue;
        return facts.child_queried&&facts.child_call_succeeded?facts.child_result:int.MinValue;
    }
    public static void MouseMove(int x,int y) {
        RequirePhysicalCaller();
        if(!InputDesktopReady()) throw new InvalidOperationException("guest input desktop inactive before mouse move");
        int vx=GetSystemMetrics(76),vy=GetSystemMetrics(77),
            vw=GetSystemMetrics(78),vh=GetSystemMetrics(79);
        if(vw<2||vh<2||x<vx||x>=vx+vw||y<vy||y>=vy+vh)
            throw new ArgumentOutOfRangeException("cursor outside virtual desktop");
        var input=new Input { type=0,mouse=new MouseInput {
            dx=(int)Math.Round((double)(x-vx)*65535/(vw-1)),
            dy=(int)Math.Round((double)(y-vy)*65535/(vh-1)),
            flags=MOUSE_MOVE|ABSOLUTE|VIRTUALDESK,extra=new UIntPtr(0x50424D58)
        }};
        if(SendInput(1,new[]{input},Marshal.SizeOf(typeof(Input)))!=1)
            throw new Win32Exception(Marshal.GetLastWin32Error(),"SendInput move");
    }
    public static void MouseButton(bool down) {
        RequirePhysicalCaller();
        if(!InputDesktopReady()) throw new InvalidOperationException("guest input desktop inactive before mouse button");
        var input=new Input {type=0,mouse=new MouseInput {
            flags=down?LEFTDOWN:LEFTUP,extra=new UIntPtr(0x50424D58)
        }};
        if(SendInput(1,new[]{input},Marshal.SizeOf(typeof(Input)))!=1)
            throw new Win32Exception(Marshal.GetLastWin32Error(),"SendInput button");
    }
    public static void Control(bool down) {
        RequirePhysicalCaller();
        if(!InputDesktopReady()) throw new InvalidOperationException("guest input desktop inactive before Ctrl key");
        var input=new Input {type=1,key=new KeyInput {
            key=0x11,flags=down?0:KEYUP,extra=new UIntPtr(0x50424D58)
        }};
        if(SendInput(1,new[]{input},Marshal.SizeOf(typeof(Input)))!=1)
            throw new Win32Exception(Marshal.GetLastWin32Error(),"SendInput Ctrl");
    }
    public static bool LeftDown() {
        RequirePhysicalCaller();
        return (GetAsyncKeyState(1)&0x8000)!=0;
    }
    public static int[] WorkArea() {
        RequirePhysicalCaller();
        int x=GetSystemMetrics(76),y=GetSystemMetrics(77),
            width=GetSystemMetrics(78),height=GetSystemMetrics(79);
        return new[]{x,y,width,height};
    }
}
'@

$initialDesktopSnapshot = [PaneBindMvpGuestInput]::InputDesktopSnapshot()
Write-Output ($initialDesktopSnapshot | ConvertTo-Json -Depth 3 -Compress)
if (-not $initialDesktopSnapshot.ready) {
    throw 'guest_input_desktop_not_active_at_driver_start'
}

function Record([string]$Kind, [hashtable]$Facts = @{}) {
    $script:sequence++
    $entry = [ordered]@{ schema = 'r1c4b-mvp1-explorer-guest-driver/v1'; sequence = $script:sequence;
        type = $Kind; run_id = $RunId; configuration = $Configuration }
    foreach ($key in $Facts.Keys) { $entry[$key] = $Facts[$key] }
    $script:writer.WriteLine(($entry | ConvertTo-Json -Compress -Depth 7))
    $script:writer.Flush()
}
function Read-GuestProcessIdentityChain([int]$ProcessId,[int]$MaximumNodes = 3) {
    # Exact-PID, bounded post-fact observation only. Never enumerate processes,
    # retrieve CommandLine, or infer historical ownership/control permission.
    $records = @(); $seen = [Collections.Generic.HashSet[int]]::new()
    $current = $ProcessId
    for ($depth=0; $depth -lt [Math]::Min($MaximumNodes,3); $depth++) {
        if ($current -le 0 -or -not $seen.Add($current)) { break }
        $before = [DateTime]::UtcNow.ToString('O')
        try {
            $matches = @(Get-CimInstance -ClassName Win32_Process -Filter ("ProcessId="+$current) `
                -Property ProcessId,Name,ParentProcessId,CreationDate -OperationTimeoutSec 2 -ErrorAction Stop)
            if ($matches.Count -ne 1 -or $matches[0].ProcessId -ne $current) {
                $records += @{ depth=$depth; query_pid=$current; result='UNKNOWN';
                    observation_utc_before=$before; observation_utc_after=[DateTime]::UtcNow.ToString('O');
                    reason='exact_pid_not_observed' }
                break
            }
            $process = $matches[0]
            $created = if ($process.CreationDate -is [DateTime]) {
                $process.CreationDate.ToUniversalTime().ToString('O')
            } else { $null }
            $records += @{ depth=$depth; query_pid=$current; result='OBSERVED_CURRENT_PID';
                pid=$process.ProcessId; name=$process.Name; parent_pid=$process.ParentProcessId;
                creation_utc=$created; observation_utc_before=$before;
                observation_utc_after=[DateTime]::UtcNow.ToString('O');
                historic_callback_identity='NOT_PROVEN_BY_POST_FACT_QUERY' }
            $current = [int]$process.ParentProcessId
        } catch {
            $records += @{ depth=$depth; query_pid=$current; result='UNKNOWN';
                observation_utc_before=$before; observation_utc_after=[DateTime]::UtcNow.ToString('O');
                error_category=[string]$_.CategoryInfo.Category; error_id=$_.FullyQualifiedErrorId }
            break
        }
    }
    return $records
}
function Product-Rows {
    if (-not (Test-Path -LiteralPath $productLog -PathType Leaf)) { return @() }
    $rows = @()
    # The product's CREATE_NEW writer shares READ only. Our reader must share
    # its already-open WRITE access, without acquiring any write permission.
    $file = [IO.FileStream]::new($productLog,[IO.FileMode]::Open,
        [IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
    $reader = [IO.StreamReader]::new($file,[Text.Encoding]::UTF8)
    try { $text = $reader.ReadToEnd() } finally { $reader.Dispose() }
    $lastNewline = $text.LastIndexOf("`n")
    if ($lastNewline -lt 0) { return @() }
    # An in-progress tail is UNKNOWN until a complete newline arrives. Never
    # hide a malformed completed record or rewrite the product evidence.
    foreach ($line in ($text.Substring(0,$lastNewline) -split "`n")) {
        if ([string]::IsNullOrWhiteSpace($line)) { throw 'product_empty_completed_jsonl' }
        $rows += ($line | ConvertFrom-Json -ErrorAction Stop)
    }
    return $rows
}
function Wait-ProductRow([string]$Type, [scriptblock]$Match, [int]$Seconds = 15,
                        [scriptblock]$Reject = $null) {
    $until = [DateTime]::UtcNow.AddSeconds($Seconds)
    do {
        foreach ($row in @(Product-Rows)) {
            if ($row.type -ceq $Type -and (& $Match $row)) { return $row }
            if ($Reject -and (& $Reject $row)) {
                throw "product_evidence_rejected:$($row.event):$($row.reason)"
            }
            if ($row.type -ceq 'shutdown') { throw "product_shutdown:$($row.reason)" }
        }
        if ($script:productPid -and -not (Get-Process -Id $script:productPid -ErrorAction SilentlyContinue)) {
            throw "product_exited_before:$Type"
        }
        Start-Sleep -Milliseconds 100 # bounded test-only orchestration, never a product event source
    } while ([DateTime]::UtcNow -lt $until)
    throw "product_evidence_timeout:$Type"
}
function Shell-Roots([string]$ExpectedPath) {
    $shell = New-Object -ComObject Shell.Application
    $expected = if ($ExpectedPath) { [IO.Path]::GetFullPath($ExpectedPath).TrimEnd('\') } else { $null }
    $found = @()
    foreach ($window in @($shell.Windows())) {
        try {
            $hwnd = [long]$window.HWND
            $windowPid = 0; $windowTid = 0
            if (-not [PaneBindMvpGuestInput]::ExplorerRoot($hwnd,[ref]$windowPid,[ref]$windowTid)) { continue }
            $process = Get-Process -Id $windowPid -ErrorAction Stop
            if (-not $process.Path -or
                -not [string]::Equals($process.Path, (Join-Path $env:WINDIR 'explorer.exe'),
                    [StringComparison]::OrdinalIgnoreCase)) { continue }
            $uri = [string]$window.LocationURL
            if (-not $uri.StartsWith('file:',[StringComparison]::OrdinalIgnoreCase)) { continue }
            $path = [IO.Path]::GetFullPath(([Uri]$uri).LocalPath).TrimEnd('\')
            if ($expected -and -not [string]::Equals($path,$expected,
                    [StringComparison]::OrdinalIgnoreCase)) { continue }
            $found += [pscustomobject]@{ hwnd=$hwnd; pid=$windowPid; tid=$windowTid; path=$path }
        } catch { }
    }
    return @($found)
}
function Exact-Root([int]$Member) {
    $target = $script:targets[$Member]
    $matches = @(Shell-Roots $target.path | Where-Object { $_.hwnd -eq $target.hwnd })
    if ($matches.Count -ne 1 -or $matches[0].pid -ne $target.pid -or
        $matches[0].tid -ne $target.tid) { throw "exact_root_changed:$Member" }
    return $matches[0]
}
function Assert-GuestHighEvidence($Facts,[int]$Member,$TokenBinding = $null) {
    # These are reported REAL native security facts, not launch permission or
    # boolean substitutes for the product's per-operation scope checks.
    $session = (Get-Process -Id $PID).SessionId
    if ($Facts.identity_policy -cne 'sandbox_same_sid_session_high' -or
        $Facts.sandbox_high_exception_applied -ne $true -or
        $Facts.caller_integrity -ne 12288 -or $Facts.target_integrity -ne 12288 -or
        $Facts.caller_medium -ne $false -or $Facts.target_medium -ne $false -or
        $Facts.caller_elevated -ne $true -or $Facts.target_elevated -ne $true -or
        $Facts.same_sid -ne $true -or $Facts.same_session -ne $true -or
        $Facts.caller_session -ne $session -or $Facts.target_session -ne $session -or
        $Facts.sandbox_exception_run_id -cne $RunId -or
        $Facts.sandbox_exception_member_slot -ne ($Member+1) -or
        $Facts.sandbox_exception_target_confirmation_generation -le 0 -or
        $Facts.sandbox_exception_token_consent_generation -le 0 -or
        $Facts.sandbox_exception_capability_generation -le 0) {
        throw "actual_guest_high_security_scope_incomplete:$Member"
    }
    if ($null -ne $TokenBinding -and
        ($Facts.sandbox_exception_token_consent_generation -ne $TokenBinding.consent_generation -or
         $Facts.sandbox_exception_capability_generation -ne $TokenBinding.capability_generation -or
         $Facts.sandbox_exception_target_confirmation_generation -ne
            $script:confirmationRows[$Member].confirmation_generation)) {
        throw "actual_guest_high_scope_generation_changed:$Member"
    }
}
function Get-GuestCommandBytes([string]$Line) {
    if ($Line -cnotmatch '^(|Y|S|Q)$') { throw 'guest_pipe_command_invalid' }
    return ,([Text.Encoding]::ASCII.GetBytes($Line + "`n"))
}
function Test-GuestPipeStartup($Startup,[int]$DriverPid,[string]$PipePath) {
    return $Startup.command_channel -ceq 'guest_named_pipe' -and
        $Startup.pipe_name -ceq $PipePath -and [int]$Startup.driver_pid -eq $DriverPid -and
        $Startup.process_has_console -is [bool] -and $Startup.process_has_console -eq $false
}
function Guest-Command([string]$Line) {
    if (-not [PaneBindMvpGuestInput]::InputDesktopReady()) {
        throw 'guest_input_desktop_not_active_for_command'
    }
    if ($script:usesGuestPipe) {
        if (-not $script:commandPipe -or -not $script:commandPipe.IsConnected) {
            throw 'guest_pipe_command_not_ready'
        }
        $bytes = Get-GuestCommandBytes $Line
        $script:commandPipe.Write($bytes,0,$bytes.Length)
        $script:commandPipe.Flush()
        Record 'guest_command' @{ text=$Line; source='automated_guest_driver';
            channel='guest_named_pipe'; pipe_name=$script:pipePath;
            client_name=$script:pipeName; product_pid=$script:productPid }
    } else {
        [PaneBindMvpGuestInput]::ConsoleLine($script:consoleInput,$Line)
        Record 'console_input' @{ text=$Line; source='automated_guest_driver'; product_pid=$script:productPid }
    }
}
function Find-Hit([long]$Hwnd, [int]$Wanted) {
    $snapshot = [PaneBindMvpGuestInput]::ObserveRoot($Hwnd)
    $rect = [PaneBindMvpGuestInput]::Rectangle($Hwnd,$false)
    $width = $rect[2]-$rect[0]; $height = $rect[3]-$rect[1]
    if ($snapshot.exact_explorer_root -ne $true -or $snapshot.dpi -le 0) {
        throw 'hit_probe_actual_root_or_dpi_unavailable'
    }
    # Retain the old physical probes and cover a bounded actual-DPI caption
    # band. Every candidate still requires the REAL HTCAPTION; HTCLIENT is
    # never treated as titlebar permission just because Explorer can drag it.
    $ys = if ($Wanted -eq 2) {
        @(@(10,17,24,31,40,48,56) + @(@(10,17,24,31,40,48,56,64) |
            ForEach-Object { [int][Math]::Round($_*$snapshot.dpi/96.0) })) |
            Where-Object { $_ -gt 0 -and $_ -lt $height } | Sort-Object -Unique
    } else {
        @(($height-3),($height-5),($height-8))
    }
    $probes = @()
    foreach ($y in $ys) {
        foreach ($fraction in @(0.2,0.3,0.4,0.5,0.6,0.7)) {
            $pointX = $rect[0]+[int]($width*$fraction)
            $pointY = $rect[1]+$y
            $hit = [PaneBindMvpGuestInput]::ObserveHit($Hwnd,$pointX,$pointY)
            $actualHit = [PaneBindMvpGuestInput]::ExactEndpointHit($hit,$Hwnd,$snapshot.pid,$snapshot.tid)
            $probes += @{cursor=@($pointX,$pointY); actual_hit=$actualHit; facts=$hit}
            if ($actualHit -eq $Wanted) {
                Record 'actual_hit_probe' @{ hwnd=$Hwnd; wanted_hit=$Wanted;
                    dpi=$snapshot.dpi; positioning=$rect; foreground=$snapshot.foreground;
                    matched=$true; probes=$probes }
                return @($pointX,$pointY)
            }
        }
    }
    Record 'actual_hit_probe' @{ hwnd=$Hwnd; wanted_hit=$Wanted;
        dpi=$snapshot.dpi; positioning=$rect; foreground=$snapshot.foreground;
        matched=$false; probes=$probes }
    # One crop of the exact test-created root's desktop region helps interpret
    # client-rendered captions without injecting a guessed drag. New file only;
    # this guest driver never captures the host or writes outside its output.
    $screenshot = Join-Path $outputDir "$RunId-caption-$Hwnd-$Wanted.png"
    try {
        Add-Type -AssemblyName System.Drawing
        $bitmap = [Drawing.Bitmap]::new($width,$height)
        $graphics = [Drawing.Graphics]::FromImage($bitmap)
        try {
            $graphics.CopyFromScreen($rect[0],$rect[1],0,0,$bitmap.Size)
            $imageStream = [IO.FileStream]::new($screenshot,[IO.FileMode]::CreateNew,
                [IO.FileAccess]::Write,[IO.FileShare]::Read)
            try { $bitmap.Save($imageStream,[Drawing.Imaging.ImageFormat]::Png) }
            finally { $imageStream.Dispose() }
        } finally { $graphics.Dispose(); $bitmap.Dispose() }
        Record 'caption_region_snapshot' @{ hwnd=$Hwnd; positioning=$rect;
            saved=$true; image=$screenshot;
            sha256=(Get-FileHash -LiteralPath $screenshot -Algorithm SHA256).Hash;
            scope='guest_exact_test_root_desktop_crop_not_host'; input_sent=$false }
    } catch {
        Record 'caption_region_snapshot' @{ hwnd=$Hwnd; saved=$false;
            error=$_.Exception.Message; input_sent=$false }
    }
    throw "exact_root_hit_unavailable:$Wanted"
}
function Send-MouseMove([int]$X,[int]$Y,[string]$Phase) {
    if (-not [PaneBindMvpGuestInput]::InputDesktopReady()) {
        throw "guest_input_desktop_not_active_for_mouse_move:$Phase"
    }
    [PaneBindMvpGuestInput]::MouseMove($X,$Y)
    Record 'test_input' @{ phase=$Phase; kind='mouse_move'; cursor=@($X,$Y); sent=$true;
        delivery_observed=$null }
}
function Assert-Test-DownTarget([long]$Hwnd,[int]$X,[int]$Y,[string]$Phase,[int]$Wanted = 2) {
    if (-not [PaneBindMvpGuestInput]::InputDesktopReady()) {
        throw "guest_input_desktop_not_active:$Phase"
    }
    $cursor = [PaneBindMvpGuestInput]::Cursor()
    $root = [PaneBindMvpGuestInput]::CursorRoot()
    $snapshot = [PaneBindMvpGuestInput]::ObserveRoot($Hwnd)
    $target = @($script:targets | Where-Object { $_.hwnd -eq $Hwnd })
    $hit = [PaneBindMvpGuestInput]::ObserveHit($Hwnd,$X,$Y)
    $actualHit = [PaneBindMvpGuestInput]::ExactEndpointHit($hit,$Hwnd,$snapshot.pid,$snapshot.tid)
    Record 'input_precondition' @{ phase=$Phase; cursor=$cursor; expected=@($X,$Y);
        hit_root=$root; exact_root=($root -eq $Hwnd); desktop_ready=$true;
        wanted_hit=$Wanted; actual_endpoint_hit=$actualHit; fresh_hit=$hit;
        root_pid=$snapshot.pid; root_tid=$snapshot.tid; foreground=$snapshot.foreground }
    if ($cursor[0] -ne $X -or $cursor[1] -ne $Y -or $root -ne $Hwnd -or
        $target.Count -ne 1 -or $snapshot.exact_explorer_root -ne $true -or
        $snapshot.pid -ne $target[0].pid -or $snapshot.tid -ne $target[0].tid -or
        $actualHit -ne $Wanted -or
        ($Phase -ceq 'gesture' -and $snapshot.foreground -ne $Hwnd)) {
        throw "cursor_or_hit_not_exact:$Phase"
    }
}
function Test-RectEqual($Left,$Right) {
    if ($null -eq $Left -or $null -eq $Right -or $Left.Count -ne 4 -or $Right.Count -ne 4) {
        return $false
    }
    for ($coordinate=0; $coordinate -lt 4; $coordinate++) {
        if ([long]$Left[$coordinate] -ne [long]$Right[$coordinate]) { return $false }
    }
    return $true
}
function Assert-ProductGeometry([string]$Phase,[bool]$RequireGlue = $false) {
    $prior = @(Product-Rows | Select-Object -Last 1)
    $watermark = if ($prior.Count) { [long]$prior[0].sequence } else { 0 }
    Guest-Command 'S'
    $status = Wait-ProductRow 'status' { param($row) $row.sequence -gt $watermark } 10
    if ($status.gesture_active -eq $true -or $status.glue_active -eq $true -or
        @($status.windows).Count -ne 3) { throw "product_status_not_idle:$Phase" }
    for ($member=0; $member -lt 3; $member++) {
        $root = Exact-Root $member
        $actual = [PaneBindMvpGuestInput]::Rectangle([long]$root.hwnd,$true)
        $reported = @($status.windows | Where-Object { $_.member -eq $member })
        if ($reported.Count -ne 1 -or -not (Test-RectEqual $actual $reported[0].visible)) {
            throw "product_geometry_not_refreshed:$Phase/$member"
        }
        Assert-GuestHighEvidence $reported[0] $member $script:bound[$member]
        Record 'actual_relation_snapshot' @{ phase=$Phase; member=$member;
            product_sequence=$status.sequence; visible=$actual;
            reported_visible=$reported[0].visible; exact=$true }
    }
    Record 'product_status' @{ phase=$Phase; product_sequence=$status.sequence;
        topology_ready=$status.topology_ready; relation_count=$status.relation_count;
        windows=$status.windows; actual_geometry_exact=$true }
    if ($RequireGlue -and ($status.topology_ready -ne $true -or
        [int]$status.relation_count -lt 2)) { throw "refreshed_ctrl_layout_not_ready:$Phase" }
    return $status
}
function Invoke-MagnetWaypoint([int]$Member,[long]$Generation,[string]$Phase,
                               $Down,$Initial,$WantedFree,$Expected,[bool]$Snapped,
                               [long]$AfterQuantum,[bool]$RequireNative = $true) {
    # Coordinates come from the original DOWN/window anchor. Reading each
    # native result verifies geometry; it never becomes a new intent anchor.
    $cursor=@([int]($Down[0]+$WantedFree[0]-$Initial[0]),
              [int]($Down[1]+$WantedFree[1]-$Initial[1]))
    Start-Sleep -Milliseconds 65 # bounded driver pacing, below the existing speed limit
    Send-MouseMove $cursor[0] $cursor[1] $Phase
    $receipt = Wait-ProductRow 'gesture_event' {
        param($row) $row.generation -eq $Generation -and $row.event -ceq 'writer' -and
            $row.quantum -gt $AfterQuantum -and ($row.native_attempted -eq $true -or
                (-not $RequireNative -and $row.native_attempted -eq $false -and
                    $row.reason -ceq 'already_exact'))
    } 4
    $root = Exact-Root $Member
    $actual=[PaneBindMvpGuestInput]::Rectangle([long]$root.hwnd,$true)
    $nativeOrConfirmedNoop = ($receipt.native_attempted -eq $true -and
        $receipt.native_succeeded -eq $true) -or (-not $RequireNative -and
        $receipt.native_attempted -eq $false -and $receipt.reason -ceq 'already_exact')
    $exact = $nativeOrConfirmedNoop -and
        $receipt.geometry_exact -eq $true -and $receipt.post_context_exact -eq $true -and
        $receipt.snapped -eq $Snapped -and
        (Test-RectEqual $receipt.target_visible $Expected) -and
        (Test-RectEqual $receipt.actual_visible $Expected) -and
        (Test-RectEqual $actual $Expected)
    Record 'magnet_waypoint' @{ phase=$Phase; member=$Member; generation=$Generation;
        original_down=$Down; original_visible=$Initial; cursor=$cursor;
        free_visible=$WantedFree; expected_visible=$Expected; actual_visible=$actual;
        expected_snapped=$Snapped; observed_snapped=$receipt.snapped;
        native_required=$RequireNative; native_attempted=$receipt.native_attempted;
        native_succeeded=$receipt.native_succeeded; receipt_reason=$receipt.reason;
        product_sequence=$receipt.sequence; quantum=$receipt.quantum;
        raw_sequence=$receipt.raw_sequence; exact=$exact }
    if (-not $exact) { throw "magnet_waypoint_not_exact:$Member/$Phase" }
    return [long]$receipt.quantum
}
function Invoke-Drag([int]$Member,[string]$Kind,[int]$DeltaX,[int]$DeltaY,
                     [string]$ExpectedRoute) {
    $root = Exact-Root $Member
    $hwnd = [long]$root.hwnd
    $wantedHit = if ($Kind -ceq 'resize') { 15 } else { 2 } # HTBOTTOM / HTCAPTION
    # Test-created exact root activation precedes caption discovery. This is
    # guest driver setup, never a product foreground/focus/capture operation.
    $activation = [PaneBindMvpGuestInput]::Foreground($hwnd)
    Record 'test_foreground' @{ member=$Member; hwnd=$hwnd; facts=$activation;
        phase='before_hit_probe'; source='guest_test_created_exact_root' }
    $point = Find-Hit $hwnd $wantedHit
    # A SetForegroundWindow transition may complete after its immediate
    # readback. Use the actual post-probe state, not the stale API receipt,
    # before considering a separate test-only activation click.
    $afterProbe = [PaneBindMvpGuestInput]::ObserveRoot($hwnd)
    Record 'test_foreground_readback' @{ member=$Member; hwnd=$hwnd;
        phase='after_hit_probe'; foreground=$afterProbe.foreground;
        exact_after=($afterProbe.foreground -eq $hwnd) }
    if ($afterProbe.pid -ne $root.pid -or $afterProbe.tid -ne $root.tid) {
        throw "foreground_identity_changed_after_hit_probe:$Member"
    }
    if ($afterProbe.foreground -ne $hwnd) {
        Send-MouseMove $point[0] $point[1] 'activate'
        Assert-Test-DownTarget $hwnd $point[0] $point[1] 'activate' $wantedHit
        $script:pressed=$true
        try {
            [PaneBindMvpGuestInput]::MouseButton($true)
            Start-Sleep -Milliseconds 50
        } finally {
            $activationUpRoot = $null
            try { $activationUpRoot = [PaneBindMvpGuestInput]::CursorRoot() } catch { }
            $released = $false
            try {
                [PaneBindMvpGuestInput]::MouseButton($false)
                $released = $true
            }
            finally {
                if ($released) { $script:pressed=$false }
                Record 'activation_up' @{ hit_root=$activationUpRoot; source_hwnd=$hwnd;
                    cleanup_send_succeeded=$released;
                    left_held_after=[PaneBindMvpGuestInput]::LeftDown();
                    product_delivery_claimed=$false }
            }
        }
        Start-Sleep -Milliseconds 100
    }
    $foregroundReadback = [PaneBindMvpGuestInput]::ObserveRoot($hwnd)
    Record 'test_foreground_readback' @{ member=$Member; hwnd=$hwnd;
        foreground=$foregroundReadback.foreground; pid=$foregroundReadback.pid;
        tid=$foregroundReadback.tid; exact_after=($foregroundReadback.foreground -eq $hwnd) }
    if ($foregroundReadback.foreground -ne $hwnd -or
        $foregroundReadback.pid -ne $root.pid -or $foregroundReadback.tid -ne $root.tid) {
        throw "foreground_not_exact:$Member"
    }
    $before = [PaneBindMvpGuestInput]::Rectangle($hwnd,$true)
    $beforeGroup = @()
    if ($Kind -ceq 'ctrl') {
        for ($index=0; $index -lt 3; $index++) {
            $exact = Exact-Root $index
            $beforeGroup += ,([PaneBindMvpGuestInput]::Rectangle([long]$exact.hwnd,$true))
        }
    }
    $prior = @(Product-Rows | Select-Object -Last 1)
    $watermark = if ($prior.Count) { [long]$prior[0].sequence } else { 0 }
    Record 'gesture_begin' @{ member=$Member; hwnd=$hwnd; pid=$root.pid; kind=$Kind;
        expected_route=$ExpectedRoute; before_visible=$before; down=$point;
        delta=@($DeltaX,$DeltaY); product_watermark=$watermark }
    try {
        if ($Kind -ceq 'ctrl') {
            $script:ctrlPressed=$true
            [PaneBindMvpGuestInput]::Control($true)
        }
        Send-MouseMove $point[0] $point[1] 'anchor'
        Assert-Test-DownTarget $hwnd $point[0] $point[1] 'gesture' $wantedHit
        $script:pressed=$true
        [PaneBindMvpGuestInput]::MouseButton($true)
        # A fixed delay does not prove that the receiver/owner accepted the
        # original anchor. Wait for its REAL candidate before requesting a
        # native Move/Resize; an observed rejection stops this test immediately.
        $downCandidate = Wait-ProductRow 'gesture_event' {
            param($row) $row.sequence -gt $watermark -and $row.event -ceq 'down' -and
                $row.source_member -eq $Member -and $row.reason -ceq 'candidate_only' -and
                $row.generation -gt 0 -and $row.raw_sequence -gt 0
        } 4 {
            param($row) $row.type -ceq 'gesture_event' -and
                $row.sequence -gt $watermark -and $row.source_member -eq $Member -and
                $row.event -ceq 'rejected'
        }
        if (@($downCandidate.cursor).Count -ne 2 -or
            $downCandidate.cursor[0] -ne $point[0] -or $downCandidate.cursor[1] -ne $point[1] -or
            -not (Test-RectEqual $downCandidate.initial_visible $before)) {
            throw 'observed_down_anchor_differs_from_actual_test_down'
        }
        Record 'observed_down' @{ member=$Member; product_sequence=$downCandidate.sequence;
            generation=$downCandidate.generation; raw_sequence=$downCandidate.raw_sequence;
            original_cursor=$downCandidate.cursor; original_visible=$downCandidate.initial_visible;
            input_source='actual_product_candidate_not_driver_delay' }
        if (-not [PaneBindMvpGuestInput]::LeftDown()) { throw 'physical_left_not_held_after_down' }
        Send-MouseMove ($point[0]+[Math]::Sign($DeltaX)*8) ($point[1]+[Math]::Sign($DeltaY)*8) 'native_start'
        $start = Wait-ProductRow 'gesture_event' {
            param($row) $row.sequence -gt $watermark -and $row.event -ceq 'start' -and
                $row.source_member -eq $Member
        } 4
        if ($start.generation -ne $downCandidate.generation) { throw 'start_generation_differs_from_real_down' }
        if ($start.route -cne $ExpectedRoute) { throw "route_mismatch:$($start.route)" }
        if (-not $script:gestureGenerations.Add([long]$start.generation)) {
            throw 'gesture_generation_reused'
        }
        Record 'observed_start' @{ member=$Member; product_sequence=$start.sequence;
            generation=$start.generation; route=$start.route }
        if ($Kind -ceq 'plain') {
            $captureReady=Wait-ProductRow 'gesture_event' {
                param($row) $row.generation -eq $start.generation -and
                    $row.event -ceq 'resource' -and $row.reason -ceq 'capture_ready_observed'
            } 4
            $capture=$captureReady.shield_capture
            $association=$capture.association
            if (-not $capture -or $capture.attempted -ne $true -or
                [long]$capture.previous -ne 0 -or [long]$capture.actual_after -ne
                    [long]$captureReady.overlay -or [long]$captureReady.overlay -eq 0 -or
                [long]$capture.owner_thread_id -le 0 -or [int]$capture.failure -ne 0 -or
                [long]$capture.foreground_before -ne $hwnd -or
                [long]$capture.foreground_after -ne $hwnd -or
                $capture.source_before.available -ne $true -or
                [long]$capture.source_before.capture -ne 0 -or
                [long]$capture.source_before.move_size -ne 0 -or
                $capture.owner_before.available -ne $true -or
                [long]$capture.owner_before.capture -ne 0 -or
                [long]$capture.owner_after.capture -ne [long]$captureReady.overlay) {
                throw "post_end_own_capture_not_exact:$Member"
            }
            if (-not $association -or $association.attach_attempted -ne $true -or
                $association.attach_completed -ne $true -or
                $association.attach_succeeded -ne $true -or $association.desktop_verified -ne $true -or
                [long]$association.generation -ne [long]$start.generation -or
                [long]$association.source -ne $hwnd -or
                [long]$association.source_thread_id -ne [long]$root.tid -or
                [long]$association.owner_thread_id -ne [long]$capture.owner_thread_id -or
                $association.detach_attempted -ne $false) {
                throw "post_end_input_association_not_exact:$Member"
            }
            $handoff = Wait-ProductRow 'gesture_event' {
                param($row) $row.sequence -gt $start.sequence -and $row.event -ceq 'handoff' -and
                    $row.generation -eq $start.generation -and $row.succeeded -eq $true
            } 4
            Record 'observed_handoff' @{ generation=$handoff.generation;
                product_sequence=$handoff.sequence; actual_visible=$handoff.actual_visible;
                capture_product_sequence=$captureReady.sequence;
                shield_capture=$captureReady.shield_capture }
        }
        if ($Kind -ceq 'plain') {
            $targetMember = if ($Member -eq 0) { 1 } else { 0 }
            $targetRoot=Exact-Root $targetMember
            $target=[PaneBindMvpGuestInput]::Rectangle([long]$targetRoot.hwnd,$true)
            $width=$before[2]-$before[0]; $height=$before[3]-$before[1]
            $snapLeft = if ($Member -eq 0) { $target[0]-$width }
                elseif ($Member -eq 1) { $target[2] } else { $target[0] }
            $snapTop = if ($Member -eq 2) { $target[3] } else { $target[1] }
            $snap=@($snapLeft,$snapTop,($snapLeft+$width),($snapTop+$height))
            $outX=if($Member -eq 1){1}else{-1}
            $outY=if($Member -eq 2){1}else{-1}
            $quantum=0L
            foreach ($waypoint in @(
                @{phase='free';distance=48;snapped=$false},
                @{phase='xy_snap';distance=4;snapped=$true},
                @{phase='hold_outside_attraction';distance=12;snapped=$true;allow_exact_noop=$true},
                @{phase='detach_beyond_release';distance=48;snapped=$false},
                @{phase='xy_resnap';distance=4;snapped=$true})) {
                $x=$snapLeft+$outX*$waypoint.distance
                $y=$snapTop+$outY*$waypoint.distance
                $free=@($x,$y,($x+$width),($y+$height))
                $expected=if($waypoint.snapped){$snap}else{$free}
                $quantum=Invoke-MagnetWaypoint $Member $start.generation $waypoint.phase `
                    $point $before $free $expected $waypoint.snapped $quantum `
                    (-not $waypoint.allow_exact_noop)
            }
            Record 'xy_alignment' @{ member=$Member; target_member=$targetMember;
                generation=$start.generation; source_visible=$snap; target_visible=$target;
                horizontal_edge_exact=$true; vertical_edge_exact=$true;
                verified_by='native_rect_and_exact_writer_receipts' }
        } else {
            foreach ($step in @(0.25,0.50,0.75,1.0)) {
                Send-MouseMove ($point[0]+[int]($DeltaX*$step)) ($point[1]+[int]($DeltaY*$step)) 'continuation'
                Start-Sleep -Milliseconds 65
            }
        }
    } finally {
        # This is test-only physical cleanup, not a claim that product observed
        # Raw/legacy UP or retired its writer. Evidence checks below decide that.
        if ($script:pressed) {
            $upRoot = $null; $upCursor = $null; $released = $false
            try { $upRoot = [PaneBindMvpGuestInput]::CursorRoot() } catch { }
            try { $upCursor = [PaneBindMvpGuestInput]::Cursor() } catch { }
            try {
                [PaneBindMvpGuestInput]::MouseButton($false)
                $released = $true
            } finally {
                if ($released) { $script:pressed=$false }
                Record 'test_input' @{ kind='left_up'; sent=$released; cursor=$upCursor;
                    hit_root_before=$upRoot; left_held_after=[PaneBindMvpGuestInput]::LeftDown();
                    product_delivery_observed=$null }
            }
        }
        if ($script:ctrlPressed) {
            $released = $false
            try {
                [PaneBindMvpGuestInput]::Control($false)
                $released = $true
            } finally {
                if ($released) { $script:ctrlPressed=$false }
                Record 'test_input' @{ kind='ctrl_up'; sent=$released; delivery_observed=$null }
            }
        }
    }
    $end = Wait-ProductRow 'gesture_event' {
        param($row) $row.sequence -gt $start.sequence -and $row.event -ceq 'native_end' -and
            $row.generation -eq $start.generation
    } 4
    if ($Kind -ceq 'plain') {
        $null = Wait-ProductRow 'gesture_event' {
            param($row) $row.event -ceq 'raw_up' -and $row.generation -eq $start.generation -and
                $row.reason -ceq 'receiver_packet_observed'
        } 4
        $null = Wait-ProductRow 'gesture_event' {
            param($row) $row.event -ceq 'legacy_up' -and $row.generation -eq $start.generation -and
                $row.reason -ceq 'overlay_message_observed'
        } 4
        $gone = Wait-ProductRow 'gesture_event' {
            param($row) $row.event -ceq 'isolation_gone' -and $row.generation -eq $start.generation -and
                $row.reason -ceq 'normal_up'
        } 4
        if ($gone.overlay_destroyed -ne $true -or $gone.hotkey_unregistered -ne $true -or
            $gone.association_detached -ne $true) {
            throw "plain_product_isolation_cleanup_incomplete:$Member"
        }
        $captureReleased=Wait-ProductRow 'gesture_event' {
            param($row) $row.event -ceq 'resource' -and
                $row.reason -ceq 'capture_released_observed' -and
                $row.generation -eq $start.generation
        } 4
        $released=$captureReleased.shield_capture
        if (-not $released -or $released.release_attempted -ne $true -or
            $released.release_completed -ne $true -or
            $released.release_succeeded -ne $true -or
            [long]$released.release_before -ne [long]$captureReady.overlay -or
            [long]$released.release_after -ne 0 -or $released.own_release_message -ne $true) {
            throw "normal_up_capture_release_not_observed:$Member"
        }
        $detached=Wait-ProductRow 'gesture_event' {
            param($row) $row.event -ceq 'resource' -and
                $row.reason -ceq 'input_detached_observed' -and
                $row.generation -eq $start.generation
        } 4
        $pair=$detached.shield_capture.association
        if (-not $pair -or $pair.attach_succeeded -ne $true -or
            $pair.detach_attempted -ne $true -or $pair.detach_completed -ne $true -or
            $pair.detach_succeeded -ne $true -or
            [long]$pair.source -ne $hwnd -or
            [long]$pair.owner_thread_id -ne [long]$capture.owner_thread_id -or
            [long]$pair.detach_before_qpc -lt [long]$pair.attach_after_qpc) {
            throw "normal_up_input_detach_not_observed:$Member"
        }
        Record 'observed_normal_capture_release' @{ generation=$start.generation;
            product_sequence=$captureReleased.sequence; shield_capture=$released;
            normal_up=$true; overlay_destroyed=$gone.overlay_destroyed }
    }
    $after = [PaneBindMvpGuestInput]::Rectangle($hwnd,$true)
    $related = @(Product-Rows | Where-Object {
        $_.type -ceq 'gesture_event' -and $_.generation -eq $start.generation
    })
    $writers = @($related | Where-Object { $_.event -ceq 'writer' -and $_.native_attempted -eq $true })
    $snapped = @($writers | Where-Object { $_.snapped -eq $true -and $_.geometry_exact -eq $true })
    $free = @($writers | Where-Object { $_.snapped -eq $false -and $_.geometry_exact -eq $true })
    $rawUp = @($related | Where-Object { $_.event -ceq 'raw_up' })
    $legacyUp = @($related | Where-Object { $_.event -ceq 'legacy_up' })
    Record 'gesture_result' @{ member=$Member; generation=$start.generation;
        route=$start.route; native_end_sequence=$end.sequence; before_visible=$before;
        after_visible=$after; writer_native_attempts=$writers.Count;
        snapped_exact_writes=$snapped.Count; free_exact_writes=$free.Count;
        raw_up_observed=($rawUp.Count -gt 0); legacy_up_observed=($legacyUp.Count -gt 0) }
    if ($Kind -ceq 'plain' -and ($writers.Count -lt 1 -or $rawUp.Count -lt 1)) {
        throw "plain_writer_or_raw_up_missing:$Member"
    }
    if ($Kind -ceq 'plain' -and ($snapped.Count -lt 1 -or $free.Count -lt 1)) {
        throw "plain_free_or_snap_evidence_missing:$Member"
    }
    if ($Kind -cne 'plain' -and $writers.Count -ne 0) {
        throw "non_plain_route_used_source_writer:$Member"
    }
    $duplicateQuanta=@($writers | Group-Object quantum | Where-Object { $_.Count -gt 1 })
    if ($duplicateQuanta.Count -ne 0) { throw "multiple_native_receipts_per_quantum:$Member" }
    if ($Kind -ceq 'plain') {
        $up=@($rawUp | Where-Object { [long]$_.raw_up_qpc -gt 0 })
        if ($up.Count -ne 1) { throw "matching_raw_up_qpc_missing_or_ambiguous:$Member" }
        $badClaims=@($writers | Where-Object {
            [long]$_.native_attempt_qpc -le 0 -or
                [long]$_.native_attempt_qpc -gt [long]$up[0].raw_up_qpc
        })
        Record 'native_claim_up_boundary' @{ member=$Member; generation=$start.generation;
            raw_up_qpc=$up[0].raw_up_qpc;
            native_attempt_qpc=@($writers | ForEach-Object { $_.native_attempt_qpc });
            claims_after_reliable_up=$badClaims.Count;
            definition='final_native_attempt_claim_not_cpu_instruction_or_jsonl_order' }
        if ($badClaims.Count -ne 0) { throw "native_attempt_claim_after_raw_up:$Member" }
        if (@($related | Where-Object {
            $_.event -ceq 'resource' -and ($_.reason -ceq 'capture_lost_observed' -or
                $_.reason -ceq 'capture_failure_observed')
        }).Count -ne 0) { throw "normal_move_capture_failure:$Member" }
    }
    if ($Kind -cne 'plain' -and @($related | Where-Object {
        $_.event -ceq 'isolation_ready' -or $_.event -ceq 'cancel' -or
            ($_.event -ceq 'resource' -and $_.reason -ceq 'capture_ready_observed')
    }).Count -ne 0) { throw "non_plain_route_used_isolation_or_cancel:$Member" }
    if ($Kind -cne 'plain' -and $before[0] -eq $after[0] -and $before[1] -eq $after[1] -and
        $before[2] -eq $after[2] -and $before[3] -eq $after[3]) {
        throw "gesture_geometry_unchanged:$Kind/$Member"
    }
    if ($Kind -ceq 'plain') {
        # After BOTH real UP observations and NormalUp removal, an additional
        # cursor event must not create another writer receipt or alter source.
        $lastWriterQuantum=if($writers.Count){[long]($writers | Select-Object -Last 1).quantum}else{0L}
        Send-MouseMove ($point[0]+7) ($point[1]+5) 'post_up_no_write_probe'
        Start-Sleep -Milliseconds 180
        $late=@(Product-Rows | Where-Object {
            $_.type -ceq 'gesture_event' -and $_.generation -eq $start.generation -and
            $_.event -ceq 'writer' -and $_.native_attempted -eq $true -and
            $_.quantum -gt $lastWriterQuantum
        })
        $still=[PaneBindMvpGuestInput]::Rectangle($hwnd,$true)
        $unchanged=(Test-RectEqual $after $still)
        Record 'post_up_no_write' @{ member=$Member; generation=$start.generation;
            after_normal_up_visible=$after; observed_visible=$still;
            additional_native_receipts=$late.Count; geometry_unchanged=$unchanged;
            scope='bounded_cursor_probe_after_normal_cleanup_not_cross_queue_line_order' }
        if ($late.Count -ne 0 -or -not $unchanged) { throw "placement_after_normal_up:$Member" }
    }
    if ($Kind -ceq 'resize' -and ($after[0] -ne $before[0] -or
        $after[1] -ne $before[1] -or $after[2] -ne $before[2] -or
        $after[3] -le $before[3])) { throw 'native_bottom_resize_not_observed' }
    if ($Kind -ceq 'ctrl') {
        $dx = $after[0]-$before[0]; $dy = $after[1]-$before[1]
        if ($dx -eq 0 -and $dy -eq 0) { throw 'ctrl_leader_did_not_move' }
        for ($index=0; $index -lt 3; $index++) {
            if ($index -eq $Member) { continue }
            $exact = Exact-Root $index
            $follower = [PaneBindMvpGuestInput]::Rectangle([long]$exact.hwnd,$true)
            $followerDx = $follower[0]-$beforeGroup[$index][0]
            $followerDy = $follower[1]-$beforeGroup[$index][1]
            Record 'ctrl_follower' @{ member=$index; source_member=$Member;
                before_visible=$beforeGroup[$index]; after_visible=$follower;
                leader_delta=@($dx,$dy); follower_delta=@($followerDx,$followerDy) }
            if ($followerDx -ne $dx -or $followerDy -ne $dy) {
                throw "ctrl_follower_delta_not_exact:$index"
            }
        }
    }
}

$stream = [IO.FileStream]::new($driverLog,[IO.FileMode]::CreateNew,
    [IO.FileAccess]::Write,[IO.FileShare]::Read)
$script:writer = [IO.StreamWriter]::new($stream,[Text.UTF8Encoding]::new($false))
try {
    Record 'startup' @{ implementation_sha=$manifest.source_commit;
        initial_desktop_snapshot=$initialDesktopSnapshot;
        guest_test_only=$true; product=$product; product_log=$productLog;
        input_source='automated_guest_driver'; live_validation=$false;
        requested_test_exception=$manifest.explorer_test_exception;
        requested_exception_run_id=$RunId; product_authority_granted=$false }
    Record 'guest_entry_mode' @{ mode=$Mode; automatic_binding=$true;
        automatic_gestures_planned=($Mode -ceq 'AutomatedValidation'); human_uat='NOT_RUN' }
    $driverConsole = [PaneBindMvpGuestInput]::DriverConsoleWindow()
    Record 'guest_command_transport' @{ channel=$(if($script:usesGuestPipe){'guest_named_pipe'}else{'console'});
        pipe_name=$(if($script:usesGuestPipe){$script:pipePath}else{$null});
        client_name=$(if($script:usesGuestPipe){$script:pipeName}else{$null});
        driver_pid=$PID; driver_console_hwnd=$driverConsole;
        driver_console_absent=($driverConsole -eq 0);
        scope='this_driver_only_not_all_guest_consoles' }
    if ($script:usesGuestPipe -and $driverConsole -ne 0) {
        throw 'no_console_driver_creation_not_observed'
    }
    Record 'guest_driver_process_identity' @{ caller_pid=$PID;
        observed_chain=@(Read-GuestProcessIdentityChain $PID 2);
        scope='current_driver_and_parent_runner_read_only'; operation_authority_granted=$false }
    $callerDpi = [PaneBindMvpGuestInput]::EstablishCallerPhysicalCoordinates()
    Record 'test_driver_dpi_context' @{ thread_id=$callerDpi.thread_id;
        previous_context=$callerDpi.previous_context;
        current_context=$callerDpi.current_context; win32_error=$callerDpi.win32_error;
        set_returned_previous=$callerDpi.set_returned_previous;
        current_per_monitor_v2=$callerDpi.current_per_monitor_v2;
        established=$callerDpi.established; coordinate_space='physical_pixels' }
    if (-not $callerDpi.established) { throw 'guest_caller_pmv2_not_established_no_geometry_or_input' }
    if ([PaneBindMvpGuestInput]::WorkArea()[2] -lt 800 -or
        [PaneBindMvpGuestInput]::WorkArea()[3] -lt 600) { throw 'guest_desktop_too_small' }
    $args = "--sandbox-run-id $RunId --evidence-log `"$productLog`" --guest-automated-driver --sandbox-high-integrity-test-exception"
    if ($script:usesGuestPipe) { $args += " --guest-command-pipe --guest-driver-pid $PID" }
    $creation = [PaneBindMvpGuestInput]::StartProduct($product,$args,$script:usesGuestPipe)
    $script:productPid = [int]$creation.pid
    $script:productStarted = $creation.started
    Record 'product_creation_parameters' @{ mode=$Mode; application=$product; arguments=$args;
        facts=$creation; command_channel=$(if($script:usesGuestPipe){'guest_named_pipe'}else{'console'});
        creation_scope='own_new_process_only_not_foreign_hwnd' }
    if ($script:usesGuestPipe) {
        $script:commandPipe = [IO.Pipes.NamedPipeClientStream]::new('.',
            $script:pipeName,[IO.Pipes.PipeDirection]::Out,[IO.Pipes.PipeOptions]::None)
        $script:commandPipe.Connect(20000)
        $productStartup = Wait-ProductRow 'startup' { param($row) $row.run_id -ceq $RunId } 10
        if (-not (Test-GuestPipeStartup $productStartup $PID $script:pipePath)) {
            throw 'actual_product_pipe_entry_not_observed'
        }
        Record 'product_started' @{ pid=$script:productPid; exact_console_attached=$false;
            command_channel='guest_named_pipe'; pipe_name=$script:pipePath;
            client_name=$script:pipeName;
            product_console_absent=$true; actual_product_startup_sequence=$productStartup.sequence }
    } else {
        $originalConsole = [PaneBindMvpGuestInput]::ObserveAttachedConsole(0)
        Record 'console_attachment_identity' @{ phase='before_attach_product'; facts=$originalConsole;
            ownership='observed_original_console_not_hide_authority' }
        $script:consoleInput = [PaneBindMvpGuestInput]::AttachInput($script:productPid)
        $script:attachedConsole = [PaneBindMvpGuestInput]::ObserveAttachedConsole($script:productPid)
        Record 'console_attachment_identity' @{ phase='after_attach_product'; facts=$script:attachedConsole;
            ownership='actual_current_console_clients_not_old_failure_foreground_attribution' }
        Record 'product_started' @{ pid=$script:productPid; exact_console_attached=$true }
    }
    $area = [PaneBindMvpGuestInput]::WorkArea()
    $width=[int]([Math]::Min(420,($area[2]-300)/2))
    $height=[int]([Math]::Min(280,($area[3]-300)/2))
    $left=$area[0]+120; $top=$area[1]+140
    $places=@(@($left,$top),@(($left+$width),$top),@($left,($top+$height)))
    for ($member=0; $member -lt 3; $member++) {
        $prompt = Wait-ProductRow 'target_prompt' {
            param($row) $row.member -eq $member
        } 20
        $directory = [string]$prompt.directory
        if (-not $directory.StartsWith([IO.Path]::GetTempPath(),
                [StringComparison]::OrdinalIgnoreCase) -or
            -not (Test-Path -LiteralPath $directory -PathType Container) -or
            (Get-ChildItem -LiteralPath $directory -Force).Count -ne 0) {
            throw "unexpected_target_directory:$member"
        }
        $baseline = @(Shell-Roots $null | ForEach-Object { $_.hwnd })
        $explorerExe = Join-Path $env:WINDIR 'explorer.exe'
        Start-Process -FilePath $explorerExe -ArgumentList ('/n,"' + $directory + '"')
        $newRoot = $null
        $until = [DateTime]::UtcNow.AddSeconds(15)
        do {
            $candidates = @(Shell-Roots $directory | Where-Object {
                $baseline -notcontains $_.hwnd
            })
            if ($candidates.Count -eq 1) { $newRoot=$candidates[0]; break }
            if ($candidates.Count -gt 1) { throw "ambiguous_new_explorer_root:$member" }
            Start-Sleep -Milliseconds 100
        } while ([DateTime]::UtcNow -lt $until)
        if (-not $newRoot) { throw "new_explorer_root_not_observed:$member" }
        $script:targets += $newRoot
        Record 'target_created' @{ member=$member; directory=$directory;
            hwnd=$newRoot.hwnd; pid=$newRoot.pid; tid=$newRoot.tid;
            baseline_count=$baseline.Count; exact_new_root=$true }
        $targetSnapshot = [PaneBindMvpGuestInput]::ObserveRoot([long]$newRoot.hwnd)
        Record 'target_before_confirmation' @{ member=$member; snapshot=$targetSnapshot }
        if ($targetSnapshot.exact_explorer_root -ne $true -or
            $targetSnapshot.pid -ne $newRoot.pid -or $targetSnapshot.tid -ne $newRoot.tid) {
            throw "exact_new_root_identity_changed_before_confirmation:$member"
        }
        if ($member -eq 0) {
            if ($targetSnapshot.dpi -le 0) { throw 'test_layout_actual_dpi_unavailable' }
            # The old 420 physical pixels became only 210 logical pixels in
            # the observed DPI=192 guest: tabs/system buttons can consume the
            # entire caption band. Size this test-created layout from actual
            # DPI and available area, then still REQUIRE a real HTCAPTION.
            $width=[int]([Math]::Min([Math]::Round(520*$targetSnapshot.dpi/96.0),
                ($area[2]-300)/2))
            $height=[int]([Math]::Min([Math]::Round(280*$targetSnapshot.dpi/96.0),
                ($area[3]-300)/2))
            if ($width -le 0 -or $height -le 0) { throw 'test_layout_no_available_area' }
            $places=@(@($left,$top),@(($left+$width),$top),@($left,($top+$height)))
            Record 'test_layout_parameters' @{ actual_dpi=$targetSnapshot.dpi;
                virtual_screen=$area; coordinate_space='actual_caller_physical_pixels';
                requested_physical_width=$width; requested_physical_height=$height;
                desired_logical_width=520; desired_logical_height=280;
                source='exact_new_root_before_confirmation'; caption_authority_granted=$false }
        }
        # The target token is issued only for a normal window with room for a
        # safe translation. Prepare the already-authorized test-created root
        # BEFORE confirmation, not after all three tokens have been requested.
        # This is guest driver setup, never product authority or a mutation of
        # a baseline/user frame. Recheck exact Shell path and PID/TID first.
        $root = Exact-Root $member
        if ($baseline -contains $root.hwnd) {
            throw "preexisting_root_before_test_layout:$member"
        }
        [PaneBindMvpGuestInput]::Place([long]$root.hwnd,$places[$member][0],
            $places[$member][1],$width,$height)
        $preparedSnapshot = [PaneBindMvpGuestInput]::ObserveRoot([long]$root.hwnd)
        Record 'test_layout' @{ member=$member; hwnd=$root.hwnd;
            visible=$preparedSnapshot.visible_rect; snapshot=$preparedSnapshot;
            source='guest_test_preconfirmation' }
        if ($preparedSnapshot.exact_explorer_root -ne $true -or
            $preparedSnapshot.pid -ne $root.pid -or $preparedSnapshot.tid -ne $root.tid -or
            $preparedSnapshot.minimized -ne $false -or $preparedSnapshot.maximized -ne $false -or
            $preparedSnapshot.visible -ne $true -or $preparedSnapshot.outer_rect_available -ne $true -or
            $preparedSnapshot.visible_rect_available -ne $true) {
            throw "test_created_normal_layout_not_observed:$member"
        }
        Guest-Command ''
        $confirmed = Wait-ProductRow 'target_confirmed' {
            param($row) $row.member -eq $member
        } 15
        if ($confirmed.baseline_exclusion_complete -ne $true -or
            $confirmed.unique_new_target -ne $true -or
            $confirmed.exact_location -ne $true) {
            throw "product_target_facts_incomplete:$member"
        }
        Assert-GuestHighEvidence $confirmed $member
        if ($confirmed.sandbox_exception_target_confirmation_generation -ne
                $confirmed.confirmation_generation -or
            $confirmed.sandbox_exception_token_consent_generation -ne $confirmed.token_generation) {
            throw "target_confirmation_scope_generation_mismatch:$member"
        }
        $script:confirmationRows += $confirmed
    }
    # Align actual DWM-visible edges, not requested outer-window coordinates.
    # This is test-created, pre-consent layout preparation only; the product
    # remains the sole writer after the session starts.
    $base = [PaneBindMvpGuestInput]::Rectangle([long]$script:targets[0].hwnd,$true)
    foreach ($member in @(1,2)) {
        $root = Exact-Root $member
        $visible = [PaneBindMvpGuestInput]::Rectangle([long]$root.hwnd,$true)
        $outer = [PaneBindMvpGuestInput]::Rectangle([long]$root.hwnd,$false)
        $desiredLeft = if ($member -eq 1) { $base[2] } else { $base[0] }
        $desiredTop = if ($member -eq 1) { $base[1] } else { $base[3] }
        [PaneBindMvpGuestInput]::Place([long]$root.hwnd,
            ($outer[0]+$desiredLeft-$visible[0]),($outer[1]+$desiredTop-$visible[1]),
            ($outer[2]-$outer[0]),($outer[3]-$outer[1]))
        $aligned = [PaneBindMvpGuestInput]::Rectangle([long]$root.hwnd,$true)
        Record 'test_layout_visible_alignment' @{ member=$member;
            desired_left=$desiredLeft; desired_top=$desiredTop; actual=$aligned;
            exact=($aligned[0] -eq $desiredLeft -and $aligned[1] -eq $desiredTop) }
        if ($aligned[0] -ne $desiredLeft -or $aligned[1] -ne $desiredTop) {
            throw "test_layout_alignment_failed:$member"
        }
    }
    Guest-Command 'Y'
    $consent = Wait-ProductRow 'product_consent' { param($row) $row.confirmed -eq $true } 15
    if ($consent.input_source -cne 'automated_guest_driver') {
        throw 'product_consent_source_mislabeled'
    }
    for ($member=0; $member -lt 3; $member++) {
        $binding = Wait-ProductRow 'binding' { param($row) $row.member -eq $member } 15
        $root = Exact-Root $member
        if ([long]$binding.hwnd -ne $root.hwnd -or
            [int]$binding.pid -ne $root.pid -or [int]$binding.tid -ne $root.tid) {
            throw "binding_differs_from_new_exact_root:$member"
        }
        Assert-GuestHighEvidence $binding $member $binding
        $script:bound += $binding
        Record 'binding_verified' @{ member=$member; hwnd=$root.hwnd;
            pid=$root.pid; tid=$root.tid; product_window_id=$binding.window_id;
            exact_new_root=$true; identity_policy=$binding.identity_policy;
            caller_integrity=$binding.caller_integrity; target_integrity=$binding.target_integrity;
            caller_medium=$binding.caller_medium; target_medium=$binding.target_medium;
            caller_elevated=$binding.caller_elevated; target_elevated=$binding.target_elevated;
            same_sid=$binding.same_sid; same_session=$binding.same_session;
            exception_run_id=$binding.sandbox_exception_run_id;
            exception_member_slot=$binding.sandbox_exception_member_slot;
            confirmation_generation=$binding.sandbox_exception_target_confirmation_generation;
            token_consent_generation=$binding.sandbox_exception_token_consent_generation;
            capability_generation=$binding.sandbox_exception_capability_generation }
    }
    $initialStatus = Wait-ProductRow 'status' {
        param($row) $row.sequence -gt $binding.sequence
    } 10
    Record 'initial_product_status' @{ product_sequence=$initialStatus.sequence;
        topology_ready=$initialStatus.topology_ready;
        relation_count=$initialStatus.relation_count; windows=$initialStatus.windows }
    if ($initialStatus.topology_ready -ne $true -or
        [int]$initialStatus.relation_count -lt 2) {
        throw 'ctrl_glue_layout_not_ready'
    }
    if ($Mode -ceq 'BindOnlyIndependentReview') {
        # Leave the SAME Release product/visible console running. Preparation
        # is automated and labelled; no gesture below is sent in this mode.
        $reviewConsole = [PaneBindMvpGuestInput]::ObserveAttachedConsole($script:productPid)
        Record 'console_attachment_identity' @{ phase='before_visible_independent_review'; facts=$reviewConsole }
        if (-not $reviewConsole.strict_dedicated -or -not $reviewConsole.visible) {
            throw 'actual_product_console_not_visible_for_independent_review'
        }
        Record 'review_bound' @{ result='BOUND_FOR_INDEPENDENT_REVIEW';
            product_pid=$script:productPid; product=$product; product_log=$productLog;
            product_status_sequence=$initialStatus.sequence; binding_count=3;
            automatic_gestures_sent=$false; functional_acceptance='NOT_CLAIMED';
            status_command='S'; stop_command='Q'; gesture_stop_hotkey='Ctrl+Shift+F11';
            configuration='release'; human_uat='NOT_RUN' }
        exit 0
    }
    # Exercise the actual command reader before ANY synthetic gesture. These
    # two modes share normal binding/consent, but never send mouse/key input.
    $null=Assert-ProductGeometry 'before_guest_commands' $true
    if ($Mode -ceq 'CommandStopValidation') {
        Guest-Command 'Q'
        $shutdown=Wait-ProductRow 'shutdown' { param($row) $row.result -ceq 'STOPPED' } 10
        if ($shutdown.resources_stopped -ne $true -or
            $shutdown.gesture_events_recorded -ne $true -or
            $shutdown.source -cne 'guest_pipe_command') {
            throw 'command_stop_cleanup_not_observed'
        }
        Record 'shutdown' @{ result='COMMAND_STOP_RECORDED'; automatic_gestures_sent=$false;
            product_shutdown=$shutdown.result; product_resources_stopped=$true; human_uat='NOT_RUN' }
        exit 0
    }
    if ($Mode -ceq 'CommandDisconnectValidation') {
        # Closing the only client and exiting this exact driver are REAL
        # platform facts; the runner separately observes product cleanup.
        Record 'command_disconnect' @{ channel='guest_named_pipe'; driver_pid=$PID;
            product_pid=$script:productPid; pipe_name=$script:pipePath;
            client_name=$script:pipeName;
            action='dispose_client_then_exit_driver'; product_cleanup_observed=$false }
        $script:commandPipe.Dispose()
        $script:commandPipe=$null
        Record 'shutdown' @{ result='COMMAND_DRIVER_EXIT_REQUESTED';
            automatic_gestures_sent=$false; product_pid=$script:productPid;
            product_cleanup_observed=$false; human_uat='NOT_RUN' }
        exit 0
    }
    # Both configurations exercise the same small, representative product
    # flow. Each plain gesture proves individual free/snap/hold/detach/resnap
    # waypoints; neither a boolean proposal nor one snap stands in for these.
    # First prove the cross-process ordinary Move handoff, actual writes and
    # NormalUp cleanup before allowing Ctrl or the rest of the three-window flow.
    Invoke-Drag 0 'plain' -48 -48 'plain_move_candidate'
    Record 'first_explorer_plain_closed' @{ member=0;
        scope='same_exact_bound_frame_real_handoff_native_magnet_raw_legacy_up_and_detach';
        human_uat='NOT_RUN' }
    $null=Assert-ProductGeometry 'after_first_plain' $true
    Invoke-Drag 0 'ctrl' 34 0 'ctrl_move'
    Invoke-Drag 1 'plain' 48 -48 'plain_move_candidate'
    Invoke-Drag 2 'plain' -48 48 'plain_move_candidate'
    Invoke-Drag 1 'resize' 0 28 'native_resize'
    $null=Assert-ProductGeometry 'after_native_resize' $true
    # Return to A only after B's actual resized frame was refreshed. The
    # second A gesture has its own generation, original anchor and target set.
    Invoke-Drag 0 'plain' -48 -48 'plain_move_candidate'
    $null=Assert-ProductGeometry 'after_a_b_c_a' $true
    Invoke-Drag 2 'ctrl' 22 18 'ctrl_move'
    $null=Assert-ProductGeometry 'after_dynamic_ctrl_leader' $true
    Guest-Command 'Q'
    $shutdown = Wait-ProductRow 'shutdown' { param($row) $row.result -ceq 'STOPPED' } 10
    if ($shutdown.resources_stopped -ne $true -or
        $shutdown.gesture_events_recorded -ne $true) { throw 'product_cleanup_not_observed' }
    Record 'shutdown' @{ result='AUTOMATED_BASIC_FLOW_RECORDED'; product_shutdown=$shutdown.result;
        product_resources_stopped=$true;
        coverage='a_b_c_a_each_free_xy_snap_hold_detach_resnap_ctrl_a_c_resize_refresh';
        xy_alignment='NATIVE_EXACT_ASSERTED'; gesture_generations=$script:gestureGenerations.Count;
        human_uat='NOT_RUN' }
    exit 0
} catch {
    $reason = $_.Exception.Message
    $failureLine = $_.InvocationInfo.ScriptLineNumber
    $failureStack = $_.ScriptStackTrace
    if ($script:pressed) {
        try { [PaneBindMvpGuestInput]::MouseButton($false); Record 'cleanup_up' @{ sent=$true } }
        catch { Record 'cleanup_up' @{ sent=$false; error=$_.Exception.Message } }
        $script:pressed=$false
    }
    if ($script:ctrlPressed) {
        try { [PaneBindMvpGuestInput]::Control($false) } catch { }
        $script:ctrlPressed=$false
    }
    # Invoke-Drag finally has already attempted the actual test UP. This late
    # query cannot explain native ordering or turn an unknown callback process
    # into authorized control. Keep the callback QPC separate from UTC lookup.
    try {
        $rows = @(Product-Rows)
        $startup = @($rows | Where-Object { $_.type -ceq 'startup' })
        if ($startup.Count -eq 1 -and $startup[0].run_id -ceq $RunId) {
            $loss = @($rows | Where-Object {
                $_.type -ceq 'gesture_event' -and $_.event -ceq 'resource' -and
                $_.reason -in @('capture_lost_observed','context_lost_observed') -and
                $_.resource_loss_callback
            } | Select-Object -Last 1)
            if ($loss.Count -eq 1) {
                $callback = $loss[0].resource_loss_callback
                $queryPid = if ($callback.root_pid -gt 0) { [int]$callback.root_pid }
                    elseif ($callback.foreground_pid -gt 0) { [int]$callback.foreground_pid }
                    else { 0 }
                Record 'post_failure_process_identity' @{ product_sequence=$loss[0].sequence;
                    generation=$loss[0].generation; callback_qpc=$callback.qpc;
                    actual_callback_foreground=$callback.foreground;
                    actual_callback_root=$callback.foreground_root; query_pid=$queryPid;
                    observed_chain=@(Read-GuestProcessIdentityChain $queryPid 3);
                    timing='post_failure_after_test_up_cleanup_not_native_total_order';
                    identity_or_control_authority_granted=$false }
            }
        }
    } catch {
        Record 'post_failure_process_identity' @{ result='UNKNOWN';
            error_category=[string]$_.CategoryInfo.Category; error_id=$_.FullyQualifiedErrorId;
            identity_or_control_authority_granted=$false }
    }
    if ($script:productStarted -and ($script:commandPipe -or
        $script:consoleInput -ne [IntPtr]::Zero)) {
        try { Guest-Command 'Q' } catch { }
    }
    Record 'shutdown' @{ result='ABORTED'; reason=$reason;
        script_line=$failureLine; script_stack=$failureStack;
        product_stopped_observed=$false; human_uat='NOT_RUN' }
    exit 2
} finally {
    if ($script:commandPipe) { $script:commandPipe.Dispose() }
    if ($script:consoleInput -ne [IntPtr]::Zero) {
        [PaneBindMvpGuestInput]::CloseHandle($script:consoleInput) | Out-Null
    }
    if ($script:writer) { $script:writer.Dispose() }
}
