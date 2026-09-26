# R1-C4B Pivot 1 — Fix A 执行报告

2026-09-26。**FALSE_LOCAL_ACTIVATION_GATE_REMOVED=YES**。
新 probe / validator 使用 `verified_global_foreground_v2`；142 项 synthetic
checks PASS。环境解锁后已真正观察 background Raw movement/UP、native Move、
单 WM_CANCELMODE、capture release 与 held-button EXIT。当前预先固定的
**cancel-return rect 必须保持冻结**门 FAIL，按本轮合同输出
`REJECTED_AT_CANCEL_STAGE` 并停止。实际是 EXIT 前的 terminal restoration
pattern，不是退出失败或 EXIT 后 reassertion；更广泛的 wait-EXIT-then-takeover
变体仍未验证，不能由本轮狭义门失败推出所有 Raw takeover 不可能。

## 基线与变更边界

```text
Branch = codex/r1c4b-live-magnet
Starting HEAD = 94242959568b28d782d1a89a6f78c95c2dd8b098
Starting working tree = clean
Starting local/upstream = 0/0
Standard fetch origin = PASS
origin/main = 81e40facf52ffdb96f76e4d737740167d485f4a7
Starting origin/main...HEAD = 0 behind / 16 ahead
origin = ssh://git@ssh.github.com:443/zynis/panebind.git
github-https = https://github.com/zynis/panebind.git
```

仅独立 test-owned probe、其 runner/validator/fixtures、研究/架构/报告文档。
未改产品 runtime、Core/Pure Magnet math、C4A dynamic leader、HDWP、Relation
Graph、R0 observer、Fix 1 diagnostics、Fix 2/3 benchmark 或 Human runner。
没有 Human 拖动、computer-use、真实 Explorer、PR/merge/tag/release。

已读 [Fix A 官方合约覆盖](../research/R1C4B_PIVOT1_INPUT_CONTRACTS.md)，
沿用实际 inspected 的 AltSnap/FancyZones exact pins/licenses/history，未虚称
重新 inspect upstream，也没有外部代码复制/翻译/适配。详见
[SOURCE_PROVENANCE](../research/SOURCE_PROVENANCE.md)。研究 gate 只授权 test probe。

## IMPLEMENTED / AUTOMATED TESTED：移除错误前提

- 删除 `prepare_local_activation()` 及调用；无 SetActiveWindow(NULL)、
  SetFocus(NULL) prerequisite、AttachThreadInput、Alt trick 或 foreground-lock 修改。
- `source_thread_local_active/focus` 从 owner UI 或 exact source TID 查询，
  仅 bounded diagnostic；非 NULL 不改变新实验的 verdict。
- Direct 与 strict verified click fallback 都由 fresh exact global source
  HWND/PID/TID、identity、desktop、visible、GUI capture/menu/MoveSize、键钮及
  topmost 恢复证明。Fallback 保留真实 source DOWN/UP receipts 和所有 input fences。
  WM_ACTIVATE/SETFOCUS 仅记录，不等待新的 callback 来取得 authority。
- Receiver 是独立 PID，exact mouse-only INPUTSINK message window；只有 actual
  RIM_INPUTSINK movement/UP、source foreground PID≠receiver、sequence/tag/actual
  normalized INPUT/QPC/watermark 与最终 cursor/button fence 才能通过 Raw gate。
  不恢复 same-callback cursor == submitted point，不积分 Raw 坐标驱动 geometry。
- 完整实际 preflight stimulus 缺 movement/UP 的可靠 negative 可 FAIL；尚未完成
  stimulus、identity/context 丢失或未执行只 BLOCKED。新增 UP deadline/outcome
  与独立 final fence，不伪造 Raw END。
- V2 的 cancel deadline negative 必须有真实 ENTER/DRAG、一次 cancel、实际
  sample3/Raw receipt、完整 deadline 及 fresh owner GUI/desktop/cursor/键钮/receiver
  context。孤立 timeout 仍 UNKNOWN；可靠无法退出才按 Fix A 判 FAIL 并停止。
- 原 v1 decoder 仅复核历史语义，原三日志不追认 PASS。显式新 runner 要求 v2
  contract，否则不能通过新验收。没有修改旧 Fix 3 shared helper。

新 142 synthetic checks 包括：nonnull local active/focus、缺 fresh callbacks
的 direct/fallback 正例、wrong global PID/TID、foreground stale、foreign GUI、
missing click receipt/restore、同 receiver/foreground PID、RIM_INPUT 而非 SINK、
完整 missing movement/UP negative、early incomplete preflight BLOCKED、可靠
retained-native deadline FAIL 及各项 context 缺失 UNKNOWN。它们不是实测 PASS。

## AUTOMATED OBSERVED：首次 v2 实测及环境 blocker

```text
Configuration = Debug
Stage = cancel_only
JSONL = uat/r1c4b-takeover-owned/20260926T103024833Z-Debug-fb0c0cc9907043a88edb5065a5dc1fda.jsonl
Log SHA256 = F53037BD2C6D7BB8299A5D92E3A166370F87F3833BCB268C7FE50A7FDF75D63E
Binary SHA256 before/after = 4E877BAA4256421DAC16581C0B87D3449CAD15700A8F84E2552EC2DF1D653FC4
ExecutedHEAD / AfterHEAD = 94242959568b28d782d1a89a6f78c95c2dd8b098
WorktreeDirty = true
Source process PID / UI TID = 47344 / 42044
Foreground contract = verified_global_foreground_v2
Probe / runner exit = 2 / 2
Result = BLOCKED_BY_INTERACTIVE_DESKTOP
```

4 条 JSONL 合法、startup/shutdown 完整、sequence1–4 连续、QPC 递增。
启动前 desktop_available 检查即停止：target0、未尝试 foreground setter/click，
没有 source/guard/receiver 创建、SendInput、Raw、ENTER/DRAG/EXIT、cancel 或 writer。
`receiver_stopped=false` 表示 receiver 未创建，不是清理失败；没有 receiver error。
日志由独立 reviewer 逐行及 hash 复核，最新 validator 保持 BLOCKED/UNKNOWN。
这份开发观察明确绑定 dirty starting HEAD 与实际 binary hash，不冒充正式20/20。

后续只读 Win32/WTS 诊断：input/current thread desktop names 均为 Default；
OpenInputDesktop 与 Unicode name queries 成功，foreground 非 NULL；WTS query
成功、232 bytes、Level1、SessionState0 (Active)、SessionFlags0 (LOCK)。
当前 Windows NT10.0.26200 不使用 Windows7 的历史反转规则。Active/Default
本身不能代替 unlocked 条件。因此安全门正确，**不再放宽或绕过**。
首次独立 PowerShell name diagnostic 漏了 Unicode marshaling，输出单字符 D；
已纠正声明后只读复核为 Default，不将 D 作为真实桌面名。

已向用户确认解锁环境；这不是要求 Human UAT。需要会话解锁、保持普通桌面，
才可继续同一 owned self-driving probe。没有因为此环境 blocker 改判 Raw/cancel
架构，没有重试 locked 环境的输入、修改锁屏配置或控制其他窗口。

## 环境恢复后：真正的 Raw / native Move 观察

工具等待后一次只读 WTS 复核得到 Level1/Active0/**UNLOCK1**。没有修改
安全门或锁屏配置；原任务授权下继续同一 binary。首个解锁后的尝试：
`20260926T141635694Z-Debug-62da4696888e4ec8bea159e2d447d82f.jsonl`，14条，
SHA256 `15085FF1A62E5A86CEFFBD608027D7D5776D92F450991757FD1A486AE39FABC9`。
普通 foreground setter 成功，source local active/focus 非 NULL、fresh ready
PASS；首次 input fence 报 INPUT_INTERFERENCE，仍为零输入/Raw/native/cancel。
没有记录失败时各项 cursor/key 值，不能猜是用户移动还是其他瞬时状态。
明确避免操作输入后，仅再有界尝试一次；不是 retry-until-stick。

### Clean implementation-bound 最终观察

```text
JSONL = uat/r1c4b-takeover-owned/20260926T141825568Z-Debug-2b64762128cc4148b849947e6c05b2b1.jsonl
Log SHA256 = F146A4235600F0FFFCD1E708238E5960FBEA2C7AD0597EECC3D569770D65B74E
ExecutedHEAD / AfterHEAD = e2ea24d0ad2e51837e8f73f1a0513b6f306485a5
WorktreeDirty = false
Binary SHA256 before/after = 4E877BAA4256421DAC16581C0B87D3449CAD15700A8F84E2552EC2DF1D653FC4
Source HWND / PID / TID = 15666532 / 1316 / 1764
Receiver message HWND / PID / TID = 43191140 / 39976 / 32360
Foreground HWND / PID = 15666532 / 1316
Monitor work area = [0,0,3072,1824]; DPI = 192
Probe / runner exit = 2 / 2
Mapper = FAIL; RawBackground = PASS; CancelMove = FAIL; CancelResize = UNKNOWN
```

114条合法JSONL、完整startup/shutdown、sequence/QPC和receiver stream可信。
Source HWND/PID/TID未变；exact背景receiver不同于foreground PID，TLC1/2、
INPUTSINK256注册/读回。共9 Raw packets：6 movement、1UP、2DOWN；全部actual
RIM_INPUTSINK、tag/normalized INPUT/QPC/watermark关联。正式 preflight 是
2movement+1UP及最终cursor/button fence；另有native gesture阶段4movement+1DOWN。
Raw snapshot lag仍保留，不用同callback cursor等于submitted point作为前提。
原trigger与Raw UP transition独立于async high bit。

这次普通setter denied，但strict activation move/down/up、actual source
DOWN/UP、NOTOPMOST恢复及fresh foreground全部成功；local active/focus均为
source，click epoch的新WM_ACTIVATE/WM_SETFOCUS/activation-event均未出现，
bootstrap仍PASS。这是FalseLocalGate被移除后的**实测**，不是只靠synthetic。

### Native cancel 时序：terminal restoration，不能虚报 continued drag

| seq / QPC | 实际事实 |
| --- | --- |
| 75 / 920323517379 | 真实 ENTER；标题HTCAPTION，左键仍按下。 |
| 76、87 | 两次真实 WM_MOVING；native rect由初始向右移动9、18px。 |
| 92、93 | exactly one WM_CANCELMODE，exactsource；capturebefore=source、LBheld。 |
| 94 / 920324876100 | 真实CAPTURE_CHANGED到NULL；ownercapture=0。 |
| 95 / returned920324888216 | SendMessageTimeout成功、error0、recipientLRESULT0；captureafter=0、LBheld，P/V仍为移动后rect。 |
| 96 / 920324898057 | 在任何新mouse MOVE之前，POSITION_CHANGED回到native START的原始P/V；比APIreturn晚0.9841ms。 |
| 97 / 920324909397 | 真实EXIT、capture0、LB仍held；比APIreturn晚2.1181ms。 |
| 102 / injectionstart920325319257 | 第3个既定MOVE才提交，已晚于EXIT40.986ms；actualtagged后台Raw送达。 |
| 106–108 | FreshGUI已清、cursor到[1366,641]、LBheld；readback仍为恢复后的原始rect。 |
| 109–114 | 因return-baseline mismatch立即停止；单次安全cleanup UP、receiver注册移除/销毁与空source/guard销毁。 |

| 状态 | Positioning rect | Visible rect |
| --- | --- | --- |
| Native START initial | [1126,632,1766,1072] | [1137,632,1755,1061] |
| API-return baseline | [1144,632,1784,1072] | [1155,632,1773,1061] |
| Terminal restoration / EXIT / third-MOVE readback | [1126,632,1766,1072] | [1137,632,1755,1061] |

实际 ENTER1、WM_MOVING2、cancel1、EXIT1，post-return WM_MOVING0。
没有post-EXIT新geometry transition或reassertion证据。取消时的矩形回原点是
**observed terminal restoration pattern**；不能凭相同形状证明未记录的Windows
内部机制，也不能说WM_CANCELMODE未释放capture/未结束loop。

当前已预先固定在probe/validator/architecture文档中的return-baseline retention
合同不满足：#108的P/V不是#95的P/V。按Fix A第12/14项的本轮严格baseline
门保持MoveFAIL、`REJECTED_AT_CANCEL_STAGE`，不在看到结果后移动验收边界来
追认PASS。**该拒绝只针对API-return geometry冻结的当前测试合同**；若允许
EXIT前terminal restoration并以EXIT-final rect为anchor，是新的合同/研究决定，
不能在本轮悄悄改写，也不能据当前FAIL推广否决这种尚未测试的变体。

Mapper `GeometryChanges=2` 计的是#108和#112相对return baseline不同的两次
observation，不是两次新native写入或两次reassertion；#96真正restoration早于
第3MOVE，未进入该filtered计数。Takeover writes始终0。
没有queue/drop/Raw read/pipe error记录，source/receiver identity与cleanup可信。
Failure scope在cleanup之前退休，cleanup UP不充当native gesture的Raw END。
Cursor未恢复，原false字段保留；没有在失败后的未知authority下强行恢复指针。

两个独立reviewers亲读114条、hash和最新mapper，一致确认上述时序及狭义门失败。
未完成余下15+cursor samples/native gesture Raw UP，不能声称complete continuity。
发现当前合同反例即STOP：不运行Resize、Release交互、20/20、free writer、Magnet或Explorer。

## 当前四级 verdict / NOT TESTED

```text
CURRENT_CONCURRENT_NATIVE_LOOP_ARCHITECTURE = REJECTED
FALSE_LOCAL_ACTIVATION_GATE_REMOVED = YES
SOURCE_THREAD_ACTIVE_USED_AS_AUTHORITY = NO
SOURCE_THREAD_FOCUS_USED_AS_AUTHORITY = NO
RAW_RECEIVER_PID = 39976
FOREGROUND_PID = 1316
OWNED_RAW_INPUT_BACKGROUND = PASS
OWNED_RAW_MOVEMENT_PACKETS = 6
OWNED_RAW_UP_PACKETS = 1
OWNED_NATIVE_CANCEL_MOVE = FAIL
OWNED_NATIVE_CANCEL_RESIZE = UNKNOWN
CANCEL_GATE_DEBUG = 0/20
CANCEL_GATE_RELEASE = 0/20
OWNED_TAKEOVER_MOVE = NOT_RUN
OWNED_TAKEOVER_RESIZE = NOT_RUN
OWNED_TAKEOVER_MODAL_REASSERTIONS = 0
OWNED_TAKEOVER_PRE_RELEASE_CONTROL = NOT_RUN
RAW_INPUT_TAKEOVER_ARCHITECTURE = REJECTED_AT_CANCEL_STAGE
EXPLORER_STAGE = NOT_RUN
WH_MOUSE_LL_FALLBACK = TECHNICALLY_POSSIBLE
C_SNAP_ON_RELEASE = UNSELECTED_FALLBACK
R1C4B_HUMAN_UAT = NOT_READY
PRODUCT_RAW_INPUT = NOT_IMPLEMENTED
PRODUCT_SENDINPUT_DEPENDENCY = NONE
PRODUCT_GLOBAL_MOUSE_HOOK = NONE
PRODUCT_DLL_INJECTION = NONE
PRODUCT_POLLING = NONE
PR = NO
MERGE = NO
TAG = NO
RELEASE = NO
```

0/20 是没有完成一次 successful cancellation pair，不是已测20次都 FAIL；Debug
两次安全前置阻断、一次实际Move合同反例，Resize/Release interactive未运行。
Reassertions0 是未执行takeover的空计数，不是sole-writer PASS。
完整cancel余下15+样本/gesture UP、Debug/Release20/20、free takeover
Move/Bottom Resize、pre-release/sole writer、Magnet、Explorer、physical input/
多显示器/mixed DPI、长时 throughput/idle resource 均 NOT TESTED。
WH_MOUSE_LL 仅官方 non-injected/shared-desktop 研究候选，未实现。

## 自动回归

CMake4.2.3-msvc3、VS18/MSBuild18.5.4、SDK10.0.26100、C++20/PMv2；
PowerShell5.1。工具路径前缀：
`D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\`。

```powershell
$env:MSBUILDDISABLENODEREUSE='1'
cmake --build out/r1c4b-live-magnet-debug --config Debug --parallel 2 -- /nodeReuse:false
cmake --build out/r1c4b-live-magnet-release --config Release --parallel 1 -- /nodeReuse:false
ctest --test-dir out/r1c4b-live-magnet-debug -C Debug --output-on-failure
ctest --test-dir out/r1c4b-live-magnet-release -C Release --output-on-failure
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c4b-takeover-owned.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/run-r1c4b-takeover-owned.ps1 -Configuration Debug
```

Debug/Release full builds PASS；Host CTest各28/28 PASS（含 Owned/Companion/
C2B/C3A/C3B/C4A/C4B 与 read-only VDM）；新 Pivot fixtures142 PASS；旧 Fix3
`test-r1c4b-auto-owned-modal.ps1`144 PASS。
Release 初次 parallel2 full build 曾出现 MSB6003，CL tracker 试图访问
`D:\Program Files\MacType\panebind-explorer-profile-glue-harness.dir\Release\...tlog`
而被拒。显式 repo working directory、parallel1 一次有界重试 PASS；未改配置，
没有证据确认该异常根因，也不将它认定为 PaneBind 实现失败。最终回归在构建完成后复核。

额外默认 offline synthetic（统一 `powershell.exe -NoProfile -ExecutionPolicy Bypass
-File scripts/<name>`，无真实 UAT 参数）：

| 脚本 | 结果 |
| --- | --- |
| test-r1c2b-evidence-runner.ps1 | exit0；26 stdout fixture PASS |
| test-r1c3a-evidence-runner.ps1 | exit0；61 stdout fixture PASS |
| test-r1c3b-profile-runner.ps1 | exit0；47 stdout fixture PASS |
| test-r1c3b-frame-profile-runner.ps1 | exit0；41 script checks |
| test-r1c3b-vdm-profile-runner.ps1 | exit0；28 script checks |
| test-r1c4a-capture-json.ps1 | exit0；Debug/Release各4 synthetic stages |
| test-r1c4a-evidence-runner.ps1 | exit0；140 checks |
| test-r1c4b-evidence-runner.ps1 | exit0；29 checks |
| test-r1c4b-postverify.ps1 | exit0；14 checks |

Release capture command另加 `-BuildDirectory out/r1c4b-live-magnet-release
-Configuration Release`。前三项计数来自输出 fixture PASS 行，不虚称全部内部
assert数；其余为脚本汇总。这些测试不运行 input/live harness。

## 证据及 Git 收口

旧 Pivot 三份、Fix3 一份 JSONL SHA256 均未变、仍 ignored；新 JSONL/metadata
也只在 local ignored uat/。未追认旧日志或将 synthetic 当实测。
本轮以标准 commit/push 收口当前分支，最终 SHA、remote verification、clean
及 local/upstream divergence 在交付时报告。不改历史，不合并 main。
