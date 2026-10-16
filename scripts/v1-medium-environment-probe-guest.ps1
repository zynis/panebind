[CmdletBinding()]
param([Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-f]{32}$')][string]$RunId)
$ErrorActionPreference='Stop'
$inputDir='C:\PaneBindV1\Input'
$outputDir='C:\PaneBindV1\Output'
$leaf='v1-medium-environment-probe-guest.ps1'
$evidence=$null
function Read-ExactMarker([string]$Path) {
    $item=Get-Item -LiteralPath $Path -ErrorAction Stop
    if($item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -or $item.Length -gt 34){throw 'Run marker is not a bounded plain file.'}
    $value=[IO.File]::ReadAllText($Path,[Text.Encoding]::ASCII)
    if($value.EndsWith("`r`n")){$value=$value.Substring(0,$value.Length-2)}
    if($value -cnotmatch '^[0-9a-f]{32}$'){throw 'Exact run marker is invalid.'}
    return $value
}
try {
    # Host misuse protection precedes Add-Type, account APIs or any evidence
    # write. There is no bypass switch and this is not a general RunAs tool.
    if(-not [string]::Equals([IO.Path]::GetFullPath($PSScriptRoot),$inputDir,[StringComparison]::OrdinalIgnoreCase) -or
       -not [string]::Equals([IO.Path]::GetFullPath($PSCommandPath),(Join-Path $inputDir $leaf),[StringComparison]::OrdinalIgnoreCase)){
        throw 'Probe must execute from its exact Sandbox input path.'}
    foreach($directory in @('C:\PaneBindV1',$inputDir,$outputDir)){
        $item=Get-Item -LiteralPath $directory -ErrorAction Stop
        if(-not $item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Exact guest directories must be plain directories.'}
    }
    $identity=[Security.Principal.WindowsIdentity]::GetCurrent()
    try{if(-not [string]::Equals($identity.Name,($env:COMPUTERNAME+'\WDAGUtilityAccount'),[StringComparison]::OrdinalIgnoreCase)){
        throw 'Only the local disposable Windows Sandbox account is supported.'}}finally{$identity.Dispose()}
    if((Read-ExactMarker (Join-Path $inputDir 'panebind-v1-input.marker')) -cne $RunId -or
       (Read-ExactMarker (Join-Path $outputDir 'panebind-v1-output.marker')) -cne $RunId){throw 'Input/output run markers disagree.'}
    $manifestFile=Get-Item -LiteralPath (Join-Path $inputDir 'manifest.json') -ErrorAction Stop
    if($manifestFile.PSIsContainer -or ($manifestFile.Attributes -band [IO.FileAttributes]::ReparsePoint) -or $manifestFile.Length -gt 1MB){throw 'Manifest must be a bounded plain file.'}
    $manifest=[IO.File]::ReadAllText($manifestFile.FullName,[Text.Encoding]::UTF8)|ConvertFrom-Json
    if($manifest.schema -cne 'panebind-v1-sandbox/1' -or $manifest.run_id -cne $RunId -or
       $manifest.guest_contract -cne 'panebind-v1-guest-contract/2' -or
       $manifest.guest_input -cne $inputDir -or $manifest.guest_output -cne $outputDir -or
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
    $evidence=Join-Path $outputDir ($RunId+'-medium-environment-probe.json')
    if(Test-Path -LiteralPath $evidence){throw 'Probe evidence already exists; use a fresh RunId.'}
    # Original test orchestration only. Official NetUserAdd/LogonUser,
    # AccessCheck and window-station/desktop security contracts are used.
    # No process creation, input, impersonation, ACL mutation or token mutation.
    Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Security.Principal;
using System.Text;

public static class PaneBindV1MediumEnvironmentProbe {
    [StructLayout(LayoutKind.Sequential)] struct UserInfo1 {
        public IntPtr name,password; public uint passwordAge,privilege;
        public IntPtr home,comment; public uint flags; public IntPtr script;
    }
    [StructLayout(LayoutKind.Sequential)] struct UserInfo23 {
        public IntPtr name,fullName,comment; public uint flags; public IntPtr sid;
    }
    [StructLayout(LayoutKind.Sequential)] struct GenericMapping { public uint read,write,execute,all; }
    [DllImport("netapi32.dll",CharSet=CharSet.Unicode)] static extern uint NetUserAdd(string server,uint level,ref UserInfo1 info,out uint parameter);
    [DllImport("netapi32.dll",CharSet=CharSet.Unicode)] static extern uint NetUserGetInfo(string server,string user,uint level,out IntPtr data);
    [DllImport("netapi32.dll",CharSet=CharSet.Unicode)] static extern uint NetUserDel(string server,string user);
    [DllImport("netapi32.dll")] static extern uint NetApiBufferFree(IntPtr data);
    [DllImport("advapi32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool LogonUserW(string user,string domain,IntPtr password,uint type,uint provider,out IntPtr token);
    [DllImport("advapi32.dll",SetLastError=true)] static extern bool GetTokenInformation(IntPtr token,int kind,IntPtr data,uint size,out uint needed);
    [DllImport("advapi32.dll",SetLastError=true)] static extern bool DuplicateTokenEx(IntPtr original,uint rights,IntPtr attributes,int level,int type,out IntPtr token);
    [DllImport("advapi32.dll",SetLastError=true)] static extern bool AccessCheck(IntPtr descriptor,IntPtr token,uint desired,ref GenericMapping mapping,IntPtr privileges,ref uint size,out uint granted,out bool allowed);
    [DllImport("advapi32.dll",CharSet=CharSet.Unicode)] static extern uint GetNamedSecurityInfoW(string name,uint type,uint information,out IntPtr owner,out IntPtr group,out IntPtr dacl,out IntPtr sacl,out IntPtr descriptor);
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
    static Dictionary<string,object> Desktop() {
        using(var identity=WindowsIdentity.GetCurrent())
            if(!String.Equals(identity.Name,Environment.MachineName+"\\WDAGUtilityAccount",StringComparison.OrdinalIgnoreCase))throw new InvalidOperationException("Exact local Sandbox account required");
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
        if(size==0||size>65536)throw new InvalidOperationException("Token information size unavailable");
        var data=Marshal.AllocHGlobal((int)size);
        if(!GetTokenInformation(token,kind,data,size,out size)){Marshal.FreeHGlobal(data);throw new InvalidOperationException("Token information query failed");}
        return data;
    }
    static int TokenNumber(IntPtr token,int kind) { var data=TokenInfo(token,kind);try{return Marshal.ReadInt32(data);}finally{Marshal.FreeHGlobal(data);} }
    static string TokenSid(IntPtr token) { var data=TokenInfo(token,1);try{return new SecurityIdentifier(Marshal.ReadIntPtr(data)).Value;}finally{Marshal.FreeHGlobal(data);} }
    static Dictionary<string,object> TokenFacts(IntPtr token,string createdSid,int desktopSession,out bool accepted) {
        var sid=TokenSid(token);var data=TokenInfo(token,25);int integrity;
        try { var label=new SecurityIdentifier(Marshal.ReadIntPtr(data)).Value;var separator=label.LastIndexOf('-');integrity=Int32.Parse(label.Substring(separator+1)); }finally{Marshal.FreeHGlobal(data);}
        var elevated=TokenNumber(token,20)!=0;var session=TokenNumber(token,12);
        var uiAccess=TokenNumber(token,26)!=0;var appContainer=TokenNumber(token,29)!=0;
        bool admin;using(var identity=new WindowsIdentity(token))admin=new WindowsPrincipal(identity).IsInRole(WindowsBuiltInRole.Administrator);
        accepted=sid==createdSid&&integrity==8192&&!elevated&&!admin&&!uiAccess&&!appContainer;
        // This is a credential token, NOT a created interactive process. Its
        // real SessionId may differ; no token/session mutation is performed.
        return new Dictionary<string,object>{{"sid_sha256",Hash(sid)},{"created_account_sid_matches",sid==createdSid},{"integrity_rid",integrity},{"elevated",elevated},{"admin_enabled",admin},{"ui_access",uiAccess},{"app_container",appContainer},{"credential_token_session_id",session},{"current_desktop_session_id",desktopSession},{"token_session_matches_current",session==desktopSession},{"ordinary_security_facts_verified",accepted},{"interactive_child_session_validation","NOT_RUN"}};
    }
    static string AccountSid(string name,out uint status) {
        IntPtr data;status=NetUserGetInfo(null,name,23,out data);
        if(status!=0)return null;
        try { var user=(UserInfo23)Marshal.PtrToStructure(data,typeof(UserInfo23));return new SecurityIdentifier(user.sid).Value; }finally{NetApiBufferFree(data);}
    }
    static Dictionary<string,object> Check(IntPtr descriptor,IntPtr token,uint desired,GenericMapping mapping,string name) {
        var result=new Dictionary<string,object>{{"object",name},{"requested_access",desired},{"result","UNKNOWN"},{"read_only_access_check",true},{"mandatory_integrity_or_actual_process_access_proved",false}};
        uint size=4096,granted;bool allowed;var privileges=Marshal.AllocHGlobal((int)size);
        try {
            if(!AccessCheck(descriptor,token,desired,ref mapping,privileges,ref size,out granted,out allowed)){
                result["win32_error"]=Marshal.GetLastWin32Error();return result;
            }
            result["result"]=allowed?"ALLOW":"DENIED";result["granted_access"]=granted;result["win32_error"]=0;return result;
        }finally{Marshal.FreeHGlobal(privileges);}
    }
    static Dictionary<string,object> CheckUserObject(IntPtr handle,IntPtr token,uint desired,GenericMapping mapping,string name) {
        uint information=7,size;GetUserObjectSecurity(handle,ref information,IntPtr.Zero,0,out size);
        if(size==0||size>131072)return new Dictionary<string,object>{{"object",name},{"requested_access",desired},{"result","UNKNOWN"},{"read_only_access_check",true},{"win32_error",Marshal.GetLastWin32Error()}};
        var descriptor=Marshal.AllocHGlobal((int)size);
        try {
            if(!GetUserObjectSecurity(handle,ref information,descriptor,size,out size))return new Dictionary<string,object>{{"object",name},{"requested_access",desired},{"result","UNKNOWN"},{"read_only_access_check",true},{"win32_error",Marshal.GetLastWin32Error()}};
            return Check(descriptor,token,desired,mapping,name);
        }finally{Marshal.FreeHGlobal(descriptor);}
    }
    static Dictionary<string,object> CheckDirectory(string path,IntPtr token,uint desired,string name) {
        IntPtr owner,group,dacl,sacl,descriptor;var code=GetNamedSecurityInfoW(path,1,7,out owner,out group,out dacl,out sacl,out descriptor);
        if(code!=0)return new Dictionary<string,object>{{"object",name},{"requested_access",desired},{"result","UNKNOWN"},{"read_only_access_check",true},{"win32_error",code}};
        try { return Check(descriptor,token,desired,new GenericMapping{read=0x120089,write=0x120116,execute=0x1200A0,all=0x1F01FF},name); }
        finally{if(descriptor!=IntPtr.Zero)LocalFree(descriptor);}
    }
    public static Dictionary<string,object> Run(string runId) {
        if(runId==null||runId.Length!=32)throw new InvalidOperationException("Exact RunId required");
        foreach(char c in runId)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))throw new InvalidOperationException("Exact RunId required");
        var result=new Dictionary<string,object>{{"status","UNKNOWN"},{"account_created",false},{"account_cleanup_confirmed",false},{"gui_environment_ready",false},{"product_execution","NOT_RUN"},{"explorer_execution","NOT_RUN"},{"synthetic_input_sent",false},{"existing_acl_changed",false},{"existing_account_or_group_changed",false},{"original_token_or_policy_changed",false},{"password_logged_or_written",false}};
        // Must pass the actual session/desktop guard before account APIs.
        var desktop=Desktop();result["bootstrap_desktop"]=desktop;
        var name="PBV1_"+runId.Substring(0,12);result["created_account_name"]=name;
        var checks=new List<Dictionary<string,object>>();result["access_checks"]=checks;
        var cleanup=Item();result["cleanup"]=cleanup;
        IntPtr password=IntPtr.Zero,nameMemory=IntPtr.Zero,comment=IntPtr.Zero,token=IntPtr.Zero,impersonation=IntPtr.Zero;
        string createdSid=null;bool created=false;const int passwordLength=48;
        try {
            var random=new byte[passwordLength];using(var generator=RandomNumberGenerator.Create())generator.GetBytes(random);
            password=Marshal.AllocHGlobal((passwordLength+1)*2);
            const string alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-";
            for(int n=0;n<passwordLength;n++)Marshal.WriteInt16(password,n*2,(short)alphabet[random[n]&63]);
            Marshal.WriteInt16(password,0,(short)'a');Marshal.WriteInt16(password,2,(short)'A');Marshal.WriteInt16(password,4,(short)'7');Marshal.WriteInt16(password,6,(short)'!');
            Marshal.WriteInt16(password,passwordLength*2,0);Array.Clear(random,0,random.Length);
            nameMemory=Marshal.StringToHGlobalUni(name);comment=Marshal.StringToHGlobalUni("PaneBind disposable guest probe "+runId);
            var info=new UserInfo1{name=nameMemory,password=password,privilege=1,comment=comment,flags=0x201};
            uint parameter;var added=NetUserAdd(null,1,ref info,out parameter);
            result["NetUserAdd_status"]=added;result["NetUserAdd_parameter_error"]=parameter;
            if(added!=0){result["status"]=added==5?"DENIED":"UNKNOWN";result["reason"]=added==2224?"unique_account_already_exists_not_reused":"new_local_account_creation_rejected";return result;}
            created=true;result["account_created"]=true;
            uint sidStatus;createdSid=AccountSid(name,out sidStatus);result["created_sid_query_status"]=sidStatus;
            if(createdSid==null){result["reason"]="new_account_sid_unknown_no_unverified_deletion";return result;}
            result["created_account_sid_sha256"]=Hash(createdSid);
            if(!LogonUserW(name,".",password,2,0,out token)){result["status"]="DENIED";result["reason"]="new_account_interactive_logon_rejected";result["LogonUser_win32_error"]=Marshal.GetLastWin32Error();return result;}
            result["LogonUser_succeeded"]=true;
            bool ordinary;result["credential_token"]=TokenFacts(token,createdSid,(int)desktop["session_id"],out ordinary);
            if(!ordinary){result["status"]="DENIED";result["reason"]="new_account_token_not_exact_standard_medium";return result;}
            if(!DuplicateTokenEx(token,0x0E,IntPtr.Zero,2,2,out impersonation)){result["reason"]="read_only_accesscheck_token_copy_failed";result["token_copy_win32_error"]=Marshal.GetLastWin32Error();return result;}
            // Only explicit rights needed by a GUI/input test are requested.
            // No EXITWINDOWS, WRITE_DAC, WRITE_OWNER or desktop-switch rights.
            checks.Add(CheckUserObject(GetProcessWindowStation(),impersonation,0x322,new GenericMapping{read=0x20303,write=0x2001C,execute=0x20060,all=0xF037F},"WinSta0:READATTRIBUTES|ACCESSGLOBALATOMS|ENUMERATE|READSCREEN"));
            checks.Add(CheckUserObject(GetThreadDesktop(GetCurrentThreadId()),impersonation,0xCF,new GenericMapping{read=0x20041,write=0x200BE,execute=0x20100,all=0xF01FF},"Default:READOBJECTS|CREATEWINDOW|CREATEMENU|HOOKCONTROL|ENUMERATE|WRITEOBJECTS"));
            checks.Add(CheckDirectory("C:\\PaneBindV1\\Input",impersonation,0x1200A9,"Input:READ|TRAVERSE"));
            checks.Add(CheckDirectory("C:\\PaneBindV1\\Output",impersonation,0x1201BF,"Output:READ|WRITE|TRAVERSE"));
            bool denied=false,unknown=false;
            foreach(var check in checks){denied|=(string)check["result"]=="DENIED";unknown|=(string)check["result"]=="UNKNOWN";}
            result["status"]=denied?"DENIED":unknown?"UNKNOWN":"READ_ONLY_ACL_ALLOWED_PROCESS_NOT_RUN";
            result["reason"]=denied?"existing_guest_acl_denies_required_access_no_acl_change_attempted":unknown?"required_access_could_not_be_proved":"candidate_acl_only_gui_shell_and_child_session_still_unverified";
            return result;
        }catch(Exception error){result["status"]="UNKNOWN";result["reason"]="bounded_probe_exception";result["error_type"]=error.GetType().Name;return result;}
        finally {
            if(impersonation!=IntPtr.Zero)CloseHandle(impersonation);if(token!=IntPtr.Zero)CloseHandle(token);
            if(password!=IntPtr.Zero){for(int n=0;n<=passwordLength;n++)Marshal.WriteInt16(password,n*2,0);Marshal.FreeHGlobal(password);}
            if(nameMemory!=IntPtr.Zero)Marshal.FreeHGlobal(nameMemory);if(comment!=IntPtr.Zero)Marshal.FreeHGlobal(comment);
            cleanup["delete_attempted"]=false;cleanup["profile_deletion_attempted"]=false;
            if(!created){cleanup["result"]="NOT_CREATED_NO_DELETION";result["account_cleanup_confirmed"]=true;}
            else {
                try {
                    uint status;var current=AccountSid(name,out status);cleanup["sid_query_status"]=status;
                    if(createdSid!=null&&current==createdSid){
                        cleanup["sid_matches_created_account"]=true;cleanup["delete_attempted"]=true;
                        var deleted=NetUserDel(null,name);cleanup["NetUserDel_status"]=deleted;
                        uint afterStatus;AccountSid(name,out afterStatus);cleanup["after_query_status"]=afterStatus;
                        var confirmed=deleted==0&&afterStatus==2221;
                        cleanup["result"]=confirmed?"EXACT_CREATED_ACCOUNT_REMOVED":"UNKNOWN";result["account_cleanup_confirmed"]=confirmed;
                    }else{cleanup["sid_matches_created_account"]=false;cleanup["result"]="UNKNOWN_IDENTITY_NOT_DELETED";}
                }catch(Exception cleanupError){cleanup["result"]="UNKNOWN_CLEANUP_EXCEPTION";cleanup["error_type"]=cleanupError.GetType().Name;}
                if(!(bool)result["account_cleanup_confirmed"]){result["probe_result_before_cleanup_failure"]=result["status"];result["status"]="UNKNOWN";result["reason"]="created_account_cleanup_not_confirmed";}
            }
        }
    }
}
'@
    $result=[PaneBindV1MediumEnvironmentProbe]::Run($RunId)
    $record=[ordered]@{schema='panebind-v1-medium-environment-probe/1';utc=[DateTime]::UtcNow.ToString('o');
        run_id=$RunId;probe_source_commit=$manifest.packaging_head;driver_source_commit=$manifest.driver_source_commit;
        probe_sha256=$scriptEntry[0].sha256;
        automated_guest_probe=$true;human_uat='NOT_RUN';result=$result}
    $text=$record|ConvertTo-Json -Depth 12
    $stream=[IO.File]::Open($evidence,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
    try{$bytes=(New-Object Text.UTF8Encoding($false)).GetBytes($text);$stream.Write($bytes,0,$bytes.Length)}finally{$stream.Dispose()}
    if($result.status -ceq 'READ_ONLY_ACL_ALLOWED_PROCESS_NOT_RUN' -and $result.account_cleanup_confirmed){exit 0}
    if($result.status -ceq 'DENIED' -and $result.account_cleanup_confirmed){exit 2}
    exit 3
}catch{
    # Guard failures do not create accounts or claim a platform observation.
    # Never stringify native call arguments or credentials into error output.
    Write-Error ('Guest medium environment probe stopped: '+$_.Exception.Message)
    exit 78
}
