# R1-C4B Fix 2 — Self-driving native modal probe

2026-09-16。**自驱动 EXE 已实现并从 shell 执行；真实 OS foreground gate 阻断，架构未定。**
本轮不依赖、不调用 computer-use；未要求 Human 拖动窗口。

## 基线与范围

```text
Branch = codex/r1c4b-live-magnet
Starting HEAD = aa037f0017bdc93d30227d959289d2059eff969c
Starting working tree = clean
Starting local/upstream = 0/0
Standard fetch origin = PASS
CODEX_COMPUTER_USE_REQUIRED = NO
```

保留 Fix 1 的诊断、人工日志和旧 `panebind-magnet-modal-loop-probe`，没有回退。
新增独立 `panebind-magnet-auto-modal-probe`、显式 runner、独立离线 validator 和 synthetic tests。
新 EXE 仅接收执行开关及 CREATE_NEW evidence path，不接收外部 HWND。
没有修改任何产品 Core、ExplorerLiveMagnetSession、Glue runtime、postverify policy、
参数 10/16/2000、旧 human runner hard block，也没有开始 Explorer provisioning。

## 实现的测试流程

1. WTS session Active/Unlocked + OpenInputDesktop/current desktop matching；
   初始前台非空。不会把 OpenInputDesktop 成功单独等同于已解锁或 RDP 已连接。
2. 三秒 console 倒计时，Esc 取消；保存 cursor；新建一个 own process/UI thread 的空白窗口。
3. 主屏工作区中央附近 deterministic setup，为 +180 X、+120 bottom 及 +7 pulse 留出
   100 px margin。ShowWindow、SetForegroundWindow、SetFocus 后验证 exact foreground。
4. bounded WM_NCHITTEST 扫描 HTCAPTION/HTBOTTOM；单独 driver thread 发 20 个绝对
   mouse move samples，one-shot waitable timer 30 ms test-input pacing。
   这是有限输入轨迹，不是产品 polling/event source、sleep 或 native placement retry。
5. UI thread 只由真实 WM_ENTERSIZEMOVE、WM_MOVING/SIZING、WM_EXITSIZEMOVE 驱动。
   首次 drag callback 后 posted 私有消息执行一次 SetWindowPos pulse；不修改 native
   drag RECT，不伪造 WM_MOVING/SIZING。记录 T0/T1/T2/T3、原始 proposed RECT 和双矩形。
6. 输入前检查 desktop/identity/foreground、预期 cursor/button/modifiers；API 边界再次
   验证 foreground。正常完成恢复 cursor；异常不强制恢复。Cleanup 仅释放已确认仍在
   own foreground/desktop 的测试按键、销毁自己的空窗口；不关闭其他应用。

Schema：`r1c4b-auto-owned-modal/v1`；`human_input=false`、`real_explorer=false`。
Nominal cadence 标为 `planned_duration_ms`，不伪装成实测时长。没有用户输入内容日志。
该 integration EXE 不注册进 CTest；必须显式启动。

## 实际执行证据（全部保留 ignored local 文件）

目录：`uat/r1c4b-auto-owned/`。

| 运行 | JSONL 文件名 | SHA256 |
| --- | --- | --- |
| Debug 开发验证 1 | `20260916T033641473Z-Debug-6798c2a2acbc4e649c6e6a1a179fd464-1.jsonl` | `998C2FBF663118646085C0F4FEA188FBC8C020D74A8B061204A1E4AC0B0787A0` |
| Debug 开发验证 2（增加 foreground 可见性取证） | `20260916T033819525Z-Debug-ea88fcd36b50471e9114515c324b1ca1-1.jsonl` | `C9609DCED99DA9F85E7EB4D54EFCD3509863334EAFAEE790ECD8DEC3BF6C9F84` |
| Release 正式批次第 1/20 | `20260916T034016572Z-Release-911264932adb449e80f8caeacc64766b-1.jsonl` | `862947EF7221AECB68D0DC5848D7EFBC894FA5417203465C68D9ABEC5C7E8552` |

对应 `.aggregate.json` 保存 binary hash、执行时 HEAD、dirty 标记、逐次原始文件 hash、
退出码和分类。开发阶段 worktree dirty 为真实记录，不能宣称这些日志来自最终 clean SHA。

三次 EXE 均退出 **2 / BLOCKED_BY_FOREGROUND**；所有日志合法、sequence 连续、
startup/shutdown 完整；所有自建窗口已销毁。三次各自：

```text
desktop active/unlocked = true
input desktop matches = true
SendInput records = 0
modal ENTER = 0
native corrections = 0
```

第二次 Debug：`visible=true`、`startup_flags=0`、`SetForegroundWindow=false`，
target HWND=1772986，foreground HWND=3607370。
Release：`visible=true`、`startup_flags=0`、`SetForegroundWindow=false`，
target HWND=2428346，foreground HWND=3607370。
因此“窗口被启动参数隐藏”假设不成立。实际是 Windows 拒绝把 test window 变为前台。
该情况下按任务书必须 **NO INPUT / STOP**，不是尝试点击目标或注入 Alt 来取得焦点。
没有 AttachThreadInput、修改 foreground lock/system settings 或调用 GUI 工具兜底。

两次 Debug 开发尝试没有成功，也没有被从报告剔除；第二次用于精确诊断而非重试直到 PASS。
Release 默认 20 次批次在首次安全门禁失败后停止，后 19 次未运行。Debug 正式 20 次没有
启动（同一前台限制已确认）；不能将开发尝试计入正式重复成功数。
`cursor_restored=false` 是因为输入尚未开始，没有需要撤销的测试 cursor movement，
不是一次成功拖动后忘记恢复。没有操纵既有 Explorer、其他应用或用户文件。

## 证据分类与当前限制

离线 validator 独立重算 +7 pulse target、raw trajectory、T1 exact flags、T2 proposed、
T3/final geometry，区分 STABLE / REASSERTED / IMMEDIATE_REJECTED /
DWM_ASYNC_ONLY / UNKNOWN。STABLE 还要求后续所有 observed drag RECT 一致保留 pulse。
它验证 source HWND/PID/TID、schema、顺序、单次 correction、输入 path、fresh fences、
Move/Bottom Resize 类型、工作区 margin 和跨 gesture 连续性；不信任 harness 的 PASS。

**本轮没有真实 T0/T1/T2/T3，所以 Move/Resize authority 均 UNKNOWN。**
不可将三次 foreground block 推断成 modal reassertion、DWM lag 或 SetWindowPos 不可行。
也不可声称真实鼠标驱动、hit-test 正例、native lifecycle、cursor restoration 已验证。
尚无完整 repetition 可计算 modal flaky rate，标记 NOT_MEASURED；前台 gate 阻断为 3/3。

## 自动回归

环境：Windows 10.0.26200，PowerShell 5.1，VS 18 2026 x64 / MSVC 19.50，
Windows SDK 10.0.26100.0。已有 Debug/Release build 目录自动重新生成；parallel=2，
仅本进程 `MSBUILDDISABLENODEREUSE=1`，`/nodeReuse:false`。

主要命令（本轮实际执行，不是下一次 Human UAT 命令）：

```powershell
$cmake = 'D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ctest = 'D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
& $cmake --build out/r1c4b-live-magnet-debug --config Debug --parallel 2 -- /nodeReuse:false
& $ctest --test-dir out/r1c4b-live-magnet-debug -C Debug --output-on-failure
& $cmake --build out/r1c4b-live-magnet-release --config Release --parallel 2 -- /nodeReuse:false
& $ctest --test-dir out/r1c4b-live-magnet-release -C Release --output-on-failure
# 已执行的自驱动尝试：
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/run-r1c4b-auto-owned-modal.ps1 -Configuration Debug -Repetitions 1 -DevelopmentRun
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/run-r1c4b-auto-owned-modal.ps1 -Configuration Release -Repetitions 20
```

每配置另执行 Owned/Companion `--self-test`、C4A capture JSON serializer。
C2B、C3A、C3B Phase1/2/3、C4A、C4B 与新增 auto-owned validator fixtures
均通过 `powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-*.ps1` 显式执行。
具体脚本：`test-r1c2b-evidence-runner`、`test-r1c3a-evidence-runner`、
`test-r1c3b-profile-runner`、`test-r1c3b-frame-profile-runner`、
`test-r1c3b-vdm-profile-runner`、`test-r1c4a-evidence-runner`、
`test-r1c4b-evidence-runner`、`test-r1c4b-postverify`、`test-r1c4b-auto-owned-modal`。

| 回归 | 最终结果 |
| --- | --- |
| Debug / Release build + CTest | PASS；**27/27 + 27/27** |
| Owned Debug / Release | PASS / PASS，failures=0 |
| Companion Debug / Release | PASS / PASS，failures=0 |
| C2B / C3A runner | PASS / PASS |
| C3B Phase1 / Phase2 / Phase3 runner | PASS / PASS（41）/ PASS（28） |
| C4A runner | PASS（140） |
| C4A capture JSON | 两配置各四种 stage PASS；mutable capture/console/group tests 均在 CTest PASS |
| Pure Magnet / C4B gesture / Resize bridge / postverify unit | 两配置 CTest PASS |
| C4B evidence / postverify fixtures | PASS（29）/ PASS（14） |
| 新 auto-owned classifier / full evidence fixtures | PASS（41，synthetic only） |
| 新 EXE 无参数 / external-HWND 参数拒绝 | 两配置退出 2，不启动 UI |
| **真实 owned Move / Resize** | **未发生，前台 gate 阻断，不计 PASS** |

`git diff --check` PASS；产品 Core/Explorer/console、Fix 1 诊断与 human runner 无 diff。
原真人 JSONL SHA256 仍为 `8F008F8756C828C83370AECDFEA8A136859FEC06CDF00240A4F1157A845E7317`。
所有本地新旧原始日志保留、ignored、不提交。
最后只增加了输入 API 边界再验证和 planned-duration 标识；未再次运行已知会被前台 gate
阻断的输入尝试。最终两个 probe binary 已重新构建，正例路径仍明确 NOT TESTED。

## 最终 gate

```text
AUTO_OWNED_MODAL_DRIVER = BLOCKED
AUTO_OWNED_BLOCKER = BLOCKED_BY_FOREGROUND
AUTO_OWNED_MOVE_REPETITIONS = Debug 0/20; Release 0/20
AUTO_OWNED_RESIZE_REPETITIONS = Debug 0/20; Release 0/20
OWNED_MOVE_MODAL_AUTHORITY = UNKNOWN
OWNED_RESIZE_MODAL_AUTHORITY = UNKNOWN
AUTO_EXPLORER_BOOTSTRAP = NOT_RUN
EXPLORER_MOVE_MODAL_AUTHORITY = UNKNOWN
EXPLORER_RESIZE_MODAL_AUTHORITY = UNKNOWN
CURRENT_LIVE_MAGNET_NATIVE_ARCHITECTURE = UNRESOLVED
AUTO_EXPLORER_FULL_GATE = BLOCKED_BY_ARCHITECTURE
AUTO_DEBUG_REPETITIONS = 0/10
AUTO_RELEASE_REPETITIONS = 0/10
R1C4B_IMPLEMENTATION_READY = NO
R1C4B_HUMAN_UAT = NOT_READY
CODEX_COMPUTER_USE_REQUIRED = NO
PRODUCT_SENDINPUT_DEPENDENCY = NONE
PRODUCT_GLOBAL_MOUSE_HOOK = NONE
PRODUCT_GLOBAL_KEYBOARD_HOOK = NONE
PRODUCT_POLLING = NONE
PR = NO
MERGE = NO
TAG = NO
RELEASE = NO
```

未获 owned gate 放行，不进入 Explorer bootstrap/full gate，也不设计替代产品架构。
现有 human runner hard block 完全保留。下次继续所缺的是可获 exact foreground 的显式
测试执行环境，不是让 Human 手工拖动或再次替产品查找 runtime failure。
Final SHA、标准 push/remote SHA 校验、工作树及 divergence 在最终交接给出，
避免在提交内伪造 self-referential SHA。
