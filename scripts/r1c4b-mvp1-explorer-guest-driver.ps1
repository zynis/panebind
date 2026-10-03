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
$script:gestureGenerations = [Collections.Generic.HashSet[long]]::new()

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
    $ys = if ($Wanted -eq 2) { @(10,17,24,31,40,48,56) } else {
        @(($height-3),($height-5),($height-8))
    }
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
    Console-Line 'S'
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
                               [long]$AfterQuantum) {
    # Coordinates come from the original DOWN/window anchor. Reading each
    # native result verifies geometry; it never becomes a new intent anchor.
    $cursor=@([int]($Down[0]+$WantedFree[0]-$Initial[0]),
              [int]($Down[1]+$WantedFree[1]-$Initial[1]))
    Start-Sleep -Milliseconds 65 # bounded driver pacing, below the existing speed limit
    Send-MouseMove $cursor[0] $cursor[1] $Phase
    $receipt = Wait-ProductRow 'gesture_event' {
        param($row) $row.generation -eq $Generation -and $row.event -ceq 'writer' -and
            $row.quantum -gt $AfterQuantum -and $row.native_attempted -eq $true
    } 4
    $root = Exact-Root $Member
    $actual=[PaneBindMvpGuestInput]::Rectangle([long]$root.hwnd,$true)
    $exact = $receipt.native_succeeded -eq $true -and
        $receipt.geometry_exact -eq $true -and $receipt.post_context_exact -eq $true -and
        $receipt.snapped -eq $Snapped -and
        (Test-RectEqual $receipt.target_visible $Expected) -and
        (Test-RectEqual $receipt.actual_visible $Expected) -and
        (Test-RectEqual $actual $Expected)
    Record 'magnet_waypoint' @{ phase=$Phase; member=$Member; generation=$Generation;
        original_down=$Down; original_visible=$Initial; cursor=$cursor;
        free_visible=$WantedFree; expected_visible=$Expected; actual_visible=$actual;
        expected_snapped=$Snapped; observed_snapped=$receipt.snapped;
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
                @{phase='hold_outside_attraction';distance=12;snapped=$true},
                @{phase='detach_beyond_release';distance=48;snapped=$false},
                @{phase='xy_resnap';distance=4;snapped=$true})) {
                $x=$snapLeft+$outX*$waypoint.distance
                $y=$snapTop+$outY*$waypoint.distance
                $free=@($x,$y,($x+$width),($y+$height))
                $expected=if($waypoint.snapped){$snap}else{$free}
                $quantum=Invoke-MagnetWaypoint $Member $start.generation $waypoint.phase `
                    $point $before $free $expected $waypoint.snapped $quantum
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
        if ($gone.overlay_destroyed -ne $true -or $gone.hotkey_unregistered -ne $true) {
            throw "plain_product_isolation_cleanup_incomplete:$Member"
        }
        $captureReleased=Wait-ProductRow 'gesture_event' {
            param($row) $row.event -ceq 'resource' -and
                $row.reason -ceq 'capture_released_observed' -and
                $row.generation -eq $start.generation
        } 4
        $released=$captureReleased.shield_capture
        if (-not $released -or $released.release_attempted -ne $true -or
            $released.release_succeeded -ne $true -or
            [long]$released.release_before -ne [long]$captureReady.overlay -or
            [long]$released.release_after -ne 0 -or $released.own_release_message -ne $true) {
            throw "normal_up_capture_release_not_observed:$Member"
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
    $width=[int]([Math]::Min(420,($area[2]-300)/2))
    $height=[int]([Math]::Min(280,($area[3]-300)/2))
    $left=$area[0]+120; $top=$area[1]+140
    $places=@(@($left,$top),@(($left+$width),$top),@($left,($top+$height)))
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
    # Both configurations exercise the same small, representative product
    # flow. Each plain gesture proves individual free/snap/hold/detach/resnap
    # waypoints; neither a boolean proposal nor one snap stands in for these.
    Invoke-Drag 0 'ctrl' 34 0 'ctrl_move'
    Invoke-Drag 0 'plain' -48 -48 'plain_move_candidate'
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
    Console-Line 'Q'
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
