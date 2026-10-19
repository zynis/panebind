[CmdletBinding()]
param([Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-f]{32}$')][string]$RunId,
    [ValidateSet('Parent','Child')][string]$Mode='Parent',
    [ValidatePattern('^[0-9a-f]{32}$')][string]$ChildNonce,
    [ValidatePattern('^[A-F0-9]{64}$')][string]$BindingSha256,
    [switch]$NativeIdentityOnly,
    [switch]$StandardGuestNativeClients,
    [ValidateRange(30,1800)][int]$NativeClientsTimeoutSeconds=900)
$ErrorActionPreference='Stop'
$mappedInputDir='C:\PaneBindV1\Input'
$mappedOutputDir='C:\PaneBindV1\Output'
$localRunDir=Join-Path 'C:\PaneBindV1\MediumDesktop' $RunId
$inputDir=if($Mode -ceq 'Parent'){$mappedInputDir}else{Join-Path $localRunDir 'Input'}
$outputDir=if($Mode -ceq 'Parent'){$mappedOutputDir}else{Join-Path $localRunDir 'Output'}
$leaf='v1-medium-desktop-probe-guest.ps1'
$evidence=$null;$childPhasePath=$null
# TOKEN_QUERY-only inspection uses actual TokenGroups, not a managed identity
# constructor that can require more token rights. Official contracts reviewed:
# https://learn.microsoft.com/windows/win32/api/winnt/ns-winnt-token_groups
# https://learn.microsoft.com/windows/win32/api/securitybaseapi/nf-securitybaseapi-gettokeninformation
# https://learn.microsoft.com/dotnet/api/system.security.principal.windowsidentity.-ctor
function Read-ExactMarker([string]$Path) {
    $item=Get-Item -LiteralPath $Path -ErrorAction Stop
    if($item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -or $item.Length -gt 34){throw 'Run marker is not a bounded plain file.'}
    $value=[IO.File]::ReadAllText($Path,[Text.Encoding]::ASCII)
    if($value.EndsWith("`r`n")){$value=$value.Substring(0,$value.Length-2)}
    if($value -cnotmatch '^[0-9a-f]{32}$'){throw 'Exact run marker is invalid.'}
    return $value
}
function Write-ChildPhase([string]$Stage,[bool]$Create=$false) {
    if(-not $childPhasePath){throw 'Validated exact child phase path is not established.'}
    $count=0
    if(-not $Create){
        $item=Get-Item -LiteralPath $childPhasePath
        if($item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -or $item.Length -gt 16KB){throw 'Child phase evidence is not bounded/plain.'}
        $count=@([IO.File]::ReadAllLines($childPhasePath)).Count
    }
    if($count -ge 12){throw 'Child phase line bound reached.'}
    $row=[ordered]@{schema='panebind-v1-medium-desktop-phase/1';run_id=$RunId;phase=$Stage;pid=$PID;utc=[DateTime]::UtcNow.ToString('o')}
    $bytes=(New-Object Text.UTF8Encoding($false)).GetBytes(($row|ConvertTo-Json -Compress)+"`n")
    $mode=if($Create){[IO.FileMode]::CreateNew}else{[IO.FileMode]::Append}
    $stream=[IO.File]::Open($childPhasePath,$mode,[IO.FileAccess]::Write,[IO.FileShare]::Read)
    try{if($stream.Length+$bytes.Length -gt 16KB){throw 'Child phase byte bound reached.'};$stream.Write($bytes,0,$bytes.Length)}finally{$stream.Dispose()}
}
try {
    # Host misuse protection precedes Add-Type, account APIs or any evidence
    # write. There is no bypass switch and this is not a general RunAs tool.
    if(-not [string]::Equals([IO.Path]::GetFullPath($PSScriptRoot),$inputDir,[StringComparison]::OrdinalIgnoreCase) -or
       -not [string]::Equals([IO.Path]::GetFullPath($PSCommandPath),(Join-Path $inputDir $leaf),[StringComparison]::OrdinalIgnoreCase)){
        throw 'Probe must execute from its exact parent mapped input or bound child local input path.'}
    $directories=@('C:\PaneBindV1',$inputDir,$outputDir)
    if($Mode -ceq 'Child'){$directories+=@('C:\PaneBindV1\MediumDesktop',$localRunDir)}
    foreach($directory in $directories){
        $item=Get-Item -LiteralPath $directory -ErrorAction Stop
        if(-not $item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Exact guest directories must be plain directories.'}
    }
    $identity=[Security.Principal.WindowsIdentity]::GetCurrent()
    try{
        $wanted=if($Mode -ceq 'Parent'){$env:COMPUTERNAME+'\WDAGUtilityAccount'}else{$env:COMPUTERNAME+'\PBV1D_'+$RunId.Substring(0,12)}
        if(-not [string]::Equals($identity.Name,$wanted,[StringComparison]::OrdinalIgnoreCase)){throw 'Exact disposable parent or newly bound child account required.'}
    }finally{$identity.Dispose()}
    if($Mode -ceq 'Parent' -and ($ChildNonce -or $BindingSha256)){throw 'Parent mode does not accept child binding arguments.'}
    if($Mode -ceq 'Child' -and (-not $ChildNonce -or -not $BindingSha256)){throw 'Child requires its exact one-time binding.'}
    if($Mode -ceq 'Child' -and ($NativeIdentityOnly -or $StandardGuestNativeClients)){throw 'Native modes have no PowerShell child entry.'}
    if((Read-ExactMarker (Join-Path $inputDir 'panebind-v1-input.marker')) -cne $RunId -or
       (Read-ExactMarker (Join-Path $outputDir 'panebind-v1-output.marker')) -cne $RunId){throw 'Input/output run markers disagree.'}
    $manifestFile=Get-Item -LiteralPath (Join-Path $inputDir 'manifest.json') -ErrorAction Stop
    if($manifestFile.PSIsContainer -or ($manifestFile.Attributes -band [IO.FileAttributes]::ReparsePoint) -or $manifestFile.Length -gt 1MB){throw 'Manifest must be a bounded plain file.'}
    $manifest=[IO.File]::ReadAllText($manifestFile.FullName,[Text.Encoding]::UTF8)|ConvertFrom-Json
    if($manifest.schema -cne 'panebind-v1-sandbox/1' -or $manifest.run_id -cne $RunId -or
       $manifest.guest_contract -cne 'panebind-v1-guest-contract/2' -or
       $manifest.guest_input -cne $mappedInputDir -or $manifest.guest_output -cne $mappedOutputDir -or
       $manifest.host_root -cne ('I:\PaneBindMVP1Runs\'+$RunId) -or
       $manifest.packaging_head -cnotmatch '^[0-9a-f]{40}$' -or
       $manifest.driver_source_commit -cnotmatch '^[0-9a-f]{40}$'){
        throw 'Immutable guest manifest scope does not match this probe.'}
    $scriptItem=Get-Item -LiteralPath $PSCommandPath -ErrorAction Stop
    $scriptEntry=@($manifest.files|Where-Object relative_path -ceq $leaf)
    if($scriptItem.PSIsContainer -or ($scriptItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -or
       $scriptEntry.Count -ne 1 -or [long]$scriptEntry[0].size -ne $scriptItem.Length -or
       $scriptEntry[0].sha256 -cnotmatch '^[A-F0-9]{64}$' -or
       (Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash -cne $scriptEntry[0].sha256){
        throw 'Probe itself is not bound to the immutable input manifest.'}
    $nativeRequested=$null -ne $manifest.medium_desktop_probe -and $manifest.medium_desktop_probe.native_identity_only -eq $true
    if([bool]$NativeIdentityOnly -ne $nativeRequested){throw 'Native-only mode must match the immutable package selection.'}
    $standardRequested=$null -ne $manifest.medium_desktop_probe -and $manifest.medium_desktop_probe.standard_native_clients -eq $true
    if([bool]$StandardGuestNativeClients -ne $standardRequested -or ($NativeIdentityOnly -and $StandardGuestNativeClients)){
        throw 'Standard native-client mode must match its separate immutable package selection.'}
    if($StandardGuestNativeClients -and ($Mode -cne 'Parent' -or
       $manifest.medium_desktop_probe.enabled -ne $true -or $manifest.medium_desktop_probe.kind -cne 'OneStandardGuestGuiPrecheck' -or
       $manifest.medium_desktop_probe.standard_native_clients -isnot [bool] -or $manifest.medium_desktop_probe.native_identity_only -ne $false -or
       $manifest.medium_desktop_probe.stop_after_probe -ne $true -or $manifest.medium_desktop_probe.actual_suite -cne 'StandardGuestNativeClients' -or
       $manifest.medium_desktop_probe.standard_native_api -cne 'CreateProcessAsUserW' -or $manifest.medium_desktop_probe.followup_suite -cne 'NativeClientsOnly' -or
       $manifest.guest_account_mode -cne 'RunBoundStandardNativeClients' -or $manifest.guest_standard_run_id -cne $RunId -or
       $manifest.guest_standard_account -cne ('PBV1D_'+$RunId.Substring(0,12)) -or
       $manifest.guest_suite -cne 'NativeClientsOnly' -or $manifest.guest_configurations -cne 'Release')){
        throw 'Standard native clients require the exact Run-bound account, API, follow-up and guest contract.'}
    $nativeImage=$null
    if($NativeIdentityOnly -or $StandardGuestNativeClients){
        if($NativeIdentityOnly -and ($manifest.medium_desktop_probe.enabled -ne $true -or $manifest.medium_desktop_probe.kind -cne 'OneStandardGuestGuiPrecheck' -or
           $manifest.medium_desktop_probe.stop_after_probe -ne $true -or $manifest.medium_desktop_probe.actual_suite -cne 'MediumNativeIdentityOnly' -or
           $manifest.medium_desktop_probe.native_identity_image -cne 'panebind-v1-guest-fixture.exe' -or
           $manifest.guest_suite -cne 'NativeClientsOnly' -or $manifest.guest_configurations -cne 'Release')){
            throw 'Native identity entry requires its exact probe-only contract.'}
        if($manifest.medium_desktop_probe.native_identity_image -cne 'panebind-v1-guest-fixture.exe'){throw 'Exact native identity fixture required.'}
        $nativeEntry=@($manifest.files|Where-Object relative_path -ceq 'panebind-v1-guest-fixture.exe')
        $nativeImage=Get-Item -LiteralPath (Join-Path $inputDir 'panebind-v1-guest-fixture.exe')
        if($nativeEntry.Count -ne 1 -or $nativeImage.PSIsContainer -or ($nativeImage.Attributes -band [IO.FileAttributes]::ReparsePoint) -or
           $nativeImage.Length -le 0 -or $nativeImage.Length -gt 16MB -or $nativeImage.Length -ne [long]$nativeEntry[0].size -or
           $nativeEntry[0].sha256 -cnotmatch '^[A-F0-9]{64}$' -or
           $nativeEntry[0].sha256 -cne $manifest.driver_release_sha256 -or
           (Get-FileHash -LiteralPath $nativeImage.FullName -Algorithm SHA256).Hash -cne $nativeEntry[0].sha256){
            throw 'Native identity image is not the exact frozen Release fixture.'}
    }
    $evidence=Join-Path $outputDir ($RunId+$(if($Mode -ceq 'Parent'){'-medium-desktop-probe.json'}else{'-medium-desktop-child.json'}))
    if(Test-Path -LiteralPath $evidence){throw 'Probe evidence already exists; use a fresh RunId.'}
    if($Mode -ceq 'Child'){
        # The first phase exists only after the real one-time binding, exact
        # script/manifest, local account, paths and RunId have been checked.
        # Child reads only this copied, guest-local input. It never opens a
        # mapped vmsmb input/output file during the contrast.
        $bindingPath=Join-Path $inputDir ($RunId+'-medium-desktop-binding.json')
        $bindingItem=Get-Item -LiteralPath $bindingPath
        if($bindingItem.PSIsContainer -or ($bindingItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -or
           $bindingItem.Length -gt 4096 -or (Get-FileHash -LiteralPath $bindingPath).Hash -cne $BindingSha256){throw 'Child one-time binding is not exact.'}
        $binding=[IO.File]::ReadAllText($bindingPath,[Text.Encoding]::UTF8)|ConvertFrom-Json
        if($binding.schema -cne 'panebind-v1-medium-desktop-binding/1' -or $binding.run_id -cne $RunId -or
           $binding.source_commit -cne $manifest.packaging_head -or $binding.probe_sha256 -cne $scriptEntry[0].sha256 -or
           $binding.io_contract -cne 'panebind-v1-guest-local-io-contrast/1' -or
           $binding.child_input -cne $inputDir -or $binding.child_output -cne $outputDir -or
           $binding.manifest_sha256 -cne (Get-FileHash -LiteralPath $manifestFile.FullName -Algorithm SHA256).Hash -or
           $binding.nonce -cne $ChildNonce -or $binding.account_sid_sha256 -cnotmatch '^[A-F0-9]{64}$' -or
           [int]$binding.session_id -le 0){throw 'Child account/script/session binding mismatch.'}
        $childPhasePath=Join-Path $outputDir ($RunId+'-medium-desktop-child-phase.jsonl')
        Write-ChildPhase 'validated_before_AddType' $true
    }
    # Exact one-purpose guest environment contrast. Child never creates users,
    # grants access, launches other programs, controls Explorer or sends input.
    Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Security.AccessControl;
using System.IO;
using System.Security.Principal;
using System.Text;

public static class PaneBindV1MediumDesktopProbe {
    [StructLayout(LayoutKind.Sequential)] struct UserInfo1 {
        public IntPtr name,password; public uint passwordAge,privilege;
        public IntPtr home,comment; public uint flags; public IntPtr script;
    }
    [StructLayout(LayoutKind.Sequential)] struct UserInfo23 {
        public IntPtr name,fullName,comment; public uint flags; public IntPtr sid;
    }
    [DllImport("netapi32.dll",CharSet=CharSet.Unicode)] static extern uint NetUserAdd(string server,uint level,ref UserInfo1 info,out uint parameter);
    [DllImport("netapi32.dll",CharSet=CharSet.Unicode)] static extern uint NetUserGetInfo(string server,string user,uint level,out IntPtr data);
    [DllImport("netapi32.dll",CharSet=CharSet.Unicode)] static extern uint NetUserDel(string server,string user);
    [DllImport("netapi32.dll")] static extern uint NetApiBufferFree(IntPtr data);
    [DllImport("advapi32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool LogonUserW(string user,string domain,IntPtr password,uint type,uint provider,out IntPtr token);
    [DllImport("advapi32.dll",SetLastError=true)] static extern bool GetTokenInformation(IntPtr token,int kind,IntPtr data,uint size,out uint needed);
    [DllImport("user32.dll",SetLastError=true)] static extern bool GetUserObjectSecurity(IntPtr handle,ref uint information,IntPtr descriptor,uint size,out uint needed);
    [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
    [DllImport("kernel32.dll")] static extern IntPtr LocalFree(IntPtr handle);
    [DllImport("kernel32.dll")] static extern uint GetCurrentThreadId();
    [DllImport("user32.dll")] static extern IntPtr GetProcessWindowStation();
    [DllImport("user32.dll")] static extern IntPtr GetThreadDesktop(uint thread);
    [DllImport("user32.dll",SetLastError=true)] static extern IntPtr OpenInputDesktop(uint flags,bool inherit,uint rights);
    [DllImport("user32.dll")] static extern bool CloseDesktop(IntPtr desktop);
    [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool GetUserObjectInformationW(IntPtr handle,int kind,StringBuilder data,uint size,out uint needed);
    [DllImport("user32.dll",EntryPoint="GetUserObjectInformationW",SetLastError=true)] static extern bool GetInputState(IntPtr handle,int kind,out int value,uint size,out uint needed);
    [DllImport("wtsapi32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool WTSQuerySessionInformationW(IntPtr server,uint session,int kind,out IntPtr data,out uint size);
    [DllImport("wtsapi32.dll")] static extern void WTSFreeMemory(IntPtr data);
    static Dictionary<string,object> Item() { return new Dictionary<string,object>(); }
    static string Hash(string value) {
        using(var sha=SHA256.Create()) return BitConverter.ToString(sha.ComputeHash(Encoding.UTF8.GetBytes(value))).Replace("-","");
    }
    static string ObjectName(IntPtr handle) {
        var value=new StringBuilder(128);uint needed;
        if(handle==IntPtr.Zero || !GetUserObjectInformationW(handle,2,value,256,out needed))throw new InvalidOperationException("Object name query failed");
        return value.ToString();
    }
    static Dictionary<string,object> Desktop(bool bootstrap) {
        using(var identity=WindowsIdentity.GetCurrent())
            if(bootstrap&&!String.Equals(identity.Name,Environment.MachineName+"\\WDAGUtilityAccount",StringComparison.OrdinalIgnoreCase))throw new InvalidOperationException("Exact local Sandbox account required");
        var session=Process.GetCurrentProcess().SessionId;
        if(session<=0)throw new InvalidOperationException("Interactive session required");
        var station=GetProcessWindowStation();var thread=GetThreadDesktop(GetCurrentThreadId());var input=OpenInputDesktop(0,false,1);
        try {
            var stationName=ObjectName(station);var threadName=ObjectName(thread);var inputName=ObjectName(input);
            uint bytes;int receives,threadReceives;
            if(!GetInputState(input,6,out receives,4,out bytes)||bytes>4 || !GetInputState(thread,6,out threadReceives,4,out bytes)||bytes>4)throw new InvalidOperationException("Input desktop state unknown");
            IntPtr data;uint size;
            if(!WTSQuerySessionInformationW(IntPtr.Zero,(uint)session,8,out data,out size))throw new InvalidOperationException("WTS state unknown");
            int state;try { if(size<4)throw new InvalidOperationException("Incomplete WTS state");state=Marshal.ReadInt32(data); }finally { WTSFreeMemory(data); }
            if(stationName!="WinSta0"||threadName!="Default"||inputName!="Default"||receives==0||threadReceives==0||state!=0)throw new InvalidOperationException("Active Default input desktop required");
            return new Dictionary<string,object>{{"session_id",session},{"wts_state",state},{"window_station",stationName},{"thread_desktop",threadName},{"input_desktop",inputName},{"input_uoi_io",true},{"thread_uoi_io",true}};
        }finally { if(input!=IntPtr.Zero)CloseDesktop(input); }
    }
    static IntPtr TokenInfo(IntPtr token,int kind) {
        uint size;GetTokenInformation(token,kind,IntPtr.Zero,0,out size);
        int sizeError=Marshal.GetLastWin32Error();
        if(size==0)throw new System.ComponentModel.Win32Exception(sizeError,"Token information size unavailable for class "+kind);
        if(size>65536)throw new InvalidOperationException("Token information size bound exceeded for class "+kind);
        var data=Marshal.AllocHGlobal((int)size);
        if(!GetTokenInformation(token,kind,data,size,out size)){int error=Marshal.GetLastWin32Error();Marshal.FreeHGlobal(data);throw new System.ComponentModel.Win32Exception(error,"Token information query failed for class "+kind);}
        return data;
    }
    static int TokenNumber(IntPtr token,int kind) { var data=TokenInfo(token,kind);try{return Marshal.ReadInt32(data);}finally{Marshal.FreeHGlobal(data);} }
    static string TokenSid(IntPtr token) { var data=TokenInfo(token,1);try{return new SecurityIdentifier(Marshal.ReadIntPtr(data)).Value;}finally{Marshal.FreeHGlobal(data);} }
    static Dictionary<string,object> TokenFacts(IntPtr token,string createdSid,int desktopSession,out bool accepted) {
        var sid=TokenSid(token);var data=TokenInfo(token,25);int integrity;
        try { var label=new SecurityIdentifier(Marshal.ReadIntPtr(data)).Value;var separator=label.LastIndexOf('-');integrity=Int32.Parse(label.Substring(separator+1)); }finally{Marshal.FreeHGlobal(data);}
        var elevated=TokenNumber(token,20)!=0;var session=TokenNumber(token,12);
        var uiAccess=TokenNumber(token,26)!=0;var appContainer=TokenNumber(token,29)!=0;
        // This handle deliberately has TOKEN_QUERY only. WindowsIdentity's
        // token constructor / principal membership path can require additional
        // rights. Read actual groups without duplicating or changing the token.
        bool admin;var adminGroup=AdministratorGroup(token,out admin);
        accepted=sid==createdSid&&integrity==8192&&!elevated&&!admin&&!uiAccess&&!appContainer;
        // This is a credential token, NOT a created interactive process. Its
        // real SessionId may differ; no token/session mutation is performed.
        return new Dictionary<string,object>{{"sid_sha256",Hash(sid)},{"created_account_sid_matches",sid==createdSid},{"integrity_rid",integrity},{"elevated",elevated},{"admin_enabled",admin},{"admin_group",adminGroup},{"ui_access",uiAccess},{"app_container",appContainer},{"credential_token_session_id",session},{"current_desktop_session_id",desktopSession},{"token_session_matches_current",session==desktopSession},{"ordinary_security_facts_verified",accepted},{"interactive_child_session_validation","NOT_RUN"}};
    }
    static string AccountSid(string name,out uint status) {
        IntPtr data;status=NetUserGetInfo(null,name,23,out data);
        if(status!=0)return null;
        try { var user=(UserInfo23)Marshal.PtrToStructure(data,typeof(UserInfo23));return new SecurityIdentifier(user.sid).Value; }finally{NetApiBufferFree(data);}
    }

    [StructLayout(LayoutKind.Sequential)] struct SidAttributes { public IntPtr sid; public uint attributes; }
    [StructLayout(LayoutKind.Sequential)] struct GroupsOne { public uint count; public SidAttributes first; }
    static Dictionary<string,object> AdministratorGroup(IntPtr token,out bool enabled) {
        var data=TokenInfo(token,2);try {
            var count=Marshal.ReadInt32(data);if(count<0||count>512)throw new InvalidOperationException("Bounded token group inventory required");
            int offset=(int)Marshal.OffsetOf(typeof(GroupsOne),"first"),stride=Marshal.SizeOf(typeof(SidAttributes));
            bool present=false;uint attributes=0;
            for(int n=0;n<count;n++){
                var group=(SidAttributes)Marshal.PtrToStructure(IntPtr.Add(data,offset+n*stride),typeof(SidAttributes));
                if(new SecurityIdentifier(group.sid).Value=="S-1-5-32-544"){
                    if(present)throw new InvalidOperationException("Duplicate administrators group is not an ordinary-token fact");
                    present=true;attributes=group.attributes;
                }
            }
            bool groupEnabled=present&&(attributes&0x4u)!=0,denyOnly=present&&(attributes&0x10u)!=0;
            enabled=groupEnabled&&!denyOnly;
            return new Dictionary<string,object>{{"source","actual_TokenGroups_TOKEN_QUERY"},{"group_count",count},
                {"administrators_sid_present",present},{"attributes",present?(object)attributes:null},{"SE_GROUP_ENABLED",groupEnabled},
                {"SE_GROUP_USE_FOR_DENY_ONLY",denyOnly},{"admin_enabled",enabled},{"token_duplicated_or_modified",false}};
        }finally{Marshal.FreeHGlobal(data);}
    }
    [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)] struct Startup {
        public uint size;public string reserved,desktop,title;public uint x,y,width,height,countX,countY,fill,flags;
        public ushort show,reservedSize;public IntPtr reservedBytes,input,output,error;
    }
    [StructLayout(LayoutKind.Sequential)] struct ProcessInfo { public IntPtr process,thread;public uint pid,tid; }
    [StructLayout(LayoutKind.Sequential)] struct FileTime { public uint low,high; }
    [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr OpenWindowStationW(string name,bool inherit,uint access);
    [DllImport("user32.dll",SetLastError=true)] static extern bool CloseWindowStation(IntPtr value);
    [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr OpenDesktopW(string name,uint flags,bool inherit,uint access);
    [DllImport("user32.dll",SetLastError=true)] static extern bool SetUserObjectSecurity(IntPtr value,ref uint information,IntPtr descriptor);
    [DllImport("advapi32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool CreateProcessWithTokenW(IntPtr token,uint logon,string application,StringBuilder command,uint flags,IntPtr environment,string directory,ref Startup startup,out ProcessInfo process);
    [DllImport("advapi32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool CreateProcessAsUserW(IntPtr token,string application,StringBuilder command,IntPtr processSecurity,IntPtr threadSecurity,bool inherit,uint flags,IntPtr environment,string directory,ref Startup startup,out ProcessInfo process);
    [DllImport("advapi32.dll",SetLastError=true)] static extern bool OpenProcessToken(IntPtr process,uint access,out IntPtr token);
    [DllImport("kernel32.dll",SetLastError=true)] static extern uint ResumeThread(IntPtr thread);
    [DllImport("kernel32.dll",SetLastError=true)] static extern uint WaitForSingleObject(IntPtr value,uint milliseconds);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool TerminateProcess(IntPtr process,uint code);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool GetExitCodeProcess(IntPtr process,out uint code);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool GetProcessTimes(IntPtr process,out FileTime birth,out FileTime exit,out FileTime kernel,out FileTime user);
    [StructLayout(LayoutKind.Sequential)] struct JobBasic {
        public long processTime,jobTime;public uint flags;public UIntPtr minimum,maximum;
        public uint activeLimit;public UIntPtr affinity;public uint priority,scheduling;
    }
    [StructLayout(LayoutKind.Sequential)] struct IoCounters {public ulong a,b,c,d,e,f;}
    [StructLayout(LayoutKind.Sequential)] struct JobExtended {
        public JobBasic basic;public IoCounters io;public UIntPtr processMemory,jobMemory,peakProcess,peakJob;
    }
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr CreateJobObjectW(IntPtr security,string name);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool SetInformationJobObject(IntPtr job,int kind,ref JobExtended data,uint bytes);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool AssignProcessToJobObject(IntPtr job,IntPtr process);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool QueryInformationJobObject(IntPtr job,int kind,IntPtr data,uint bytes,out uint needed);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool TerminateJobObject(IntPtr job,uint code);
    static bool SameBytes(byte[] a,byte[] b) {if(a.Length!=b.Length)return false;for(int n=0;n<a.Length;n++)if(a[n]!=b[n])return false;return true;}
    static string ByteHash(byte[] bytes) {using(var sha=SHA256.Create())return BitConverter.ToString(sha.ComputeHash(bytes)).Replace("-","");}
    static string LogonSid(IntPtr token) {
        var data=TokenInfo(token,2);try {
            var count=Marshal.ReadInt32(data);if(count<1||count>512)throw new InvalidOperationException("Bounded token group inventory required");
            int offset=(int)Marshal.OffsetOf(typeof(GroupsOne),"first"),stride=Marshal.SizeOf(typeof(SidAttributes));string found=null;
            for(int n=0;n<count;n++){
                var group=(SidAttributes)Marshal.PtrToStructure(IntPtr.Add(data,offset+n*stride),typeof(SidAttributes));
                if((group.attributes&0xC0000000u)==0xC0000000u){
                    var candidate=new SecurityIdentifier(group.sid).Value;
                    if(found!=null||!candidate.StartsWith("S-1-5-5-",StringComparison.Ordinal))throw new InvalidOperationException("Unique actual logon SID required");
                    found=candidate;
                }
            }
            if(found==null)throw new InvalidOperationException("Actual logon SID unavailable");return found;
        }finally{Marshal.FreeHGlobal(data);}
    }
    sealed class AclState {
        public RawSecurityDescriptor descriptor; public byte[] bytes;
        public string hash {get{return ByteHash(bytes);}}
    }
    static byte[] AceBytes(GenericAce ace) {var bytes=new byte[ace.BinaryLength];ace.GetBinaryForm(bytes,0);return bytes;}
    static AclState ReadDacl(IntPtr handle) {
        uint information=7,size;GetUserObjectSecurity(handle,ref information,IntPtr.Zero,0,out size);
        if(size==0||size>131072)throw new InvalidOperationException("Bounded DACL query unavailable");
        var buffer=Marshal.AllocHGlobal((int)size);try{
            if(!GetUserObjectSecurity(handle,ref information,buffer,size,out size))throw new InvalidOperationException("DACL query failed");
            var bytes=new byte[size];Marshal.Copy(buffer,bytes,0,bytes.Length);
            var descriptor=new RawSecurityDescriptor(bytes,0);
            if(descriptor.DiscretionaryAcl==null||descriptor.DiscretionaryAcl.Count>512)throw new InvalidOperationException("Explicit bounded DACL required");
            var acl=new byte[descriptor.DiscretionaryAcl.BinaryLength];descriptor.DiscretionaryAcl.GetBinaryForm(acl,0);
            return new AclState{descriptor=descriptor,bytes=acl};
        }finally{Marshal.FreeHGlobal(buffer);}
    }
    static Dictionary<string,int> Counts(RawAcl acl) {
        var count=new Dictionary<string,int>();foreach(GenericAce ace in acl){
            var key=Convert.ToBase64String(AceBytes(ace));if(!count.ContainsKey(key))count[key]=0;count[key]++;
        }return count;
    }
    sealed class AddedAce {public byte[] bytes;public string key;public Dictionary<string,object> facts;}
    static List<AddedAce> Delta(AclState before,AclState after,string account,string logon,out bool explained,out List<object> facts) {
        var remaining=Counts(before.descriptor.DiscretionaryAcl);var added=new List<AddedAce>();facts=new List<object>();explained=true;int originalIndex=0;
        foreach(GenericAce ace in after.descriptor.DiscretionaryAcl) {
            var bytes=AceBytes(ace);var key=Convert.ToBase64String(bytes);
            if(remaining.ContainsKey(key)&&remaining[key]>0){
                // An identical ACE multiset is insufficient: changing the
                // original deny/allow order can change effective access.
                if(originalIndex>=before.descriptor.DiscretionaryAcl.Count||
                   !SameBytes(bytes,AceBytes(before.descriptor.DiscretionaryAcl[originalIndex])))explained=false;
                originalIndex++;remaining[key]--;continue;
            }
            var common=ace as CommonAce;var sid=common==null?null:common.SecurityIdentifier.Value;
            bool exact=ace.AceType==AceType.AccessAllowed&&common!=null&&(sid==account||sid==logon);
            var fact=new Dictionary<string,object>{{"ace_sha256",ByteHash(bytes)},{"ace_type",(int)ace.AceType},{"flags",(int)ace.AceFlags},
                {"access_mask",common==null?(object)null:unchecked((uint)common.AccessMask)},{"sid_sha256",sid==null?null:Hash(sid)},{"exact_created_account_or_logon_sid",exact}};
            facts.Add(fact);if(exact)added.Add(new AddedAce{bytes=bytes,key=key,facts=fact});else explained=false;
        }
        foreach(var entry in remaining)if(entry.Value!=0)explained=false;
        return added;
    }
    static Dictionary<string,object> RestoreOwnAddedAces(IntPtr handle,AclState before,List<AddedAce> recorded,string account,string logon) {
        var report=new Dictionary<string,object>{{"confirmed",false},{"set_attempted",false},{"old_dacl_whole_restore",false}};
        try {
            var current=ReadDacl(handle);bool explained;List<object> difference;
            var currentOwned=Delta(before,current,account,logon,out explained,out difference);
            if(recorded==null){report["reason"]="API_after_snapshot_not_recorded_no_guessed_removal";return report;}
            var permitted=new Dictionary<string,int>();foreach(var ace in recorded){if(!permitted.ContainsKey(ace.key))permitted[ace.key]=0;permitted[ace.key]++;}
            var owned=new List<AddedAce>();
            foreach(var ace in currentOwned){
                if(permitted.ContainsKey(ace.key)&&permitted[ace.key]>0){permitted[ace.key]--;owned.Add(ace);}
                else explained=false;
            }
            report["before_sha256"]=before.hash;report["current_sha256"]=current.hash;report["actual_delta"]=difference;
            report["delta_fully_explained"]=explained;
            var remove=new Dictionary<string,int>();foreach(var ace in owned){if(!remove.ContainsKey(ace.key))remove[ace.key]=0;remove[ace.key]++;}
            var next=new RawAcl(current.descriptor.DiscretionaryAcl.Revision,current.descriptor.DiscretionaryAcl.Count);
            foreach(GenericAce ace in current.descriptor.DiscretionaryAcl){
                var key=Convert.ToBase64String(AceBytes(ace));
                if(remove.ContainsKey(key)&&remove[key]>0){remove[key]--;continue;}
                next.InsertAce(next.Count,ace);
            }
            var desired=new byte[next.BinaryLength];next.GetBinaryForm(desired,0);report["desired_sha256"]=ByteHash(desired);
            if(owned.Count>0){
                current.descriptor.DiscretionaryAcl=next;
                var descriptor=new byte[current.descriptor.BinaryLength];current.descriptor.GetBinaryForm(descriptor,0);
                var native=Marshal.AllocHGlobal(descriptor.Length);try{
                    Marshal.Copy(descriptor,0,native,descriptor.Length);uint information=4;report["set_attempted"]=true;
                    var returned=SetUserObjectSecurity(handle,ref information,native);report["SetUserObjectSecurity_returned"]=returned;
                    report["SetUserObjectSecurity_error"]=returned?0:Marshal.GetLastWin32Error();
                    if(!returned)return report;
                }finally{Marshal.FreeHGlobal(native);}
            }
            var after=ReadDacl(handle);report["after_sha256"]=after.hash;
            report["confirmed"]=explained&&SameBytes(after.bytes,desired);
            report["known_added_aces_removed"]=owned.Count;return report;
        }catch(Exception e){report["error_type"]=e.GetType().Name;return report;}
    }
    static void NewText(string path,string text) {
        var bytes=new UTF8Encoding(false).GetBytes(text);
        using(var stream=new FileStream(path,FileMode.CreateNew,FileAccess.Write,FileShare.Read))stream.Write(bytes,0,bytes.Length);
    }
    static string LocalRun(string runId) {
        if(runId==null||runId.Length!=32)throw new InvalidOperationException("Exact local RunId required");
        foreach(char c in runId)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))throw new InvalidOperationException("Exact local RunId required");
        return "C:\\PaneBindV1\\MediumDesktop\\"+runId;
    }
    static string ChildInput(string runId) {return LocalRun(runId)+"\\Input";}
    static string ChildOutput(string runId) {return LocalRun(runId)+"\\Output";}
    static void PlainDirectory(string path) {
        if(!Directory.Exists(path)||(File.GetAttributes(path)&FileAttributes.ReparsePoint)!=0)throw new InvalidOperationException("Exact plain guest-local directory required");
    }
    static void NewDirectory(string path) {
        if(Directory.Exists(path)||File.Exists(path))throw new InvalidOperationException("Guest-local contrast never reuses a RunId directory");
        Directory.CreateDirectory(path);PlainDirectory(path);
    }
    static void NewDirectoryAccess(string path,string account,bool writable) {
        // Only newly created directories BELOW the fresh exact RunId receive
        // ACLs. No ancestor, mapped folder, profile or global ACL is changed.
        var security=new DirectorySecurity();security.SetAccessRuleProtection(true,false);
        using(var identity=WindowsIdentity.GetCurrent()){
            security.SetOwner(identity.User);
            foreach(var sid in new SecurityIdentifier[]{identity.User,new SecurityIdentifier("S-1-5-18"),new SecurityIdentifier("S-1-5-32-544")})
                security.AddAccessRule(new FileSystemAccessRule(sid,FileSystemRights.FullControl,InheritanceFlags.ContainerInherit|InheritanceFlags.ObjectInherit,PropagationFlags.None,AccessControlType.Allow));
        }
        security.AddAccessRule(new FileSystemAccessRule(new SecurityIdentifier(account),
            writable?FileSystemRights.Modify:FileSystemRights.ReadAndExecute,
            InheritanceFlags.ContainerInherit|InheritanceFlags.ObjectInherit,PropagationFlags.None,AccessControlType.Allow));
        Directory.SetAccessControl(path,security);
    }
    static Dictionary<string,object> CopyPlain(string source,string destination,int maximum) {
        var file=new FileInfo(source);
        if(!file.Exists||file.Length>maximum||(file.Attributes&(FileAttributes.Directory|FileAttributes.ReparsePoint))!=0)
            throw new InvalidOperationException("Exact bounded plain copy source required");
        var bytes=File.ReadAllBytes(source);if(bytes.Length>maximum)throw new InvalidOperationException("Copy source byte bound exceeded");
        var hash=ByteHash(bytes);
        using(var stream=new FileStream(destination,FileMode.CreateNew,FileAccess.Write,FileShare.Read))stream.Write(bytes,0,bytes.Length);
        var copied=new FileInfo(destination);var after=File.ReadAllBytes(destination);
        bool exact=copied.Exists&&copied.Length==bytes.Length&&(copied.Attributes&FileAttributes.ReparsePoint)==0&&SameBytes(bytes,after);
        if(!exact)throw new InvalidOperationException("Guest-local copy hash/size verification failed");
        return new Dictionary<string,object>{{"source",source},{"destination",destination},{"size",bytes.Length},{"sha256",hash},{"copied_sha256",ByteHash(after)},{"hash_and_size_match",true}};
    }
    static Dictionary<string,object> PrepareLocalIo(string runId,string account,string scriptHash) {
        string parent="C:\\PaneBindV1\\MediumDesktop",root=LocalRun(runId),input=ChildInput(runId),output=ChildOutput(runId);
        if(!Directory.Exists(parent)){if(File.Exists(parent))throw new InvalidOperationException("Local contrast parent is not a directory");Directory.CreateDirectory(parent);}
        PlainDirectory(parent);NewDirectory(root);NewDirectoryAccess(root,account,false);
        NewDirectory(input);NewDirectoryAccess(input,account,false);NewDirectory(output);NewDirectoryAccess(output,account,true);
        var files=new List<object>();
        var script=CopyPlain("C:\\PaneBindV1\\Input\\v1-medium-desktop-probe-guest.ps1",input+"\\v1-medium-desktop-probe-guest.ps1",262144);
        if((string)script["sha256"]!=scriptHash)throw new InvalidOperationException("Local child script changed after original immutable guard");files.Add(script);
        var manifest=CopyPlain("C:\\PaneBindV1\\Input\\manifest.json",input+"\\manifest.json",1048576);files.Add(manifest);
        files.Add(CopyPlain("C:\\PaneBindV1\\Input\\panebind-v1-input.marker",input+"\\panebind-v1-input.marker",34));
        files.Add(CopyPlain("C:\\PaneBindV1\\Output\\panebind-v1-output.marker",output+"\\panebind-v1-output.marker",34));
        return new Dictionary<string,object>{{"hypothesis","guest_local_IO_contrast_NOT_YET_PROVEN"},{"io_contract","panebind-v1-guest-local-io-contrast/1"},
            {"child_input",input},{"child_output",output},{"manifest_sha256",manifest["sha256"]},{"copied_files",files},
            {"only_fresh_RunId_directory_ACLs_changed",true},{"child_input_access","exact_new_account_ReadAndExecute"},
            {"child_output_access","exact_new_account_Modify"},{"other_SID_actual_file_IO_before_child","NOT_RUN"},{"full_Shell_PASS_inferred",false}};
    }
    static string ChildPhasePath(string runId) {return ChildOutput(runId)+"\\"+runId+"-medium-desktop-child-phase.jsonl";}
    static void ChildPhase(Dictionary<string,object> result,string runId,string phase) {
        // Fixed child-only stages, not a general logger. No credentials,
        // binding nonce, document data, callback or product input is recorded.
        try {
            string path=ChildPhasePath(runId);var file=new FileInfo(path);
            if(!file.Exists||file.Length>16384||(file.Attributes&FileAttributes.ReparsePoint)!=0)throw new InvalidOperationException("Exact bounded child phase file required");
            if(File.ReadAllLines(path,Encoding.UTF8).Length>=12)throw new InvalidOperationException("Child phase line bound reached");
            var row="{\"schema\":\"panebind-v1-medium-desktop-phase/1\",\"run_id\":\""+runId+"\",\"phase\":\""+phase+
                "\",\"pid\":"+Process.GetCurrentProcess().Id+",\"tid\":"+GetCurrentThreadId()+",\"utc\":\""+DateTime.UtcNow.ToString("o")+"\"}\n";
            var bytes=new UTF8Encoding(false).GetBytes(row);
            using(var stream=new FileStream(path,FileMode.Append,FileAccess.Write,FileShare.Read)){
                if(stream.Length+bytes.Length>16384)throw new InvalidOperationException("Child phase byte bound reached");stream.Write(bytes,0,bytes.Length);
            }
        }catch(Exception e){result["child_phase_recording_failed"]=true;result["child_phase_error_type"]=e.GetType().Name;}
    }
    static Dictionary<string,object> ReadChildPhases(string runId,bool exited) {
        var report=new Dictionary<string,object>{{"status","NOT_PRESENT"},{"relative_path",runId+"-medium-desktop-child-phase.jsonl"},{"max_lines",12},{"max_bytes",16384}};
        if(!exited){report["status"]="CHILD_EXIT_NOT_CONFIRMED_NOT_READ";return report;}
        try {
            var file=new FileInfo(ChildPhasePath(runId));if(!file.Exists)return report;
            if(file.Length>16384||(file.Attributes&FileAttributes.ReparsePoint)!=0)throw new InvalidOperationException("Exact bounded child phase file required");
            var bytes=File.ReadAllBytes(file.FullName);var text=Encoding.UTF8.GetString(bytes);var lines=File.ReadAllLines(file.FullName,Encoding.UTF8);
            if(bytes.Length>16384||lines.Length>12)throw new InvalidOperationException("Child phase evidence bound exceeded");
            report["status"]="COLLECTED_AFTER_EXACT_CHILD_EXIT";report["size"]=bytes.Length;report["sha256"]=ByteHash(bytes);report["line_count"]=lines.Length;report["text"]=text;
            report["mapped_output_copy"]=CopyPlain(file.FullName,"C:\\PaneBindV1\\Output\\"+runId+"-medium-desktop-child-phase.jsonl",16384);
            report["mapped_output_copy_confirmed"]=true;
        }catch(Exception e){report["status"]="UNKNOWN";report["error_type"]=e.GetType().Name;}
        return report;
    }
    static void CollectChildReceipt(Dictionary<string,object> result,string runId) {
        // Called once, only after exact child exit (including a bounded test
        // termination). A failure receipt is retained, never converted to a
        // successful child/desktop/cleanup result.
        result["child_receipt_collection_attempted"]=true;
        try {
            var path=ChildOutput(runId)+"\\"+runId+"-medium-desktop-child.json";var file=new FileInfo(path);
            if(!file.Exists){result["child_receipt_collection"]="NOT_PRESENT_AFTER_EXACT_CHILD_EXIT";return;}
            if(file.Length>65536||(file.Attributes&FileAttributes.ReparsePoint)!=0)throw new InvalidOperationException("Bounded plain local child receipt required");
            result["child_receipt_text"]=File.ReadAllText(path,Encoding.UTF8);
            result["child_receipt_sha256"]=ByteHash(File.ReadAllBytes(path));
            result["child_receipt_mapped_output_copy"]=CopyPlain(path,"C:\\PaneBindV1\\Output\\"+runId+"-medium-desktop-child.json",65536);
            result["child_receipt_mapped_output_copy_confirmed"]=true;result["child_receipt_collection"]="COLLECTED_AFTER_EXACT_CHILD_EXIT";
        }catch(Exception e){result["child_receipt_collection"]="UNKNOWN";result["child_receipt_collection_error_type"]=e.GetType().Name;}
    }
    static Dictionary<string,object> ReadNativeOutput(string path) {
        // The frozen identity entry emits one small JSON line. Only read after
        // exact process exit; an empty file is not evidence that entry failed.
        var report=new Dictionary<string,object>{{"status","UNKNOWN"},{"maximum_bytes",4096}};
        var file=new FileInfo(path);
        if(!file.Exists||(file.Attributes&FileAttributes.ReparsePoint)!=0||file.Length>4096){report["reason"]="missing_nonplain_or_over_bound";return report;}
        var bytes=File.ReadAllBytes(path);if(bytes.Length>4096)throw new InvalidOperationException("Native output bound changed");
        report["size"]=bytes.Length;report["sha256"]=ByteHash(bytes);report["text"]=new UTF8Encoding(false,true).GetString(bytes);
        report["status"]=bytes.Length==0?"EMPTY_ENTRY_UNKNOWN":"CAPTURED_NOT_YET_VALIDATED";return report;
    }
    static Dictionary<string,object> RunNativeIdentity(IntPtr token,string expectedSid,string expectedLogon,int session,bool standard,
        string runId,string driverSource,string imageHash,long imageSize,string phase,IntPtr station,IntPtr desktop,
        AclState beforeStation,AclState beforeDesktop,out List<AddedAce> recordedStation,out List<AddedAce> recordedDesktop,bool asUser=false) {
        // Exactly two callers: current WDAG token baseline, then the freshly
        // credentialed standard account. Both use the same image/API/stdio.
        recordedStation=null;recordedDesktop=null;
        var result=new Dictionary<string,object>{{"phase",phase},{"identity_observed",false},{"status","UNKNOWN"},
            {"child_created",false},{"child_process_exited",false},{"child_cleanup_confirmed",true},
            {"test_child_force_termination_attempted",false},{"CreateProcessWithTokenW_attempted",false},
            {"lpDesktop","NULL"},{"LOGON_WITH_PROFILE",true},{"requested_command","--build-identity"},
            {"stdout_empty_means_native_entry_not_reached",false},{"GUI_or_Shell_tested",false}};
        if(asUser){result["CreateProcessAsUserW_attempted"]=false;result["launch_api"]="CreateProcessAsUserW";
            result["lpDesktop"]="winsta0\\default";result["LOGON_WITH_PROFILE"]=false;}
        var child=new ProcessInfo();IntPtr childToken=IntPtr.Zero;
        FileStream input=null,output=null,error=null;bool resumed=false;string stage="exact_image_recheck";
        string executable=asUser?"C:\\PaneBindV1\\Input\\panebind-v1-guest-fixture.exe":ChildInput(runId)+"\\panebind-v1-guest-fixture.exe";
        string prefix=(asUser?LocalRun(runId)+"\\AsUserIO":ChildOutput(runId))+"\\"+phase;
        try {
            var file=new FileInfo(executable);
            if(!file.Exists||file.Length!=imageSize||file.Length>16777216||(file.Attributes&FileAttributes.ReparsePoint)!=0||
                ByteHash(File.ReadAllBytes(executable))!=imageHash)throw new InvalidOperationException("Exact native image changed");
            result["image_sha256"]=imageHash;result["image_size"]=imageSize;
            input=new FileStream(prefix+"-stdin.bin",FileMode.CreateNew,FileAccess.ReadWrite,FileShare.Read|FileShare.Inheritable);
            output=new FileStream(prefix+"-stdout.txt",FileMode.CreateNew,FileAccess.ReadWrite,FileShare.Read|FileShare.Inheritable);
            error=new FileStream(prefix+"-stderr.txt",FileMode.CreateNew,FileAccess.ReadWrite,FileShare.Read|FileShare.Inheritable);
            var startup=new Startup{size=(uint)Marshal.SizeOf(typeof(Startup)),desktop=asUser?"winsta0\\default":null,flags=0x100,
                input=input.SafeFileHandle.DangerousGetHandle(),output=output.SafeFileHandle.DangerousGetHandle(),error=error.SafeFileHandle.DangerousGetHandle()};
            result["STARTF_USESTDHANDLES"]=true;result["stdio_kind"]="fresh_local_CreateNew_inheritable_FileStream";
            result["stdio_handle_validity_in_child"]="UNKNOWN_UNTIL_ACTUAL_IDENTITY_OUTPUT";
            // The old CPWT observation is unchanged. The new explicit route
            // uses only AsUser, with a real inheritable stdio baseline first.
            string api=asUser?"CreateProcessAsUserW":"CreateProcessWithTokenW";
            stage=api;result[api+"_attempted"]=true;result["launch_api"]=api;
            result["lpDesktop"]=asUser?"winsta0\\default":"NULL";result["LOGON_WITH_PROFILE"]=!asUser;
            result["bInheritHandles"]=asUser;
            bool launched=asUser?
                CreateProcessAsUserW(token,executable,new StringBuilder("\""+executable+"\" --build-identity"),IntPtr.Zero,IntPtr.Zero,true,
                    0x08000004,IntPtr.Zero,"C:\\PaneBindV1\\Input",ref startup,out child):
                CreateProcessWithTokenW(token,1,executable,new StringBuilder("\""+executable+"\" --build-identity"),
                    0x08000004,IntPtr.Zero,ChildInput(runId),ref startup,out child);
            result[api+"_succeeded"]=launched;result[api+"_error"]=launched?0:Marshal.GetLastWin32Error();
            result["child_created"]=launched;result["child_cleanup_confirmed"]=!launched;
            stage="post_launch_dacl_inventory";
            var afterStation=ReadDacl(station);var afterDesktop=ReadDacl(desktop);
            result["station_dacl_after_sha256"]=afterStation.hash;result["desktop_dacl_after_sha256"]=afterDesktop.hash;
            bool explained;
            if(standard&&!asUser){
                bool stationExplained,desktopExplained;List<object> stationDelta,desktopDelta;
                recordedStation=Delta(beforeStation,afterStation,expectedSid,expectedLogon,out stationExplained,out stationDelta);
                recordedDesktop=Delta(beforeDesktop,afterDesktop,expectedSid,expectedLogon,out desktopExplained,out desktopDelta);
                result["station_added_aces"]=stationDelta;result["desktop_added_aces"]=desktopDelta;explained=stationExplained&&desktopExplained;
            }else{
                // A baseline uses an existing account: never guess ownership
                // of same-SID ACL changes or remove somebody else's ACE.
                explained=SameBytes(beforeStation.bytes,afterStation.bytes)&&SameBytes(beforeDesktop.bytes,afterDesktop.bytes);
                result["baseline_dacls_unchanged"]=explained;
            }
            result["dacl_delta_explained"]=explained;
            if(!launched||!explained){result["reason"]=!launched?"native_launch_failed":"unexplained_dacl_change_not_resumed";return result;}
            result["child_pid"]=child.pid;result["child_tid"]=child.tid;
            stage="actual_child_birth";FileTime birth,exit,kernel,user;
            bool birthRead=GetProcessTimes(child.process,out birth,out exit,out kernel,out user);
            result["GetProcessTimes_succeeded"]=birthRead;result["GetProcessTimes_error"]=birthRead?0:Marshal.GetLastWin32Error();
            if(!birthRead)throw new InvalidOperationException("Actual native child birth unavailable");
            result["child_birth_filetime"]=((ulong)birth.high<<32)|birth.low;
            stage="actual_child_token";
            bool opened=OpenProcessToken(child.process,8,out childToken);result["OpenProcessToken_succeeded"]=opened;
            result["OpenProcessToken_requested_access"]=8;result["OpenProcessToken_error"]=opened?0:Marshal.GetLastWin32Error();
            if(!opened)throw new InvalidOperationException("Actual native child token unavailable");
            bool ordinary;var actualFacts=TokenFacts(childToken,expectedSid,session,out ordinary);result["actual_child_token"]=actualFacts;
            bool match=TokenSid(childToken)==expectedSid&&TokenNumber(childToken,12)==session&&LogonSid(childToken)==expectedLogon;
            if(standard)match&=ordinary;
            else {
                bool ignored;var requestedFacts=TokenFacts(token,expectedSid,session,out ignored);
                match&=(int)actualFacts["integrity_rid"]==(int)requestedFacts["integrity_rid"]&&
                    (bool)actualFacts["elevated"]==(bool)requestedFacts["elevated"]&&
                    (bool)actualFacts["admin_enabled"]==(bool)requestedFacts["admin_enabled"]&&
                    (bool)actualFacts["ui_access"]==(bool)requestedFacts["ui_access"]&&
                    (bool)actualFacts["app_container"]==(bool)requestedFacts["app_container"];
            }
            result["actual_child_identity_matches_requested_token"]=match;
            if(!match)throw new InvalidOperationException("Actual native child identity rejected");
            stage="child_desktop_before_resume";
            try{result["actual_child_thread_desktop_before_resume"]=ObjectName(GetThreadDesktop(child.tid));}
            catch(Exception e){result["actual_child_thread_desktop_before_resume"]="UNKNOWN";result["child_desktop_query_error_type"]=e.GetType().Name;}
            stage="ResumeThread";uint resume=ResumeThread(child.thread);result["ResumeThread_return"]=resume;
            if(resume!=1)throw new InvalidOperationException("Exact suspended native child resume not confirmed");resumed=true;
            stage="bounded_native_child_exit";uint waited=WaitForSingleObject(child.process,20000);result["child_wait_return"]=waited;
            if(waited!=0){result["reason"]="bounded_native_exit_not_observed_entry_UNKNOWN";return result;}
            uint code;bool codeRead=GetExitCodeProcess(child.process,out code);result["GetExitCodeProcess_succeeded"]=codeRead;
            if(!codeRead)throw new InvalidOperationException("Actual native exit code unavailable");
            result["child_exit_code"]=code;result["child_process_exited"]=true;
            result["reason"]="native_stdout_identity_requires_validation";return result;
        }catch(Exception e){result["reason"]="native_identity_stage_UNKNOWN";result["exception_stage"]=stage;result["error_type"]=e.GetType().Name;
            result["exception_hresult"]=e.HResult;return result;}
        finally {
            if(child.process!=IntPtr.Zero){
                uint waited=WaitForSingleObject(child.process,0);result["test_child_force_termination_attempted"]=waited==258;
                if(waited==258){bool killed=TerminateProcess(child.process,78);result["test_child_force_termination_returned"]=killed;
                    result["test_child_force_termination_error"]=killed?0:Marshal.GetLastWin32Error();waited=WaitForSingleObject(child.process,5000);}
                result["child_process_exited"]=waited==0;result["child_cleanup_confirmed"]=waited==0;result["child_ever_resumed"]=resumed;
                if(waited==0&&!result.ContainsKey("child_exit_code")){uint code;if(GetExitCodeProcess(child.process,out code))result["child_exit_code"]=code;}
                CloseHandle(child.process);
            }
            if(child.thread!=IntPtr.Zero)CloseHandle(child.thread);if(childToken!=IntPtr.Zero)CloseHandle(childToken);
            if(input!=null)input.Dispose();if(output!=null)output.Dispose();if(error!=null)error.Dispose();
            if((bool)result["child_process_exited"]){
                try {
                    var stdout=ReadNativeOutput(prefix+"-stdout.txt");var stderr=ReadNativeOutput(prefix+"-stderr.txt");
                    result["stdout"]=stdout;result["stderr"]=stderr;
                    string expected="{\"product\":\"PaneBind V1 guest fixture\",\"source_commit\":\""+driverSource+
                        "\",\"host_synthetic_input\":\"forbidden\",\"requires_medium_guest\":true,\"guest_contract\":\"panebind-v1-guest-contract/2\",\"product_interface\":\"panebind-v1-product-guest-interface/1\"}\n";
                    bool observed=resumed&&result.ContainsKey("child_exit_code")&&(uint)result["child_exit_code"]==0&&
                        !(bool)result["test_child_force_termination_attempted"]&&result.ContainsKey("actual_child_identity_matches_requested_token")&&
                        (bool)result["actual_child_identity_matches_requested_token"]&&stdout.ContainsKey("text")&&(string)stdout["text"]==expected;
                    result["identity_observed"]=observed;result["status"]=observed?"NATIVE_IDENTITY_OBSERVED_NOT_GUI_OR_SHELL":"UNKNOWN";
                    if(observed)result["reason"]="exact_native_identity_stdout_and_actual_token_exit_observed";
                    else if(!result.ContainsKey("reason")||(string)result["reason"]=="native_stdout_identity_requires_validation")
                        result["reason"]="native_identity_output_or_exit_UNKNOWN";
                }catch(Exception e){result["output_collection_error_type"]=e.GetType().Name;}
            }
        }
    }
    static uint ActiveJobProcesses(IntPtr job) {
        var data=Marshal.AllocHGlobal(48);try {uint needed;
            if(!QueryInformationJobObject(job,1,data,48,out needed))throw new InvalidOperationException("Exact fixture job accounting unavailable");
            return (uint)Marshal.ReadInt32(data,40);
        }finally{Marshal.FreeHGlobal(data);}
    }
    static IntPtr NativeClientEnvironment(string runId) {
        // A child-only block, never SetEnvironmentVariable or a host setting.
        // Keep inherited profile facts honest: this is not a loaded full Shell.
        var variables=new SortedDictionary<string,string>(StringComparer.OrdinalIgnoreCase);
        foreach(System.Collections.DictionaryEntry entry in Environment.GetEnvironmentVariables())variables[(string)entry.Key]=(string)entry.Value;
        variables["TEMP"]="C:\\Windows\\Temp\\PaneBindV1-"+runId;variables["TMP"]=variables["TEMP"];
        variables["PBV1_STANDARD_BOOTSTRAP_PID"]=Process.GetCurrentProcess().Id.ToString();
        var block=new StringBuilder();foreach(var entry in variables)block.Append(entry.Key).Append('=').Append(entry.Value).Append('\0');
        block.Append('\0');return Marshal.StringToHGlobalUni(block.ToString());
    }
    static Dictionary<string,object> RunStandardNativeFixture(IntPtr token,string account,string logon,int session,string bootstrapSid,string bootstrapLogon,
        string runId,string productHash,uint timeoutSeconds,IntPtr station,IntPtr desktop,AclState stationBefore,AclState desktopBefore) {
        var result=new Dictionary<string,object>{{"status","BLOCKED"},{"fixture_started",false},{"product_cleanup_pass",false},
            {"all_children_stopped",false},{"forced_test_cleanup",false},{"launch_api","CreateProcessAsUserW"},
            {"same_standard_credential_token",true},{"suite","NativeClientsOnly"},{"product_started",false},
            {"product_function_result","NOT_RUN"},{"full_Shell_PASS_inferred",false}};
        var child=new ProcessInfo();IntPtr job=IntPtr.Zero,environment=IntPtr.Zero,childToken=IntPtr.Zero;bool assigned=false;
        string stage="same_standard_account_and_bootstrap_recheck";
        try {
            var bootstrap=Desktop(true);result["bootstrap_desktop_recheck"]=bootstrap;
            if((int)bootstrap["session_id"]!=session)throw new InvalidOperationException("Bootstrap desktop session changed");
            using(var own=Process.GetCurrentProcess()){
                result["bootstrap_actual_PID"]=own.Id;result["bootstrap_actual_TID"]=GetCurrentThreadId();
                FileTime ownBirth,ownExit,ownKernel,ownUser;if(!GetProcessTimes(own.Handle,out ownBirth,out ownExit,out ownKernel,out ownUser))
                    throw new InvalidOperationException("Actual bootstrap birth unavailable");
                result["bootstrap_actual_birth_filetime"]=((ulong)ownBirth.high<<32)|ownBirth.low;
                IntPtr ownToken;if(!OpenProcessToken(own.Handle,8,out ownToken))throw new InvalidOperationException("Actual bootstrap token recheck unavailable");
                try {bool ignored;var facts=TokenFacts(ownToken,bootstrapSid,session,out ignored);result["bootstrap_actual_token_recheck"]=facts;
                    if(TokenSid(ownToken)!=bootstrapSid||LogonSid(ownToken)!=bootstrapLogon||TokenNumber(ownToken,12)!=session||
                       (int)facts["integrity_rid"]!=12288||!(bool)facts["elevated"]||!(bool)facts["admin_enabled"])
                        throw new InvalidOperationException("Exact WDAG High bootstrap identity changed");
                }finally{CloseHandle(ownToken);}
            }
            uint accountStatus;if(AccountSid("PBV1D_"+runId.Substring(0,12),out accountStatus)!=account||accountStatus!=0)
                throw new InvalidOperationException("Fresh exact account identity changed");
            bool ordinary;result["launch_token"]=TokenFacts(token,account,session,out ordinary);
            if(!ordinary||TokenNumber(token,12)!=session||LogonSid(token)!=logon)throw new InvalidOperationException("Standard token no longer exact");
            if(!SameBytes(ReadDacl(station).bytes,stationBefore.bytes)||!SameBytes(ReadDacl(desktop).bytes,desktopBefore.bytes))
                throw new InvalidOperationException("Desktop DACL changed; no native fixture launched");
            string executable="C:\\PaneBindV1\\Input\\panebind-v1-guest-fixture.exe";
            string actualDirectory="C:\\PaneBindV1\\Output\\"+runId+"-product-Release";
            NewDirectory(actualDirectory);var copy=CopyPlain("C:\\PaneBindV1\\Input\\panebind.exe",actualDirectory+"\\panebind.exe",16777216);
            if((string)copy["sha256"]!=productHash)throw new InvalidOperationException("Untouched product copy identity changed");
            NewText(actualDirectory+"\\portable.flag","PaneBind V1 portable test candidate\n");result["product_copy"]=copy;
            string arguments="--run-id "+runId+" --output \"C:\\PaneBindV1\\Output\" --product \""+actualDirectory+"\\panebind.exe\" --suite NativeClientsOnly";
            result["arguments"]=arguments;result["requested_executable"]=executable;result["requested_desktop"]="winsta0\\default";
            result["child_environment_policy"]="inherited_bootstrap_except_exact_Run_TEMP_TMP_not_full_profile";
            result["profile_provenance"]="WDAG_bootstrap_environment_inherited_not_standard_user_profile";
            result["LoadUserProfile_called"]=false;result["CreateEnvironmentBlock_called"]=false;
            result["ordinary_standard_user_profile_proven"]=false;
            var profileFacts=new Dictionary<string,object>();foreach(string key in new string[]{"USERPROFILE","APPDATA","LOCALAPPDATA","HOMEDRIVE","HOMEPATH"})
                profileFacts[key]=Environment.GetEnvironmentVariable(key);result["inherited_profile_environment"]=profileFacts;
            result["child_TEMP"]="C:\\Windows\\Temp\\PaneBindV1-"+runId;
            environment=NativeClientEnvironment(runId);var startup=new Startup{size=(uint)Marshal.SizeOf(typeof(Startup)),desktop="winsta0\\default"};
            stage="CreateProcessAsUserW_native_clients";
            bool launched=CreateProcessAsUserW(token,executable,new StringBuilder("\""+executable+"\" "+arguments),IntPtr.Zero,IntPtr.Zero,false,
                0x08000404,environment,"C:\\PaneBindV1\\Output",ref startup,out child);
            result["CreateProcessAsUserW_attempted"]=true;result["CreateProcessAsUserW_succeeded"]=launched;
            result["CreateProcessAsUserW_error"]=launched?0:Marshal.GetLastWin32Error();result["bInheritHandles"]=false;
            if(!launched){result["reason"]="native_client_creation_failed_no_fallback";return result;}
            result["fixture_pid"]=child.pid;result["fixture_tid"]=child.tid;
            FileTime birth,exit,kernel,user;if(!GetProcessTimes(child.process,out birth,out exit,out kernel,out user))throw new InvalidOperationException("Native fixture birth unavailable");
            result["fixture_birth_filetime"]=((ulong)birth.high<<32)|birth.low;
            if(!OpenProcessToken(child.process,8,out childToken))throw new InvalidOperationException("Native fixture actual token unavailable");
            result["actual_child_token"]=TokenFacts(childToken,account,session,out ordinary);
            if(!ordinary||TokenNumber(childToken,12)!=session||LogonSid(childToken)!=logon)throw new InvalidOperationException("Native fixture actual token mismatch");
            result["actual_child_identity_matches_requested_token"]=true;
            job=CreateJobObjectW(IntPtr.Zero,null);if(job==IntPtr.Zero)throw new InvalidOperationException("Native fixture-only job unavailable");
            var limits=new JobExtended();limits.basic.flags=0x2000;
            if(!SetInformationJobObject(job,9,ref limits,(uint)Marshal.SizeOf(typeof(JobExtended)))||!AssignProcessToJobObject(job,child.process))
                throw new InvalidOperationException("Native fixture-only job assignment failed");assigned=true;
            if(!SameBytes(ReadDacl(station).bytes,stationBefore.bytes)||!SameBytes(ReadDacl(desktop).bytes,desktopBefore.bytes))
                throw new InvalidOperationException("DACL changed before native fixture resume");
            stage="ResumeThread_native_clients";uint resumed=ResumeThread(child.thread);result["ResumeThread_return"]=resumed;
            if(resumed!=1)throw new InvalidOperationException("Native fixture resume not confirmed");result["fixture_started"]=true;
            result["creation_flags"]="CREATE_SUSPENDED|CREATE_NO_WINDOW|CREATE_UNICODE_ENVIRONMENT";
            uint waited=WaitForSingleObject(child.process,timeoutSeconds*1000);result["wait_result"]=waited;
            if(waited!=0){result["reason"]="bounded_native_fixture_exit_not_observed";return result;}
            uint code;if(!GetExitCodeProcess(child.process,out code))throw new InvalidOperationException("Native fixture exit unavailable");result["fixture_exit_code"]=code;
            uint active=ActiveJobProcesses(job);result["descendants_alive_at_fixture_exit"]=active;
            result["status"]=code==0&&active==0?"FIXTURE_EXITED_ZERO_EVIDENCE_REQUIRED":"FIXTURE_FAILED";
            result["reason"]="actual_driver_receipts_required_not_product_or_Shell_PASS";return result;
        }catch(Exception e){result["reason"]="standard_native_fixture_stage_UNKNOWN";result["exception_stage"]=stage;
            result["error_type"]=e.GetType().Name;result["exception_hresult"]=e.HResult;return result;}
        finally {
            try {
                if(assigned){uint active=ActiveJobProcesses(job);if(active!=0){result["forced_test_cleanup"]=true;
                    result["TerminateJobObject_attempted"]=true;bool killed=TerminateJobObject(job,78);result["TerminateJobObject_returned"]=killed;
                    result["TerminateJobObject_error"]=killed?0:Marshal.GetLastWin32Error();WaitForSingleObject(child.process,5000);WaitForSingleObject(job,1000);}
                    uint after=ActiveJobProcesses(job);result["job_active_after_cleanup"]=after;result["all_children_stopped"]=after==0;
                }else if(child.process!=IntPtr.Zero){uint waited=WaitForSingleObject(child.process,0);if(waited==258){result["forced_test_cleanup"]=true;
                    result["unassigned_exact_child_terminate_returned"]=TerminateProcess(child.process,78);waited=WaitForSingleObject(child.process,5000);}
                    result["all_children_stopped"]=waited==0;
                }else result["all_children_stopped"]=true;
            }catch(Exception e){result["cleanup_error_type"]=e.GetType().Name;result["all_children_stopped"]=false;}
            if(job!=IntPtr.Zero)CloseHandle(job);if(childToken!=IntPtr.Zero)CloseHandle(childToken);
            if(child.thread!=IntPtr.Zero)CloseHandle(child.thread);if(child.process!=IntPtr.Zero)CloseHandle(child.process);
            if(environment!=IntPtr.Zero)Marshal.FreeHGlobal(environment);
        }
    }
    public static Dictionary<string,object> RunNativeParent(string runId,string scriptHash,string driverSource,string imageHash,long imageSize) {
        return RunNativeParent(runId,scriptHash,driverSource,imageHash,imageSize,false,null,900);
    }
    public static Dictionary<string,object> RunNativeParent(string runId,string scriptHash,string driverSource,string imageHash,long imageSize,
        bool asUserClients,string productHash,uint nativeTimeout) {
        if(asUserClients&&(nativeTimeout<30||nativeTimeout>1800||productHash==null||productHash.Length!=64))
            throw new InvalidOperationException("Bounded exact standard native-client contract required");
        var desktop=Desktop(true);int session=(int)desktop["session_id"];string name="PBV1D_"+runId.Substring(0,12);
        var result=new Dictionary<string,object>{{"status","UNKNOWN"},{"probe_mode",asUserClients?"StandardGuestNativeClients":"MediumNativeIdentityOnly"},{"bootstrap_desktop",desktop},
            {"account_created",false},{"account_cleanup_confirmed",false},{"dacl_cleanup_confirmed",false},{"all_probe_children_stopped",true},
            {"product_started",false},{"product_GUI_started",false},{"explorer_started",false},{"synthetic_input_sent",false},
            {"host_configuration_changed",false},{"GUI_or_Shell_result","NOT_RUN"},{"baseline_required_before_account",true},
            {"standard_native_clients_requested",asUserClients},{"native_clients_fixture_started",false}};
        IntPtr station=IntPtr.Zero,defaultDesktop=IntPtr.Zero,bootstrapToken=IntPtr.Zero,token=IntPtr.Zero;
        IntPtr password=IntPtr.Zero,nameMemory=IntPtr.Zero,comment=IntPtr.Zero;
        AclState beforeStation=null,beforeDesktop=null;List<AddedAce> recordedStation=null,recordedDesktop=null;
        string account=null,logon=null;bool created=false,standardAttempted=false;const int passwordLength=48;
        string stage="bootstrap_query_cleanup_rights";
        try {
            station=OpenWindowStationW("WinSta0",false,asUserClients?0x20000u:0x60000u);
            defaultDesktop=OpenDesktopW("Default",0,false,asUserClients?0x20001u:0x60081u);
            result["station_open_for_query_cleanup"]=station!=IntPtr.Zero;result["desktop_open_for_query_cleanup"]=defaultDesktop!=IntPtr.Zero;
            if(station==IntPtr.Zero||defaultDesktop==IntPtr.Zero){result["reason"]="query_cleanup_rights_unavailable_no_account_or_launch";return result;}
            beforeStation=ReadDacl(station);beforeDesktop=ReadDacl(defaultDesktop);
            result["station_dacl_before_sha256"]=beforeStation.hash;result["desktop_dacl_before_sha256"]=beforeDesktop.hash;
            using(var current=Process.GetCurrentProcess()){
                if(!OpenProcessToken(current.Handle,0xB,out bootstrapToken))throw new InvalidOperationException("Unmodified current primary token unavailable");
            }
            var bootstrapSid=TokenSid(bootstrapToken);var bootstrapLogon=LogonSid(bootstrapToken);
            bool ignored;result["bootstrap_actual_token"]=TokenFacts(bootstrapToken,bootstrapSid,session,out ignored);
            stage="fresh_guest_local_payload";Dictionary<string,object> local;
            if(asUserClients){
                string parent="C:\\PaneBindV1\\MediumDesktop";if(!Directory.Exists(parent))Directory.CreateDirectory(parent);
                PlainDirectory(parent);NewDirectory(LocalRun(runId));NewDirectory(LocalRun(runId)+"\\AsUserIO");
                local=new Dictionary<string,object>{{"child_input","C:\\PaneBindV1\\Input"},{"stdout_parent",LocalRun(runId)+"\\AsUserIO"},
                    {"directory_DACLs_modified",false},{"stdio_inherited_not_child_file_open",true},{"full_Shell_PASS_inferred",false}};
                result["no_station_desktop_or_directory_DACL_changes_attempted"]=true;
            }else{
                local=PrepareLocalIo(runId,bootstrapSid,scriptHash);
                local["child_input_access"]="current_WDAG_only_before_native_baseline";local["child_output_access"]="current_WDAG_only_before_native_baseline";
            }
            result["guest_local_io"]=local;
            if(!asUserClients){
                var image=CopyPlain("C:\\PaneBindV1\\Input\\panebind-v1-guest-fixture.exe",ChildInput(runId)+"\\panebind-v1-guest-fixture.exe",16777216);
                if((string)image["sha256"]!=imageHash||(int)image["size"]!=imageSize)throw new InvalidOperationException("Frozen native fixture copy differs");
                ((List<object>)local["copied_files"]).Add(image);
            }
            stage="WDAG_same_API_stdio_native_baseline";List<AddedAce> unusedStation,unusedDesktop;
            var baseline=RunNativeIdentity(bootstrapToken,bootstrapSid,bootstrapLogon,session,false,runId,driverSource,imageHash,imageSize,
                "wdag-baseline",station,defaultDesktop,beforeStation,beforeDesktop,out unusedStation,out unusedDesktop,asUserClients);
            result["wdag_baseline"]=baseline;result["all_probe_children_stopped"]=(bool)baseline["child_cleanup_confirmed"];
            if(!(bool)baseline["identity_observed"]||!(bool)baseline["child_cleanup_confirmed"]){result["reason"]="WDAG_stdio_or_native_entry_UNKNOWN_no_account_created";return result;}
            // Baseline success alone never authorizes GUI or ordinary apps.
            // Only now create the one fresh credentialed standard account.
            stage="create_unique_standard_guest_account";
            var random=new byte[passwordLength];using(var rng=RandomNumberGenerator.Create())rng.GetBytes(random);
            password=Marshal.AllocHGlobal((passwordLength+1)*2);const string alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-";
            for(int n=0;n<passwordLength;n++)Marshal.WriteInt16(password,n*2,(short)alphabet[random[n]&63]);Array.Clear(random,0,random.Length);
            Marshal.WriteInt16(password,0,(short)'a');Marshal.WriteInt16(password,2,(short)'A');Marshal.WriteInt16(password,4,(short)'7');Marshal.WriteInt16(password,6,(short)'!');Marshal.WriteInt16(password,passwordLength*2,0);
            nameMemory=Marshal.StringToHGlobalUni(name);comment=Marshal.StringToHGlobalUni("PaneBind disposable native identity probe "+runId);
            var info=new UserInfo1{name=nameMemory,password=password,privilege=1,comment=comment,flags=0x201};uint parameter;
            uint added=NetUserAdd(null,1,ref info,out parameter);result["NetUserAdd_status"]=added;result["NetUserAdd_parameter_error"]=parameter;
            if(added!=0){result["reason"]="unique_standard_account_not_created_or_reused";return result;}
            created=true;result["account_created"]=true;uint sidStatus;account=AccountSid(name,out sidStatus);result["created_sid_query_status"]=sidStatus;
            if(account==null)throw new InvalidOperationException("Fresh standard account SID unknown");result["created_account_sid_sha256"]=Hash(account);
            stage="credential_token";
            bool loggedOn=LogonUserW(name,".",password,2,0,out token);result["LogonUser_succeeded"]=loggedOn;result["LogonUser_error"]=loggedOn?0:Marshal.GetLastWin32Error();
            if(!loggedOn){result["reason"]="standard_credential_logon_failed";return result;}
            bool ordinary;result["credential_token"]=TokenFacts(token,account,session,out ordinary);logon=LogonSid(token);
            result["actual_logon_sid_sha256"]=Hash(logon);
            if(!ordinary||TokenNumber(token,12)!=session){result["reason"]="actual_standard_token_or_session_rejected";return result;}
            stage="exact_fresh_local_account_access";
            if(!asUserClients){
                NewDirectoryAccess(LocalRun(runId),account,false);NewDirectoryAccess(ChildInput(runId),account,false);NewDirectoryAccess(ChildOutput(runId),account,true);
                local["child_input_access"]="exact_new_account_ReadAndExecute";local["child_output_access"]="exact_new_account_Modify";
            }
            stage="standard_same_API_stdio_native_identity";
            var child=RunNativeIdentity(token,account,logon,session,true,runId,driverSource,imageHash,imageSize,
                "standard-native",station,defaultDesktop,beforeStation,beforeDesktop,out recordedStation,out recordedDesktop,asUserClients);
            standardAttempted=(bool)child[asUserClients?"CreateProcessAsUserW_attempted":"CreateProcessWithTokenW_attempted"];
            result["standard_native_child"]=child;result["all_probe_children_stopped"]=(bool)baseline["child_cleanup_confirmed"]&&(bool)child["child_cleanup_confirmed"];
            result["status"]=(bool)child["identity_observed"]?"NATIVE_ENTRY_IDENTITY_OBSERVED_NOT_GUI_OR_SHELL":"UNKNOWN";
            result["reason"]=(bool)child["identity_observed"]?"actual_native_entry_only_GUI_and_Shell_NOT_RUN":"standard_native_entry_or_stdout_UNKNOWN";
            if(asUserClients&&(bool)child["identity_observed"]&&(bool)result["all_probe_children_stopped"]){
                // Same account and ORIGINAL credential token remain alive.
                // Do not return to the WDAG restricted-token launcher.
                stage="same_standard_token_native_clients_followup";
                var launch=RunStandardNativeFixture(token,account,logon,session,bootstrapSid,bootstrapLogon,runId,productHash,nativeTimeout,
                    station,defaultDesktop,beforeStation,beforeDesktop);
                result["native_clients_launch"]=launch;result["native_clients_fixture_started"]=launch["fixture_started"];
                result["synthetic_input_sent"]=(bool)launch["fixture_started"]?(object)null:false;
                result["native_client_input_facts_source"]="actual_fixture_JSONL_not_launch_success";
                result["all_probe_children_stopped"]=(bool)result["all_probe_children_stopped"]&&(bool)launch["all_children_stopped"];
                result["status"]=(string)launch["status"]=="FIXTURE_EXITED_ZERO_EVIDENCE_REQUIRED"&&!(bool)launch["forced_test_cleanup"]?
                    "STANDARD_NATIVE_CLIENTS_EXITED_REVIEW_EVIDENCE":"STANDARD_NATIVE_CLIENTS_FAILED_OR_BLOCKED";
                result["reason"]="native_application_receipts_require_review_product_and_full_Shell_NOT_RUN";
            }
            return result;
        }catch(Exception e){result["reason"]="native_parent_stage_UNKNOWN";result["exception_stage"]=stage;result["error_type"]=e.GetType().Name;
            result["exception_hresult"]=e.HResult;return result;}
        finally {
            bool clean=beforeStation!=null&&beforeDesktop!=null;
            if(!asUserClients&&standardAttempted&&account!=null&&logon!=null){
                if(beforeStation!=null){var report=RestoreOwnAddedAces(station,beforeStation,recordedStation,account,logon);result["station_dacl_cleanup"]=report;clean&=(bool)report["confirmed"];}
                if(beforeDesktop!=null){var report=RestoreOwnAddedAces(defaultDesktop,beforeDesktop,recordedDesktop,account,logon);result["desktop_dacl_cleanup"]=report;clean&=(bool)report["confirmed"];}
            }else try{clean&=beforeStation!=null&&beforeDesktop!=null&&SameBytes(ReadDacl(station).bytes,beforeStation.bytes)&&SameBytes(ReadDacl(defaultDesktop).bytes,beforeDesktop.bytes);}catch{clean=false;}
            result["dacl_cleanup_confirmed"]=clean;
            if(defaultDesktop!=IntPtr.Zero)CloseDesktop(defaultDesktop);if(station!=IntPtr.Zero)CloseWindowStation(station);
            if(bootstrapToken!=IntPtr.Zero)CloseHandle(bootstrapToken);if(token!=IntPtr.Zero)CloseHandle(token);
            if(password!=IntPtr.Zero){for(int n=0;n<=passwordLength;n++)Marshal.WriteInt16(password,n*2,0);Marshal.FreeHGlobal(password);}
            if(nameMemory!=IntPtr.Zero)Marshal.FreeHGlobal(nameMemory);if(comment!=IntPtr.Zero)Marshal.FreeHGlobal(comment);
            var cleanup=new Dictionary<string,object>{{"delete_attempted",false},{"profile_deletion_attempted",false}};result["account_cleanup"]=cleanup;
            if(!created){cleanup["result"]="NOT_CREATED";result["account_cleanup_confirmed"]=true;}
            else try {
                uint status;var actual=AccountSid(name,out status);cleanup["sid_query_status"]=status;
                if(account!=null&&actual==account&&(bool)result["all_probe_children_stopped"]){
                    cleanup["sid_matches_created_account"]=true;cleanup["delete_attempted"]=true;uint removed=NetUserDel(null,name);
                    cleanup["NetUserDel_status"]=removed;uint after;AccountSid(name,out after);cleanup["after_query_status"]=after;
                    result["account_cleanup_confirmed"]=removed==0&&after==2221;
                }else cleanup["result"]="IDENTITY_OR_CHILD_CLEANUP_UNKNOWN_NOT_DELETED";
            }catch(Exception e){cleanup["error_type"]=e.GetType().Name;}
            if(!clean||!(bool)result["account_cleanup_confirmed"]||!(bool)result["all_probe_children_stopped"]){result["status"]="UNKNOWN";result["reason"]="cleanup_not_confirmed";}
        }
    }
    public static Dictionary<string,object> RunParent(string runId,string source,string scriptHash) {
        var desktop=Desktop(true);int session=(int)desktop["session_id"];var name="PBV1D_"+runId.Substring(0,12);
        var result=new Dictionary<string,object>{{"status","UNKNOWN"},{"bootstrap_desktop",desktop},{"created_account_name",name},
            {"account_created",false},{"account_cleanup_confirmed",false},{"dacl_cleanup_confirmed",false},{"child_process_exited",false},
            {"CreateProcessWithTokenW_attempts",0},{"lpDesktop","NULL"},{"LOGON_WITH_PROFILE",true},{"product_started",false},
            {"GetProcessTimes_attempted",false},{"GetProcessTimes_succeeded",null},{"GetProcessTimes_error",null},
            {"OpenProcessToken_attempted",false},{"OpenProcessToken_requested_access",8},{"OpenProcessToken_succeeded",null},{"OpenProcessToken_error",null},
            {"guest_local_root",LocalRun(runId)},{"guest_local_evidence_collected",false},{"guest_local_IO_hypothesis_proven",false},
            {"explorer_started",false},{"synthetic_input_sent",false},{"host_configuration_changed",false}};
        IntPtr password=IntPtr.Zero,nameMemory=IntPtr.Zero,comment=IntPtr.Zero,token=IntPtr.Zero,childToken=IntPtr.Zero;
        IntPtr station=IntPtr.Zero,defaultDesktop=IntPtr.Zero;var child=new ProcessInfo();AclState beforeStation=null,beforeDesktop=null;
        string account=null,logon=null;bool created=false,resumed=false;string stage="before_launch_preconditions";
        List<AddedAce> recordedStation=null,recordedDesktop=null;
        const int passwordLength=48;
        try {
            // Without pre-held read/write-DACL rights no API that might grant
            // access is attempted. No privilege, UAC or account-policy changes.
            station=OpenWindowStationW("WinSta0",false,0x60000);
            result["station_open_for_query_cleanup"]=station!=IntPtr.Zero;result["station_open_error"]=station!=IntPtr.Zero?0:Marshal.GetLastWin32Error();
            defaultDesktop=OpenDesktopW("Default",0,false,0x60081);
            result["desktop_open_for_query_cleanup"]=defaultDesktop!=IntPtr.Zero;result["desktop_open_error"]=defaultDesktop!=IntPtr.Zero?0:Marshal.GetLastWin32Error();
            if(station==IntPtr.Zero||defaultDesktop==IntPtr.Zero){result["reason"]="query_and_cleanup_rights_unavailable_no_launch_attempt";return result;}
            beforeStation=ReadDacl(station);beforeDesktop=ReadDacl(defaultDesktop);
            result["station_dacl_before_sha256"]=beforeStation.hash;result["desktop_dacl_before_sha256"]=beforeDesktop.hash;
            var random=new byte[passwordLength];using(var rng=RandomNumberGenerator.Create())rng.GetBytes(random);
            password=Marshal.AllocHGlobal((passwordLength+1)*2);const string alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-";
            for(int n=0;n<passwordLength;n++)Marshal.WriteInt16(password,n*2,(short)alphabet[random[n]&63]);
            Marshal.WriteInt16(password,0,(short)'a');Marshal.WriteInt16(password,2,(short)'A');Marshal.WriteInt16(password,4,(short)'7');Marshal.WriteInt16(password,6,(short)'!');
            Marshal.WriteInt16(password,passwordLength*2,0);Array.Clear(random,0,random.Length);
            nameMemory=Marshal.StringToHGlobalUni(name);comment=Marshal.StringToHGlobalUni("PaneBind disposable desktop probe "+runId);
            var info=new UserInfo1{name=nameMemory,password=password,privilege=1,comment=comment,flags=0x201};uint parameter;
            uint added=NetUserAdd(null,1,ref info,out parameter);result["NetUserAdd_status"]=added;result["NetUserAdd_parameter_error"]=parameter;
            if(added!=0){result["reason"]="unique_standard_account_not_created_or_not_reused";return result;}
            created=true;result["account_created"]=true;uint sidStatus;account=AccountSid(name,out sidStatus);result["created_sid_query_status"]=sidStatus;
            if(account==null){result["reason"]="created_account_identity_unknown";return result;}
            result["created_account_sid_sha256"]=Hash(account);
            if(!LogonUserW(name,".",password,2,0,out token)){result["reason"]="standard_logon_failed";result["LogonUser_error"]=Marshal.GetLastWin32Error();return result;}
            stage="credential_token_facts";bool ordinary;result["credential_token"]=TokenFacts(token,account,session,out ordinary);logon=LogonSid(token);
            result["actual_logon_sid_sha256"]=Hash(logon);
            if(!ordinary||TokenNumber(token,12)!=session){result["reason"]="actual_standard_token_or_session_rejected";return result;}
            stage="prepare_exact_guest_local_IO";var local=PrepareLocalIo(runId,account,scriptHash);result["guest_local_io"]=local;
            var childInput=(string)local["child_input"];var childOutput=(string)local["child_output"];
            var nonce=Guid.NewGuid().ToString("N");
            var binding="{\"schema\":\"panebind-v1-medium-desktop-binding/1\",\"run_id\":\""+runId+"\",\"source_commit\":\""+source+
                "\",\"probe_sha256\":\""+scriptHash+"\",\"account_sid_sha256\":\""+Hash(account)+"\",\"session_id\":"+session+",\"nonce\":\""+nonce+
                "\",\"io_contract\":\"panebind-v1-guest-local-io-contrast/1\",\"child_input\":\""+childInput.Replace("\\","\\\\")+
                "\",\"child_output\":\""+childOutput.Replace("\\","\\\\")+"\",\"manifest_sha256\":\""+(string)local["manifest_sha256"]+"\"}";
            var bindingPath="C:\\PaneBindV1\\Output\\"+runId+"-medium-desktop-binding.json";NewText(bindingPath,binding);
            var copiedBinding=CopyPlain(bindingPath,childInput+"\\"+runId+"-medium-desktop-binding.json",4096);
            ((List<object>)local["copied_files"]).Add(copiedBinding);
            var bindingHash=ByteHash(new UTF8Encoding(false).GetBytes(binding));result["binding_sha256"]=bindingHash;result["binding_nonce"]=nonce;
            if((string)copiedBinding["sha256"]!=bindingHash)throw new InvalidOperationException("Local one-time binding copy changed");
            var executable="C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe";
            var command=new StringBuilder("\""+executable+"\" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File \""+childInput+"\\v1-medium-desktop-probe-guest.ps1\" -RunId "+runId+
                " -Mode Child -ChildNonce "+nonce+" -BindingSha256 "+bindingHash);
            if(command.Length>1000)throw new InvalidOperationException("Bounded exact child command required");
            var startup=new Startup();startup.size=(uint)Marshal.SizeOf(typeof(Startup));startup.desktop=null;
            stage="CreateProcessWithTokenW";
            result["CreateProcessWithTokenW_attempts"]=1;
            bool launched=CreateProcessWithTokenW(token,1,executable,command,0x08000004,IntPtr.Zero,childInput,ref startup,out child);
            result["CreateProcessWithTokenW_succeeded"]=launched;result["CreateProcessWithTokenW_error"]=launched?0:Marshal.GetLastWin32Error();
            stage="post_launch_dacl_inventory";
            var afterStation=ReadDacl(station);var afterDesktop=ReadDacl(defaultDesktop);bool stationExplained,desktopExplained;List<object> stationDelta,desktopDelta;
            recordedStation=Delta(beforeStation,afterStation,account,logon,out stationExplained,out stationDelta);
            recordedDesktop=Delta(beforeDesktop,afterDesktop,account,logon,out desktopExplained,out desktopDelta);
            result["station_dacl_after_sha256"]=afterStation.hash;result["desktop_dacl_after_sha256"]=afterDesktop.hash;
            result["station_added_aces"]=stationDelta;result["desktop_added_aces"]=desktopDelta;
            result["dacl_delta_explained"]=stationExplained&&desktopExplained;
            if(!launched||!stationExplained||!desktopExplained){result["reason"]=!launched?"documented_launch_failed":"unexplained_acl_change_child_not_resumed";return result;}
            result["child_pid"]=child.pid;result["child_tid"]=child.tid;
            stage="GetProcessTimes";result["GetProcessTimes_attempted"]=true;
            FileTime birth,exit,kernel,user;bool birthRead=GetProcessTimes(child.process,out birth,out exit,out kernel,out user);
            int birthError=birthRead?0:Marshal.GetLastWin32Error();result["GetProcessTimes_succeeded"]=birthRead;result["GetProcessTimes_error"]=birthError;
            if(!birthRead)throw new InvalidOperationException("Exact child birth unknown");
            result["child_birth_filetime"]=((ulong)birth.high<<32)|birth.low;
            stage="OpenProcessToken";result["OpenProcessToken_attempted"]=true;
            bool tokenOpened=OpenProcessToken(child.process,8,out childToken);
            int tokenOpenError=tokenOpened?0:Marshal.GetLastWin32Error();result["OpenProcessToken_succeeded"]=tokenOpened;result["OpenProcessToken_error"]=tokenOpenError;
            if(!tokenOpened)throw new InvalidOperationException("Actual suspended child token unknown");
            stage="actual_child_token_facts";bool actualOrdinary;result["actual_child_token"]=TokenFacts(childToken,account,session,out actualOrdinary);
            stage="actual_child_identity_recheck";
            if(!actualOrdinary||TokenNumber(childToken,12)!=session||LogonSid(childToken)!=logon)throw new InvalidOperationException("Actual suspended child identity rejected");
            stage="ResumeThread";uint resume=ResumeThread(child.thread);result["ResumeThread_return"]=resume;
            if(resume==UInt32.MaxValue)throw new InvalidOperationException("Exact child resume failed");resumed=true;
            stage="child_exit_wait";uint waited=WaitForSingleObject(child.process,20000);result["child_wait_return"]=waited;
            if(waited!=0){result["reason"]="bounded_child_exit_not_observed";return result;}
            stage="child_exit_status";uint code;if(!GetExitCodeProcess(child.process,out code))throw new InvalidOperationException("Child exit status unavailable");
            result["child_exit_code"]=code;result["child_process_exited"]=true;
            stage="child_receipt";CollectChildReceipt(result,runId);
            if(!result.ContainsKey("child_receipt_mapped_output_copy_confirmed")){result["reason"]="bounded_child_receipt_missing_or_copy_unconfirmed";return result;}
            result["status"]=code==0?"CHILD_EXITED_ZERO_RECEIPT_REQUIRED":"UNKNOWN";result["reason"]="actual_child_receipt_requires_independent_validation";return result;
        }catch(Exception e){result["reason"]="bounded_desktop_probe_exception";result["exception_stage"]=stage;result["error_type"]=e.GetType().Name;result["exception_hresult"]=e.HResult;
            var native=e as System.ComponentModel.Win32Exception;if(native!=null)result["exception_native_error"]=native.NativeErrorCode;return result;}
        finally {
            if(childToken!=IntPtr.Zero)CloseHandle(childToken);
            if(child.process!=IntPtr.Zero){
                var wait=WaitForSingleObject(child.process,0);
                result["test_child_force_termination_attempted"]=wait==258;
                if(wait==258){bool killed=TerminateProcess(child.process,78);result["test_child_force_termination_returned"]=killed;
                    result["test_child_force_termination_error"]=killed?0:Marshal.GetLastWin32Error();wait=WaitForSingleObject(child.process,5000);}
                result["child_process_exited"]=wait==0;result["child_ever_resumed"]=resumed;CloseHandle(child.process);
            }
            if(child.thread!=IntPtr.Zero)CloseHandle(child.thread);
            if((bool)result["child_process_exited"]&&!result.ContainsKey("child_receipt_collection_attempted"))CollectChildReceipt(result,runId);
            var phaseTrace=ReadChildPhases(runId,(bool)result["child_process_exited"]);result["child_phase_trace"]=phaseTrace;
            result["guest_local_evidence_collected"]=result.ContainsKey("child_receipt_mapped_output_copy_confirmed")&&
                (bool)result["child_receipt_mapped_output_copy_confirmed"]&&phaseTrace.ContainsKey("mapped_output_copy_confirmed")&&
                (bool)phaseTrace["mapped_output_copy_confirmed"];
            bool aclClean=beforeStation!=null&&beforeDesktop!=null;
            if(account!=null&&logon!=null&&(int)result["CreateProcessWithTokenW_attempts"]==1){
                if(beforeStation!=null){var restored=RestoreOwnAddedAces(station,beforeStation,recordedStation,account,logon);result["station_dacl_cleanup"]=restored;aclClean&=(bool)restored["confirmed"];}
                if(beforeDesktop!=null){var restored=RestoreOwnAddedAces(defaultDesktop,beforeDesktop,recordedDesktop,account,logon);result["desktop_dacl_cleanup"]=restored;aclClean&=(bool)restored["confirmed"];}
            }else{
                // No launch was possible before exact account/logon facts.
                try{aclClean&=beforeStation!=null&&ReadDacl(station).hash==beforeStation.hash&&beforeDesktop!=null&&ReadDacl(defaultDesktop).hash==beforeDesktop.hash;}catch{aclClean=false;}
            }
            result["dacl_cleanup_confirmed"]=aclClean;
            if(defaultDesktop!=IntPtr.Zero)CloseDesktop(defaultDesktop);if(station!=IntPtr.Zero)CloseWindowStation(station);
            if(token!=IntPtr.Zero)CloseHandle(token);
            if(password!=IntPtr.Zero){for(int n=0;n<=passwordLength;n++)Marshal.WriteInt16(password,n*2,0);Marshal.FreeHGlobal(password);}
            if(nameMemory!=IntPtr.Zero)Marshal.FreeHGlobal(nameMemory);if(comment!=IntPtr.Zero)Marshal.FreeHGlobal(comment);
            var cleanup=new Dictionary<string,object>{{"delete_attempted",false},{"profile_deletion_attempted",false}};result["account_cleanup"]=cleanup;
            if(!created){cleanup["result"]="NOT_CREATED";result["account_cleanup_confirmed"]=true;}
            else try {
                uint status;var actual=AccountSid(name,out status);cleanup["sid_query_status"]=status;
                if(account!=null&&account==actual&&(child.process==IntPtr.Zero||(bool)result["child_process_exited"])){
                    cleanup["sid_matches_created_account"]=true;cleanup["delete_attempted"]=true;
                    uint deleted=NetUserDel(null,name);cleanup["NetUserDel_status"]=deleted;uint after;AccountSid(name,out after);cleanup["after_query_status"]=after;
                    result["account_cleanup_confirmed"]=deleted==0&&after==2221;
                }else cleanup["result"]="IDENTITY_UNKNOWN_NOT_DELETED";
            }catch(Exception e){cleanup["error_type"]=e.GetType().Name;}
            if(!aclClean||!(bool)result["account_cleanup_confirmed"]){result["status"]="UNKNOWN";result["reason"]="cleanup_not_confirmed";}
        }
    }

    delegate IntPtr WindowProcedure(IntPtr window,uint message,IntPtr wp,IntPtr lp);
    [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)] struct WindowClass {
        public uint size,style;public WindowProcedure procedure;public int extraClass,extraWindow;public IntPtr instance,icon,cursor,brush;
        public string menu,className;public IntPtr smallIcon;
    }
    [StructLayout(LayoutKind.Sequential)] struct Point {public int x,y;}
    [StructLayout(LayoutKind.Sequential)] struct Message {public IntPtr window;public uint message;public UIntPtr wp;public IntPtr lp;public uint time;public Point point;public uint privateData;}
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode)] static extern IntPtr GetModuleHandleW(string module);
    [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern ushort RegisterClassExW(ref WindowClass value);
    [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr CreateWindowExW(uint ex,string cls,string title,uint style,int x,int y,int width,int height,IntPtr parent,IntPtr menu,IntPtr instance,IntPtr parameter);
    [DllImport("user32.dll")] static extern bool ShowWindow(IntPtr window,int command);
    [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] static extern bool IsWindow(IntPtr window);
    [DllImport("user32.dll")] static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll",SetLastError=true)] static extern UIntPtr SetTimer(IntPtr window,UIntPtr id,uint milliseconds,IntPtr callback);
    [DllImport("user32.dll")] static extern bool KillTimer(IntPtr window,UIntPtr id);
    [DllImport("user32.dll",SetLastError=true)] static extern bool DestroyWindow(IntPtr window);
    [DllImport("user32.dll")] static extern void PostQuitMessage(int code);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern IntPtr DefWindowProcW(IntPtr window,uint message,IntPtr wp,IntPtr lp);
    [DllImport("user32.dll",SetLastError=true)] static extern int GetMessageW(out Message message,IntPtr window,uint min,uint max);
    [DllImport("user32.dll")] static extern bool TranslateMessage(ref Message message);
    [DllImport("user32.dll")] static extern IntPtr DispatchMessageW(ref Message message);
    [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool UnregisterClassW(string name,IntPtr instance);
    [DllImport("ole32.dll")] static extern int CoInitializeEx(IntPtr reserved,uint model);
    [DllImport("ole32.dll")] static extern void CoUninitialize();
    [ComImport,Guid("a5cd92ff-29be-454c-8d04-d82879fb3f1b"),InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface DesktopManager {
        [PreserveSig] int IsWindowOnCurrentVirtualDesktop(IntPtr window,[MarshalAs(UnmanagedType.Bool)] out bool current);
        [PreserveSig] int GetWindowDesktopId(IntPtr window,out Guid desktop);
        [PreserveSig] int MoveWindowToDesktop(IntPtr window,ref Guid desktop);
    }
    static bool timerObserved,destroyObserved,destroyReturned;
    static readonly WindowProcedure windowProcedure=Procedure;
    static IntPtr Procedure(IntPtr window,uint message,IntPtr wp,IntPtr lp) {
        if(message==0x113&&wp==new IntPtr(1)){
            KillTimer(window,new UIntPtr(1));timerObserved=true;destroyReturned=DestroyWindow(window);return IntPtr.Zero;
        }
        if(message==2){destroyObserved=true;PostQuitMessage(0);return IntPtr.Zero;}
        return DefWindowProcW(window,message,wp,lp);
    }
    public static Dictionary<string,object> RunChild(string runId,string expectedSid,int expectedSession) {
        var result=new Dictionary<string,object>{{"status","UNKNOWN"},{"gui_created",false},{"window_destroyed",false},{"product_started",false},{"explorer_started",false},{"synthetic_input_sent",false},{"child_phase_recording_failed",false}};
        IntPtr window=IntPtr.Zero;string className="PaneBindMediumDesktopProbe_"+runId;var instance=GetModuleHandleW(null);bool registered=false;int initialized=unchecked((int)0x80004005);
        string stage="actual_child_token";
        try {
            using(var identity=WindowsIdentity.GetCurrent()){
                var actualSid=identity.User.Value;bool ordinary;result["actual_token"]=TokenFacts(identity.Token,actualSid,expectedSession,out ordinary);
                if(!ordinary||Hash(actualSid)!=expectedSid||TokenNumber(identity.Token,12)!=expectedSession)throw new InvalidOperationException("Bound child token mismatch");
            }
            ChildPhase(result,runId,"token_verified");stage="active_desktop";
            result["desktop"]=Desktop(false);result["actual_pid"]=Process.GetCurrentProcess().Id;result["actual_tid"]=GetCurrentThreadId();
            ChildPhase(result,runId,"desktop_verified");stage="window_class_registration";
            var foreground=GetForegroundWindow();result["foreground_before"]=foreground.ToInt64();
            var cls=new WindowClass{size=(uint)Marshal.SizeOf(typeof(WindowClass)),procedure=windowProcedure,instance=instance,brush=new IntPtr(6),className=className};
            registered=RegisterClassExW(ref cls)!=0;result["class_registered"]=registered;result["RegisterClassExW_error"]=registered?0:Marshal.GetLastWin32Error();
            if(!registered)throw new InvalidOperationException("Own Win32 class registration failed");
            ChildPhase(result,runId,"class_registered");stage="window_create_show";
            window=CreateWindowExW(0,className,"PaneBind standard guest desktop probe",0x00CF0000,120,120,420,240,IntPtr.Zero,IntPtr.Zero,instance,IntPtr.Zero);
            result["gui_created"]=window!=IntPtr.Zero;result["CreateWindowExW_error"]=window!=IntPtr.Zero?0:Marshal.GetLastWin32Error();result["window_handle"]=window.ToInt64();
            if(window==IntPtr.Zero)throw new InvalidOperationException("Own blank Win32 window creation failed");
            ShowWindow(window,8);result["window_visible"]=IsWindowVisible(window);
            ChildPhase(result,runId,"window_created_and_shown");stage="COM_initialize_and_query";ChildPhase(result,runId,"before_COM");
            initialized=CoInitializeEx(IntPtr.Zero,2);result["COM_initialize_hresult"]=initialized;object manager=null;
            try {
                if(initialized>=0){
                    manager=Activator.CreateInstance(Type.GetTypeFromCLSID(new Guid("aa509086-5ca9-4c25-8f95-589d3c07b48a")));
                    bool current;int hr=((DesktopManager)manager).IsWindowOnCurrentVirtualDesktop(window,out current);
                    result["virtual_desktop_query_hresult"]=hr;result["on_current_virtual_desktop"]=hr==0?(object)current:null;
                }
            }catch(Exception e){result["virtual_desktop_error_type"]=e.GetType().Name;}
            finally{if(manager!=null&&Marshal.IsComObject(manager))Marshal.ReleaseComObject(manager);if(initialized>=0){CoUninitialize();initialized=unchecked((int)0x80004005);}}
            ChildPhase(result,runId,"after_COM");stage="own_timer_and_message_pump";
            var timer=SetTimer(window,new UIntPtr(1),300,IntPtr.Zero);result["one_shot_timer_started"]=timer!=UIntPtr.Zero;
            if(timer==UIntPtr.Zero)throw new InvalidOperationException("Own bounded GUI timer failed");
            ChildPhase(result,runId,"before_message_pump");
            Message message;int received;
            while((received=GetMessageW(out message,IntPtr.Zero,0,0))>0){TranslateMessage(ref message);DispatchMessageW(ref message);}
            ChildPhase(result,runId,"after_message_pump");stage="actual_GUI_result";
            result["message_pump_return"]=received;result["actual_timer_message"]=timerObserved;result["DestroyWindow_returned"]=destroyReturned;
            result["actual_WM_DESTROY"]=destroyObserved;result["window_destroyed"]=destroyObserved&&!IsWindow(window);
            result["foreground_after"]=GetForegroundWindow().ToInt64();
            bool vd=result.ContainsKey("on_current_virtual_desktop")&&result["on_current_virtual_desktop"] is bool&&(bool)result["on_current_virtual_desktop"];
            bool accepted=(bool)result["window_visible"]&&timerObserved&&destroyReturned&&destroyObserved&&!IsWindow(window)&&received==0&&vd;
            result["status"]=accepted&&!(bool)result["child_phase_recording_failed"]?"GUI_PRECHECK_OBSERVED_NOT_FULL_SHELL":"UNKNOWN";return result;
        }catch(Exception e){result["error_type"]=e.GetType().Name;result["exception_stage"]=stage;return result;}
        finally {
            if(window!=IntPtr.Zero&&IsWindow(window)){result["abort_own_window_destroy_attempted"]=true;result["abort_own_window_destroy_returned"]=DestroyWindow(window);}
            if(registered)result["own_class_unregistered"]=UnregisterClassW(className,instance);
            if(initialized>=0)CoUninitialize();
            ChildPhase(result,runId,"cleanup_calls_returned");if((bool)result["child_phase_recording_failed"])result["status"]="UNKNOWN";
        }
    }
}
'@
    if($Mode -ceq 'Child'){
        Write-ChildPhase 'after_AddType'
        Write-ChildPhase 'before_RunChild'
        $result=[PaneBindV1MediumDesktopProbe]::RunChild($RunId,$binding.account_sid_sha256,[int]$binding.session_id)
        $record=[ordered]@{schema='panebind-v1-medium-desktop-child/1';run_id=$RunId;nonce=$ChildNonce;binding_sha256=$BindingSha256;
            source_commit=$manifest.packaging_head;probe_sha256=$scriptEntry[0].sha256;automated_guest_probe=$true;
            io_contract=$binding.io_contract;child_input=$inputDir;child_output=$outputDir;manifest_sha256=$binding.manifest_sha256;result=$result}
    }else{
        foreach($suffix in @('-medium-desktop-binding.json','-medium-desktop-child.json','-medium-desktop-child-phase.jsonl')){
            if(Test-Path -LiteralPath (Join-Path $outputDir ($RunId+$suffix))){throw 'Child binding/evidence already exists; preserve the original RunId.'}
        }
        if($StandardGuestNativeClients){
            if($manifest.product_release_sha256 -cnotmatch '^[A-F0-9]{64}$'){throw 'Exact unchanged Release product hash required.'}
            $result=[PaneBindV1MediumDesktopProbe]::RunNativeParent($RunId,$scriptEntry[0].sha256,
                $manifest.driver_source_commit,$nativeEntry[0].sha256,[long]$nativeEntry[0].size,
                $true,$manifest.product_release_sha256,[uint32]$NativeClientsTimeoutSeconds)
        }elseif($NativeIdentityOnly){
            $result=[PaneBindV1MediumDesktopProbe]::RunNativeParent($RunId,$scriptEntry[0].sha256,
                $manifest.driver_source_commit,$nativeEntry[0].sha256,[long]$nativeEntry[0].size)
        }else{
            $result=[PaneBindV1MediumDesktopProbe]::RunParent($RunId,$manifest.packaging_head,$scriptEntry[0].sha256)
        }
        if($result.ContainsKey('child_receipt_text')){
            try {
                $child=$result.child_receipt_text|ConvertFrom-Json
                $result.Remove('child_receipt_text')
                $result['child_receipt']=$child
                $matched=$child.schema -ceq 'panebind-v1-medium-desktop-child/1' -and $child.run_id -ceq $RunId -and
                    $child.binding_sha256 -ceq $result.binding_sha256 -and $child.nonce -ceq $result.binding_nonce -and
                    $child.source_commit -ceq $manifest.packaging_head -and
                    $child.io_contract -ceq 'panebind-v1-guest-local-io-contrast/1' -and
                    $child.child_input -ceq (Join-Path $localRunDir 'Input') -and $child.child_output -ceq (Join-Path $localRunDir 'Output') -and
                    $child.manifest_sha256 -ceq $result.guest_local_io.manifest_sha256 -and
                    $result.guest_local_evidence_collected -eq $true -and
                    $child.probe_sha256 -ceq $scriptEntry[0].sha256 -and $child.result.actual_token.sid_sha256 -ceq $result.created_account_sid_sha256 -and
                    [uint32]$child.result.actual_pid -eq [uint32]$result.child_pid -and [uint32]$child.result.actual_tid -eq [uint32]$result.child_tid -and
                    $child.result.status -ceq 'GUI_PRECHECK_OBSERVED_NOT_FULL_SHELL' -and $child.result.window_destroyed -eq $true -and
                    $child.result.actual_WM_DESTROY -eq $true -and $child.result.own_class_unregistered -eq $true -and
                    $result.child_exit_code -eq 0 -and $result.child_process_exited -eq $true -and
                    $result.dacl_cleanup_confirmed -eq $true -and $result.account_cleanup_confirmed -eq $true -and
                    $result.test_child_force_termination_attempted -eq $false
                $result['child_receipt_verified']=$matched
                $result['status']=if($matched){'GUI_PRECHECK_OBSERVED_NOT_FULL_SHELL'}else{'UNKNOWN'}
            }catch{$result['status']='UNKNOWN';$result['child_receipt_parse_error_type']=$_.Exception.GetType().Name}
        }
        $record=[ordered]@{schema='panebind-v1-medium-desktop-probe/1';utc=[DateTime]::UtcNow.ToString('o');run_id=$RunId;
            probe_source_commit=$manifest.packaging_head;driver_source_commit=$manifest.driver_source_commit;probe_sha256=$scriptEntry[0].sha256;
            requested_suite=$manifest.guest_suite;actual_suite=$(if($NativeIdentityOnly){'MediumNativeIdentityOnly'}elseif($StandardGuestNativeClients){'StandardGuestNativeClients'}else{'MediumDesktopGuiPrecheck'});
            compatibility_envelope_kind=$manifest.medium_desktop_probe.kind;compatibility_envelope_is_GUI_evidence=$false;
            automated_guest_probe=$true;human_uat='NOT_RUN';result=$result}
    }
    $text=$record|ConvertTo-Json -Depth 18
    $stream=[IO.File]::Open($evidence,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
    try{$bytes=(New-Object Text.UTF8Encoding($false)).GetBytes($text);$stream.Write($bytes,0,$bytes.Length)}finally{$stream.Dispose()}
    if($result.status -cin @('GUI_PRECHECK_OBSERVED_NOT_FULL_SHELL','NATIVE_ENTRY_IDENTITY_OBSERVED_NOT_GUI_OR_SHELL','STANDARD_NATIVE_CLIENTS_EXITED_REVIEW_EVIDENCE')){exit 0}
    exit 3
}catch{
    if($Mode -ceq 'Child' -and $childPhasePath){try{Write-ChildPhase ('PowerShell_exception_'+$_.Exception.GetType().Name)}catch{}}
    Write-Error ('Guest medium desktop probe stopped: '+$_.Exception.Message)
    exit 78
}

