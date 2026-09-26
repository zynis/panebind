# R1-C4B Fix 3 — Verified foreground and native authority result

2026-09-26。**前台 bootstrap 已通过；当前 live Magnet native architecture REJECTED。**
正式 Debug 第一次完整 owned Move / Bottom Resize 均为 REASSERTED，依据任务书第十九条
立即停止后续交互与 Explorer 阶段。不是 foreground blocker，也不是 DWM-only 延迟。

## 基线、范围与实现

```text
Branch = codex/r1c4b-live-magnet
Starting HEAD = 167be83db73f62bf6cc0e0691ab88697fc08871c
Starting working tree = clean
Starting local/upstream = 0/0
Starting origin/main...HEAD = 0 behind / 14 ahead
Standard fetch origin = PASS
origin/main = 81e40facf52ffdb96f76e4d737740167d485f4a7
Primary remote = ssh://git@ssh.github.com:443/zynis/panebind.git
Secondary remote = https://github.com/zynis/panebind.git
```

研究先于实现；[foreground research](../research/R1C4B_FIX3_VERIFIED_FOREGROUND.md)
及 SOURCE_PROVENANCE 记录 actual pinned AltSnap / FancyZones、历史与官方契约。
没有复制、翻译或改写 GPL 实现，MIT 源也未复用。

只修改 test-only auto-modal EXE、新 foreground policy/model tests、离线 evidence
validator、显式 runner 与文档。CMake 仅增加独立 synthetic test target；foreground
helper 未进入产品 target。产品 Core、ExplorerLiveMagnetSession、Glue runtime、
R0 observer、STA console、原 human runner、Fix 1 postverify policy 及 10/16/2000 不变。

新增独立 verified activation path：普通 SetForegroundWindow denied 后，exact own
PID/TID/HWND、desktop / integrity / visibility、临时 TOPMOST（NOACTIVATE）、client center、
fresh root / HTCLIENT / GUI capture/menu/move-size / modifier/button proof，才允许 tagged
move/down/up。实际 WndProc activation/focus/client button events + bounded waits 后，
fresh foreground 再核对，NOTOPMOST 恢复后才允许 modal drag。失败 cleanup 保留原 blocker，
单次安全 release 不是第四次 activation retry。正常 drag 仍每次要求 foreground==owned。

另外修正 native driver 的真实启动时序：LEFTDOWN 后等真实 WM_NCLBUTTONDOWN；首个
有限轨迹 sample 可在 ENTER 前、无 capture 且 root==owned 的严格 priming fence 下触发
native modal loop；ENTER 后所有 held samples 要求 exact own capture。首个 sample 后等待
真实 ENTER/DRAG 和 correction dispatch 完成。没有模拟 WM_MOVING/SIZING 或改写 drag RECT。
修正 absolute coordinate 到像素 cell center，记录真实 input/native QPC；business failure
不会关闭日志导致 shutdown 丢失。UI thread 使用 message/event wait，输入 pacing 为有限
20 samples 的 one-shot timer，不是产品 poller。每个 gesture 仍只有一次 correction。

## 实际证据与完整性

本轮仅一个正式 owned native run，无 DevelopmentRun、无 Human 输入、无真实 Explorer。

```text
Directory = uat/r1c4b-auto-owned/ (ignored local evidence only)
JSONL = 20260926T063002719Z-Debug-cefad6b3351147d8b2969bd93ff5ac1b-1.jsonl
SHA256 = 1CCC89BC9DB54317AE5531B8D822D715EA80EAB9EF8108B1F7B121CDA9D2DF5B
Aggregate = 20260926T063002719Z-Debug-cefad6b3351147d8b2969bd93ff5ac1b.aggregate.json
Executed binary SHA256 = A162FEDBB616EA97EE261A689C0511CA774211E917D89C8E092416958B301E55
Executed HEAD = 167be83db73f62bf6cc0e0691ab88697fc08871c
Executed worktree dirty = true
Binary unchanged during run = true
Requested = 20; Attempted = 1; Captured = 1; ArchitectureAccepted = 0
EXE exit = 0 / CAPTURED_NOT_ACCEPTED
Validator Result = CAPTURED (evidence, not architecture acceptance)
Runner exit = 3 / ArchitectureGate REJECTED
Aggregate EvidenceCaptureGate = PASS
Aggregate DriverGate = FAIL_OR_BLOCKED
```

这是诚实记录的开发 worktree evidence，**不冒充最终 clean commit SHA 的运行**。
后续增强离线 validator/test coverage 和整理文档，并使 fallback cleanup 的 fresh
hit-test/proof exception 只记录安全 release skipped，而不遮蔽原 blocker、跳过 topmost
restore 或 shutdown。该未使用的失败分支没有发送新 input；direct/modal 正例逻辑与
classification 未改变。最终 validator 重新离线复核同一文件，仍得到双 REASSERTED。
没有因 Git 提交或新 regression build 重跑已被明确拒绝的交互。

201 条 JSONL 全部合法、sequence 连续、QPC 单调，startup/shutdown 各一；desktop
active/unlocked/input-desktop matches。HWND/PID/TID 唯一且稳定，没有 log overflow、
写入失败、blocked、probe_failure 或缺失事件的证据。该专用 probe 没有产品 observer queue；
不能将其完整日志伪装成新 Explorer queue/drop UAT。

```text
Target = own empty test window (not explorer.exe)
HWND = 5181160; PID = 47492; UI TID = 36812
DPI = 192; work area = [0,0,3072,1824]
Saved cursor = [993,627]
Initial positioning = [1126,632,1766,1072]
Initial visible = [1137,632,1755,1061]
```

`#6/#8` 普通 SetForegroundWindow 直接成功，fresh foreground==owned、visible=true；
实际 WM_ACTIVATE / WM_SETFOCUS 已观察。bootstrap PASS；click_required=false、
temporary_topmost=false、activation inputs=0。direct success **1/1**；verified activation
click **0/0 NOT_USED**，其 native 正例仍 **NOT TESTED**。A–H synthetic PASS 不能改称
Windows 实测。没有为了强迫覆盖 fallback 再开窗口、抢其他应用前台或修改配置。

## Native Move：T0 → T1 → T2 → actual change → END

路径 `#13`：HTCAPTION=2，cursor [1339,641]→[1519,641]，20 samples，+180 X。
真实 ENTER=`#18`，20 个真实 WM_MOVING callback（含 T2），T1 后还有 19 个 callback。

| 阶段 / sequence | Positioning / proposed | Visible | 观察 |
|---|---|---|---|
| T0 #20 | before [1135,632,1775,1072]; target [1135,639,1775,1079] | before [1146,632,1764,1061]; target [1146,639,1764,1068] | 当前 real drag；+7 Y pulse |
| T1 #21 | [1135,639,1775,1079] | [1146,639,1764,1068] | success=true; error=0; flags=21; calls=1；双 rect exact |
| T2 #24 | proposed [1144,632,1784,1072]; actual [1135,639,1775,1079] | actual [1146,639,1764,1068] | cursor [1357,641]；proposed 已是无 pulse raw trajectory |
| POSITION_CHANGED #25 | [1144,632,1784,1072] | [1155,632,1773,1061] | 真实 actual 随后覆盖 +7 pulse |
| END/T3 #100、final #102 | [1306,632,1946,1072] | [1317,632,1935,1061] | 最终 raw +180 X，+7 Y 未保留 |

QPC：ENTER=639293102388；T0=639293224013；native start/return=
639293224317/639293243323；T1=639293244059；T2=639293738042；T3=639302641039。
保留 pulse 的期望 final positioning 应为 [1306,639,1946,1079]，actual 不是。
`OWNED_MOVE_MODAL_AUTHORITY = REASSERTED`。

## Native Bottom Resize

路径 `#106`：HTBOTTOM=15，cursor [1519,1071]→[1519,1191]，20 samples，bottom +120。
真实 ENTER=`#109`，20 个真实 WM_SIZING callback，edge=6/WMSZ_BOTTOM，T1 后 19 个。

| 阶段 / sequence | Positioning / proposed | Visible | 观察 |
|---|---|---|---|
| T0 #113 | before [1306,632,1946,1078]; target [1306,632,1946,1085] | before [1317,632,1935,1067]; target [1317,632,1935,1074] | +7 bottom pulse，不改变其它边 |
| T1 #114 | [1306,632,1946,1085] | [1317,632,1935,1074] | success=true; error=0; flags=20; calls=1；双 rect exact |
| T2 #119 | proposed [1306,632,1946,1084]; actual [1306,632,1946,1085] | actual [1317,632,1935,1074] | cursor [1519,1083]；proposed 已是无 pulse raw trajectory |
| POSITION_CHANGED #120 | [1306,632,1946,1084] | [1317,632,1935,1073] | 真实 actual 随后覆盖 +7 pulse |
| END/T3 #195、final #197 | [1306,632,1946,1192] | [1317,632,1935,1181] | 最终 raw bottom +120，不保留 pulse |

QPC：ENTER=639303719775；T0=639304204226；native start/return=
639304204654/639304231262；T1=639304232295；T2=639304684954；T3=639313539024。
保留 pulse 的 final positioning bottom 应为1199，actual=1192。
`OWNED_RESIZE_MODAL_AUTHORITY = REASSERTED`。

两项 T2 的 actual 仍是先前 corrected rect，**不能把 T2 callback 前 snapshot 描述成
已经 actual reasserted**；proposed 是操作系统接下来要应用的 rect。紧随的实际
POSITION_CHANGED 与 END/final 双 rect 才构成完整覆盖证据。T1 双 rect 已 exact，
不存在只靠 visible 延迟解释该 positioning 回退的空间。没有 immediate rejection。

47 次普通 SendInput 均成功且 tagged，fresh fences 全部 foreground==owned；无 foreign
capture。`#99/#194` LEFTUP，`#101/#196` capture=0、left_down=false；`#199/#200`
恢复 cursor，shutdown `#201` owned_window_destroyed=true / external_windows_touched=false。
无异常 cleanup、无第三方窗口内容/标题收集，没有强制关闭用户应用。

## Stop gate 与替代研究

Debug 捕获完成 Move/Resize **1/20**；Release **0/20 NOT RUN**。
两配置 architecture accepted 均为0。单样本 aggregate Consistent=true 仅是集合结果，
不等于20/20，不构成非 flaky 保证，modal flaky rate **NOT MEASURED**。

一份 formal run 包含 Move+Resize，完成后离线分类立即阻止第2次 owned run、Release
modal run、Explorer provisioning/bootstrap/full gate；没有重复 SetWindowPos、改10/16/2000
或要求 Human 再拖动。本次不是 Explorer empirical verdict，Explorer 两项仍 UNKNOWN。

[A/B/C alternatives](../research/R1C4B_FIX3_ALTERNATIVES.md) 已在 rejection 后完成：
A recipient RECT authority 在禁止注入的 Explorer 场景中 BLOCKED；B cancel/takeover
未获得可靠 authority/输入接管证明，UNPROVEN_AND_BLOCKED；C snap-on-release 是需
Human 产品决策和新测试 Gate 的研究候选，不能静默降级 live UX。未实现任何替代方案。
旧真人失败的 Fix 1 forensic 保持原结论，不能用 owned 证据追认其缺失的 subsequent END。

## 自动回归（不是新的交互验收）

Windows 10.0.26200、PowerShell5.1、VS18 2026 x64/MSVC19.50、SDK10.0.26100.0。
已有 build directories；进程内 MSBUILDDISABLENODEREUSE=1，parallel2、/nodeReuse:false。

```powershell
$cmake = 'D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ctest = 'D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
& $cmake --build out/r1c4b-live-magnet-debug --config Debug --parallel 2 -- /nodeReuse:false
& $ctest --test-dir out/r1c4b-live-magnet-debug -C Debug --output-on-failure
& $cmake --build out/r1c4b-live-magnet-release --config Release --parallel 2 -- /nodeReuse:false
& $ctest --test-dir out/r1c4b-live-magnet-release -C Release --output-on-failure
# 本轮已执行、随后依 architecture STOP 中止批次的正式输入命令：
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/run-r1c4b-auto-owned-modal.ps1 -Configuration Debug -Repetitions 20
```

两个 restricted process CTest 最初各27/28，唯一失败为已有 VDM real read-only owned-frame
desktop query。未改代码；普通主机进程 full CTest 各28/28 PASS，记录环境边界，不抹掉失败。
这不是 retry-until-pass modal gate，不触发新的 input/Explorer/human interaction。

| 回归 | 结果 |
|---|---|
| Debug/Release build | PASS/PASS |
| Host Debug/Release CTest | **28/28 + 28/28 PASS** |
| Owned/Companion --self-test，两配置 | 全部 PASS，failures=0；third_party_window_control=false |
| Foreground A–H model，两配置 | 各46 checks PASS，synthetic only |
| C2B/C3A evidence runner | PASS/PASS |
| C3B Phase1/2/3 evidence runner | PASS/PASS(41)/PASS(28) |
| C4A evidence runner + capture serializer | PASS(140)；两配置各4 stages PASS |
| Pure Magnet/C4B gesture/resize bridge/postverify units | 两配置 CTest PASS |
| C4B evidence/postverify fixtures | PASS(29)/PASS(14) |
| Fix2 retained + new foreground/modal evidence fixtures | PASS(144)，synthetic only |
| Auto modal EXE 无参数/外部 HWND 参数拒绝 | 两配置均 exit2；没有 UI |
| Actual owned formal Move/Bottom Resize | CAPTURED，**REASSERTED/REASSERTED**，不计架构 PASS |
| Explorer bootstrap/full validators | NOT IMPLEMENTED/NOT RUN，由提前 STOP 所致 |

各 PS fixture 显式命令为 `powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/<name>.ps1`：
`test-r1c2b-evidence-runner`、`test-r1c3a-evidence-runner`、`test-r1c3b-profile-runner`、
`test-r1c3b-frame-profile-runner`、`test-r1c3b-vdm-profile-runner`、`test-r1c4a-evidence-runner`、
`test-r1c4b-evidence-runner`、`test-r1c4b-postverify`、`test-r1c4b-auto-owned-modal`。
serializer 使用 `test-r1c4a-capture-json.ps1 -BuildDirectory <上述build> -Configuration <config>`。
没有持久修改 execution policy、Git HTTP/TLS/proxy、foreground lock 或系统配置。

旧真人 JSONL 的 SHA256 仍为
`8F008F8756C828C83370AECDFEA8A136859FEC06CDF00240A4F1157A845E7317`；新旧 raw evidence
全部留在 ignored uat/，不提交。Final SHA、commit、标准 push/remote equality、clean 状态
及 divergence 在最终交接列出，避免提交内伪造 self-referential SHA。

## 最终状态

```text
FIX3_FOREGROUND_BOOTSTRAP = PASS
SETFOREGROUNDWINDOW_DIRECT_SUCCESS_RATE = 1/1
VERIFIED_ACTIVATION_CLICK_SUCCESS_RATE = 0/0 (NOT_USED; native positive NOT TESTED)
AUTO_OWNED_MODAL_DRIVER = FAIL (formal gate; single evidence capture PASS)
AUTO_OWNED_MOVE_REPETITIONS = Debug 1/20; Release 0/20 (captured; accepted 0)
AUTO_OWNED_RESIZE_REPETITIONS = Debug 1/20; Release 0/20 (captured; accepted 0)
OWNED_MOVE_MODAL_AUTHORITY = REASSERTED
OWNED_RESIZE_MODAL_AUTHORITY = REASSERTED
AUTO_EXPLORER_BOOTSTRAP = NOT_RUN
EXPLORER_MOVE_MODAL_AUTHORITY = UNKNOWN
EXPLORER_RESIZE_MODAL_AUTHORITY = UNKNOWN
CURRENT_LIVE_MAGNET_NATIVE_ARCHITECTURE = REJECTED
AUTO_EXPLORER_FULL_GATE = BLOCKED_BY_ARCHITECTURE
AUTO_DEBUG_REPETITIONS = 0/10
AUTO_RELEASE_REPETITIONS = 0/10
R1C4B_IMPLEMENTATION_READY = NO
R1C4B_HUMAN_UAT = NOT_READY
PRODUCT_SENDINPUT_DEPENDENCY = NONE
PRODUCT_FOREGROUND_FORCE = NONE
PRODUCT_GLOBAL_MOUSE_HOOK = NONE
PRODUCT_GLOBAL_KEYBOARD_HOOK = NONE
PRODUCT_POLLING = NONE
PR = NO
MERGE = NO
TAG = NO
RELEASE = NO
```

剩余 NOT TESTED：native fallback activation click、其真实失败分支、Release native modal、
20/20重复与 flaky rate、所有 Explorer authority/full gate、新 Human UAT/subjective feel，
以及 A/B/C 替代方案的实际可行性。原有 C2B/C3A/C3B 人工基线未被这些 owned 证据推翻。
本轮 STOP；保留 hard human block，不给 Human UAT 命令，不开始下一架构实现。
