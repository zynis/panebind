[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-f]{40}$')][string]$SourceCommit,
    [ValidatePattern('^[0-9a-f]{32}$')][string]$PackageId = ([Guid]::NewGuid().ToString('N')),
    [string]$BuildDirectory
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'v1-package-identity.ps1')
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $BuildDirectory) { $BuildDirectory = Join-Path $repo 'out\v1-dev' }
$build = [IO.Path]::GetFullPath($BuildDirectory)
if (-not $build.StartsWith($repo + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'BuildDirectory must be an existing build inside this repository.'
}
function Assert-OrdinaryPath([string]$Path) {
    $cursor = [IO.Path]::GetFullPath($Path)
    while ($cursor -and $cursor.StartsWith($script:repo,[StringComparison]::OrdinalIgnoreCase)) {
        if ((Test-Path -LiteralPath $cursor) -and ((Get-Item -LiteralPath $cursor).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Reparse-point input/output paths are not permitted: $cursor"
        }
        $parent = [IO.Path]::GetDirectoryName($cursor)
        if ($parent -eq $cursor) { break }
        $cursor = $parent
    }
}
Assert-OrdinaryPath $build
$branch = (& git -C $repo branch --show-current).Trim()
if ($LASTEXITCODE -ne 0 -or $branch -cne 'codex/r1c4b-live-magnet') { throw 'Unexpected development branch.' }
$packagingHead = (& git -C $repo rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Cannot identify actual packaging HEAD.' }
$packagingIdentity=Assert-PackagingSource $repo $SourceCommit $packagingHead
$head=$SourceCommit
$scope = @('src','tests','scripts','cmake','CMakeLists.txt')
$dirty = @(& git -C $repo diff --name-only HEAD -- @scope)
if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect tracked source changes.' }
$untracked = @(& git -C $repo ls-files --others --exclude-standard -- @scope)
if ($LASTEXITCODE -ne 0 -or $dirty.Count -or $untracked.Count) {
    throw 'Commit source/test/build/script changes before portable packaging. Untracked documentation input is not a blocker.'
}
$trackedNonSource = @(& git -C $repo diff --name-only HEAD --)
if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect documentation input status.' }
$root = Join-Path $repo ('uat\v1-portable\' + $PackageId)
Assert-OrdinaryPath $root
if (Test-Path -LiteralPath $root) { throw 'Package directory already exists; preserve it and use a new PackageId.' }
$paths = @((Join-Path $build 'src\platform\windows\Release\panebind.exe'),
    (Join-Path $build 'Release\panebind.exe'),(Join-Path $build 'bin\Release\panebind.exe'))
$found = @($paths | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf })
if ($found.Count -ne 1) { throw 'Expected exactly one Release/panebind.exe in the documented build layouts.' }
$executable = $found[0]
Assert-OrdinaryPath $executable
$cache = Join-Path $build 'CMakeCache.txt'
Assert-OrdinaryPath $cache
if (-not (Test-Path -LiteralPath $cache -PathType Leaf)) { throw 'CMakeCache.txt is required to verify the static runtime build contract.' }
$cacheText = [IO.File]::ReadAllText($cache)
if ($cacheText -cnotmatch '(?m)^CMAKE_MSVC_RUNTIME_LIBRARY:[^=\r\n]+=MultiThreaded\r?$') {
    throw 'Portable Release requires the existing /MT (MultiThreaded) CMake runtime configuration; this script never rebuilds or changes it.'
}
$linker = [regex]::Match($cacheText,'(?m)^CMAKE_LINKER:FILEPATH=([^\r\n]+)\r?$')
if (-not $linker.Success) { throw 'Cannot find the actual CMake linker toolchain for dependency inspection.' }
$dumpbin = Join-Path ([IO.Path]::GetDirectoryName($linker.Groups[1].Value)) 'dumpbin.exe'
if (-not (Test-Path -LiteralPath $dumpbin -PathType Leaf)) { throw 'The existing toolchain dumpbin.exe is required; no dependency is installed by this script.' }
$imports = @(& $dumpbin /NOLOGO /DEPENDENTS $executable)
if ($LASTEXITCODE -ne 0) { throw 'Release dependency inspection failed.' }
$dependencies = @($imports | ForEach-Object { if ($_ -match '^\s+([a-zA-Z0-9_.-]+\.dll)\s*$') { $Matches[1].ToLowerInvariant() } } | Sort-Object -Unique)
Assert-V1PortableDependencies $dependencies
$beforeHash = (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash
# This documented identity-only entry returns before singleton/UI/hook setup.
$start = New-Object Diagnostics.ProcessStartInfo
$start.FileName=$executable; $start.Arguments='--build-identity'; $start.UseShellExecute=$false
$start.CreateNoWindow=$true; $start.RedirectStandardOutput=$true; $start.RedirectStandardError=$true
$process = New-Object Diagnostics.Process
$process.StartInfo=$start
try {
    if (-not $process.Start()) { throw 'Identity-only process did not start.' }
    $stdout=$process.StandardOutput.ReadToEndAsync(); $stderr=$process.StandardError.ReadToEndAsync()
    if (-not $process.WaitForExit(10000)) { $process.Kill(); throw 'Identity-only process timed out; no interactive product launch was requested.' }
    if ($process.ExitCode -ne 0) { throw 'Identity-only process failed.' }
    $identity=$stdout.GetAwaiter().GetResult() | ConvertFrom-Json
    Assert-V1BinaryInterface $identity Product $head
} finally { $process.Dispose() }
if ((Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash -cne $beforeHash) { throw 'Release binary changed during inspection.' }
$quickPath=Join-Path $repo 'docs\V1_QUICK_START.md'
$reportPath=Join-Path $repo 'docs\reports\V1_DELIVERY_REPORT.md'
$licensePath=Join-Path $repo 'LICENSE'
foreach ($path in @($quickPath,$reportPath,$licensePath)) {
    Assert-OrdinaryPath $path
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required delivery input is missing: $path" }
}
$inputHashes=@{}
foreach ($path in @($quickPath,$reportPath,$licensePath)) { $inputHashes[$path]=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
$quick=[IO.File]::ReadAllText($quickPath)
$report=[IO.File]::ReadAllText($reportPath)
$issues=[regex]::Match($report,'(?ms)^## 集中缺口与待核验项\s*\r?\n(.*?)(?=^## |\z)')
if (-not $issues.Success) { throw 'Cannot locate the concentrated known-problem list; do not package an empty success claim.' }
foreach ($path in @($quickPath,$reportPath,$licensePath)) {
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -cne $inputHashes[$path]) { throw 'A delivery input changed while it was read; retry only after editing has finished.' }
}
$utf8=New-Object Text.UTF8Encoding($false)
function Write-NewText([string]$Path,[string]$Text) {
    $bytes=$script:utf8.GetBytes($Text)
    $stream=[IO.File]::Open($Path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
    try { $stream.Write($bytes,0,$bytes.Length) } finally { $stream.Dispose() }
}
# All rejection/identity checks precede creation. Incomplete new packages remain
# visible for diagnosis; this script never deletes or overwrites an old batch.
$parent=Split-Path -Parent $root
[IO.Directory]::CreateDirectory($parent) | Out-Null
New-Item -ItemType Directory -Path $root -ErrorAction Stop | Out-Null
$payload=Join-Path $root 'PaneBindV1'
New-Item -ItemType Directory -Path $payload -ErrorAction Stop | Out-Null
[IO.File]::Copy($executable,(Join-Path $payload 'panebind.exe'),$false)
if ((Get-FileHash -LiteralPath (Join-Path $payload 'panebind.exe') -Algorithm SHA256).Hash -cne $beforeHash -or
    (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash -cne $beforeHash) { throw 'Copied Release binary does not match the inspected identity; incomplete package preserved.' }
[IO.File]::Copy($licensePath,(Join-Path $payload 'LICENSE'),$false)
if ((Get-FileHash -LiteralPath (Join-Path $payload 'LICENSE') -Algorithm SHA256).Hash -cne $inputHashes[$licensePath]) { throw 'LICENSE changed during packaging; incomplete package preserved.' }
Write-NewText (Join-Path $payload 'portable.flag') "PaneBind V1 portable candidate`n"
$banner=@"
> 二进制源码：$head；文档打包提交：$packagingHead；包标识：$PackageId。实际二进制身份与文件校验见 manifest.json。
> 状态：开发候选，尚无本便携包从干净 guest 启动的验收结论；不授权宿主试用。下方复用源内使用说明，其历史包身份／历史测试状态不指代本包。是否满足整版验收须以独立证据判断；本打包动作不授予 PASS。

"@
$quick=$quick.Replace('(reports/V1_ACCEPTANCE.json)','(已知问题.md)').Replace('(reports/V1_DELIVERY_REPORT.md)','(已知问题.md)')
Write-NewText (Join-Path $payload '中文快速入门.md') ($banner+$quick)
$configuration=@'
# PaneBind V1 便携配置说明

portable.flag 与 panebind.exe 同目录，运行时在该目录使用 settings.ini。初次启动使用内置默认；不要复制开发宿主的个人配置。删除便携标记后改用当前用户 LOCALAPPDATA\PaneBind，不能把本行为误记为配置已迁移。

优先用托盘“设置”编辑。六页全部为草稿；保存成功才生效，取消／关闭不保存。导入只更新草稿；导出是经过验证的草稿。活动手势固定起手配置，下次手势采用新值。登录启动默认关闭，不由包生成脚本启用。

配置为 UTF-8 key=value，schema=1；generation 由产品保存时推进。必须包含完整标量、dock.0…7 和每组 count／连续索引，不支持未知字段／重复键／模糊通配符；完整文件不超过 256 KiB。损坏备份和保存失败需检查实际提示，不能据文件存在判为成功。

主要参数：attraction_dip 0–100；release_dip 1–200 且严格大于 attraction_dip；speed_dip 0–20000 DIP/秒；adjacency_dip 0–20；step_dip 1–1000；preview_alpha 1–255；preview_color 0–16777215（十进制）；drag_alpha／shake_alpha 16–255；shake_amplitude_dip 4–300；shake_reversals 2–20；shake_window_ms 100–5000；seam_dwell_ms 100–3000。

开关：enabled、magnet_move、magnet_resize、glue、docking、stretch、smart_docking、smart_stretch、preview、feedback、drag_transparency、shake、animations、startup，使用 true／false。animations 仅控制设置窗有界淡入，仍服从系统减少动画／高对比度，不表示窗口几何动画。

键盘：hotkey_count=N；hotkey.0=修饰键,虚拟键十进制,动作名。例 hotkey.0=3,67,Center。修饰键 Alt=1、Ctrl=2、Shift=4、Win=8，可相加；F12 保留，Ctrl+Shift+F11 紧急暂停不可覆盖。冲突注册失败应保留原映射。

鼠标：mouse_count=N；mouse.0=区域,按键,次数,修饰键,动作名。区域 标题0、左1、右2、上3、下4、左上5、右上6、左下7、右下8；按键 左1、右2、中3、X1=4、X2=5；次数 1 或2。删除映射时删除条目并修正 count／连续索引。客户区／系统按钮不由区域映射整体接管；实际硬件覆盖另行验收。

应用：rule_count=N；rule.0=exe|可选窗口类|excluded|feature_mask。例 rule.0=notepad.exe||true|4294967295。exe 可用名称或完整路径；路径可统一斜线，类名不是路径，/ 与 \ 不等价。不采集窗口标题。空格、%、|、=、# 按 UTF-8 字节 %HH 转义。excluded=true 为完全排除；false 时 feature_mask 按位允许：1 磁吸、2 联动、4 停靠、8 拉伸、16 窗口命令、32 视觉增强；4294967295 保留全部。多条匹配规则取允许位交集，排除优先。

动作名与中文解释以当前设置“快捷键／鼠标”页的同源动作表为准，不从旧验收命令复制 RunId／token。默认 Windows+方向键不变。退出使用托盘“退出”；强杀、崩溃恢复、普通 Medium 兼容及完整连续操作仍须实际验收。
'@
Write-NewText (Join-Path $payload '配置说明.md') $configuration
Write-NewText (Join-Path $payload '已知问题.md') ("# PaneBind V1 集中已知问题与验证边界`n`n本包源码 $head；便携包独立验收 NOT_RUN，真人 UAT NOT_RUN。本文件保留打包输入报告中的集中缺口，不因生成 ZIP 关闭缺陷。普通权限 Release、连续混合操作、显示器／应用兼容及 32 必过能力需独立证据。历史 High guest 不等于当前产品通过。`n`n"+$issues.Groups[1].Value)
$names=@('panebind.exe','portable.flag','LICENSE','中文快速入门.md','配置说明.md','已知问题.md')
$files=@($names | ForEach-Object { $item=Get-Item -LiteralPath (Join-Path $payload $_); [ordered]@{relative_path=$_;size=$item.Length;sha256=(Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash} })
$manifest=[ordered]@{schema='panebind-v1-portable/1';package_id=$PackageId;source_commit=$head;packaging_head=$packagingHead;branch=$branch;
    product_source_commit=$head;product_interface=$packagingIdentity.interface_contract;
    product_unchanged_across_tool_changes=$packagingIdentity.product_unchanged_across_tool_changes;
    source_scope_equivalent=$true;docs_only_changes=$packagingIdentity.docs_only_changes;documentation_inputs=$packagingIdentity.documentation_inputs;
    created_utc=[DateTime]::UtcNow.ToString('o');configuration='Release';runtime='MultiThreaded (/MT)';
    binary_identity=$identity;binary_sha256=$beforeHash;system_imports=$dependencies;files=$files;
    source_inputs=@(@{path='docs/V1_QUICK_START.md';sha256=$inputHashes[$quickPath]},
        @{path='docs/reports/V1_DELIVERY_REPORT.md';sha256=$inputHashes[$reportPath]},@{path='LICENSE';sha256=$inputHashes[$licensePath]});
    tracked_non_source_changes=$trackedNonSource;manifest_self_hash='package-receipt.json outside ZIP';
    clean_guest_portable_validation='NOT_RUN';human_uat='NOT_RUN';whole_version_acceptance='NOT_GRANTED_BY_PACKAGING';
    public_release=$false;host_trial_authorized_by_package=$false}
Write-NewText (Join-Path $payload 'manifest.json') ($manifest | ConvertTo-Json -Depth 9)
$manifestItem=Get-Item -LiteralPath (Join-Path $payload 'manifest.json')
$archivedFiles=@($files)+@([ordered]@{relative_path='manifest.json';size=$manifestItem.Length;sha256=(Get-FileHash -LiteralPath $manifestItem.FullName -Algorithm SHA256).Hash})
$zip=Join-Path $root ('PaneBindV1-Release-'+$PackageId+'.zip')
Compress-Archive -LiteralPath $payload -DestinationPath $zip -CompressionLevel Optimal
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive=[IO.Compression.ZipFile]::OpenRead($zip)
try {
    $entries=@($archive.Entries | Where-Object { $_.Name })
    if ($entries.Count -ne 7 -or @($entries | Where-Object { $_.Name -like '*.exe' }).Count -ne 1) { throw 'ZIP has unexpected payload layout or extra executables.' }
    $seen=@{}
    foreach ($entry in $entries) {
        $name=$entry.FullName.Replace('\','/')
        if ($seen.ContainsKey($name)) { throw 'ZIP contains a duplicate payload path.' }
        $seen[$name]=$true
        $expected=@($archivedFiles | Where-Object { $name -ceq ('PaneBindV1/'+$_.relative_path) })
        if ($expected.Count -ne 1 -or $entry.Length -ne $expected[0].size) { throw 'ZIP contains a missing, duplicate, unknown or size-mismatched file.' }
        $stream=$entry.Open();$digest=[Security.Cryptography.SHA256]::Create()
        try { $hash=([BitConverter]::ToString($digest.ComputeHash($stream))).Replace('-','') } finally { $digest.Dispose();$stream.Dispose() }
        if ($hash -cne $expected[0].sha256) { throw 'ZIP content does not match the packaged file manifest.' }
    }
} finally { $archive.Dispose() }
$receipt=[ordered]@{schema='panebind-v1-portable-receipt/1';package_id=$PackageId;source_commit=$head;packaging_head=$packagingHead;package_root=$root;
    product_source_commit=$head;product_interface=$packagingIdentity.interface_contract;
    source_scope_equivalent=$true;documentation_inputs=$packagingIdentity.documentation_inputs;
    zip=$zip;zip_sha256=(Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash;
    manifest_sha256=(Get-FileHash -LiteralPath (Join-Path $payload 'manifest.json') -Algorithm SHA256).Hash;
    binary_sha256=$beforeHash;files=$archivedFiles;gui_started=$false;clean_guest_portable_validation='NOT_RUN';human_uat='NOT_RUN'}
Write-NewText (Join-Path $root 'package-receipt.json') ($receipt | ConvertTo-Json -Depth 5)
$receipt | ConvertTo-Json -Depth 5
