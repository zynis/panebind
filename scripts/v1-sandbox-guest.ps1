[CmdletBinding()]
param([Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-f]{32}$')][string]$RunId,
      [ValidateRange(30,1200)][int]$StageTimeoutSeconds=360)
$ErrorActionPreference='Stop'
$inputDir='C:\PaneBindV1\Input'
$outputDir='C:\PaneBindV1\Output'
$utf8=New-Object Text.UTF8Encoding($false)
$summaryPath=$null
$summary=[ordered]@{schema='panebind-v1-guest/1';run_id=$RunId;status='NOT_READY';
    source_commit=$null;automated_driver=$true;human_uat='NOT_RUN';stages=@();error=$null}
function New-Evidence([string]$Path,[string]$Text) {
    $stream=[IO.File]::Open($Path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
    try{$bytes=$script:utf8.GetBytes($Text);$stream.Write($bytes,0,$bytes.Length)}finally{$stream.Dispose()}
}
function Marker([string]$Path) {
    $value=[IO.File]::ReadAllText($Path,[Text.Encoding]::ASCII)
    if($value.EndsWith("`r`n")){$value=$value.Substring(0,$value.Length-2)}
    if($value -cnotmatch '^[0-9a-f]{32}$'){throw 'Invalid exact run marker.'}
    return $value
}
function Fixture-ExitReason([uint32]$Code) {
    # Safe meanings of the actual fixture exit status. These never infer a
    # platform cause, input delivery or product PASS from a numeric exit.
    $reasons=@{
        0=@('driver_exited_zero_evidence_required',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('cZqoUgCQ+lH2lgz/zU57mA1ZOGifUv2ADk4FbgZ0wYtuYw=='))))
        1=@('fixture_runtime_or_function_failed',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('ZgBpAHgAdAB1AHIAZQAgANCPTIgWYp9S/YDAaOVnMVkljQ=='))))
        64=@('fixture_command_line_unavailable',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('4GXVbPuL1lMgAGYAaQB4AHQAdQByAGUAIAB9VOROTIg='))))
        78=@('initial_guest_scope_not_authorized',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('HVLLWSAAZwB1AGUAcwB0ACAAJo03YgEwUgB1AG4ASQBkACAAFmLCU3BlA4P0Vg1OJns='))))
        80=@('input_RunId_marker_invalid',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('k49lUe52VV8gAFIAdQBuAEkAZAAgAAdosIsNTu9T+4sWYg1OOVNNkQ=='))))
        81=@('output_RunId_marker_invalid',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('k4/6Ue52VV8gAFIAdQBuAEkAZAAgAAdosIsNTu9T+4sWYg1OOVNNkQ=='))))
        82=@('output_lexical_path_not_exact',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('k4/6UcJTcGUNTi9mvnxueIhjQ2fvjYRf'))))
        83=@('product_lexical_path_not_exact',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('p07BVMJTcGUNTi9mLGcgAFIAdQBuAEkAZAAgAIR2vnxueCAARABlAGIAdQBnAC8AUgBlAGwAZQBhAHMAZQAgAO+NhF8='))))
        84=@('output_directory_not_plain',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('k4/6Ue52VV8NTu9T+4sBMA1OL2budlVfFmJ3UQlnzZHjiZBnXlwnYBv/KmeZUSAAZwB1AGEAcgBkACAA5WXXXw=='))))
        85=@('guard_evidence_creation_failed',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('4GXVbLBl+l4gAGcAdQBhAHIAZAAgAOVl118b//JdCWeHZfZODU4aT4aJ1nY='))))
        86=@('input_directory_not_plain',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('k49lUe52VV8NTu9T+4sBMA1OL2budlVfFmJ3UQlnzZHjiZBnXlwnYA=='))))
        87=@('own_token_not_verified_Medium_non_elevated',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('ZgBpAHgAdAB1AHIAZQAgAOqBq44gAHQAbwBrAGUAbgAgACpnOGieWzpOIABNAGUAZABpAHUAbQAgABROXpfQY0dT'))))
        88=@('own_interactive_session_unavailable',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('ZgBpAHgAdAB1AHIAZQAgAOqBq45el/aWpE6STiAAcwBlAHMAcwBpAG8AbgAgAA1O71M4aJ5b'))))
        89=@('own_Default_input_desktop_not_verified',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('ZgBpAHgAdAB1AHIAZQAgAOqBq44gAEQAZQBmAGEAdQBsAHQAIACTj2VRTGhilxZiIABVAE8ASQBfAEkATwAgACpnGpDHjw=='))))
        90=@('own_WTS_session_not_active',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('ZgBpAHgAdAB1AHIAZQAgAOqBq44gAFcAVABTACAAGk/diypnOGieWzpOIABBAGMAdABpAHYAZQA='))))
        91=@('output_canonical_path_not_exact',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('k4/6Ue52VV/EiQOD742EXw1O71P7ixZiDU5Je45OiGNDZ++NhF8='))))
        92=@('product_directory_not_plain',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('LGcgAFIAdQBuAEkAZAAgAKdOwVTudlVfDU7hbrONbmYakO52VV+BiUJs'))))
        93=@('Debug_input_directory_not_plain',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('RABlAGIAdQBnACAAk49lUVBb7nZVXw1O4W6zjW5mGpDudlVfgYlCbA=='))))
        94=@('actual_product_not_plain_file',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('8l0NWTZSp07BVA1OL2bvUzhonluEdm5mGpCHZfZO'))))
        95=@('input_product_not_plain_file',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('k49lUadOwVQNTi9m71M4aJ5bhHZuZhqQh2X2Tg=='))))
        96=@('product_canonical_path_not_exact',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('p07BVMSJA4PvjYRfDU7vU/uLFmINTkl7jk4sZyAAUgB1AG4ASQBkACAAvnxueO+NhF8='))))
        97=@('product_source_hash_mismatch_or_unreadable',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('p07BVA5Ok49lUTZSwVTIVAxeDU4ATvSBFmINTu9T+4vWUw=='))))
        98=@('portable_run_marker_missing',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('LGcgAFIAdQBuAEkAZAAgAKdOwVQ6fxFcv086ZBlQCZAHaLCL'))))
        99=@('preexisting_product_instance_present',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('8l0JZ6dOwVSeW4tPDP/SYt1+DVkodQ=='))))
        100=@('PMV2_context_not_available',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('4GXVbDhonlsgAFAATQB2ADIAIABEAFAASQAgAApOC06HZQ=='))))
        101=@('product_runtime_directory_is_not_fresh',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('LGcgAFIAdQBuAEkAZAAgAPJdCWenTsFU5WXXXxZivotufwz/0mLdfoaJ1nY='))))
        102=@('product_changed_after_guard',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('jFsQYiAAZwB1AGEAcgBkACAADlQ2UsFUyFQMXo1RIWvAaOVnMVkljQ=='))))
        103=@('guard_observation_or_evidence_exception',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('ZwB1AGEAcgBkACAAwonfWxZiwYtuY5lRZVECXzheG/8NTqhjrWV3UVNPr3ODWJ9T4FY='))))
    }
    if($Code -le [int]::MaxValue -and $reasons.ContainsKey([int]$Code)){$reason=$reasons[[int]$Code]}
    else{$reason=@('unmapped_fixture_exit_status',([Text.Encoding]::Unicode.GetString([Convert]::FromBase64String('KmcgZgRchHYgAGYAaQB4AHQAdQByAGUAIAAAkPpRtnIBYAz/AJfAaOVnn1PLWcGLbmM='))))}
    [ordered]@{code=$Code;reason_en=$reason[0];reason_zh=$reason[1];feature_pass_inferred=$false}
}
try {
    # These checks precede compilation or any token/process creation API. This
    # entry is not a general launch tool and has no host override switch.
    if(-not [string]::Equals([IO.Path]::GetFullPath($PSScriptRoot),$inputDir,[StringComparison]::OrdinalIgnoreCase)){
        throw 'Guest script must execute from the exact mapped input directory.'}
    $identity=[Security.Principal.WindowsIdentity]::GetCurrent()
    try{$account=$identity.Name}finally{$identity.Dispose()}
    if($account -notmatch '(?i)\\WDAGUtilityAccount$'){throw 'Only the disposable Windows Sandbox account is supported.'}
    if((Marker (Join-Path $inputDir 'panebind-v1-input.marker')) -cne $RunId -or
       (Marker (Join-Path $outputDir 'panebind-v1-output.marker')) -cne $RunId){throw 'Input/output RunId markers disagree.'}
    $summaryPath=Join-Path $outputDir ($RunId+'-guest-summary.json')
    if(Test-Path -LiteralPath $summaryPath){throw 'A summary already exists; preserve this batch and use a new RunId.'}
    $manifest=Get-Content -LiteralPath (Join-Path $inputDir 'manifest.json') -Encoding UTF8 -Raw | ConvertFrom-Json
    if($manifest.schema -cne 'panebind-v1-sandbox/1' -or $manifest.run_id -cne $RunId -or
       $manifest.source_commit -cnotmatch '^[0-9a-f]{40}$' -or
       $manifest.guest_input -cne $inputDir -or $manifest.guest_output -cne $outputDir -or
       $manifest.host_root -cne ('I:\PaneBindMVP1Runs\'+$RunId)){
        throw 'Manifest identity or exact deployment paths disagree.'}
    $summary.source_commit=$manifest.source_commit
    $allowed=@('panebind.exe','panebind-v1-guest-fixture.exe','Debug\panebind.exe',
        'Debug\panebind-v1-guest-fixture.exe','v1-sandbox-guest.ps1','panebind-v1-input.marker')
    $seen=@{}
    foreach($file in @($manifest.files)){
        if($file.relative_path -cnotin $allowed -or $seen.ContainsKey($file.relative_path)){throw 'Unknown or duplicate payload file.'}
        $seen[$file.relative_path]=$true
        $actual=Get-Item -LiteralPath (Join-Path $inputDir $file.relative_path)
        if($actual.PSIsContainer -or $actual.Length -ne [long]$file.size -or
           (Get-FileHash -LiteralPath $actual.FullName -Algorithm SHA256).Hash -cne $file.sha256){throw 'Payload size/hash mismatch.'}
    }
    if($seen.Count -ne $allowed.Count){throw 'Payload manifest is incomplete.'}
    # Official API references inspected 2026-10-15. Original orchestration code:
    # /windows/win32/api/winnt/ne-winnt-token_information_class (TokenLinkedToken)
    # /windows/win32/api/securitybaseapi/nf-securitybaseapi-createrestrictedtoken
    # /windows/win32/api/securitybaseapi/nf-securitybaseapi-duplicatetokenex
    # /windows/win32/api/securitybaseapi/nf-securitybaseapi-settokeninformation
    # /windows/win32/api/winbase/nf-winbase-createprocesswithtokenw
    # /windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessasuserw
    # /windows/win32/api/winuser/nf-winuser-getuserobjectinformationw (UOI_IO=6)
    # No SetTokenInformation targets the original token. No credentials,
    # session-id/DACL/policy/UAC changes, SAFER_INERT or explicit privilege
    # enable. AsUser may temporarily enable its documented required privileges.
    # A restricted child sharing this disposable guest desktop is a test
    # environment, not a new security boundary against the guest's High apps.
    Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Collections.Generic;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Security.Principal;
using System.Security.Cryptography;

public static class PaneBindV1MediumGuest {
    const uint TOKEN_RIGHTS=0x8b, QUERY=8, CREATE_SUSPENDED=4, NO_WINDOW=0x08000000;
    [StructLayout(LayoutKind.Sequential)] struct SidAttributes { public IntPtr Sid; public uint Attributes; }
    [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)] struct Startup {
        public uint cb; public string reserved,desktop,title; public uint x,y,cx,cy,xchars,ychars,fill,flags;
        public ushort show,reserved2; public IntPtr reservedBytes,input,output,error;
    }
    [StructLayout(LayoutKind.Sequential)] struct ProcessInfo { public IntPtr process,thread;public uint pid,tid; }
    [StructLayout(LayoutKind.Sequential)] struct JobBasic {
        public long processTime,jobTime;public uint flags;public UIntPtr minWorking,maxWorking;
        public uint activeLimit;public UIntPtr affinity;public uint priority,scheduling;
    }
    [StructLayout(LayoutKind.Sequential)] struct IoCounters { public ulong a,b,c,d,e,f; }
    [StructLayout(LayoutKind.Sequential)] struct JobExtended {
        public JobBasic basic;public IoCounters io;public UIntPtr processMemory,jobMemory,peakProcess,peakJob;
    }
    [DllImport("kernel32.dll")] static extern IntPtr GetCurrentProcess();
    [DllImport("kernel32.dll")] static extern uint GetCurrentThreadId();
    [DllImport("kernel32.dll")] static extern uint GetCurrentProcessId();
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool ProcessIdToSessionId(uint pid,out uint session);
    [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);
    [DllImport("kernel32.dll",SetLastError=true)] static extern uint ResumeThread(IntPtr h);
    [DllImport("kernel32.dll",SetLastError=true)] static extern uint WaitForSingleObject(IntPtr h,uint ms);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool GetExitCodeProcess(IntPtr h,out uint code);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool TerminateProcess(IntPtr h,uint code);
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr CreateJobObjectW(IntPtr sa,string name);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool AssignProcessToJobObject(IntPtr job,IntPtr process);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool SetInformationJobObject(IntPtr job,int kind,ref JobExtended data,uint size);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool QueryInformationJobObject(IntPtr job,int kind,IntPtr data,uint size,out uint needed);
    [DllImport("advapi32.dll",SetLastError=true)] static extern bool OpenProcessToken(IntPtr p,uint rights,out IntPtr token);
    [DllImport("advapi32.dll",SetLastError=true)] static extern bool GetTokenInformation(IntPtr token,int kind,IntPtr data,int size,out int needed);
    [DllImport("advapi32.dll",SetLastError=true)] static extern bool DuplicateTokenEx(IntPtr source,uint rights,IntPtr sa,int impersonation,int type,out IntPtr token);
    [DllImport("advapi32.dll",SetLastError=true)] static extern bool CreateRestrictedToken(IntPtr source,uint flags,uint disable,IntPtr sids,uint delete,IntPtr privileges,uint restrict,IntPtr restricted,out IntPtr token);
    [DllImport("advapi32.dll",SetLastError=true)] static extern bool SetTokenInformation(IntPtr token,int kind,ref SidAttributes data,uint size);
    [DllImport("advapi32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool ConvertStringSidToSidW(string value,out IntPtr sid);
    [DllImport("advapi32.dll")] static extern uint GetLengthSid(IntPtr sid);
    [DllImport("advapi32.dll")] static extern bool IsTokenRestricted(IntPtr token);
    [DllImport("kernel32.dll")] static extern IntPtr LocalFree(IntPtr memory);
    [DllImport("advapi32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool CreateProcessWithTokenW(IntPtr token,uint flags,string exe,StringBuilder command,uint creation,IntPtr environment,string directory,ref Startup startup,out ProcessInfo info);
    [DllImport("advapi32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool CreateProcessAsUserW(IntPtr token,string exe,StringBuilder command,IntPtr processSa,IntPtr threadSa,bool inherit,uint creation,IntPtr environment,string directory,ref Startup startup,out ProcessInfo info);
    [DllImport("user32.dll")] static extern IntPtr GetThreadDesktop(uint thread);
    [DllImport("user32.dll")] static extern IntPtr GetProcessWindowStation();
    [DllImport("user32.dll",SetLastError=true)] static extern IntPtr OpenInputDesktop(uint flags,bool inherit,uint access);
    [DllImport("user32.dll")] static extern bool CloseDesktop(IntPtr desktop);
    [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool GetUserObjectInformationW(IntPtr h,int kind,StringBuilder data,uint size,out uint needed);
    [DllImport("user32.dll",EntryPoint="GetUserObjectInformationW",ExactSpelling=true,SetLastError=true)] static extern bool GetUserObjectInput(IntPtr h,int kind,out int receiving,uint size,out uint needed);
    [DllImport("wtsapi32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool WTSQuerySessionInformationW(IntPtr server,uint session,int kind,out IntPtr data,out uint bytes);
    [DllImport("wtsapi32.dll")] static extern void WTSFreeMemory(IntPtr data);
    sealed class TokenFacts {
        public string Sid;public int Integrity,Session,ElevationType;public bool Elevated,AdminEnabled,UIAccess,AppContainer,Restricted;
        public Dictionary<string,object> Public() {
            using(var sha=SHA256.Create()) {
                return new Dictionary<string,object>{{"sid_sha256",BitConverter.ToString(sha.ComputeHash(Encoding.UTF8.GetBytes(Sid))).Replace("-","")},
                    {"integrity_rid",Integrity},{"session_id",Session},{"elevated",Elevated},{"elevation_type",ElevationType},
                    {"admin_sid_enabled",AdminEnabled},{"uiaccess",UIAccess},{"app_container",AppContainer},{"restricted",Restricted}};
            }
        }
    }
    static Exception Failure(string api) {return new Win32Exception(Marshal.GetLastWin32Error(),api);}
    static IntPtr Information(IntPtr token,int kind) {
        int needed;GetTokenInformation(token,kind,IntPtr.Zero,0,out needed);
        if(needed<=0||needed>1048576)throw Failure("GetTokenInformation size");
        IntPtr data=Marshal.AllocHGlobal(needed);
        if(!GetTokenInformation(token,kind,data,needed,out needed)){Marshal.FreeHGlobal(data);throw Failure("GetTokenInformation "+kind);}
        return data;
    }
    static int Number(IntPtr token,int kind) {IntPtr p=Information(token,kind);try{return Marshal.ReadInt32(p);}finally{Marshal.FreeHGlobal(p);}}
    static TokenFacts Facts(IntPtr token) {
        var result=new TokenFacts();IntPtr data=Information(token,1);
        try{result.Sid=new SecurityIdentifier(Marshal.ReadIntPtr(data)).Value;}finally{Marshal.FreeHGlobal(data);}
        data=Information(token,25);
        try {var sid=new SecurityIdentifier(Marshal.ReadIntPtr(data));var parts=sid.Value.Split('-');result.Integrity=int.Parse(parts[parts.Length-1]);}
        finally{Marshal.FreeHGlobal(data);}
        result.Session=Number(token,12);result.Elevated=Number(token,20)!=0;result.ElevationType=Number(token,18);
        result.UIAccess=Number(token,26)!=0;result.AppContainer=Number(token,29)!=0;result.Restricted=IsTokenRestricted(token);
        data=Information(token,2);
        try {
            int count=Marshal.ReadInt32(data),offset=IntPtr.Size==8?8:4,stride=Marshal.SizeOf(typeof(SidAttributes));
            if(count<0||count>1024)throw new InvalidOperationException("Unexpected token groups");
            for(int i=0;i<count;++i){var entry=(SidAttributes)Marshal.PtrToStructure(IntPtr.Add(data,offset+i*stride),typeof(SidAttributes));
                if(new SecurityIdentifier(entry.Sid).Value=="S-1-5-32-544"&&(entry.Attributes&4)!=0&&(entry.Attributes&16)==0)result.AdminEnabled=true;}
        }finally{Marshal.FreeHGlobal(data);}
        return result;
    }
    static bool Medium(TokenFacts candidate,TokenFacts original) {
        return candidate.Sid==original.Sid&&candidate.Session==original.Session&&candidate.Session>0&&
            candidate.Integrity==8192&&!candidate.Elevated&&!candidate.AdminEnabled&&!candidate.UIAccess&&!candidate.AppContainer;
    }
    static string ObjectName(IntPtr handle) {
        if(handle==IntPtr.Zero)throw Failure("Desktop/windowstation handle");
        uint needed;var name=new StringBuilder(256);
        if(!GetUserObjectInformationW(handle,2,name,512,out needed))throw Failure("GetUserObjectInformation name");
        return name.ToString();
    }
    static Dictionary<string,object> Desktop(int session) {
        uint actual;if(!ProcessIdToSessionId(GetCurrentProcessId(),out actual)||actual!=(uint)session||session<=0)throw new InvalidOperationException("Noninteractive or inconsistent session");
        IntPtr threadHandle=GetThreadDesktop(GetCurrentThreadId());
        var station=ObjectName(GetProcessWindowStation());var thread=ObjectName(threadHandle);
        var input=OpenInputDesktop(0,false,1);string inputName;int inputReceives,threadReceives;uint needed;
        try{
            inputName=ObjectName(input);
            if(!GetUserObjectInput(input,6,out inputReceives,4,out needed)||needed>4)throw Failure("GetUserObjectInformation input UOI_IO");
            if(!GetUserObjectInput(threadHandle,6,out threadReceives,4,out needed)||needed>4)throw Failure("GetUserObjectInformation thread UOI_IO");
        }finally{if(input!=IntPtr.Zero)CloseDesktop(input);}
        IntPtr data;uint bytes;if(!WTSQuerySessionInformationW(IntPtr.Zero,actual,8,out data,out bytes))throw Failure("WTS session state");
        int state;try{if(bytes<4)throw new InvalidOperationException("Incomplete WTS state");state=Marshal.ReadInt32(data);}finally{WTSFreeMemory(data);}
        if(station!="WinSta0"||thread!="Default"||inputName!="Default"||state!=0||inputReceives==0||threadReceives==0)
            throw new InvalidOperationException("Active WinSta0 Default desktop actually receiving input required");
        return new Dictionary<string,object>{{"window_station",station},{"thread_desktop",thread},{"input_desktop",inputName},
            {"input_uoi_io",inputReceives!=0},{"thread_uoi_io",threadReceives!=0},{"wts_state",state},{"session_id",session}};
    }
    static IntPtr MediumToken(IntPtr original,TokenFacts own,List<object> attempts,out string method) {
        IntPtr candidate=IntPtr.Zero,linked=IntPtr.Zero;method=null;
        if(Medium(own,own)) {
            if(!DuplicateTokenEx(original,TOKEN_RIGHTS,IntPtr.Zero,2,1,out candidate))throw Failure("Duplicate current Medium token");
            method="duplicate_current_medium";return candidate;
        }
        IntPtr linkInfo=IntPtr.Zero;
        try{
            linkInfo=Information(original,19);linked=Marshal.ReadIntPtr(linkInfo);
            if(!DuplicateTokenEx(linked,TOKEN_RIGHTS,IntPtr.Zero,2,1,out candidate))throw Failure("Duplicate linked primary token");
            var observed=Facts(candidate);attempts.Add(new Dictionary<string,object>{{"method","linked_standard_token"},{"facts",observed.Public()},{"accepted",Medium(observed,own)}});
            if(Medium(observed,own)){method="linked_standard_token";return candidate;}
            CloseHandle(candidate);candidate=IntPtr.Zero;
        }catch(Exception e){attempts.Add(new Dictionary<string,object>{{"method","linked_standard_token"},{"accepted",false},{"error",e.Message}});if(candidate!=IntPtr.Zero){CloseHandle(candidate);candidate=IntPtr.Zero;}}
        finally{if(linkInfo!=IntPtr.Zero)Marshal.FreeHGlobal(linkInfo);if(linked!=IntPtr.Zero)CloseHandle(linked);}
        // LUA_TOKEN and DISABLE_MAX_PRIVILEGE affect this NEW token only. No
        // SANDBOX_INERT flag, credentials, impersonation, or original adjustment.
        if(!CreateRestrictedToken(original,5,0,IntPtr.Zero,0,IntPtr.Zero,0,IntPtr.Zero,out candidate))throw Failure("CreateRestrictedToken LUA");
        IntPtr mediumSid=IntPtr.Zero;
        try {
            if(!ConvertStringSidToSidW("S-1-16-8192",out mediumSid))throw Failure("Create Medium integrity SID");
            var label=new SidAttributes{Sid=mediumSid,Attributes=32};
            if(!SetTokenInformation(candidate,25,ref label,(uint)Marshal.SizeOf(typeof(SidAttributes))+GetLengthSid(mediumSid)))throw Failure("Set NEW token Medium integrity");
            var observed=Facts(candidate);attempts.Add(new Dictionary<string,object>{{"method","restricted_lua_medium"},{"facts",observed.Public()},{"accepted",Medium(observed,own)}});
            if(!Medium(observed,own))throw new InvalidOperationException("Restricted token did not actually meet Medium non-elevated same-user/session contract");
            method="restricted_lua_medium";return candidate;
        }catch{CloseHandle(candidate);throw;}finally{if(mediumSid!=IntPtr.Zero)LocalFree(mediumSid);}
    }
    public static Dictionary<string,object> Run(string executable,string arguments,uint timeoutSeconds) {
        using(var identity=WindowsIdentity.GetCurrent())
            if(!identity.Name.EndsWith("\\WDAGUtilityAccount",StringComparison.OrdinalIgnoreCase))throw new InvalidOperationException("Disposable guest account required");
        bool exactFixture=string.Equals(executable,"C:\\PaneBindV1\\Input\\panebind-v1-guest-fixture.exe",StringComparison.OrdinalIgnoreCase)||
            string.Equals(executable,"C:\\PaneBindV1\\Input\\Debug\\panebind-v1-guest-fixture.exe",StringComparison.OrdinalIgnoreCase);
        if(timeoutSeconds<30||timeoutSeconds>1200||!exactFixture)throw new InvalidOperationException("Exact guest fixture scope required");
        var result=new Dictionary<string,object>{{"status","BLOCKED"},{"fixture_started",false},{"product_cleanup_pass",false},{"process_creation_attempts",new List<object>()},{"token_attempts",new List<object>()}};
        var tokenAttempts=(List<object>)result["token_attempts"];var processAttempts=(List<object>)result["process_creation_attempts"];
        IntPtr original=IntPtr.Zero,candidate=IntPtr.Zero,job=IntPtr.Zero;var info=new ProcessInfo();bool assigned=false;
        try {
            if(!OpenProcessToken(GetCurrentProcess(),TOKEN_RIGHTS,out original))throw Failure("Open own process token");
            var own=Facts(original);result["bootstrap_token"]=own.Public();result["bootstrap_desktop"]=Desktop(own.Session);
            string method;candidate=MediumToken(original,own,tokenAttempts,out method);result["token_method"]=method;
            var startup=new Startup();startup.cb=(uint)Marshal.SizeOf(typeof(Startup));startup.desktop="winsta0\\default";
            string command="\""+executable+"\" "+arguments;
            if(command.Length>1023)throw new InvalidOperationException("Bounded token launch command too long");
            result["requested_executable"]=executable;result["requested_desktop"]=startup.desktop;
            result["requested_directory"]="C:\\PaneBindV1\\Output";result["requested_creation_flags"]=NO_WINDOW|CREATE_SUSPENDED;
            var launchToken=Facts(candidate);result["launch_token"]=launchToken.Public();
            if(!Medium(launchToken,own))throw new InvalidOperationException("Launch token no longer satisfies exact Medium contract");
            bool created=CreateProcessWithTokenW(candidate,0,executable,new StringBuilder(command),NO_WINDOW|CREATE_SUSPENDED,IntPtr.Zero,"C:\\PaneBindV1\\Output",ref startup,out info);
            int error=created?0:Marshal.GetLastWin32Error();processAttempts.Add(new Dictionary<string,object>{{"api","CreateProcessWithTokenW"},{"succeeded",created},{"win32_error",error},{"same_candidate_token",true},{"executable",executable},{"desktop",startup.desktop}});
            // One bounded API contrast only. ERROR_ACCESS_DENIED is not proof
            // that the RO mapping or its ACL is the cause: AsUser has a
            // distinct documented token/privilege contract. Keep the token,
            // image, directory and desktop unchanged; do not adjust security.
            if(!created&&(error==1314||error==5)){
                result["as_user_fallback_trigger_win32_error"]=error;
                result["fallback_desktop"]=Desktop(own.Session);
                startup=new Startup();startup.cb=(uint)Marshal.SizeOf(typeof(Startup));startup.desktop="winsta0\\default";
                info=new ProcessInfo();
                created=CreateProcessAsUserW(candidate,executable,new StringBuilder(command),IntPtr.Zero,IntPtr.Zero,false,NO_WINDOW|CREATE_SUSPENDED,IntPtr.Zero,"C:\\PaneBindV1\\Output",ref startup,out info);
                error=created?0:Marshal.GetLastWin32Error();processAttempts.Add(new Dictionary<string,object>{{"api","CreateProcessAsUserW"},{"succeeded",created},{"win32_error",error},{"same_candidate_token",true},{"executable",executable},{"desktop",startup.desktop}});
            }
            if(!created)throw new Win32Exception(error,"Medium child creation failed; no security policy or ACL changes attempted");
            result["fixture_pid"]=info.pid;result["fixture_tid"]=info.tid;result["requested_desktop"]=startup.desktop;
            uint childSession;if(!ProcessIdToSessionId(info.pid,out childSession))throw Failure("Read suspended child process session");
            result["actual_process_session"]=childSession;
            if(childSession!=(uint)own.Session)throw new InvalidOperationException("Actual child process session changed");
            IntPtr childToken;if(!OpenProcessToken(info.process,QUERY,out childToken))throw Failure("Read suspended child token");
            try {var actual=Facts(childToken);result["actual_child_token"]=actual.Public();if(!Medium(actual,own))throw new InvalidOperationException("Actual child token is not exact Medium/non-elevated");}
            finally{CloseHandle(childToken);}
            job=CreateJobObjectW(IntPtr.Zero,null);if(job==IntPtr.Zero)throw Failure("Create fixture-only job");
            var limits=new JobExtended();limits.basic.flags=0x2000;
            if(!SetInformationJobObject(job,9,ref limits,(uint)Marshal.SizeOf(typeof(JobExtended))))throw Failure("Set fixture-only job close limit");
            if(!AssignProcessToJobObject(job,info.process))throw Failure("Assign exact suspended fixture to job");assigned=true;
            if(ResumeThread(info.thread)==uint.MaxValue)throw Failure("Resume exact Medium fixture");
            result["fixture_started"]=true;result["creation_flags"]="CREATE_SUSPENDED|CREATE_NO_WINDOW";
            uint waited=WaitForSingleObject(info.process,timeoutSeconds*1000);result["wait_result"]=waited;
            if(waited==258){result["status"]="TIMEOUT";result["forced_test_cleanup"]=true;return result;}
            if(waited!=0)throw Failure("Wait for exact fixture");
            uint exit;if(!GetExitCodeProcess(info.process,out exit))throw Failure("Read fixture exit");result["fixture_exit_code"]=exit;
            IntPtr accounting=Marshal.AllocHGlobal(48);
            try{uint needed;if(!QueryInformationJobObject(job,1,accounting,48,out needed))throw Failure("Query fixture descendants");
                uint active=(uint)Marshal.ReadInt32(accounting,40);result["descendants_alive_at_fixture_exit"]=active;result["forced_test_cleanup"]=active!=0;}
            finally{Marshal.FreeHGlobal(accounting);}
            result["status"]=exit==0?"FIXTURE_EXITED_ZERO_EVIDENCE_REQUIRED":"FIXTURE_FAILED";
            // Zero exit is never itself a feature or product-cleanup PASS.
            return result;
        }catch(Exception e){result["error"]=e.Message;return result;}
        finally {
            if(job!=IntPtr.Zero)CloseHandle(job);
            if(!assigned&&info.process!=IntPtr.Zero)TerminateProcess(info.process,78);
            if(info.thread!=IntPtr.Zero)CloseHandle(info.thread);if(info.process!=IntPtr.Zero)CloseHandle(info.process);
            if(candidate!=IntPtr.Zero)CloseHandle(candidate);if(original!=IntPtr.Zero)CloseHandle(original);
        }
    }
}
'@
    foreach($configuration in @('Debug','Release')) {
        $sourceDir=if($configuration -ceq 'Release'){$inputDir}else{Join-Path $inputDir 'Debug'}
        $sourceProduct=Join-Path $sourceDir 'panebind.exe'
        $fixture=Join-Path $sourceDir 'panebind-v1-guest-fixture.exe'
        $actualDir=Join-Path $outputDir ($RunId+'-product-'+$configuration)
        if(Test-Path -LiteralPath $actualDir){throw 'Product output directory already exists; do not overwrite evidence.'}
        [IO.Directory]::CreateDirectory($actualDir)|Out-Null
        $actualProduct=Join-Path $actualDir 'panebind.exe'
        [IO.File]::Copy($sourceProduct,$actualProduct,$false)
        New-Evidence (Join-Path $actualDir 'portable.flag') "PaneBind V1 portable test candidate`n"
        $sourceHash=(Get-FileHash -LiteralPath $sourceProduct -Algorithm SHA256).Hash
        $actualHash=(Get-FileHash -LiteralPath $actualProduct -Algorithm SHA256).Hash
        if($sourceHash -cne $actualHash){throw 'Copied product hash mismatch.'}
        $arguments='--run-id '+$RunId+' --output "'+$outputDir+'" --product "'+$actualProduct+'"'
        $stage=[ordered]@{configuration=$configuration;source_input=$sourceProduct;actual_exe=$actualProduct;
            source_input_sha256=$sourceHash;actual_exe_sha256=$actualHash;hash_equal=$true;
            fixture=$fixture;fixture_sha256=(Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash;
            arguments=$arguments;actual_result=$null;feature_result='NOT_EVALUATED';human_uat='NOT_RUN'}
        $stage.actual_result=[PaneBindV1MediumGuest]::Run($fixture,$arguments,[uint32]$StageTimeoutSeconds)
        if($stage.actual_result.ContainsKey('fixture_exit_code')){
            $stage['fixture_exit_reason']=Fixture-ExitReason ([uint32]$stage.actual_result['fixture_exit_code'])
        }
        $summary.stages+=,$stage
        New-Evidence (Join-Path $outputDir ($RunId+'-'+$configuration+'-launch.json')) ($stage|ConvertTo-Json -Depth 15)
        if($stage.actual_result.status -cne 'FIXTURE_EXITED_ZERO_EVIDENCE_REQUIRED' -or $stage.actual_result.forced_test_cleanup){
            throw ('Actual Medium '+$configuration+' fixture failed or required test-only forced cleanup; inspect launch/raw evidence.')}
    }
    $summary.status='AUTOMATED_FIXTURES_EXITED_REVIEW_EVIDENCE'
} catch {
    $summary.status='BLOCKED_OR_FAILED'
    $summary.error=$_.Exception.Message
} finally {
    if($summaryPath -and -not (Test-Path -LiteralPath $summaryPath)){
        New-Evidence $summaryPath ($summary|ConvertTo-Json -Depth 18)
    }
}
if($summary.status -cne 'AUTOMATED_FIXTURES_EXITED_REVIEW_EVIDENCE'){exit 78}
exit 0
