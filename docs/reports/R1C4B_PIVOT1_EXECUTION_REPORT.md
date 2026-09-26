# R1-C4B Architecture Pivot 1 — 自动 authority 研究收口

2026-09-26。候选 `CancelNativeLoopAndTakeOverInput = UNRESOLVED`。
三次 Debug 开发观察停在 owned 输入/activation 前置条件；**没有执行 native
cancel，也没有任何 takeover geometry write**。不是已证明取消失败。
旧 `ConcurrentNativeLoopCorrection = REJECTED` 及 Human UAT hard block 不变。

## 基线与范围

```text
Branch = codex/r1c4b-live-magnet
Starting HEAD = 070b05a8f44c8b08503b3643f838a83703495987
Starting working tree = clean
Starting local/upstream = 0/0
Starting origin/main...HEAD = 0 behind / 15 ahead
Standard fetch origin = PASS
origin/main = 81e40facf52ffdb96f76e4d737740167d485f4a7
origin = ssh://git@ssh.github.com:443/zynis/panebind.git
github-https = https://github.com/zynis/panebind.git
```

仅独立 Windows test EXE、显式 cancel-only runner、离线 validator/fixtures、
CMake test target 及研究/架构/报告文档。没有产品 runtime/Core math 修改。
C4A dynamic leader、HDWP、Relation Graph、Pure Magnet candidate math、Fix 1
diagnostics、旧 Fix 3 CPP/validator/runner/原始日志、Human runner 均未修改。
没有创建新分支，没有 Human 操作、computer-use 或真实 Explorer 试验。

研究先于实现：[prior art/history/license](../research/R1C4B_PIVOT1_PRIOR_ART.md)、
[官方 input/cancel 合约](../research/R1C4B_PIVOT1_INPUT_CONTRACTS.md)、
[provenance](../research/SOURCE_PROVENANCE.md) 已记录实际 inspected 范围和缓存
失败边界。AltSnap GPL reference-only，FancyZones MIT reference-only；没有外部
代码复制、改写、翻译或适配。研究 gate 仅适用于 owned probe，不授予产品 READY。

## IMPLEMENTED：独立 cancel-only probe

`panebind-magnet-takeover-probe` / `r1c4b-takeover-owned/v1`。
空 source/guard 属于 parent exact PID/TID；同 EXE 的独立 hidden child 只拥有
message-only receiver，不接收 target HWND 或 geometry authority。
Mouse TLC 1/2、INPUTSINK=256，读回注册；未注册 keyboard、NOLEGACY、CAPTUREMOUSE。
实际 WM_INPUT 读取后 DefWindowProc cleanup，movement 触发一次 GetCursorPos，
button transition 与 event-triggered async snapshot 分开记录；不积分 raw 坐标。
2048 packet/4096 log budget、有限64-message drain、pipe reader、message/event wait；
无 observation timer。唯一 timer 用于有限自驱输入 pacing。

先验证完整后台 Raw movement/UP，再允许真实 title Move 与 Bottom Resize。
后者实现了仅一次 bounded WM_CANCELMODE、capture/EXIT/第三个既定 sample
observation wait、余下15+ samples 稳定性检查，但本轮未实际达到这部分。
geometry 比较保留 cancel-return baseline，不由 confirmation 重新锚定；明确
native DRAG/定位反例将立即停止整个 driver，不继续 Resize 或下一层。
所有 takeover、Magnet、Explorer 写入阶段均未实现，非 disabled-ready 功能。

输入只作用于新建空窗的严格 identity/foreground/desktop/root/cursor/button/
GUI capture/menu/MoveSize fence；按钮 API 边界另做 fresh reconcile。
不伪造 raw/native/activation 消息，不改变 drag RECT。失败停止 Raw armed scope，
cleanup 不补成成功 END；只清理空 source/guard/receiver，不关闭用户应用。

## AUTOMATED OBSERVED：三次开发证据

均保存在 ignored `uat/r1c4b-takeover-owned/`，未进入 Git。
三次 ExecutedHEAD/AfterHEAD 均为起始 `070b05a...`、WorktreeDirty=true。
每次 binary 前后 hash 相同；这些是带明确 binary hash 的开发观察，不冒充
clean implementation SHA 的正式 20/20 acceptance。前三次原记录没有追改。

| 尝试 | 记录数 | 真实结果 | Native ENTER / DRAG / cancel / EXIT | Takeover writes |
| --- | ---: | --- | --- | ---: |
| Debug 1 | 22 | BLOCKED_BY_RAW_INPUT_CORRELATION | 0 / 0 / 0 / 0 | 0 |
| Debug 2 | 23 | BLOCKED_BY_ACTIVATION_FAILURE | 0 / 0 / 0 / 0 | 0 |
| Debug 3 | 14 | BLOCKED_BY_LOCAL_ACTIVATION_PREPARATION | 0 / 0 / 0 / 0 | 0 |

### Debug 1：真实 Raw receipt，但 callback cursor 尚未更新

文件 `20260926T074218514Z-Debug-f3ce2ff107c6448193a110520f4e152f.jsonl`。

```text
Log SHA256 = 9D5D08FDA9D0B39E25CBED39803945AF7372989975811AD0115D781EDAEA8B7A
Binary SHA256 = DDBFA484F80CE87FDC4E8716E022DC3A4B2A0559B553A07C5AE96E0459294739
Source HWND / PID / UI TID = 4785818 / 46784 / 12648
Receiver HWND / PID / TID = 4784506 / 13452 / 31944
Actual Raw packets = 1; RIM_INPUTSINK = 1; raw flags = 11
Actual raw normalized coordinates = [30858,29849]; test tag matched
Raw GetCursorPos = [2268,850]; planned point = [1446,874]
Raw receiver QPC = 682649365672
SendInput start / return QPC = 682649358162 / 682649398946
Later legacy MOVE QPC = 682649422688; screen point = [1446,874]
Probe exit = 2; runner result = BLOCKED
```

实际后台送达被观察到；“同 callback cursor 必须已经等于 submitted input point”
假设有反例。原日志仍 BLOCKED，不追认 PASS。随后仅修正 test delivery 关联为
实际 INPUT flags/normalized coordinates/tag/API QPC/fresh receiver watermark，
保留真实 snapshot lag；Raw UP transition 独立于 async cross-check。
既有有限 pacing 后的 fresh fence 仍要求实际 cursor/button 正确。
这不证明未来 cursor-derived writer 的及时性；禁止用 raw normalized 值驱动 geometry。
首次外层 PowerShell 未显式传 runner exit，返回1；不能把 wrapper1写成 probe1。

### Debug 2：真实点击改变 global foreground，但缺少新 activation callbacks

文件 `20260926T081719278Z-Debug-4bba552a333f493d8a4f9afeee2cc8f2.jsonl`。

```text
Log SHA256 = 0ECE70E9702A3EB3C9EFB5F462C27D91120D296832827D53A5208EA165B0C223
Binary SHA256 = D5626D1E78973A4AEB9C532B13B61431C33729865C5271A9127450E0C2D3CEC3
Source HWND / PID / UI TID = 13238740 / 19864 / 22224
Receiver HWND / PID / TID = 78252802 / 11972 / 45568
Ordinary SetForegroundWindow = false; initial global foreground = 854960
Verified activation move/down/up sent = 1 / 1 / 1
Actual source WM_LBUTTONDOWN / UP = 1 / 1
Final global foreground = source
Epoch1 WM_ACTIVATE / WM_SETFOCUS = 0 / 0
Raw packets = 0; Probe exit = 2; runner result = BLOCKED
```

显示期间已有 epoch0 activation/focus，且当时 foreground_matches=false；
不能拿它们补证后续 click。临时 topmost 已恢复。该 run 没有 local readback，
本线程状态解释最初只是候选假说，而非已证明根因。

### Debug 3：非激活显示后，普通 setter 仍建立 local 状态；NULL active 清理无效

文件 `20260926T084255946Z-Debug-ccc8ec31a1af4d08983ab3ca53d2d4f7.jsonl`。

```text
Log SHA256 = 0F3EC5BACEB857986CD8EC7666D67D14A4F5AA5446D55D9DF024E0666CFAAA55
Binary SHA256 = BA1994465C53741AEBAA050D8D869E70063BC8DDD1293621A19EC9F7A667C999
Source HWND / PID / UI TID = 39783966 / 13088 / 15860
Guard HWND = 27265322
Receiver HWND / PID / TID = 163252994 / 10036 / 35088
Requested ShowWindow mode = SW_SHOWNOACTIVATE (4); visible = true
Ordinary SetForegroundWindow = false
Global foreground before / after local preparation = 133064 / 133064
Local active before / after = 39783966 / 39783966
Local focus before / after = 39783966 / 0
SetFocus(NULL) prior-handle / diagnostic error = 39783966 / 0
SetActiveWindow(NULL) return / diagnostic error = 0 / 0
Local readback cleared = false; preparation success = false
SendInput / Raw / native / cancel / takeover = 0 / 0 / 0 / 0 / 0
Probe / runner exit = 2 / 2; result = BLOCKED
```

清理仅在 owner UI thread、已验证 own local handles、同一外部 foreground、
无输入/capture/menu/MoveSize 时执行。未赋 source focus/active、AttachThreadInput
或修改 foreground entitlement。NULL/error0 不是成功保证，实际 active 未清空，
因此在 click 前 fail closed。没有追加激活绕过、无限重试或更换测量边界。
这否定了本次 local-clear bootstrap 候选，不否定未执行的 native cancellation。

### 完整性与 cleanup

三份 JSONL 均合法、startup/shutdown 完整、sequence 连续、QPC 非递减。
Receiver mouse registration 全部安全移除，receiver/source/guard 全部销毁，
没有 overflow、drop、Raw read/pipe error 记录；未触及用户预存窗口。
不能由短失败日志推导长期队列/idle/输入性能已验收。
失败记录 cursor_restored=false；第一、二次不在失败后的未知 authority 下强行
恢复 pointer，第三次没有鼠标输入。未把 cleanup UP 当正式 raw END。

旧 Fix 3 原始 JSONL SHA256 仍为
`1CCC89BC9DB54317AE5531B8D822D715EA80EAB9EF8108B1F7B121CDA9D2DF5B`。

## 四级 verdict 与停止点

| 层级 | Move | Bottom Resize | 原因 |
| --- | --- | --- | --- |
| Owned cancel | UNKNOWN | UNKNOWN | 尚未达到真实 native START/cancel |
| Owned free takeover | NOT_RUN | NOT_RUN | complete cancel gate 未通过 |
| Explorer cancel | NOT_RUN | NOT_RUN | owned free/Magnet 前置未通过 |
| Explorer takeover | NOT_RUN | NOT_RUN | Explorer cancel 尚未执行 |

`OWNED_RAW_INPUT_BACKGROUND=BLOCKED` 表示完整后台 input prerequisite 未通过，
不是宣称 WM_INPUT 从未送达。已有一次真实 packet，缺完整 UP/连续手势。
没有明确 native-cancel counterexample，不能输出 REJECTED_AT_CANCEL_STAGE。
后续需重新研究 owned foreground/local-state 与真实 activation evidence contract；
不能以历史 callbacks 或 SendInput/setter success 冒充真实 gate。

```text
CURRENT_CONCURRENT_NATIVE_LOOP_ARCHITECTURE = REJECTED
OWNED_RAW_INPUT_BACKGROUND = BLOCKED
OWNED_NATIVE_CANCEL_MOVE = UNKNOWN
OWNED_NATIVE_CANCEL_RESIZE = UNKNOWN
OWNED_TAKEOVER_MOVE = NOT_RUN
OWNED_TAKEOVER_RESIZE = NOT_RUN
OWNED_TAKEOVER_MODAL_REASSERTIONS = 0
OWNED_TAKEOVER_PRE_RELEASE_CONTROL = FAIL
OWNED_MAGNET_TAKEOVER = NOT_RUN
EXPLORER_NATIVE_CANCEL_MOVE = NOT_RUN
EXPLORER_NATIVE_CANCEL_RESIZE = NOT_RUN
EXPLORER_TAKEOVER_MOVE = NOT_RUN
EXPLORER_TAKEOVER_RESIZE = NOT_RUN
RAW_INPUT_TAKEOVER_ARCHITECTURE = UNRESOLVED
WH_MOUSE_LL_FALLBACK = TECHNICALLY_POSSIBLE
C_SNAP_ON_RELEASE = UNSELECTED_FALLBACK
R1C4B_HUMAN_UAT = NOT_READY
PRODUCT_DLL_INJECTION = NONE
PRODUCT_POLLING = NONE
PRODUCT_GLOBAL_MOUSE_HOOK = NONE
PR = NO
MERGE = NO
TAG = NO
RELEASE = NO
```

上面的 reassertions=0 是未运行 takeover 的空计数，不是 sole-writer PASS。
PRE_RELEASE_CONTROL=FAIL 表示验收门未达到，**非已观察到 pre-release 写入失败**；
实际写入测试 NOT_RUN。WH_MOUSE_LL 仅官方说明下 technically possible 的研究候选：
installed-thread、non-injected、event-driven、shared-desktop resource，尚未实现/实测。
没有把它等同 DLL injection，也没有选用 release-only Snap 降级产品目标。

## AUTOMATED TESTED 与 NOT TESTED

工具链：Windows 主机、PowerShell 5.1.26100.9444、VS Community 18 MSBuild
18.5.4、CMake 4.2.3-msvc3、C++20 / PMv2。以下 `cmake`/`ctest` 使用
`D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\`。

```powershell
$env:MSBUILDDISABLENODEREUSE='1'
cmake --build out/r1c4b-live-magnet-debug --config Debug --parallel 2 -- /nodeReuse:false
cmake --build out/r1c4b-live-magnet-release --config Release --parallel 2 -- /nodeReuse:false
ctest --test-dir out/r1c4b-live-magnet-debug -C Debug --output-on-failure
ctest --test-dir out/r1c4b-live-magnet-release -C Release --output-on-failure
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c4b-auto-owned-modal.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c4b-takeover-owned.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/run-r1c4b-takeover-owned.ps1 -Configuration Debug
```

Debug/Release full build PASS；现有 Host CTest **28/28 各 PASS**（read-only VDM
需主机上下文，未以 sandbox 环境假阴性替代结果）。旧 Fix 3 synthetic **144 PASS**；
新 validator synthetic **107 PASS**，均仅测试证据 policy，不是 empirical acceptance。
新 probe 不加入默认 CTest，不由普通回归启动全局鼠标输入。
第三次交互后仅回归/安全 build identity 检查，不再运行 interactive probe。

NOT TESTED：owned native cancel、连续 Raw/UP、free takeover Debug/Release20/20、
sole writer/pre-release geometry、owned Magnet 五类场景、test-created Explorer
cancel/free/Magnet、physical mouse/precision touchpad、多显示器/mixed DPI、
focus/capture 竞争、idle CPU/内存与长时间 input throughput。未暗示任何层已 PASS。

## Git 收口

本报告与独立 probe 以有意义提交收口，标准 transport push 当前既有分支。
不改历史、不 amend/rebase/reset/force，不创建 PR/merge/tag/release。
本轮最终 commit SHA、标准 push verification、最终 clean/local-upstream divergence
在交付时报告；原始 `uat/` 不纳入提交。此文件不是可自行填写的 runtime acceptance。
