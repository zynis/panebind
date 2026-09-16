# R1-C4B Fix 1 — Human failure forensics / Amendment 001

2026-09-16. 状态：**诊断修复完成，modal authority 未证实，禁止再次真人 UAT**。
完整研究与 gate decision 见 [modal authority](../research/R1C4B_FIX1_MODAL_AUTHORITY.md)。

## HUMAN FORENSICS

只读原文件：`uat/r1c4b/20260916T022813557Z-e0cb2eda6314414190ddd1de8ef2515c.jsonl`。
SHA256：`8F008F8756C828C83370AECDFEA8A136859FEC06CDF00240A4F1157A845E7317`。
708 行合法 JSONL，sequence 1–708 连续，startup/shutdown 各一；shutdown 为 BLOCKED。
记录数量：365 receipt、150 quantum、149 sample、18 correction（仅一次 native attempted）、
2 gesture、1 action、1 relation_graph、2 capture_diagnostic。
原始文件未改写、删除或提交；分析脚本 `scripts/analyze-r1c4b-failure.ps1` 可复算。

### HUMAN_M1_ATTEMPT1

| 项目 | 实证 |
| --- | --- |
| gesture / source | 1 / C（member 2） |
| START / END receipt | 1 / 234 |
| meaningful / solver_calls | 86 / 86 |
| motion_suppressed | **0** |
| proposals / native corrections | 15 / **0** |
| nearest raw A-C gap while active | **−1 px**，receipt 228、229；均有 proposal |
| 最终 gap / overlap / relation | −5 / 1026 / NO |
| skip reasons | 5 × raw_sample_superseded；10 × outside_supported_work_area |

110 个 sample：71 no_candidate、15 proposal、24 unchanged。不是没有 owner processing，
也不是没有 candidate，更不是未进入 attraction=10。前四个 superseded proposal 是其它对齐候选；
receipt 221 的 A-C proposal 也被新的 raw 抢先。receipt 222–234 的十次 proposal 因工作区拒绝：
目标 C bottom=1889，而工作区 bottom=1824，相差 **65 px**。最后一次是 END proposal，
其余九次发生于 END 之前。当前窗口高度与 A 所处位置使该垂直目标超出支持工作区。
本轮不移动/缩小用户窗口、不放宽工作区或改变参数来掩盖这一事实。

`M1_FIRST_ATTEMPT_MISSED_DUE_TO_SPEED_POLICY = NO`。
第二个 gesture 有 8 个 motion_suppressed，不能移花接木解释第一次 M1。

### HUMAN_FAILED_CORRECTION

日志第二个 gesture 实际 source 是 **B**，不是 C；不可把用户第二次操作直接标成 C 的 M1。

```text
GESTURE = 2
OPERATION = 3
SOURCE_MEMBER = B / 1 / logical id 4
TARGET_PROCESS = explorer.exe
TARGET_HWND = 852438
TARGET_PID = 12664
TARGET_TID = 15236
DPI = 192
MONITOR = DISPLAY1
INTERACTION = Move

RAW_VISIBLE = BEFORE_VISIBLE = [1284,651,2371,1492]
BEFORE_POSITIONING = [1273,651,2382,1503]
CORRECTED_VISIBLE = TARGET_VISIBLE = [1279,651,2366,1492]
TARGET_POSITIONING = [1268,651,2377,1503]
NATIVE_ATTEMPTED = true
NATIVE_SUCCESS = true
WIN32_ERROR = 0
NATIVE_CALLS = 1
FLAGS = 21
ACTUAL_VISIBLE = [1284,653,2371,1494]
ACTUAL_POSITIONING = [1273,653,2382,1505]
VISIBLE_EXACT = false
POSITIONING_EXACT = false
VISIBLE_DELTA_FROM_TARGET = [5,2,5,2]
POSITIONING_DELTA_FROM_TARGET = [5,2,5,2]
TRIGGER_RECEIPT = 365
TRIGGER_KIND = LOCATION
NATIVE_START_BEFORE_END_CALLBACK = UNKNOWN_NO_END_RECORDED
CLASSIFICATION = POSITIONING_REJECTED (CASE B; observed result, not proven mechanism)
```

第二 gesture START=235；39 sample 中 25 no_candidate、8 motion_suppressed、3 proposal、
3 unchanged。前两个 proposal 为 raw_sample_superseded；第三个进入完整 pending/native 路径。
其它两成员的 actual visible/positioning 与 before 一致。

| 时间点 | QPC（frequency 10,000,000） |
| --- | --- |
| trigger callback | 664671675550 |
| owner | 664671690917 |
| native start | 664671751714 |
| native return | 664671813950 |
| full postverify finished | 664671901513 |

native 用时 6.2236 ms；return 到 full postverify 完成 8.7563 ms。
在后者期间存在继续移动/调整的可能；旧日志没有 API 返回瞬间的双矩形。
实际 X 回到原 raw X、Y 增加 2 是 **观察值**，不能直接升级为 CASE C 的因果证据。
本 gesture END=0、不完整；fail-closed 后未导出下一 receipt/LOCATION/END。
最终失败 quantum 也未入 quanta vector（旧代码在 stop 前返回）；不能伪造缺失尾部。
因此不能声称 native_start < END callback，也不能声称 positioning 曾立即 exact。

没有日志证据支持 queue overflow/drop 是本次原因；但 BLOCKED 日志无正常 summary 的最终
queue/drop 累计，**不能把未知累计数写成 0**。已导出 receipt 1–365 连续，150 个 quantum
active inventory/manager creates 均 0，每 quantum solver <=1、成功 correction=0。
唯一已观察 native error 为 postverify geometry mismatch，而非 SetWindowPos BOOL failure。

## 修改范围 / 自动测试

- `MagnetPostverifyDiagnostic` 独立于 GroupCaptureResult；capture_failure 改为 optional。
  Placement failure 报告 requested/actual 双矩形、edge deltas、native/error、context、
  other-members 和 receipt health；所有七种指定 failure_class 均覆盖，另区分 SourceContextChanged。
- native return 后添加直接 geometry diagnostic（带 API error/QPC），随后仍执行原 full
  capture 和 exact 接受条件。没有引入异步接受、重试、sleep、polling 或更多 native writes。
- correction 子对象记录 postverify 和可空 postverify_failure；成功 capture 不再输出
  默认 Binding/capture_not_run。Harness 与离线 validator 分别生成/复算后续事件关联。
- owned modal-loop probe 已构建，真实交互未运行。工具 native pipe 缺失，两次尝试加一次
  reset/reinitialize 后仍为 os error 2；依 computer-use 技能恢复规则停止 GUI 输入。
  Probe 可供独立审查，但并非接受证据，不向用户发出 Explorer 重测命令。
- Amendment 001 human-runner gate 已 fail-closed；即使传 IndependentReviewPassed 也不启动。
  完整自动 Explorer driver/schema/artifact verifier **未实现**，因为 architecture 前置未通过。
- 未修改 Core Magnet 数学、RelationGraph、C4A Dynamic Leader/HDWP、C3B feedback、
  C4A capture 类型/序列化、terminal mode preservation、10/16/2000 或 raw reassert abort。

不把编译、synthetic 或普通 owned 测试当成 modal PASS。

### 回归环境与复现命令

Windows 10.0.26200 / PowerShell 5.1 / VS 18 2026 x64 / MSVC 19.50 /
SDK 10.0.26100.0。沿用现有两套 build directory，CMake 自动重新生成；不改系统配置。

```powershell
$cmake = 'D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ctest = 'D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
$env:MSBUILDDISABLENODEREUSE='1'
& $cmake --build out/r1c4b-live-magnet-debug --config Debug --parallel 2 -- /nodeReuse:false
& $ctest --test-dir out/r1c4b-live-magnet-debug -C Debug --output-on-failure
& $cmake --build out/r1c4b-live-magnet-release --config Release --parallel 2 -- /nodeReuse:false
& $ctest --test-dir out/r1c4b-live-magnet-release -C Release --output-on-failure
# 每个 config 的匹配 build directory 中分别：
& "$build/src/platform/windows/$config/panebind-owned-window-harness.exe" --self-test
& "$build/src/platform/windows/$config/panebind-companion-harness.exe" --self-test
& "$build/$config/panebind-magnet-tests.exe"
& "$build/$config/panebind-magnet-gesture-tests.exe"
& "$build/src/platform/windows/$config/panebind-rect-adjustment-tests.exe"
& "$build/src/platform/windows/$config/panebind-magnet-postverify-tests.exe"
& "$build/src/platform/windows/$config/panebind-group-capture-tests.exe"
& "$build/src/platform/windows/$config/panebind-browser-sink-tests.exe"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c4a-capture-json.ps1 -BuildDirectory $build -Configuration $config
```

其它 fixtures 均由 `powershell.exe -NoProfile -ExecutionPolicy Bypass -File` 执行：
`test-r1c2b-evidence-runner.ps1`、`test-r1c3a-evidence-runner.ps1`、
`test-r1c3b-profile-runner.ps1`、`test-r1c3b-frame-profile-runner.ps1`、
`test-r1c3b-vdm-profile-runner.ps1`、`test-r1c4a-evidence-runner.ps1`、
`test-r1c4b-evidence-runner.ps1`、`test-r1c4b-postverify.ps1`。
只读原日志通过 `analyze-r1c4b-failure.ps1 -EvidencePath <原文件>` 复算；原 human
runner 的 `-ValidateEvidencePath` 分支仍正确返回 BLOCKED / exit 2。

| 回归 | 结果 |
| --- | --- |
| Debug / Release CTest | **27/27 / 27/27 PASS** |
| Owned Debug / Release | PASS / PASS，退出 0 |
| Companion Debug / Release | PASS / PASS，failures=0 |
| C2B / C3A runners | PASS / PASS |
| C3B Phase 1 / 2 / 3 runners | PASS / PASS（41）/ PASS（28） |
| C4A runner / capture / browser | PASS（140）/ 两配置 69 checks / 两配置 433 checks |
| C4A capture JSON serializer | 两配置各四种 synthetic stage PASS |
| Pure Magnet / live gesture | 两配置各 2691 / 168 checks PASS |
| Resize bridge / ordinary native probe | 两配置各 8 checks / 14 cases PASS（CTest） |
| 新 postverify unit | 两配置各 17 checks PASS |
| C4B evidence / postverify fixtures | PASS（29）/ PASS（14），均为 synthetic |
| Modal probe build / no-argument guard | Debug/Release build PASS；无参数退出 2、未启动窗口 |
| **真实 modal-loop probe** | **NOT_RUN / BLOCKED_REQUIRES_HUMAN**，独立于上述 PASS |
| **真实 Explorer 自动门禁** | **NOT_RUN，0/10 Debug + 0/10 Release** |

`git diff --check` PASS；受保护 Core/console/group/capture 文件无 diff。
本轮没有 independently reviewed 或 human acceptance 的冒称。

## Git 基线与收口

```text
Branch = codex/r1c4b-live-magnet
Starting HEAD = f53de7952816bf90550923c0edcd8d9784b247b3
AMENDMENT_BASE_SHA = f53de7952816bf90550923c0edcd8d9784b247b3
origin/main (standard origin fetch) = 81e40facf52ffdb96f76e4d737740167d485f4a7
Starting worktree = clean
Starting local/upstream = 0/0
```

正式 FINAL_HEAD、commit、standard push verification、最终 working tree 和 divergence
见本轮最终交接及对应 Git commit；不在提交内容中伪造 self-referential SHA。
未切换分支、改历史、改 transport 配置或操作 main。原日志 hash 与起点一致且 ignored。

## Gate / 未完成项

```text
R1C4B_HUMAN_FAILURE_FORENSICS = PASS
R1C4B_POSTVERIFY_DIAGNOSTICS = PASS
R1C4B_MODAL_LOOP_RESEARCH = PASS
R1C4B_MODAL_LOOP_OWNED_PROBE = BLOCKED_REQUIRES_HUMAN
R1C4B_SETWINDOWPOS_MOVE_MODAL_AUTHORITY = UNKNOWN
R1C4B_SETWINDOWPOS_RESIZE_MODAL_AUTHORITY = UNKNOWN
R1C4B_IMMEDIATE_POSITIONING_POSTVERIFY = UNKNOWN
R1C4B_IMMEDIATE_DWM_VISIBLE_POSTVERIFY = UNKNOWN
CURRENT_LIVE_MAGNET_NATIVE_ARCHITECTURE = UNRESOLVED
R1C4B_IMPLEMENTATION_READY = NO
R1C4B_HUMAN_UAT = NOT_READY
AUTO_EXPLORER_GATE = NOT_RUN
AUTO_EXPLORER_PRODUCT_GATE = BLOCKED_BY_ARCHITECTURE
AUTO_DEBUG_REPETITIONS = 0/10
AUTO_RELEASE_REPETITIONS = 0/10
PRODUCT_SENDINPUT_DEPENDENCY = NONE
PRODUCT_GLOBAL_MOUSE_HOOK = NONE
PRODUCT_GLOBAL_KEYBOARD_HOOK = NONE
PRODUCT_POLLING = NONE
PR = NO
MERGE = NO
TAG = NO
RELEASE = NO
```

Automated real Explorer driver、Move/Resize modal loop、positive-gap/overlap/
horizontal/XY Move、bottom/top Resize、relation formation/detach、Ctrl A/B/C、
fast suppression/slow reacquire 均 **NOT_RUN / NOT_IMPLEMENTED**，不能填 PASS，也不冒充
运行后的 FAIL。operation storm、postverify failure count、flaky rate、p50/p95、max queue
depth 均 **NOT_MEASURED**。没有自动 test 创建或控制第三方窗口，没有 human seal。
需要先恢复可用的受控 modal-loop 观察路径并完成独立审查；若证明 reassertion，先 REJECTED
并研究替代方案，而不是继续写入竞争。未经 architecture 放行不开始自动 Explorer gate；
未经同 SHA 自动 gate PASS + review PASS，不要求 Human Root 重测。
