# MVP1：精确三窗 High Sandbox 例外与功能验证

日期：2026-10-06（Asia/Shanghai）。分支 `codex/r1c4b-live-magnet`。
BASE `2bc40fe33c12a9110e27ff9a1c930804ddcf3323`；起点 clean，标准 origin
fetch 后分支 0/0；origin/main `81e40facf52ffdb96f76e4d737740167d485f4a7`，不合并。

## 授权与实现

用户本轮明确批准的例外仅适用于专用可丢弃 Sandbox、当前 RunId、三个分别
确认新建的 Explorer root。严格 EXE/日志路径、双 32-byte marker、WDAG、非零
活动且解锁 session、Default desktop 的实际 UOI_IO 均需满足。保护仅防误运行，
不称防伪安全边界。候选的显式 flag 不能绕过原生条件。

scope 创建读取实际 controller token；每窗在 baseline exclusion、精确路径、
Shell identity、HWND/PID/TID 及新确认代际成立后才占用唯一槽，最多三项。
签发后固定真实 token/capability generation；组要求同 scope 和互异槽。
运行中资格/Move writer/Glue follower/Resize 后刷新复用原 native validator，
重新读取实际 SID/session/IL/elevation 和原始 security tuple。未知、System、
混合或失效身份拒绝；普通无 scope 路径仍要求 Medium/non-elevated。

真实字段 `medium=false/elevated=true` 保留，例外单列 applied、RunId、槽和
代际。同 SID/session 日志来自实际比较回执，不是常量或账户名推断。
未修改账户、token、UAC、宿主设置或共享 shield/UP/清理实现。
研究/许可见 [SOURCE_PROVENANCE](../research/SOURCE_PROVENANCE.md)。

Debug/Release 构建通过；`ctest --test-dir out/r1c4b-mvp1-shield-guest-mt
-C Debug|Release -R '^(windows-explorer-unit|explorer-mvp-flow)$'
--output-on-failure` 各 2/2 PASS；实际日志共享读取回归 PASS。脚本 AST、嵌入
C# 编译及实际 driver 权限断言正反例通过，未当作现场输入证据。

## 前置证据复用

owned 九项保留 RunId `050736d561fe4b3c941e9f194b3f51aa`、来源
`3ef009f5e72c5eebdbb64b3c3e0cd6c52c1b2d52`。prepare 实际验证原始九个
JSONL hash、身份/sequence/真实清理和固定 summary hash，并比较共享 core/
operations/构建输入未改，才标 `REUSED_PRIOR_EVIDENCE`。本包 owned `NOT_RUN`，
不伪造为新实现九项通过，也不把 owned 结果外推跨进程。

## 首批现场包

RUNTIME `196d66f312ab0904fb3d1f4c21c4c6a514f4d409`；RunId
`be76ea0a60424113a4e71ace443d75e5`，专用目录
`I:\PaneBindMVP1Runs\be76ea0a60424113a4e71ace443d75e5`。
ZIP `366154F98D798862093E69DC9A2641F676F0B9A97112A7008C4D5039B6C77E8A`；
manifest `81DAFB88ACEC9666C02CD9F0C6B16F428D58B7074A3329BA043FD4F59FA60DF8`；
.wsb `6D6342E6DE9D87809EF8D0F9FD0CF612E1DFFD7C04194085616D4E248BB27570`。
Debug Explorer `00459D357AE1618F19D54F0D77BA992DB927E5709D6CFFD1CF3F5CA7DA44748B`；
Release Explorer `944A59321E49F7B39EF075309C701CC40F6BBD3C21A37F01F0F0212F4786A78D`。
远端核验全部 12 文件/marker/映射 hash；仅 input RO 与 output RW，禁用网络、
设备与剪贴板转发。只创建本轮专用任务和 guest，未重启/启用新功能。

现场顺序：当前输入预检 → 三个 exact 新窗分别绑定 → A 普通 Move 实际闭环
→ A/B/C/连续手势/磁吸保持脱离重吸附/XY → Ctrl Glue → 原生 Resize 刷新
→ 停止；Debug 排障，Release 同一功能闭环。原始失败保留，每次修复用新
源码身份/RunId。不以退出码、构建或销毁 guest 代替功能/清理结果。
Raw/WinEvent 的 JSONL 行序不是共同原生时序。实际结果待以下记录。

首批停止在测试预检入口合同：runner 新后缀 `-preflight-explorer.jsonl` 不在现有
输入环境 EXE 的精确 guest suffix allowlist，退出78、无预检 JSONL、没有 Explorer
或其输入。旧失败和终局 summary 保留，不当成环境或 High 策略失败。最小修复
仅 runner 使用已允许的 `-preflight-release-normal.jsonl`；summary 仍明确
current_input_identity，不暗示本包 owned normal 已运行。不改共享实现，仍复用
原九项。精确本批 guest/启动任务清理后以新 SHA/RunId 继续。

## 第二批：真实 High 资格已通过，driver 集合变量碰撞

RUNTIME `b29a587153ac08895391d7a699d880447ad32a53`，RunId
`07d9c471c7fd42a0a4f7bee44f4cda10`。当前预检 READY，driver 实际 caller PMV2
建立/回读成功；DPI192 Explorer 的 outer `[425,91,2025,1291]` 与 DWM visible
`[436,91,2014,1280]` 已在同物理尺度，非上轮半尺度。创建唯一新 A root
HWND197360/PID7724/TID7768，确认前 normal/visible、GUI capture/move/menu clear。
产品真实 target_confirmed：controller/target IL12288、medium=false/elevated=true、
SID/session实际相同、session1、RunId精确、slot1、confirmation3/token5/capability1。
没有改 token/UAC，也没有把 High 改 Medium。

随后 driver `PSObject op_Addition` 失败：顶层 `$confirmed` 标量和
`$script:confirmed` 集合处于同脚本 scope，赋值覆盖集合后 `+=` 报错。
源码确定为普通 driver 缺陷；只改集合名并保留实际反例，不将此失败当平台
权限或归因失败。未绑定三窗/发送 Move 输入；产品收到 Q 后 target_declined。
原始日志/包保留，精确本批 guest/任务清理后新身份重验。

## 第三批：三窗权限确认，产品创建拒绝待定位

RUNTIME `9abad65747c6140238f1f8eee98cfbc23bffd3f8`，RunId
`ca7cc51ed4e0484599b64468f9117abe`。集合反例已真实复现并修复，三窗分别
通过 exact 新建/同SID/session1/High/elevated/同RunId、不同slot1/2/3，所有
confirmation3/token5/capability1。driver 按实际 DWM 对齐 B/C，Y 实际送达；
产品返回 `product_session_create_failed`。未取得三窗组绑定、未发 Move。
现有 generic 日志不能区分 VDM、内存组封印、实际 native capture、hook 或
shield 初始化，不能猜测为 High/VDM/几何原因。最小后继增量只传出现有创建
路径的实际拒绝 stage/reason/capture facts；不删除检查，不制造资格。
原始失败保留，下一 SHA/RunId 用该原因继续修复，而非以诊断提交结束功能轮。
本地仅 f086 的未运行包保留；9abad 已修正其 WinPS5.1 scalar JSONL 回归夹具。

## 第四批：实际 Shell 事件健康拒绝

RUNTIME `bac84ceac8d6353231b369d32dd41d20f8e158b4`，RunId
`f9cfec6c32474278959d130aee8c6119`。三窗实际 High 身份确认再次成立；
组创建的实际 native capture 拒绝 member1（B）：`ShellEventStreamInvalid`，
stage `NativeValidation`，invalidation `browser_stream_invalid`，domain3/code1122。
VDM 与先前内存绑定检查已通过；尚无普通 Move 输入或 placement。
仅凭该原因仍不能判断具体 Browser 计数或订阅状态；下一增量透传该次实际
BrowserReadinessFacts，不重查或补真，不放宽原门槛。原始产品日志 SHA-256
`0A4AB7AD688D5C108A3FE2D26E4DD5DB1365D4810BC9987EF552219B1A341B42`。
本批 guest16184 与专用任务已清理，输出 hash 前后一致；此清理不算产品 cleanup PASS。

## 第五批：具体正常通知被错误判为 malformed

RUNTIME `c6949a300a0719cc04975a62f4d1a5a98e5fba12`，RunId
`73e4aeb0f5f746d3bc8f05f1390ba6d0`。实际失败同1122/member1；
Browser subscribed/accepting=true、unadvised=false，callback/latest/epoch、
matching/unrelated navigation、identity failures、quit、overflow、wrong-thread、
post-retirement 均0；malformed=2、last_malformed_dispid=108。
最初将108猜为 CommandStateChange，SDK static_assert 核对否定此猜测，
该猜测补丁未提交/部署。实际108为 ProgressChange；官方合同为两个输入 Long，
原 sink 只接受导航/退出/四个 geometry 通知，故拒绝这个合法通知。
下一最小修复只增加实际108的严格参数合同，保留错误形状和未知事件拒绝。
原产品日志 SHA-256 `ED54CAC0B044DAAD463567624369F2396BDA838A995B8FA256AF0964FB8A9CE4`。
本批仍没有 Move 输入/写入；专用 guest21476 与任务已清理，所有输出 hash 保持。

真实生产 sink 的 Debug 定向反例在旧实现 `checks=452 failures=3`，修复后
`checks=452 failures=0`。测试直接使用 BrowserReadinessEventSink：Progress0/-1
正例及8种错误形状，原未知事件/导航身份/退出检查保留。最小修复只严格接收
ProgressChange，不记录参数值、不产生导航回执或授权事实。独立 diff 复核无新阻断。

## 第六批：产品三窗绑定成立，driver 标题栏命中未找到

RUNTIME `8a02568e6a37ef6322eceec598fa45f3943c24aa`，RunId
`fb87827480db4604a1c451cd77510404`。真实三窗 binding 与 status 成立，
topology_ready=true、relation_count=2，三窗仍是同 SID/session1/High/elevated，
各自 slot1/2/3 和原 token5/capability1。ProgressChange 拒绝已不再阻断创建。
首个普通 Move 在 driver Find-Hit 中 `exact_root_hit_unavailable:2`，未发送
gesture DOWN；不能将其当作产品归因/交接失败或 Move 通过。
实际 DPI192、outer width420px；原命中候选固定偏移10..56px，且在前台获取之前
查找。下一改动仅修正测试新窗准备与实际标题栏命中查找，保留产品 HTCAPTION 门槛。
原产品日志 hash `DC267AE85154BE38B41BF51660217908199A77CB67AAD4B8FB9469394E350341`，
driver hash `618C4F6756AFC1B0B0A6C4FDD9A88A83AF332CFFEDB3DB58DAF713838FCCA755`。
Q 停止后产品实际 resources_stopped=true；专用 guest13436/任务已清理，证据不变。
Debug/Release 的三个受影响 offline CTest 均3/3 PASS，未外推为现场磁吸。

## 第七批：实际 root 命中全为 HTCLIENT，需区分查询对象

RUNTIME `ad029d491abc72342e80f53d24cf79bbe1d172f1`，RunId
`f79dc58429fa4ffc98aa544f487c2acd`。三窗绑定/关系继续成立；A 实际 outer
`[120,140,1160,700]`、DPI192，Find-Hit 时实际 foreground=A197362。
有限84个候选的真实 root WM_NCHITTEST 均1（HTCLIENT），无2；没有发送 DOWN。
SetForegroundWindow 返回true，紧接 after=0，随后真实 snapshot 为A；记录两者，
不将异步前台回读未知改为成功布尔。既有 root 标题栏判断是否应查询实际 child，
仍需最小原生观察，不能用该采样推断全部 Explorer 无标题栏或放宽归因。
driver原日志 hash `CEB6AE34461C94948BEA884CD6C95D73FECEDE20E410E2CC73BE2EFDE9A6A2AE`，
产品原日志 hash `CB3BDD75037B19C6C86CFABE7A75CE48CA2851C2290436CA9A4FE852DA6FDA23`。
Q 实际停止 resources；本批 guest14748/任务清理、输出 hash 不变。Release 尚未执行。

## 第八批：确认真实标题栏 child，原查询 endpoint 错误

RUNTIME `39c095a007afda993d2d4d185427400c2d577175`，RunId
`64b8908e57724bddbaf70b2fc6443f99`。实际 root197358/PID7752/TID7796。
root 的84次查询均HTCLIENT1；实际 WindowFromPoint child131946 同 PID/TID、
GA_ROOT197358，其18次真实 WM_NCHITTEST 为HTCAPTION2、2次为HTTOP12，
剩余64次实际 child 返回HTCLIENT1。所有API调用真实成功/error0。
截图只记录该 guest exact 新窗区域，hash
`ECB80D84E9E299A5D8EB411C1C5608669713BC7B74CE6C99F2BF2BD8F25049FC`，
不把视觉坐标或 `TITLE_BAR_SCAFFOLDING_WINDOW_CLASS` 名称当权限。
原 adapter 归一化到 root 后再问 hit，而 Windows 实际命中为 child；最小修复
改查实际 endpoint 并复核 root/PID/TID/前后稳定。取消、关联、writer 仍控制 root，
不扩大窗口权限、不把1改2。此批没有DOWN；driver hash
`32E4D58B527D1F0AECE32C2FF43C674BBF2AA9A217A09ABF3C8ED7C415D17E55`。
guest/任务已清理，原证据 hash 保持；新 SHA 将进入真实 Explorer 普通 Move 验证。

## 第九批：真实 DOWN 已进入，原始锚点复核拒绝

RUNTIME `321a2c9c94e145aba66b888e42d3b28129adc16a`，RunId
`9a7ce667b2f04a27a0998b3bbbf91b6e`。driver 已实际命中同授权 root66310/
PID7820/TID7864 的 caption child131946/HTCAPTION2，真实 DOWN raw_sequence2
进入产品；产品返回 `down_anchor_not_fresh_or_consent_lost`，随后真实 START
因 no_matching_raw_down 拒绝。没有隔离/cancel/placement。测试发出实际 UP，
产品观测 raw_sequence4；Q 后实际停止资源，专用 guest/任务已清理。
原产品日志 hash `4F3F052B03BE6FE4BEDB6B805B35A58664F4958147A49385DD1BDE24990B38DE`。
该旧合并原因不能区分 capture 失败与原始/新鲜矩形变化，不能预称延迟或权限根因。
下一最小增量分开实际 capture 失败/矩形不一致、传出已有固定数值事实；driver
等待真实 DOWN 处理回执而非90ms后无条件推进移动。产品锚点与所有权限门槛不放宽。

## 第十批：三个普通 Move 正常闭环与 Ctrl A 通过，Resize 处理受阻

RUNTIME `b32b55872a7fa9fd727ad0206cb000b6bf9bfd8a`，RunId
`1ff214d0d9e44a2c90c477ae26efc2d0`。driver149行/product117行JSONL合法连续。
Debug A gen1 / B gen3 / C gen4 五个定向 waypoint 全部实际通过：free、XY snap、
hold outside attraction、detach beyond release、XY resnap。每窗4次真实 exact placement，
hold 为已后验确认的 already_exact 无API；target/actual receipt 与实际 DWM 矩形相等。
各 quantum 最多一次native receipt。原始DOWN锚点保持，真实 END / capture / association
均有回执；Raw与legacy UP实际观察、ReleaseCapture实际true/after0、detach实际true、
NormalUp/Gone清理成立。各 native claim QPC不晚于可靠 Raw UP；UP后 cursor probe
零新native调用且几何不变，不将JSONL顺序当原生共同时间线。
Ctrl A gen2实际leader及B/C followers均平移(34,0)，source writer0，无shield/cancel。
实际caller PMV2及frame DPI192均在现场成立。

B Resize gen5已实际归因 native_resize，随后 group_product_processing_failed。
当前generic原因尚不能断定Browser/身份/尺寸原因。Strict capture本身不锁原尺寸，
不能为通过而删除尺寸或权限检查；最小传出该次group reason及原GroupCaptureResult。
该批仍ABORTED/BLOCKED；Resize刷新、A回环、Ctrl C及Release未执行。
stop_cleanup回执实际succeeded=true；shutdown resources_stopped=false是原stop返回
整体failed状态，不能擅自将整体失败改PASS。专用guest/任务已销毁且证据保持，
不以销毁guest替代产品清理结果。
driver hash `81BDB21D0AE5F79FAC283681685B85B2F6DD8D72B7CC75C4F9A94DB703692682`；
product hash `5DD81817841AB8C281023AF46B6DCB5318BC09E9B27B0A60E449DC1EF93A6F47`。

## 第十一批：Resize 失败确定为正常 CommandStateChange 未解析

RUNTIME `2ac6422c5e715703ef9f2310b06a216f75a96e0b`，RunId
`674b726caa6d4b9da0e50aefef9fbd29`。同流程再次到B Resize gen5；实际group reason
member_validation_failed，capture member1 NativeValidation/domain3/code1122。
该次实际 Browser malformed_count2 / last_malformed_dispid105（SDK
DISPID_COMMANDSTATECHANGE=105），canonical identity=true、订阅/接受true，
unadvised=false；epoch/callback/latest/navigation/identity/quit/overflow/wrong-thread/
post-retirement均0。不是已证实的尺寸、High权限、VDM或系统 modal-loop blocker。
下一最小修复仅补该105的官方严格Long/VARIANT_BOOL通知，不赋导航或写入权限。
原失败日志及专用目录保留；guest/任务已清理，证据 hash 不变。

生产sink105旧反例实际 `checks=471 failures=3`；最小修复后 `checks=471
failures=0`、Debug单项CTest1/1通过。严格两个逆序参数 Long Command / VARIANT_BOOL
Enable0/-1；通知不产生导航/身份凭据。独立diff复核通过，不支持未实测的其他DISPID。
本批产品日志 hash `90E32AD4B8A505400246F300760AB23BB10F4A4EE7FDA120FCACE4027255E825`。

## 第十二批：原生 Resize 刷新通过，A 回环采样退休待具体事实

RUNTIME `2a82d8b7fac7fc5129c2fe5167dcf5553f561294`，RunId
`656c0db5f93a4cdaa92f819bc47bba9b`。B native_resize gen5 actual DOWN/START/
RawUP/END成立；visible `[1183,140,2201,689]` → `[1183,140,2201,717]`，
下边缘+28，其余三边不变，无source writer/cancel/isolation。
END后 status116 逐窗等于实际DWM，A `[165,140,1183,689]`、B上述新尺寸、
C `[165,689,1183,1238]`；topology_ready=true、relation_count=3。
A回环gen6重新获得真实END/capture权限，first free exact成功；下一XY snap
失败为 motion_sample_retired，原记录未传实际权限/命中/采样时间原因。
使用该实际几何的 CursorMoveMagnetIntent 两次采样定向回归通过（40 checks、0 failures），
不能预称solver问题；没有修改core行为。新记录只传出实际短路检查原因与时间/命中
事实，不放宽写入条件、不将前后读取称作原子快照。
该批仍ABORTED，动态Ctrl C、A回环完整及Release未执行；专用guest/任务已清理，
原日志保留：driver `72C67F6CD31DCD87B6CCD9A887EE47E6A1E66F649A4B6F19D0FF2BD939D971D3`，
product `31EF0EEA5AFB7FC00701C8B4DE707F0974977D845F5A7E2AC1C73E1C3FD7EE9B`。

## 第十三批：确认资源先撤权，外部 capture 变化的起因尚未知

RUNTIME `b9d5df37056ed2c2d9ae64a844f8724296231b9e`，RunId
`97b54e0c8ebc45d9ad5bc14d0687eb06`。前三窗普通 Move、Ctrl A、B 原生 Resize
及实际关系刷新仍成立；A 回环 gen6 first free quantum1 exact，之后 raw54 消费时
新增实际短路原因 `gesture_retired`，raw_up=false、isolation_hit=false。
资源实际 WM_CAPTURECHANGED/capture_to0、own_release=false、release_attempted=false，
不是本程序正常 ReleaseCapture；detach_before QPC507199489 / after507206022
早于 owner sample QPC507313217，确认资源侧已先撤权，JSONL 行序不是跨线程总序。
detach 时 foreground394472 不属于 A66310/B918444/C197164 或 overlay263394，
但未记录其身份及首次变化原因，尚不能归因于具体系统窗口或 driver 行为。
driver 同期只发送鼠标移动及读取证据/几何，无主动激活、Ctrl 或提前 UP。
解除关联实际成功、遮罩销毁成立；本手势 NormalUp 未成立，整批仍 ABORTED，Release
未执行。后续仅补最初资源失败的固定原生事实，不恢复 capture 或放宽前台检查。
专用 guest14432/启动任务已清理，清理前后输出 hash 不变；不将销毁 guest 计作产品通过。
driver hash `FA6C89A4B861D51AE7EF266992B59B01D136D3C9C4AAF97151BC07A091C303F3`，
product hash `95C3D73E9C2AD73EA861C6E04BE6AAB3E694B377FC646A65B415059E3C7F5EEC`。
该源码 Release 受影响四项 offline CTest 4/4 通过；不替代此次未完成的现场闭环。

新增复核入口 `prepare-r1c4b-mvp1-shield-sandbox.ps1 -GuestMode BindOnlyIndependentReview`：
精确新 RunId/manifest/hash/guest/High 条件与真实三窗授权不变，自动绑定后不发送手势，
留下同一个 Release console，S 状态、Q 停止；手势期 Ctrl+Shift+F11。
准备状态 BOUND_FOR_INDEPENDENT_REVIEW 不是功能 PASS，真人 NOT_RUN。
实际 PS 回归已覆盖 manifest 严格 mode/Release、未知组合拒绝、分支在手势前退出，
以及既有状态集合和实时 JSONL reader；均离线通过，尚未现场运行该复核模式。
下一包的 CaptureLost/ContextLost callback 固定身份记录只在撤销 writer 后查询，
owner 线程序列化；operations/core 实现没有变更，旧 owned 九项前置不重新解释。

## 第十四批：失败时前台实际是 ConsoleWindowClass

RUNTIME `781262291c9abd7f2d24844616fa2e5ea010bcb8`，RunId
`b8b6c247aae640debdeb122c842b1798`。同样到 post-Resize A gen6 后失去 capture；
同步 CaptureLost callback QPC505612531 实际 foreground/root131268、PID1644/TID1636、
class ConsoleWindowClass，后续 ContextLost callback 同身份。不是三授权 root。
尚未记录本 driver/product 控制台 HWND，不能直接断定该窗口属于本测试。
下一必要对照只核验自有控制台归属，必要时在自动手势段隐藏确属本测试的控制台，
复核模式仍可见；不改变产品前台/capture 门槛，不控制未知窗口。
本批仍 ABORTED，Release 未运行；Debug 四项 offline 回归 4/4 通过，包 Debug/Release
构建通过。一次 native DWORD 与 uint32_t 指针类型不匹配的编译错误已用局部 DWORD
桥接修复，未强制指针转换、未修改权限行为。
guest13128/任务已清理，证据前后哈希不变。driver hash
`AFA7A1A8A6090E08BBCE9137587AA3B8071DF7BB34CA9831733812FB603EA8AC`，
product hash `68C0EA97A4D3F47D3A53F46194D8D33117C8B7167D75C2ECE271D9D7F6CE4FD0`。

## 第十五批：隐藏查询句柄不足，改为自身进程的明确启动显示状态

RUNTIME `261d46ce20f8a14bbdbd8b61396ac5307152d6cf`，RunId
`3e8e7509a8164d11a92890c35ff91cb6`。实际 GetConsoleWindow196980、owner
PID7592/TID7596（product7592）、两实际 clients7252/7592，隐藏后 visible=false，
finally 同身份非激活恢复 visible=true。原关联 console HWND0，clients5800/7252，
不据此赋任何窗口控制权限。本代6失败实际 foreground196808/PID7580/TID1532 /
ConsoleWindowClass，和被隐藏句柄不同；对照不成立，不将其归属猜成已证实的产品
或 runner console，不控制该未知 HWND。其具体显示/代理机制仍未知。
下一最小变化只通过 CreateProcess STARTUPINFO 明确自身新进程自动验证时 SW_HIDE、
复核时可见，不再堆叠未知 HWND 的控制操作；所有产品前台/capture检查仍拒绝矛盾。
本批 ABORTED，Release 未运行；guest13468/任务清理，证据保持。
driver hash `49A1A067B1D25FB82FD82B83043A0504A1D2426AC6BD064544834E8AAD80106F`，
product hash `DEB2FBFB670281FB4F1483FD5D0E1B2285AFBDD48383470656449901F13F666E`。

## 第十六批：产品隐藏启动已推进回环，仍有不同 console 前台打断

RUNTIME `5d17045156044d264f9967e1dc2e20e7256bfd16`，RunId
`c6ddd85e3f8c4eed8660b276ff038219`。本测试 product 通过 CreateProcess
STARTF_USESHOWWINDOW/SW_HIDE 创建，关联查询 HWND197352/PID7772/visible=false。
post-Resize A gen6 已实际 free exact、XY snap exact、hold already_exact；
detach waypoint 时失去 capture，callback实际前台394466/PID1144/TID1304/
ConsoleWindowClass，仍不同于产品查询句柄。原关联 console HWND0/clients5812/7460，
该前台窗口归属仍未知。本批不证明完整回环；Release 未运行。
后继只将自己 `.wsb` 启动的 guest runner 创建参数也明确 Hidden，不控制未知 HWND，
不改变产品权限；若仍失败，不继续添加无依据窗口控制 API。
专用 guest/任务清理及输出哈希保持记录见本批 cleanup-record.json。
