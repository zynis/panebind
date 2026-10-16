[CmdletBinding()]
param([Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-f]{32}$')][string]$RunId)
# Original one-purpose orchestration; official references reviewed 2026-10-16:
# https://learn.microsoft.com/windows/package-manager/winget/#install-winget-on-windows-sandbox
# https://learn.microsoft.com/windows/package-manager/winget/download
# https://learn.microsoft.com/powershell/module/appx/add-appxpackage
# https://learn.microsoft.com/windows/security/application-security/application-isolation/windows-sandbox/windows-sandbox-faq
# https://learn.microsoft.com/windows/package-manager/winget/troubleshooting
# https://raw.githubusercontent.com/microsoft/winget-cli/v1.29.380/src/AppInstallerCommonCore/HttpClientHelper.cpp
# https://learn.microsoft.com/powershell/module/international/set-winhomelocation
# Fixed Microsoft/winget-cli v1.29.380 packages retain their original signatures
# and notices. They are internal test tools, not PaneBind product dependencies.
$ErrorActionPreference='Stop'
$inputDirectory='C:\PaneBindV1\Input';$outputDirectory='C:\PaneBindV1\Output'
$receipt=$null;$receiptPath=$null;$diagnosticDirectory=$null
$originalGeoId=$null;$locationChangeAttempted=$false
$script:preparationToolRunning=$false;$script:preparationToolPid=$null
function Assert-Plain([string]$Path,[bool]$Directory){
    $item=Get-Item -LiteralPath $Path
    if($item.PSIsContainer -ne $Directory -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Expected ordinary exact guest input/output, not a reparse point.'}
}
function Write-NewJson([string]$Path,$Value){
    $encoding=New-Object Text.UTF8Encoding($false);$bytes=$encoding.GetBytes(($Value|ConvertTo-Json -Depth 18))
    $stream=[IO.File]::Open($Path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
    try{$stream.Write($bytes,0,$bytes.Length)}finally{$stream.Dispose()}
}
function Invoke-ExactPreparationProcess([string]$Executable,[string]$Arguments,[int]$Seconds){
    if($script:preparationToolRunning){throw 'Previous own preparation tool exit is not confirmed; do not start another tool.'}
    $info=New-Object Diagnostics.ProcessStartInfo
    $info.FileName=$Executable;$info.Arguments=$Arguments;$info.UseShellExecute=$false;$info.CreateNoWindow=$true
    $info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
    $process=New-Object Diagnostics.Process;$process.StartInfo=$info
    try{
        if(-not $process.Start()){throw 'Exact preparation process did not start.'}
        $script:preparationToolRunning=$true;$script:preparationToolPid=$process.Id
        $out=$process.StandardOutput.ReadToEndAsync();$err=$process.StandardError.ReadToEndAsync()
        $exited=$process.WaitForExit($Seconds*1000);$terminated=$false
        if(-not $exited){$process.Kill();$terminated=$true;$exited=$process.WaitForExit(5000)}
        if(-not $exited){throw 'Own preparation tool did not exit after the bounded deadline; guest disposal is required, not a success.'}
        $script:preparationToolRunning=$false
        if(-not $out.Wait(5000) -or -not $err.Wait(5000)){throw 'Own tool exited but redirected output did not close within five seconds; no success is inferred.'}
        $stdout=$out.GetAwaiter().GetResult();$stderr=$err.GetAwaiter().GetResult()
        return [ordered]@{executable=$Executable;arguments=$Arguments;pid=$process.Id;exit_code=$process.ExitCode;
            timed_out=$terminated;terminated_only_own_created_tool=$terminated;stdout_chars=$stdout.Length;stderr_chars=$stderr.Length;
            stdout=$stdout.Substring(0,[Math]::Min(32768,$stdout.Length));stderr=$stderr.Substring(0,[Math]::Min(32768,$stderr.Length));
            application_function_result='NOT_RUN'}
    }finally{$process.Dispose()}
}
function Copy-OwnWinGetDiagnostics([string]$Directory,[string]$Output,[string]$ExactRunId){
    # Read only the newly installed tool's own direct log directory, never a
    # profile search. Failure collection must not replace the original failure.
    $facts=[ordered]@{status='NOT_PRESENT';source_directory=$Directory;files=@();error=$null;
        max_files=8;max_file_bytes=1MB;max_total_bytes=4MB;omitted_by_bound=$false}
    try{
        if(-not (Test-Path -LiteralPath $Directory)){return $facts}
        Assert-Plain $Directory $true
        $destination=Join-Path $Output ($ExactRunId+'-winget-diagnostics')
        if(Test-Path -LiteralPath $destination){throw 'Existing diagnostic evidence must not be overwritten.'}
        [IO.Directory]::CreateDirectory($destination)|Out-Null
        $logs=@(Get-ChildItem -LiteralPath $Directory -File|Where-Object{$_.Name -cmatch '^WinGet-[A-Za-z0-9_.-]{1,128}\.log$'}|Select-Object -First 9)
        if($logs.Count -gt 8){$facts.omitted_by_bound=$true}
        [long]$total=0
        foreach($log in @($logs|Select-Object -First 8)){
            Assert-Plain $log.FullName $false
            if($log.Length -gt 1MB -or $total+$log.Length -gt 4MB){$facts.omitted_by_bound=$true;continue}
            $target=Join-Path $destination $log.Name
            [IO.File]::Copy($log.FullName,$target,$false)
            $copied=Get-Item -LiteralPath $target
            if($copied.Length -gt 1MB -or $total+$copied.Length -gt 4MB){throw 'Tool log changed beyond the diagnostic bound after process exit.'}
            $total+=$copied.Length
            $text=[IO.File]::ReadAllText($target)
            $facts.files+=,[ordered]@{relative_path=($ExactRunId+'-winget-diagnostics\'+$log.Name);size=$copied.Length;
                sha256=(Get-FileHash -LiteralPath $target).Hash;tail_chars=[Math]::Min(16384,$text.Length);
                tail=$text.Substring([Math]::Max(0,$text.Length-16384));raw_log_retained=$true}
        }
        $facts['total_bytes']=$total;$facts.status=if($facts.omitted_by_bound){'BOUNDED_WITH_OMISSIONS'}elseif($facts.files.Count){'COLLECTED'}else{'NO_MATCHING_TOOL_LOG'}
    }catch{$facts.status='FAILED';$facts.error=$_.Exception.Message}
    return $facts
}
try{
    # This is a one-purpose disposable-guest downloader, never a host install
    # command. All guards run before creating evidence or invoking deployment.
    if(-not [string]::Equals([IO.Path]::GetFullPath($PSScriptRoot),$inputDirectory,[StringComparison]::OrdinalIgnoreCase)){throw 'Exact mapped guest input directory required.'}
    $identity=[Security.Principal.WindowsIdentity]::GetCurrent()
    $account=$identity.Name;$token=$identity.Token;$sid=$identity.User.Value
    $expectedAccount=[Environment]::MachineName+'\WDAGUtilityAccount'
    if(-not [string]::Equals($account,$expectedAccount,[StringComparison]::OrdinalIgnoreCase)){$identity.Dispose();throw 'Exact local-machine WDAGUtilityAccount required; a same-named foreign account is not accepted.'}
    foreach($path in @('C:\PaneBindV1',$inputDirectory,$outputDirectory)){Assert-Plain $path $true}
    foreach($name in @('panebind-v1-input.marker','manifest.json','v1-notepad-prepare-guest.ps1')){Assert-Plain (Join-Path $inputDirectory $name) $false}
    Assert-Plain (Join-Path $outputDirectory 'panebind-v1-output.marker') $false
    if([IO.File]::ReadAllText((Join-Path $inputDirectory 'panebind-v1-input.marker')).TrimEnd("`r","`n") -cne $RunId -or
       [IO.File]::ReadAllText((Join-Path $outputDirectory 'panebind-v1-output.marker')).TrimEnd("`r","`n") -cne $RunId){$identity.Dispose();throw 'Exact RunId markers disagree.'}
    Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;
using System.Security.Principal;
using System.Collections.Generic;
public static class PaneBindNotepadPreparationFacts {
 [DllImport("advapi32.dll",SetLastError=true)] static extern bool GetTokenInformation(IntPtr token,int kind,IntPtr data,int size,out int needed);
 [DllImport("user32.dll")] static extern IntPtr GetThreadDesktop(uint id);
 [DllImport("user32.dll")] static extern IntPtr GetProcessWindowStation();
 [DllImport("user32.dll",SetLastError=true)] static extern IntPtr OpenInputDesktop(uint flags,bool inherit,uint rights);
 [DllImport("user32.dll",SetLastError=true)] static extern bool CloseDesktop(IntPtr value);
 [DllImport("kernel32.dll")] static extern uint GetCurrentThreadId();
 [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool GetUserObjectInformationW(IntPtr handle,int kind,StringBuilder data,uint bytes,out uint needed);
 [DllImport("user32.dll",EntryPoint="GetUserObjectInformationW",ExactSpelling=true,SetLastError=true)] static extern bool GetInputState(IntPtr handle,int kind,out bool data,uint bytes,out uint needed);
 [DllImport("wtsapi32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool WTSQuerySessionInformationW(IntPtr server,int session,int kind,out IntPtr value,out int bytes);
 [DllImport("wtsapi32.dll")] static extern void WTSFreeMemory(IntPtr value);
 public static int Number(IntPtr token,int kind){int needed;GetTokenInformation(token,kind,IntPtr.Zero,0,out needed);if(needed<1||needed>4096)throw new Exception("Token fact size unavailable");var data=Marshal.AllocHGlobal(needed);try{if(!GetTokenInformation(token,kind,data,needed,out needed))throw new Exception("Token fact unavailable");if(kind==25){var parts=new SecurityIdentifier(Marshal.ReadIntPtr(data)).Value.Split('-');return int.Parse(parts[parts.Length-1]);}return Marshal.ReadInt32(data);}finally{Marshal.FreeHGlobal(data);}}
 static string Name(IntPtr handle,out int error){if(handle==IntPtr.Zero){error=0;return null;}var text=new StringBuilder(256);uint needed;bool ok=GetUserObjectInformationW(handle,2,text,512,out needed);error=ok?0:Marshal.GetLastWin32Error();return ok?text.ToString():null;}
 public static Dictionary<string,object> DesktopFacts(int session){
  var result=new Dictionary<string,object>();var input=OpenInputDesktop(0,false,1);int openError=input==IntPtr.Zero?Marshal.GetLastWin32Error():0;
  var owner=GetThreadDesktop(GetCurrentThreadId());var station=GetProcessWindowStation();int inputError,ownerError,stationError;
  var inputName=Name(input,out inputError);var ownerName=Name(owner,out ownerError);var stationName=Name(station,out stationError);
  bool inputIo=false,ownerIo=false;uint needed;bool inputKnown=input!=IntPtr.Zero&&GetInputState(input,6,out inputIo,4,out needed);int inputIoError=inputKnown||input==IntPtr.Zero?0:Marshal.GetLastWin32Error();
  bool ownerKnown=owner!=IntPtr.Zero&&GetInputState(owner,6,out ownerIo,4,out needed);int ownerIoError=ownerKnown||owner==IntPtr.Zero?0:Marshal.GetLastWin32Error();
  bool closed=input!=IntPtr.Zero&&CloseDesktop(input);int closeError=closed||input==IntPtr.Zero?0:Marshal.GetLastWin32Error();
  IntPtr data;int size;bool wts=WTSQuerySessionInformationW(IntPtr.Zero,session,8,out data,out size);int wtsError=wts?0:Marshal.GetLastWin32Error();int state=-1;
  if(wts){try{if(size>=4)state=Marshal.ReadInt32(data);}finally{WTSFreeMemory(data);}}
  bool accepted=session>0&&input!=IntPtr.Zero&&closed&&inputName=="Default"&&ownerName=="Default"&&stationName=="WinSta0"&&inputKnown&&ownerKnown&&inputIo&&ownerIo&&wts&&state==0;
  result["accepted"]=accepted;result["input_name"]=inputName;result["thread_name"]=ownerName;result["window_station"]=stationName;
  result["input_queries_attempted"]=input!=IntPtr.Zero;result["thread_queries_attempted"]=owner!=IntPtr.Zero;result["station_query_attempted"]=station!=IntPtr.Zero;
  result["open_input_error"]=openError;result["input_name_error"]=inputError;result["thread_name_error"]=ownerError;result["station_name_error"]=stationError;
  result["input_uoi_io_known"]=inputKnown;result["input_uoi_io"]=inputKnown?(object)inputIo:null;result["input_uoi_io_error"]=inputIoError;
  result["thread_uoi_io_known"]=ownerKnown;result["thread_uoi_io"]=ownerKnown?(object)ownerIo:null;result["thread_uoi_io_error"]=ownerIoError;
  result["input_desktop_closed"]=closed;result["close_input_error"]=closeError;result["wts_query_succeeded"]=wts;result["wts_state"]=wts?(object)state:null;result["wts_error"]=wtsError;return result;
 }
}
'@
    $rid=[PaneBindNotepadPreparationFacts]::Number($token,25);$elevated=[PaneBindNotepadPreparationFacts]::Number($token,20) -ne 0
    $session=[Diagnostics.Process]::GetCurrentProcess().SessionId
    $desktop=[PaneBindNotepadPreparationFacts]::DesktopFacts($session);$identity.Dispose()
    if($rid -ne 12288 -or -not $elevated -or $session -le 0 -or -not $desktop['accepted']){throw ('Actual High/elevated, active WinSta0/Default input and owner desktop required. Facts: '+($desktop|ConvertTo-Json -Compress))}
    $manifest=Get-Content -LiteralPath (Join-Path $inputDirectory 'manifest.json') -Encoding UTF8 -Raw|ConvertFrom-Json
    if($manifest.schema -cne 'panebind-v1-notepad-preparation/1' -or $manifest.run_id -cne $RunId -or
       $manifest.asset_id -cne 'b91fac4c0b9c4d63afdc05bc04ba9813' -or $manifest.source_commit -cnotmatch '^[0-9a-f]{40}$' -or
       $manifest.host_root -cne ('I:\PaneBindMVP1Runs\'+$RunId) -or $manifest.guest_input -cne $inputDirectory -or $manifest.guest_output -cne $outputDirectory -or
       $manifest.networking -cne 'Enable' -or $manifest.purpose -cne 'official_Notepad_download_only_no_product_or_input'){throw 'Preparation manifest scope disagrees.'}
    $location=$manifest.preparation_location
    if($null -eq $location -or $location.geo_id -ne 244 -or
       $location.source -cne 'read_only_test_host_configuration' -or $location.physical_location_asserted -ne $false -or
       $location.allowed_initial_geo_id -ne 38970 -or $location.restore_original -ne $true){throw 'Exact reviewed disposable-guest preparation location configuration required.'}
    $expected=[ordered]@{
        'AppInstaller_x64.msix'='2E05D50B493DB869D98643533A0141C2C75FC15F122BF5723AB4A1B93C48D267'
        'Microsoft.VCLibs.140.00_14.0.33519.0_x64.appx'='9C17B521F9D690A1F504DA5108ED6EEC5669EB3A8FD1331EEF43E40D84E74283'
        'Microsoft.VCLibs.140.00.UWPDesktop_14.0.33728.0_x64.appx'='077A3D1A5D0622BD3004DCA85F5E192D6E98EC79B83D4AA06766759EA6C09C3D'
        'Microsoft.WindowsAppRuntime.1.8_8000.616.304.0_x64.appx'='A31595CC4B5AEBC18466EC24E8D4B566FE0FCAFB52D833B6D139B8691D0E5177'
    }
    $allowed=@($expected.Keys)+@('v1-notepad-prepare-guest.ps1','panebind-v1-input.marker');$seen=@{}
    foreach($file in @($manifest.files)){
        if($file.relative_path -cnotin $allowed -or $seen.ContainsKey($file.relative_path) -or $file.sha256 -cnotmatch '^[A-F0-9]{64}$'){throw 'Unknown or duplicate preparation file.'}
        $path=Join-Path $inputDirectory $file.relative_path;Assert-Plain $path $false;$item=Get-Item -LiteralPath $path
        if($item.Length -ne [long]$file.size -or (Get-FileHash -LiteralPath $path).Hash -cne $file.sha256){throw 'Preparation file hash/size mismatch.'}
        if($expected.Contains($file.relative_path) -and $expected[$file.relative_path] -cne $file.sha256){throw 'Package differs from fixed reviewed official asset.'}
        $seen[$file.relative_path]=$true
    }
    if($seen.Count -ne $allowed.Count){throw 'Preparation file inventory incomplete.'}
    $actualInput=@(Get-ChildItem -LiteralPath $inputDirectory)
    if($actualInput.Count -ne $allowed.Count+1 -or @($actualInput|Where-Object{$_.PSIsContainer -or $_.Name -cnotin (@($allowed)+@('manifest.json'))}).Count){throw 'Unknown actual preparation input content.'}
    if(@(Get-ChildItem -LiteralPath $outputDirectory|Where-Object{$_.Name -cne 'panebind-v1-output.marker'}).Count){throw 'Preparation output is not a fresh exact RunId directory.'}
    $receiptPath=Join-Path $outputDirectory ($RunId+'-notepad-preparation.json')
    if(Test-Path -LiteralPath $receiptPath){throw 'Existing preparation evidence must not be overwritten.'}
    $digest=[Security.Cryptography.SHA256]::Create()
    try{$sidHash=([BitConverter]::ToString($digest.ComputeHash([Text.Encoding]::UTF8.GetBytes($sid)))).Replace('-','')}finally{$digest.Dispose()}
    $receipt=[ordered]@{schema='panebind-v1-notepad-preparation-result/1';run_id=$RunId;source_commit=$manifest.source_commit;
        asset_id=$manifest.asset_id;status='RUNNING';actual_integrity_rid=$rid;actual_elevated=$elevated;session_id=$session;sid_sha256=$sidHash;
        exact_local_WDAG_account=$true;desktop_facts=$desktop;active_Default_desktop=$desktop['accepted'];package_deployments=@();tools=@();downloaded_files=@();error=$null;
        locale_facts=$null;winget_diagnostics=$null;
        preparation_location=$location;location_adjustment=$null;
        host_installation='NONE';product_started=$false;synthetic_input_sent=$false;application_function_result='NOT_RUN';
        offline_license_requested=$false;account_login_requested=$false;formal_test_guest_networking='Disable'}
    $work='C:\PaneBindV1\NotepadPreparation\'+$RunId
    if(Test-Path -LiteralPath $work){throw 'Guest-local preparation RunId directory already exists.'}
    [IO.Directory]::CreateDirectory($work)|Out-Null
    foreach($name in $expected.Keys){
        $source=Join-Path $inputDirectory $name;$target=Join-Path $work $name
        [IO.File]::Copy($source,$target,$false)
        if((Get-FileHash -LiteralPath $target).Hash -cne $expected[$name]){throw 'Local package copy mismatch.'}
        $signature=Get-AuthenticodeSignature -LiteralPath $target
        if($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -cne 'CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US'){throw 'Microsoft package signature is not valid.'}
    }
    $powershell=Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
    $dependencies=@($expected.Keys|Where-Object{$_ -cne 'AppInstaller_x64.msix'}|ForEach-Object{Join-Path $work $_})
    $code='$ErrorActionPreference=''Stop'';Add-AppxPackage -Path '''+(Join-Path $work 'AppInstaller_x64.msix')+''' -DependencyPath @('''+($dependencies -join ''',''')+''');Get-AppxPackage -Name Microsoft.DesktopAppInstaller | Select-Object Name,Version,PackageFullName,InstallLocation | ConvertTo-Json -Compress'
    $arguments='-NoProfile -NonInteractive -ExecutionPolicy Bypass -EncodedCommand '+[Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($code))
    $deployment=Invoke-ExactPreparationProcess $powershell $arguments 120;$receipt.package_deployments+=,$deployment
    if($deployment.timed_out -or $deployment.exit_code -ne 0){throw 'Guest-only user package deployment failed; preserve actual Appx error and do not disable verification.'}
    $installed=@(Get-AppxPackage -Name Microsoft.DesktopAppInstaller)
    if($installed.Count -ne 1 -or $installed[0].Version -cne '1.29.380.0'){throw 'Installed guest WinGet identity mismatch.'}
    # Only this disposable guest's newly installed tool state; no host profile
    # or global policy is touched. Explicit Store source avoids unrelated apps.
    $settingsDirectory=Join-Path $env:LOCALAPPDATA 'Packages\Microsoft.DesktopAppInstaller_8wekyb3d8bbwe\LocalState'
    [IO.Directory]::CreateDirectory($settingsDirectory)|Out-Null;Assert-Plain $settingsDirectory $true
    $settingsPath=Join-Path $settingsDirectory 'settings.json'
    if(Test-Path -LiteralPath $settingsPath){throw 'Unexpected preexisting WinGet guest settings; do not overwrite.'}
    Write-NewJson $settingsPath ([ordered]@{telemetry=[ordered]@{disable=$true};source=[ordered]@{autoUpdateIntervalInMinutes=0}})
    $receipt['guest_tool_telemetry_disabled']=$true
    $diagnosticDirectory=Join-Path $settingsDirectory 'DiagOutputDir'
    # Fixed task-owned guest USER setting, not the host's region or a claim
    # about anyone's physical location. Only World -> reviewed GeoID 244 is
    # permitted; no arbitrary region switch, language/security/proxy change.
    $geo=Get-WinHomeLocation
    if($null -eq $geo.GeoId){throw 'Get-WinHomeLocation returned no GeoId fact.'}
    $originalGeoId=[long]$geo.GeoId
    $receipt.locale_facts=[ordered]@{status='OBSERVED';geo_id=$originalGeoId;home_location=[string]$geo.HomeLocation;
        culture=(Get-Culture).Name;ui_culture=(Get-UICulture).Name;language_modified=$false;host_modified=$false}
    $receipt.location_adjustment=[ordered]@{scope='exact_fresh_task_guest_current_WDAG_user';initial_geo_id=$originalGeoId;
        target_geo_id=244;source='read_only_test_host_configuration';physical_location_asserted=$false;
        change_attempted=$false;change_cmdlet_completed=$false;after_change_geo_id=$null;change_verified=$false;
        restore_attempted=$false;restore_cmdlet_completed=$false;after_restore_geo_id=$null;restore_verified=$null;restore_error=$null}
    if($originalGeoId -eq 38970){
        $locationChangeAttempted=$true;$receipt.location_adjustment.change_attempted=$true
        Set-WinHomeLocation -GeoId 244 -ErrorAction Stop
        $receipt.location_adjustment.change_cmdlet_completed=$true
        $afterGeo=Get-WinHomeLocation
        if($null -eq $afterGeo.GeoId){throw 'Guest location change has no readback fact.'}
        $receipt.location_adjustment.after_change_geo_id=[long]$afterGeo.GeoId
        $receipt.location_adjustment.change_verified=[long]$afterGeo.GeoId -eq 244
        if(-not $receipt.location_adjustment.change_verified){throw 'Fixed guest location change was not confirmed; no download.'}
    }elseif($originalGeoId -eq 244){
        $receipt.location_adjustment.after_change_geo_id=244
        $receipt.location_adjustment.change_verified=$true
    }else{throw 'Guest initial location is neither exact World nor the fixed reviewed test configuration; no change/download.'}
    $winget=Join-Path $installed[0].InstallLocation 'winget.exe';Assert-Plain $winget $false
    $signature=Get-AuthenticodeSignature -LiteralPath $winget
    if($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -cne 'CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US'){throw 'Guest WinGet executable signature invalid.'}
    $version=Invoke-ExactPreparationProcess $winget '--version --disable-interactivity' 30;$receipt.tools+=,$version
    if($version.exit_code -ne 0 -or $version.stdout.Trim() -cne 'v1.29.380'){throw 'Actual guest WinGet version mismatch.'}
    $download=Join-Path $outputDirectory ($RunId+'-notepad-download')
    if(Test-Path -LiteralPath $download){throw 'Notepad download directory already exists.'}
    New-Item -ItemType Directory -Path $download -ErrorAction Stop|Out-Null
    $arguments='download --id 9MSMLRH6LZF3 --source msstore --architecture x64 --skip-license --accept-source-agreements --accept-package-agreements --disable-interactivity --no-progress --verbose-logs --download-directory "'+$download+'"'
    $result=Invoke-ExactPreparationProcess $winget $arguments 300;$receipt.tools+=,$result
    $downloadSucceeded=-not $result.timed_out -and $result.exit_code -eq 0
    $pending=New-Object 'System.Collections.Generic.Queue[string]';$pending.Enqueue($download);$files=@();[long]$total=0;$directories=0
    while($pending.Count){
        if(++$directories -gt 16){throw 'Downloaded directory bound exceeded.'};$directory=$pending.Dequeue();Assert-Plain $directory $true
        foreach($item in Get-ChildItem -LiteralPath $directory){
            if($item.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Downloaded link/reparse entry rejected.'}
            if($item.PSIsContainer){$pending.Enqueue($item.FullName);continue}
            if($files.Count -ge 64 -or $item.Length -gt 512MB){throw 'Downloaded file bound exceeded.'};$total+=$item.Length;if($total -gt 1GB){throw 'Downloaded aggregate bound exceeded.'}
            $package=$item.Extension -cin @('.appx','.msix','.msixbundle','.appxbundle');$signer=$null;$status=$null
            if($package){$sign=Get-AuthenticodeSignature -LiteralPath $item.FullName;$status=$sign.Status.ToString();$signer=$sign.SignerCertificate.Subject
                if($status -cne 'Valid' -or $signer -cne 'CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US'){throw 'Downloaded package signature invalid.'}}
            elseif($item.Extension -cnotin @('.json','.yaml','.yml','.txt','.xml')){throw 'Unexpected downloaded file type.'}
            $files+=,[ordered]@{relative_path=$item.FullName.Substring($download.Length+1);size=$item.Length;sha256=(Get-FileHash -LiteralPath $item.FullName).Hash;
                package=$package;signature_status=$status;signer=$signer}
            $receipt.downloaded_files=$files
        }
    }
    $receipt.downloaded_files=$files;$receipt.download_directory=$download;$receipt.total_bytes=$total
    if(-not $downloadSucceeded){throw 'Official Notepad package/dependency download failed; available output inventory retained, no install/credential fallback attempted.'}
    if(-not @($files|Where-Object{$_.package -and $_.relative_path -match '(?i)WindowsNotepad'}).Count){throw 'Download exited zero without a Notepad package.'}
    $receipt['offline_dependency_completeness']='NOT_VERIFIED_UNTIL_FRESH_OFFLINE_GUEST_DEPLOYMENT'
    $receipt.status='OFFICIAL_PACKAGES_DOWNLOADED_NOT_FUNCTION_TESTED'
}catch{
    if($receipt){$receipt.status='FAILED';$receipt.error=$_.Exception.Message}else{[Console]::Error.WriteLine($_.Exception.Message);exit 78}
}finally{
    if($receipt -and $receiptPath){
        if($locationChangeAttempted){
            try{
                if($script:preparationToolRunning){throw ('Own tool exit not confirmed for PID '+$script:preparationToolPid+'; location restoration not attempted while it may be running.')}
                $receipt.location_adjustment['own_tools_exit_confirmed_before_restore']=$true
                $beforeRestore=Get-WinHomeLocation
                if($null -eq $beforeRestore.GeoId){throw 'Guest location restoration has no current GeoId fact.'}
                $receipt.location_adjustment['before_restore_geo_id']=[long]$beforeRestore.GeoId
                if([long]$beforeRestore.GeoId -eq 244 -and $originalGeoId -eq 38970){
                    $receipt.location_adjustment.restore_attempted=$true
                    Set-WinHomeLocation -GeoId 38970 -ErrorAction Stop
                    $receipt.location_adjustment.restore_cmdlet_completed=$true
                }elseif([long]$beforeRestore.GeoId -ne $originalGeoId){throw 'Guest location changed to an unexpected value; do not overwrite unknown changes.'}
                $restoredGeo=Get-WinHomeLocation
                if($null -eq $restoredGeo.GeoId){throw 'Guest location restoration readback missing.'}
                $receipt.location_adjustment.after_restore_geo_id=[long]$restoredGeo.GeoId
                $receipt.location_adjustment.restore_verified=[long]$restoredGeo.GeoId -eq $originalGeoId
                if(-not $receipt.location_adjustment.restore_verified){throw 'Original guest user GeoId was not restored.'}
            }catch{
                $receipt.location_adjustment.restore_verified=$false;$receipt.location_adjustment.restore_error=$_.Exception.Message
                $receipt.status='FAILED';$receipt.error=([string]$receipt.error+'; Guest user location restoration failed: '+$_.Exception.Message).TrimStart(';',' ')
            }
        }
        $receipt['own_preparation_tool_exit_confirmed']=-not $script:preparationToolRunning
        if($diagnosticDirectory){$receipt.winget_diagnostics=Copy-OwnWinGetDiagnostics $diagnosticDirectory $outputDirectory $RunId}
        Write-NewJson $receiptPath $receipt
    }
}
if($receipt.status -cne 'OFFICIAL_PACKAGES_DOWNLOADED_NOT_FUNCTION_TESTED'){exit 78}
exit 0
