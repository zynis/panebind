[CmdletBinding()]
param([Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-f]{32}$')][string]$RunId,
    [ValidateSet('Parent','Child')][string]$Mode='Parent',
    [ValidatePattern('^[0-9a-f]{32}$')][string]$ChildNonce,
    [ValidatePattern('^[A-F0-9]{64}$')][string]$BindingSha256)
$ErrorActionPreference='Stop'
$inputDir='C:\PaneBindV1\Input'
$outputDir='C:\PaneBindV1\Output'
$leaf='v1-medium-desktop-probe-guest.ps1'
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
    try{
        $wanted=if($Mode -ceq 'Parent'){$env:COMPUTERNAME+'\WDAGUtilityAccount'}else{$env:COMPUTERNAME+'\PBV1D_'+$RunId.Substring(0,12)}
        if(-not [string]::Equals($identity.Name,$wanted,[StringComparison]::OrdinalIgnoreCase)){throw 'Exact disposable parent or newly bound child account required.'}
    }finally{$identity.Dispose()}
    if($Mode -ceq 'Parent' -and ($ChildNonce -or $BindingSha256)){throw 'Parent mode does not accept child binding arguments.'}
    if($Mode -ceq 'Child' -and (-not $ChildNonce -or -not $BindingSha256)){throw 'Child requires its exact one-time binding.'}
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
    $evidence=Join-Path $outputDir ($RunId+$(if($Mode -ceq 'Parent'){'-medium-desktop-probe.json'}else{'-medium-desktop-child.json'}))
    if(Test-Path -LiteralPath $evidence){throw 'Probe evidence already exists; use a fresh RunId.'}
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

    [StructLayout(LayoutKind.Sequential)] struct SidAttributes { public IntPtr sid; public uint attributes; }
    [StructLayout(LayoutKind.Sequential)] struct GroupsOne { public uint count; public SidAttributes first; }
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
    [DllImport("advapi32.dll",SetLastError=true)] static extern bool OpenProcessToken(IntPtr process,uint access,out IntPtr token);
    [DllImport("kernel32.dll",SetLastError=true)] static extern uint ResumeThread(IntPtr thread);
    [DllImport("kernel32.dll",SetLastError=true)] static extern uint WaitForSingleObject(IntPtr value,uint milliseconds);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool TerminateProcess(IntPtr process,uint code);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool GetExitCodeProcess(IntPtr process,out uint code);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool GetProcessTimes(IntPtr process,out FileTime birth,out FileTime exit,out FileTime kernel,out FileTime user);
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
    public static Dictionary<string,object> RunParent(string runId,string source,string scriptHash) {
        var desktop=Desktop(true);int session=(int)desktop["session_id"];var name="PBV1D_"+runId.Substring(0,12);
        var result=new Dictionary<string,object>{{"status","UNKNOWN"},{"bootstrap_desktop",desktop},{"created_account_name",name},
            {"account_created",false},{"account_cleanup_confirmed",false},{"dacl_cleanup_confirmed",false},{"child_process_exited",false},
            {"CreateProcessWithTokenW_attempts",0},{"lpDesktop","NULL"},{"LOGON_WITH_PROFILE",true},{"product_started",false},
            {"explorer_started",false},{"synthetic_input_sent",false},{"host_configuration_changed",false}};
        IntPtr password=IntPtr.Zero,nameMemory=IntPtr.Zero,comment=IntPtr.Zero,token=IntPtr.Zero,childToken=IntPtr.Zero;
        IntPtr station=IntPtr.Zero,defaultDesktop=IntPtr.Zero;var child=new ProcessInfo();AclState beforeStation=null,beforeDesktop=null;
        string account=null,logon=null;bool created=false,resumed=false;
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
            bool ordinary;result["credential_token"]=TokenFacts(token,account,session,out ordinary);logon=LogonSid(token);
            result["actual_logon_sid_sha256"]=Hash(logon);
            if(!ordinary||TokenNumber(token,12)!=session){result["reason"]="actual_standard_token_or_session_rejected";return result;}
            var nonce=Guid.NewGuid().ToString("N");
            var binding="{\"schema\":\"panebind-v1-medium-desktop-binding/1\",\"run_id\":\""+runId+"\",\"source_commit\":\""+source+
                "\",\"probe_sha256\":\""+scriptHash+"\",\"account_sid_sha256\":\""+Hash(account)+"\",\"session_id\":"+session+",\"nonce\":\""+nonce+"\"}";
            var bindingPath="C:\\PaneBindV1\\Output\\"+runId+"-medium-desktop-binding.json";NewText(bindingPath,binding);
            var bindingHash=ByteHash(new UTF8Encoding(false).GetBytes(binding));result["binding_sha256"]=bindingHash;result["binding_nonce"]=nonce;
            var executable="C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe";
            var command=new StringBuilder("\""+executable+"\" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File C:\\PaneBindV1\\Input\\v1-medium-desktop-probe-guest.ps1 -RunId "+runId+
                " -Mode Child -ChildNonce "+nonce+" -BindingSha256 "+bindingHash);
            if(command.Length>1000)throw new InvalidOperationException("Bounded exact child command required");
            var startup=new Startup();startup.size=(uint)Marshal.SizeOf(typeof(Startup));startup.desktop=null;
            result["CreateProcessWithTokenW_attempts"]=1;
            bool launched=CreateProcessWithTokenW(token,1,executable,command,0x08000004,IntPtr.Zero,"C:\\PaneBindV1\\Input",ref startup,out child);
            result["CreateProcessWithTokenW_succeeded"]=launched;result["CreateProcessWithTokenW_error"]=launched?0:Marshal.GetLastWin32Error();
            var afterStation=ReadDacl(station);var afterDesktop=ReadDacl(defaultDesktop);bool stationExplained,desktopExplained;List<object> stationDelta,desktopDelta;
            recordedStation=Delta(beforeStation,afterStation,account,logon,out stationExplained,out stationDelta);
            recordedDesktop=Delta(beforeDesktop,afterDesktop,account,logon,out desktopExplained,out desktopDelta);
            result["station_dacl_after_sha256"]=afterStation.hash;result["desktop_dacl_after_sha256"]=afterDesktop.hash;
            result["station_added_aces"]=stationDelta;result["desktop_added_aces"]=desktopDelta;
            result["dacl_delta_explained"]=stationExplained&&desktopExplained;
            if(!launched||!stationExplained||!desktopExplained){result["reason"]=!launched?"documented_launch_failed":"unexplained_acl_change_child_not_resumed";return result;}
            result["child_pid"]=child.pid;result["child_tid"]=child.tid;
            FileTime birth,exit,kernel,user;if(!GetProcessTimes(child.process,out birth,out exit,out kernel,out user))throw new InvalidOperationException("Exact child birth unknown");
            result["child_birth_filetime"]=((ulong)birth.high<<32)|birth.low;
            if(!OpenProcessToken(child.process,8,out childToken))throw new InvalidOperationException("Actual suspended child token unknown");
            bool actualOrdinary;result["actual_child_token"]=TokenFacts(childToken,account,session,out actualOrdinary);
            if(!actualOrdinary||TokenNumber(childToken,12)!=session||LogonSid(childToken)!=logon)throw new InvalidOperationException("Actual suspended child identity rejected");
            uint resume=ResumeThread(child.thread);result["ResumeThread_return"]=resume;
            if(resume==UInt32.MaxValue)throw new InvalidOperationException("Exact child resume failed");resumed=true;
            uint waited=WaitForSingleObject(child.process,20000);result["child_wait_return"]=waited;
            if(waited!=0){result["reason"]="bounded_child_exit_not_observed";return result;}
            uint code;if(!GetExitCodeProcess(child.process,out code))throw new InvalidOperationException("Child exit status unavailable");
            result["child_exit_code"]=code;result["child_process_exited"]=true;
            var childEvidence="C:\\PaneBindV1\\Output\\"+runId+"-medium-desktop-child.json";
            var file=new FileInfo(childEvidence);
            if(!file.Exists||file.Length>65536||(file.Attributes&FileAttributes.ReparsePoint)!=0){result["reason"]="bounded_child_receipt_missing";return result;}
            result["child_receipt_text"]=File.ReadAllText(childEvidence,Encoding.UTF8);
            result["child_receipt_sha256"]=ByteHash(File.ReadAllBytes(childEvidence));
            result["status"]=code==0?"CHILD_EXITED_ZERO_RECEIPT_REQUIRED":"UNKNOWN";result["reason"]="actual_child_receipt_requires_independent_validation";return result;
        }catch(Exception e){result["reason"]="bounded_desktop_probe_exception";result["error_type"]=e.GetType().Name;return result;}
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
        var result=new Dictionary<string,object>{{"status","UNKNOWN"},{"gui_created",false},{"window_destroyed",false},{"product_started",false},{"explorer_started",false},{"synthetic_input_sent",false}};
        IntPtr window=IntPtr.Zero;string className="PaneBindMediumDesktopProbe_"+runId;var instance=GetModuleHandleW(null);bool registered=false;int initialized=unchecked((int)0x80004005);
        try {
            using(var identity=WindowsIdentity.GetCurrent()){
                var actualSid=identity.User.Value;bool ordinary;result["actual_token"]=TokenFacts(identity.Token,actualSid,expectedSession,out ordinary);
                if(!ordinary||Hash(actualSid)!=expectedSid||TokenNumber(identity.Token,12)!=expectedSession)throw new InvalidOperationException("Bound child token mismatch");
            }
            result["desktop"]=Desktop(false);result["actual_pid"]=Process.GetCurrentProcess().Id;result["actual_tid"]=GetCurrentThreadId();
            var foreground=GetForegroundWindow();result["foreground_before"]=foreground.ToInt64();
            var cls=new WindowClass{size=(uint)Marshal.SizeOf(typeof(WindowClass)),procedure=windowProcedure,instance=instance,brush=new IntPtr(6),className=className};
            registered=RegisterClassExW(ref cls)!=0;result["class_registered"]=registered;result["RegisterClassExW_error"]=registered?0:Marshal.GetLastWin32Error();
            if(!registered)throw new InvalidOperationException("Own Win32 class registration failed");
            window=CreateWindowExW(0,className,"PaneBind standard guest desktop probe",0x00CF0000,120,120,420,240,IntPtr.Zero,IntPtr.Zero,instance,IntPtr.Zero);
            result["gui_created"]=window!=IntPtr.Zero;result["CreateWindowExW_error"]=window!=IntPtr.Zero?0:Marshal.GetLastWin32Error();result["window_handle"]=window.ToInt64();
            if(window==IntPtr.Zero)throw new InvalidOperationException("Own blank Win32 window creation failed");
            ShowWindow(window,8);result["window_visible"]=IsWindowVisible(window);
            initialized=CoInitializeEx(IntPtr.Zero,2);result["COM_initialize_hresult"]=initialized;object manager=null;
            try {
                if(initialized>=0){
                    manager=Activator.CreateInstance(Type.GetTypeFromCLSID(new Guid("aa509086-5ca9-4c25-8f95-589d3c07b48a")));
                    bool current;int hr=((DesktopManager)manager).IsWindowOnCurrentVirtualDesktop(window,out current);
                    result["virtual_desktop_query_hresult"]=hr;result["on_current_virtual_desktop"]=hr==0?(object)current:null;
                }
            }catch(Exception e){result["virtual_desktop_error_type"]=e.GetType().Name;}
            finally{if(manager!=null&&Marshal.IsComObject(manager))Marshal.ReleaseComObject(manager);if(initialized>=0){CoUninitialize();initialized=unchecked((int)0x80004005);}}
            var timer=SetTimer(window,new UIntPtr(1),300,IntPtr.Zero);result["one_shot_timer_started"]=timer!=UIntPtr.Zero;
            if(timer==UIntPtr.Zero)throw new InvalidOperationException("Own bounded GUI timer failed");
            Message message;int received;
            while((received=GetMessageW(out message,IntPtr.Zero,0,0))>0){TranslateMessage(ref message);DispatchMessageW(ref message);}
            result["message_pump_return"]=received;result["actual_timer_message"]=timerObserved;result["DestroyWindow_returned"]=destroyReturned;
            result["actual_WM_DESTROY"]=destroyObserved;result["window_destroyed"]=destroyObserved&&!IsWindow(window);
            result["foreground_after"]=GetForegroundWindow().ToInt64();
            bool vd=result.ContainsKey("on_current_virtual_desktop")&&result["on_current_virtual_desktop"] is bool&&(bool)result["on_current_virtual_desktop"];
            bool accepted=(bool)result["window_visible"]&&timerObserved&&destroyReturned&&destroyObserved&&!IsWindow(window)&&received==0&&vd;
            result["status"]=accepted?"GUI_PRECHECK_OBSERVED_NOT_FULL_SHELL":"UNKNOWN";return result;
        }catch(Exception e){result["error_type"]=e.GetType().Name;return result;}
        finally {
            if(window!=IntPtr.Zero&&IsWindow(window)){result["abort_own_window_destroy_attempted"]=true;result["abort_own_window_destroy_returned"]=DestroyWindow(window);}
            if(registered)result["own_class_unregistered"]=UnregisterClassW(className,instance);
            if(initialized>=0)CoUninitialize();
        }
    }
}
'@
    if($Mode -ceq 'Child'){
        $bindingPath=Join-Path $outputDir ($RunId+'-medium-desktop-binding.json')
        $bindingItem=Get-Item -LiteralPath $bindingPath
        if($bindingItem.PSIsContainer -or ($bindingItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -or
           $bindingItem.Length -gt 4096 -or (Get-FileHash -LiteralPath $bindingPath).Hash -cne $BindingSha256){
            throw 'Child one-time binding is not exact.'
        }
        $binding=[IO.File]::ReadAllText($bindingPath,[Text.Encoding]::UTF8)|ConvertFrom-Json
        if($binding.schema -cne 'panebind-v1-medium-desktop-binding/1' -or $binding.run_id -cne $RunId -or
           $binding.source_commit -cne $manifest.packaging_head -or $binding.probe_sha256 -cne $scriptEntry[0].sha256 -or
           $binding.nonce -cne $ChildNonce -or $binding.account_sid_sha256 -cnotmatch '^[A-F0-9]{64}$' -or
           [int]$binding.session_id -le 0){throw 'Child account/script/session binding mismatch.'}
        $result=[PaneBindV1MediumDesktopProbe]::RunChild($RunId,$binding.account_sid_sha256,[int]$binding.session_id)
        $record=[ordered]@{schema='panebind-v1-medium-desktop-child/1';run_id=$RunId;nonce=$ChildNonce;binding_sha256=$BindingSha256;
            source_commit=$manifest.packaging_head;probe_sha256=$scriptEntry[0].sha256;automated_guest_probe=$true;result=$result}
    }else{
        foreach($suffix in @('-medium-desktop-binding.json','-medium-desktop-child.json')){
            if(Test-Path -LiteralPath (Join-Path $outputDir ($RunId+$suffix))){throw 'Child binding/evidence already exists; preserve the original RunId.'}
        }
        $result=[PaneBindV1MediumDesktopProbe]::RunParent($RunId,$manifest.packaging_head,$scriptEntry[0].sha256)
        if($result.ContainsKey('child_receipt_text')){
            try {
                $child=$result.child_receipt_text|ConvertFrom-Json
                $result.Remove('child_receipt_text')
                $result['child_receipt']=$child
                $matched=$child.schema -ceq 'panebind-v1-medium-desktop-child/1' -and $child.run_id -ceq $RunId -and
                    $child.binding_sha256 -ceq $result.binding_sha256 -and $child.nonce -ceq $result.binding_nonce -and
                    $child.source_commit -ceq $manifest.packaging_head -and
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
            automated_guest_probe=$true;human_uat='NOT_RUN';result=$result}
    }
    $text=$record|ConvertTo-Json -Depth 18
    $stream=[IO.File]::Open($evidence,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
    try{$bytes=(New-Object Text.UTF8Encoding($false)).GetBytes($text);$stream.Write($bytes,0,$bytes.Length)}finally{$stream.Dispose()}
    if($result.status -ceq 'GUI_PRECHECK_OBSERVED_NOT_FULL_SHELL'){exit 0}
    exit 3
}catch{
    Write-Error ('Guest medium desktop probe stopped: '+$_.Exception.Message)
    exit 78
}

