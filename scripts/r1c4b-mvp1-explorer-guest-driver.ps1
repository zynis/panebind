[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('debug', 'release')][string]$Configuration,
    [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{32}$')][string]$RunId
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
$script:pressed = $false
$script:ctrlPressed = $false
$script:productPid = 0
$script:productStarted = $false
$script:bound = @()
$script:targets = @()

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
    if ($manifest.run_id -cne $RunId -or
        $manifest.source_commit -cnotmatch '^[0-9a-f]{40}$' -or
        $manifest.explorer_driver_sha256 -cne
            (Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash) {
        throw 'driver_manifest_mismatch'
    }
    $expected = if ($Configuration -ceq 'debug') {
        $manifest.debug_explorer_sha256
    } else { $manifest.binaries_sha256.'panebind-explorer-mvp1.exe' }
    if ($expected -cne (Get-FileHash -LiteralPath $product -Algorithm SHA256).Hash) {
        throw 'product_binary_mismatch'
    }
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
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    static extern IntPtr CreateFileW(string name,uint access,uint share,IntPtr security,
        uint creation,uint attributes,IntPtr template);
    [DllImport("kernel32.dll", SetLastError=true)] public static extern bool CloseHandle(IntPtr handle);
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool WriteConsoleInputW(
        IntPtr input,[In] InputRecord[] records,uint length,out uint written);
    [DllImport("user32.dll")] static extern IntPtr GetAncestor(IntPtr window,uint flags);
    [DllImport("user32.dll")] static extern IntPtr WindowFromPoint(Point point);
    [DllImport("user32.dll")] static extern bool GetCursorPos(out Point point);
    [DllImport("user32.dll")] static extern IntPtr OpenInputDesktop(uint flags,bool inherit,uint access);
    [DllImport("user32.dll")] static extern IntPtr GetThreadDesktop(uint thread);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern bool GetUserObjectInformationW(
        IntPtr handle,int index,StringBuilder value,int length,out int needed);
    [DllImport("user32.dll")] static extern bool CloseDesktop(IntPtr desktop);
    [DllImport("kernel32.dll")] static extern uint GetCurrentThreadId();
    [DllImport("kernel32.dll")] static extern uint WTSGetActiveConsoleSessionId();
    [DllImport("kernel32.dll")] static extern uint GetCurrentProcessId();
    [DllImport("kernel32.dll")] static extern bool ProcessIdToSessionId(uint pid,out uint session);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern int GetClassNameW(
        IntPtr window,StringBuilder name,int length);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(
        IntPtr window,out uint pid);
    [DllImport("user32.dll")] static extern bool IsWindow(IntPtr window);
    [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr window,out Rect rect);
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
    public static int StartProduct(string path,string arguments) {
        var startup=new StartupInfo(); startup.cb=(uint)Marshal.SizeOf(typeof(StartupInfo));
        ProcessInfo process;
        var command=new StringBuilder("\""+path+"\" "+arguments);
        if(!CreateProcessW(path,command,IntPtr.Zero,IntPtr.Zero,false,0x00000010,
            IntPtr.Zero,null,ref startup,out process))
            throw new Win32Exception(Marshal.GetLastWin32Error(),"CreateProcessW");
        CloseHandle(process.thread);CloseHandle(process.process);
        return checked((int)process.processId);
    }
    public static IntPtr AttachInput(int pid) {
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
    public static void ConsoleLine(IntPtr input,string line) {
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
        var window=new IntPtr(value);uint nativePid;
        uint nativeTid=GetWindowThreadProcessId(window,out nativePid);
        pid=(int)nativePid;tid=(int)nativeTid;
        var name=new StringBuilder(128);
        return IsWindow(window)&&GetAncestor(window,2)==window&&
            GetClassNameW(window,name,name.Capacity)>0&&
            (name.ToString()=="CabinetWClass"||name.ToString()=="ExploreWClass")&&
            nativePid!=0&&nativeTid!=0;
    }
    public static int[] Rectangle(long value,bool visible) {
        Rect rect;
        if(visible) {
            if(DwmGetWindowAttribute(new IntPtr(value),9,out rect,(uint)Marshal.SizeOf(typeof(Rect)))!=0)
                throw new Win32Exception("DwmGetWindowAttribute");
        } else if(!GetWindowRect(new IntPtr(value),out rect))
            throw new Win32Exception(Marshal.GetLastWin32Error(),"GetWindowRect");
        return new[]{rect.Left,rect.Top,rect.Right,rect.Bottom};
    }
    public static void Place(long value,int x,int y,int width,int height) {
        ShowWindow(new IntPtr(value),9);
        if(!SetWindowPos(new IntPtr(value),IntPtr.Zero,x,y,width,height,0x0014))
            throw new Win32Exception(Marshal.GetLastWin32Error(),"SetWindowPos(test-created root)");
    }
    public static bool Foreground(long value) {
        SetForegroundWindow(new IntPtr(value));
        return GetForegroundWindow()==new IntPtr(value);
    }
    public static bool InputDesktopReady() {
        uint session;
        if(!ProcessIdToSessionId(GetCurrentProcessId(),out session)||session==0||
            session!=WTSGetActiveConsoleSessionId()) return false;
        var input=OpenInputDesktop(0,false,0x0001);
        if(input==IntPtr.Zero) return false;
        try {
            var inputName=new StringBuilder(256);var currentName=new StringBuilder(256);
            int needed;
            return GetUserObjectInformationW(input,2,inputName,512,out needed)&&
                GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()),
                    2,currentName,512,out needed)&&
                inputName.ToString()=="Default"&&currentName.ToString()=="Default";
        } finally { CloseDesktop(input); }
    }
    public static int[] Cursor() {
        Point point;
        if(!GetCursorPos(out point)) throw new Win32Exception(Marshal.GetLastWin32Error(),"GetCursorPos");
        return new[]{point.X,point.Y};
    }
    public static long CursorRoot() {
        Point point;
        if(!GetCursorPos(out point)) throw new Win32Exception(Marshal.GetLastWin32Error(),"GetCursorPos");
        return GetAncestor(WindowFromPoint(point),2).ToInt64();
    }
    public static int Hit(long value,int x,int y) {
        IntPtr result;
        var point=new IntPtr((y<<16)|(x&0xffff));
        if(SendMessageTimeoutW(new IntPtr(value),0x84,IntPtr.Zero,point,0x0002,500,out result)==IntPtr.Zero)
            return int.MinValue;
        return result.ToInt32();
    }
    public static void MouseMove(int x,int y) {
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
        if(!InputDesktopReady()) throw new InvalidOperationException("guest input desktop inactive before mouse button");
        var input=new Input {type=0,mouse=new MouseInput {
            flags=down?LEFTDOWN:LEFTUP,extra=new UIntPtr(0x50424D58)
        }};
        if(SendInput(1,new[]{input},Marshal.SizeOf(typeof(Input)))!=1)
            throw new Win32Exception(Marshal.GetLastWin32Error(),"SendInput button");
    }
    public static void Control(bool down) {
        if(!InputDesktopReady()) throw new InvalidOperationException("guest input desktop inactive before Ctrl key");
        var input=new Input {type=1,key=new KeyInput {
            key=0x11,flags=down?0:KEYUP,extra=new UIntPtr(0x50424D58)
        }};
        if(SendInput(1,new[]{input},Marshal.SizeOf(typeof(Input)))!=1)
            throw new Win32Exception(Marshal.GetLastWin32Error(),"SendInput Ctrl");
    }
    public static bool LeftDown() { return (GetAsyncKeyState(1)&0x8000)!=0; }
    public static int[] WorkArea() {
        int x=GetSystemMetrics(76),y=GetSystemMetrics(77),
            width=GetSystemMetrics(78),height=GetSystemMetrics(79);
        return new[]{x,y,width,height};
    }
}
'@

if (-not [PaneBindMvpGuestInput]::InputDesktopReady()) {
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
function Product-Rows {
    if (-not (Test-Path -LiteralPath $productLog -PathType Leaf)) { return @() }
    $rows = @()
    foreach ($line in [IO.File]::ReadAllLines($productLog)) {
        if (-not $line.EndsWith('}')) { continue }
        try { $rows += ($line | ConvertFrom-Json -ErrorAction Stop) } catch { }
    }
    return $rows
}
function Wait-ProductRow([string]$Type, [scriptblock]$Match, [int]$Seconds = 15) {
    $until = [DateTime]::UtcNow.AddSeconds($Seconds)
    do {
        foreach ($row in @(Product-Rows)) {
            if ($row.type -ceq $Type -and (& $Match $row)) { return $row }
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
function Console-Line([string]$Line) {
    if (-not [PaneBindMvpGuestInput]::InputDesktopReady()) {
        throw 'guest_input_desktop_not_active_for_console'
    }
    [PaneBindMvpGuestInput]::ConsoleLine($script:consoleInput,$Line)
    Record 'console_input' @{ text = $Line; source = 'guest_automated_driver'; product_pid = $script:productPid }
}
function Find-Hit([long]$Hwnd, [int]$Wanted) {
    $rect = [PaneBindMvpGuestInput]::Rectangle($Hwnd,$false)
    $width = $rect[2]-$rect[0]; $height = $rect[3]-$rect[1]
    $ys = if ($Wanted -eq 2) { @(10,17,24,31) } else { @($height-3,$height-5,$height-8) }
    foreach ($y in $ys) {
        foreach ($fraction in @(0.2,0.3,0.4,0.5,0.6,0.7)) {
            $pointX = $rect[0]+[int]($width*$fraction)
            $pointY = $rect[1]+$y
            if ([PaneBindMvpGuestInput]::Hit($Hwnd,$pointX,$pointY) -eq $Wanted) {
                return @($pointX,$pointY)
            }
        }
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
function Assert-Test-DownTarget([long]$Hwnd,[int]$X,[int]$Y,[string]$Phase) {
    if (-not [PaneBindMvpGuestInput]::InputDesktopReady()) {
        throw "guest_input_desktop_not_active:$Phase"
    }
    $cursor = [PaneBindMvpGuestInput]::Cursor()
    $root = [PaneBindMvpGuestInput]::CursorRoot()
    Record 'input_precondition' @{ phase=$Phase; cursor=$cursor; expected=@($X,$Y);
        hit_root=$root; exact_root=($root -eq $Hwnd); desktop_ready=$true }
    if ($cursor[0] -ne $X -or $cursor[1] -ne $Y -or $root -ne $Hwnd) {
        throw "cursor_or_hit_not_exact:$Phase"
    }
}
function Invoke-Drag([int]$Member,[string]$Kind,[int]$DeltaX,[int]$DeltaY,
                     [string]$ExpectedRoute) {
    $root = Exact-Root $Member
    $hwnd = [long]$root.hwnd
    $wantedHit = if ($Kind -ceq 'resize') { 15 } else { 2 } # HTBOTTOM / HTCAPTION
    $point = Find-Hit $hwnd $wantedHit
    if (-not [PaneBindMvpGuestInput]::Foreground($hwnd)) {
        Send-MouseMove $point[0] $point[1] 'activate'
        Assert-Test-DownTarget $hwnd $point[0] $point[1] 'activate'
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
    if (-not [PaneBindMvpGuestInput]::Foreground($hwnd)) { throw "foreground_not_exact:$Member" }
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
        Assert-Test-DownTarget $hwnd $point[0] $point[1] 'gesture'
        $script:pressed=$true
        [PaneBindMvpGuestInput]::MouseButton($true)
        Start-Sleep -Milliseconds 90 # actual legacy DOWN must get a chance to dispatch
        if (-not [PaneBindMvpGuestInput]::LeftDown()) { throw 'physical_left_not_held_after_down' }
        Send-MouseMove ($point[0]+[Math]::Sign($DeltaX)*8) ($point[1]+[Math]::Sign($DeltaY)*8) 'native_start'
        $start = Wait-ProductRow 'gesture_event' {
            param($row) $row.sequence -gt $watermark -and $row.event -ceq 'start' -and
                $row.source_member -eq $Member
        } 4
        if ($start.route -cne $ExpectedRoute) { throw "route_mismatch:$($start.route)" }
        Record 'observed_start' @{ member=$Member; product_sequence=$start.sequence;
            generation=$start.generation; route=$start.route }
        if ($Kind -ceq 'plain') {
            $handoff = Wait-ProductRow 'gesture_event' {
                param($row) $row.sequence -gt $start.sequence -and $row.event -ceq 'handoff' -and
                    $row.generation -eq $start.generation -and $row.succeeded -eq $true
            } 4
            Record 'observed_handoff' @{ generation=$handoff.generation;
                product_sequence=$handoff.sequence; actual_visible=$handoff.actual_visible }
        }
        $steps = if ($Kind -ceq 'plain') {
            @(0.25,0.50,0.75,1.0,0.50,0.0,1.0)
        } else { @(0.25,0.50,0.75,1.0) }
        foreach ($step in $steps) {
            Send-MouseMove ($point[0]+[int]($DeltaX*$step)) ($point[1]+[int]($DeltaY*$step)) 'continuation'
            Start-Sleep -Milliseconds 45
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
        if ($gone.overlay_destroyed -ne $true -or $gone.hotkey_unregistered -ne $true) {
            throw "plain_product_isolation_cleanup_incomplete:$Member"
        }
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
    if ($before[0] -eq $after[0] -and $before[1] -eq $after[1] -and
        $before[2] -eq $after[2] -and $before[3] -eq $after[3]) {
        throw "gesture_geometry_unchanged:$Kind/$Member"
    }
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
        guest_test_only=$true; product=$product; product_log=$productLog;
        input_source='automated_guest_driver'; live_validation=$false }
    if ([PaneBindMvpGuestInput]::WorkArea()[2] -lt 800 -or
        [PaneBindMvpGuestInput]::WorkArea()[3] -lt 600) { throw 'guest_desktop_too_small' }
    $args = "--sandbox-run-id $RunId --evidence-log `"$productLog`" --guest-automated-driver"
    $script:productPid = [PaneBindMvpGuestInput]::StartProduct($product,$args)
    $script:productStarted = $true
    $script:consoleInput = [PaneBindMvpGuestInput]::AttachInput($script:productPid)
    Record 'product_started' @{ pid=$script:productPid; exact_console_attached=$true }
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
        Console-Line ''
        $confirmed = Wait-ProductRow 'target_confirmed' {
            param($row) $row.member -eq $member
        } 15
        if ($confirmed.baseline_exclusion_complete -ne $true -or
            $confirmed.unique_new_target -ne $true -or
            $confirmed.exact_location -ne $true) {
            throw "product_target_facts_incomplete:$member"
        }
    }
    $area = [PaneBindMvpGuestInput]::WorkArea()
    $width=[int]([Math]::Min(340,($area[2]-100)/2))
    $height=[int]([Math]::Min(245,($area[3]-140)/2))
    $left=$area[0]+40; $top=$area[1]+85
    $places=@(@($left,$top),@($left+$width,$top),@($left,$top+$height))
    for ($member=0; $member -lt 3; $member++) {
        $root = Exact-Root $member
        [PaneBindMvpGuestInput]::Place([long]$root.hwnd,$places[$member][0],
            $places[$member][1],$width,$height)
        Record 'test_layout' @{ member=$member; hwnd=$root.hwnd;
            visible=[PaneBindMvpGuestInput]::Rectangle([long]$root.hwnd,$true);
            source='guest_test_preconsent' }
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
    Console-Line 'Y'
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
        $script:bound += $binding
        Record 'binding_verified' @{ member=$member; hwnd=$root.hwnd;
            pid=$root.pid; tid=$root.tid; product_window_id=$binding.window_id;
            exact_new_root=$true }
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
    # Debug targets permission/attribution first. Release repeats a bounded
    # representative same-entry three-member flow only after Debug succeeds.
    Invoke-Drag 0 'ctrl' 34 0 'ctrl_move'
    Invoke-Drag 0 'plain' 65 0 'plain_move_candidate'
    Invoke-Drag 1 'plain' -65 0 'plain_move_candidate'
    Invoke-Drag 2 'plain' 0 -65 'plain_move_candidate'
    Invoke-Drag 1 'resize' 0 28 'native_resize'
    $beforeStatus = @(Product-Rows | Select-Object -Last 1)
    $statusWatermark = if ($beforeStatus.Count) { [long]$beforeStatus[0].sequence } else { 0 }
    Console-Line 'S'
    $status = Wait-ProductRow 'status' { param($row) $row.sequence -gt $statusWatermark } 10
    Record 'product_status' @{ product_sequence=$status.sequence;
        topology_ready=$status.topology_ready; relation_count=$status.relation_count;
        windows=$status.windows }
    Console-Line 'Q'
    $shutdown = Wait-ProductRow 'shutdown' { param($row) $row.result -ceq 'STOPPED' } 10
    if ($shutdown.resources_stopped -ne $true -or
        $shutdown.gesture_events_recorded -ne $true) { throw 'product_cleanup_not_observed' }
    Record 'shutdown' @{ result='AUTOMATED_BASIC_FLOW_RECORDED'; product_shutdown=$shutdown.result;
        product_resources_stopped=$true; coverage='ctrl_delta_abc_plain_one_each_resize_one';
        xy_alignment='NOT_ASSERTED'; human_uat='NOT_RUN' }
    exit 0
} catch {
    $reason = $_.Exception.Message
    if ($script:pressed) {
        try { [PaneBindMvpGuestInput]::MouseButton($false); Record 'cleanup_up' @{ sent=$true } }
        catch { Record 'cleanup_up' @{ sent=$false; error=$_.Exception.Message } }
        $script:pressed=$false
    }
    if ($script:ctrlPressed) {
        try { [PaneBindMvpGuestInput]::Control($false) } catch { }
        $script:ctrlPressed=$false
    }
    if ($script:productStarted -and $script:consoleInput -ne [IntPtr]::Zero) {
        try { Console-Line 'Q' } catch { }
    }
    Record 'shutdown' @{ result='ABORTED'; reason=$reason;
        product_stopped_observed=$false; human_uat='NOT_RUN' }
    exit 2
} finally {
    if ($script:consoleInput -ne [IntPtr]::Zero) {
        [PaneBindMvpGuestInput]::CloseHandle($script:consoleInput) | Out-Null
    }
    if ($script:writer) { $script:writer.Dispose() }
}
