# R1-C4B Architecture Pivot 1：Raw Input 与 native cancel 合约

审阅日期：2026-09-26。授权基线：
`070b05a8f44c8b08503b3643f838a83703495987`；分支
`codex/r1c4b-live-magnet`。本文件记录官方合约及由合约形成的 probe
约束；它不把文档保证替换成 PaneBind 的实测结果。实施与执行 verdict
须以本轮独立 probe、原始日志、离线验证和报告为准。

`ConcurrentNativeLoopCorrection = REJECTED` 是已接受的历史结论。
`CancelNativeLoopAndTakeOverInput` 是本轮待验证假设。取消 API 成功、
收到 raw packet、owned 自动测试或 upstream 行为各自不能独立证明整个
候选架构有效。不得改变 Fix 3 probe 的基准行为。

## Fix A 当前覆盖合约：撤销 false local-activation gate

2026-09-26；Fix A starting HEAD：
`94242959568b28d782d1a89a6f78c95c2dd8b098`。用户 Fix A brief 明确覆盖
旧 probe 的 local-clear 和 fresh activation/focus callback 前提。
删除或停止调用 `prepare_local_activation()` 已授权；不再调用
`SetActiveWindow(NULL)`，不把 `SetFocus(NULL)`、local active/focus 为 NULL、
本 epoch 新的 `WM_ACTIVATE/WM_SETFOCUS` 作为任何接管实验的前置 gate。
以下三次旧日志及旧设计记录只保留历史；不能据本节改判或追认旧 run。

Fix A 重新实际阅读的 primary contracts：

| 官方资料 | 当前准确结论 |
| --- | --- |
| [GetActiveWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getactivewindow) | Calling-thread message queue 的 local active fact，不是 foreground authority。 |
| [GetForegroundWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getforegroundwindow) | 返回用户当前工作的 foreground HWND；activation 变化时可能为 NULL。Probe 以 fresh exact source HWND、PID/TID 检查建立 global fact，NULL 不通过。 |
| [SetActiveWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setactivewindow) | 用于激活 calling queue 中的指定 top-level HWND；`SetActiveWindow(NULL) = NOT A DOCUMENTED CLEAR-ACTIVE CONTRACT`。不应从参数不属于 calling thread 的一般文字推导 NULL 是可靠 clear 操作。 |
| [SetFocus](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setfocus) | NULL 合法，清 keyboard focus、使 keystrokes ignored；这种操作与 `RIDEV_INPUTSINK` background eligibility 无关，Fix A 不用它作 prerequisite。 |
| [GetFocus](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getfocus) | Calling-queue keyboard focus fact；另一个队列可能仍有 focus。仅记录 bounded diagnostic，不能 gate Raw Input background。 |
| [RegisterRawInputDevices](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerrawinputdevices) | 需先成功注册，process/device class 的最后一个 exact target 生效；不要求另一个 source thread 清 local active/focus。 |
| [RAWINPUTDEVICE](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-rawinputdevice) | `RIDEV_INPUTSINK` 允许 caller 不在 foreground 时接收，要求非空 exact `hwndTarget`。本轮独立 receiver mouse TLC `0x01/0x02`、flags `256`，无 NOLEGACY/CAPTUREMOUSE。 |
| [Raw Input Overview](https://learn.microsoft.com/en-us/windows/win32/inputdev/about-raw-input) | 注册后实际 `WM_INPUT` 是事件来源；foreground/background delivery 与另一个 source queue 的 local focus 值没有上述 NULL 先决条件。 |

Source thread 的字段名为 `source_thread_local_active` 和
`source_thread_local_focus`；在真正 owner UI thread 或 exact source TID
查询后保存，valid HWND/NULL 均只是 diagnostic。其非 NULL 不阻止
bootstrap、raw preflight 或改变 verdict；不让 driver 的空 queue 查询
冒充 source-thread fact。

当前 probe bootstrap 验收只要求普通 SetForegroundWindow 之后的 fresh
exact source global foreground、identity、desktop/session、visible、无
foreign capture/menu/MoveSize 和正确键钮状态。若普通调用失败，允许已
授权的 strict verified click：move/down/up 成功、exact source 实际收到
intended click DOWN/UP、最终 global foreground 为 source、temporary
topmost 恢复、fresh identity/input fences 通过。Activation/focus callbacks
保留诊断，出现或缺失都不代替这些实际 checks，也不额外否决它们。
不引入 AttachThreadInput、Alt trick、foreground-lock 修改或新 local-clear
API 手段；Fix 3 的旧实现/fixtures 保持其原有证据意义。

Raw background proof 来自 exact registered receiver 和实际 packet：
`GetForegroundWindow() == exact source HWND`、foreground PID 为 source PID、
receiver PID 与两者均不同，mouse-only message HWND 注册正确，实际 input
code 为 `RIM_INPUTSINK`。完整 preflight PASS 还需真实 movement 和 raw
LEFT_BUTTON_UP、receiver sequence/order、actual submitted normalized INPUT/
tag/QPC/watermark 关联、最后 fresh cursor/button fence。不能仅凭注册、
一条 packet 或 local diagnostic 通过；不得恢复 same-callback cursor
必须等于 submitted point 的错误假设。

`OFFICIAL_CONTRACT_REVIEW = PASS` 和沿用的
`PRIOR_ART_SOURCE_LICENSE_HISTORY = PASS` 只授权独立 test-owned probe
取得事实。沿用 [Pivot 1 prior art](R1C4B_PIVOT1_PRIOR_ART.md) 中 AltSnap
GPL reference-only pin、FancyZones MIT reference-only pin 及已读 history，
没有新增上游源码 inspection 声称，没有复制、翻译、改写或适配 GPL/MIT
实现。该 gate 不预填 native cancel、free takeover 或产品 PASS。

Fix A 顺序仍是 raw preflight → 真实 owned Move/Bottom Resize → 各一次
WM_CANCELMODE → 完整 cancellation 证据。一个可靠完整 counterexample
可以在 cancel stage 拒绝并 STOP；孤立 timeout 或 missing witness 不能
冒充完整反例。只有两个 cancel 都通过并完成 Debug/Release 各 20/20，
才进入 owned free takeover，随后各配置 Move/Resize 20/20。Writer 始终
晚于真实 EXIT，由实际 WM_INPUT 唤醒 bounded owner quantum、event-triggered
GetCursorPos、冻结 anchor、最多一次 SetWindowPos 和 exact postverify。
Background PASS 不证明 cursor-derived writer，Magnet/Explorer 仍受后续
阶段边界约束。

`WH_MOUSE_LL_FALLBACK = TECHNICALLY_POSSIBLE` 沿用已有官方研究，仅候选、
本轮不实现。`PRODUCT_RAW_INPUT = NOT_IMPLEMENTED`、product SendInput/hook/
DLL injection/polling 均不由本研究引入；Human UAT 继续 NOT_READY。

## 已实际阅读的强制 Raw Input 官方资料

| 官方资料 | 合约及本轮约束 |
| --- | --- |
| [Raw Input Overview](https://learn.microsoft.com/en-us/windows/win32/inputdev/about-raw-input) | 应用需注册设备；输入通过 `WM_INPUT` 进入消息队列。`RIDEV_INPUTSINK` 支持后台接收。标准读取使用消息的 `lParam`；当前消息被 `GetMessage` 移除后，`GetRawInputBuffer` 不包含当前 packet。因此事件处理不可只 drain buffer 而跳过当前 `lParam`。 |
| [RegisterRawInputDevices](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerrawinputdevices) | 检查 BOOL 与失败错误；`cbSize = sizeof(RAWINPUTDEVICE)`。同一进程、同一 device class 仅最后注册的一个窗口接收。独立 probe 持有明确注册 owner，禁止通用库悄悄覆盖注册。 |
| [RAWINPUTDEVICE](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-rawinputdevice) | `RIDEV_INPUTSINK` 必须给 exact `hwndTarget`。本轮只注册 mouse TLC。`RIDEV_NOLEGACY` 抑制该应用的 legacy 消息；`RIDEV_CAPTUREMOUSE` 改变点击激活且要求 NOLEGACY；两者均禁止。本轮也不使用只在 foreground 未处理 raw 时才接收的 `RIDEV_EXINPUTSINK`。移除注册用 `RIDEV_REMOVE` 且 `hwndTarget = NULL`。 |
| [WM_INPUT](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-input) | 用 `GET_RAWINPUT_CODE_WPARAM` 区分 `RIM_INPUT` 和 `RIM_INPUTSINK`；`lParam` 是 `HRAWINPUT`。`RIM_INPUT` 必须调用 `DefWindowProc` 完成系统 cleanup。数据应先读取，再 cleanup；失败读取也不能跳过必要 cleanup。处理该消息返回零。 |
| [RAWMOUSE](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-rawmouse) | `MOUSE_MOVE_RELATIVE` 是零值，不能用按位与它检测移动。记录 `usFlags`、`lLastX/lLastY` 和 `usButtonFlags`；相对、绝对、virtual-desktop 坐标含义不同。绝对值是 0–65535 的归一化值。Raw movement 不受 Control Panel mouse speed 的相同处理，不能直接积分为 screen cursor 位移。Button flags 描述 transition，不是持续按下状态。默认可 coalesce movement。 |
| [GetRawInputData](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getrawinputdata) | `RID_INPUT` 读取当前 packet，`cbSizeHeader = sizeof(RAWINPUTHEADER)`；buffer 必须 DWORD 对齐。空 buffer 查询返回所需大小；成功读取返回复制字节数，失败返回 `(UINT)-1`。校验实际长度、header 类型与 packet 大小，拒绝截断/错误数据，不能把未初始化结构写为观察结果。 |

补充阅读 [Using Raw Input](https://learn.microsoft.com/en-us/windows/win32/inputdev/using-raw-input)：
mouse TLC 明确为 usage page `0x01`、usage `0x02`；其示例支持
`HWND_MESSAGE` target。标准 read 明确要求在 `DefWindowProc` 前读取。
同页另有 periodic buffered-read 示例；其 16 ms timer 不符合本轮约束，
不得照用。本轮只采用由真实 `WM_INPUT` 唤醒的标准读取思想，不复制代码。

[Window Features / Message-Only Windows](https://learn.microsoft.com/en-us/windows/win32/winmsg/window-features)
说明 message-only window 不可见、无 z-order、不接收广播，适合独立
receiver；它仍需所属线程正常消息循环。以上合约不保证 native modal
loop 内嵌 dispatch、消息时序或 SendInput 在本机产生 raw packet，这些
仍须实测。

实际补充阅读
[MsgWaitForMultipleObjects](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-msgwaitformultipleobjects)
与
[MsgWaitForMultipleObjectsEx](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-msgwaitformultipleobjectsex)：
Peek/GetMessage 会把队列中看到的消息标为 old。普通 MsgWait 对这些尚未
移除的输入不一定再次唤醒，因此有限 drain 后不能假定下一次无限等待
会立刻看到余量。`MWMO_INPUTAVAILABLE` 明确允许 existing input 唤醒，
适合 bounded drain 后继续阻塞等待的模式；没有输入时仍等待，不需要
timer/poll。此为已读官方合约及代码审查结论，尚无本轮 burst/overflow
的原生自动观察，不把它写成已测 throughput 或可靠性保证。

## Cursor、button 与合并边界

[GetCursorPos](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getcursorpos)
返回调用时当前 screen cursor；需检查 BOOL，调用线程需位于 input
desktop，进程需 window-station read attributes 权限。每个实际 mouse
movement `WM_INPUT` 到达时允许一次观察。记录 raw movement 与 current
cursor 各自的数据；二者不是官方保证的同一历史 sample。

[GetAsyncKeyState](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getasynckeystate)
只使用 high bit 作 event-triggered cross-check，不能使用不可靠的
“最近按过” low bit。零也可能表示不可访问 desktop/foreground。该 API
读取 physical button，按钮交换配置与逻辑按钮含义应分开记录；它不是
`RI_MOUSE_LEFT_BUTTON_UP` witness 的替代品。不得 timer 查键，也不得
用一次 cross-check 推导整个手势都保持按下。

本轮设计决策（从上述合约推导，不是 Windows 的额外保证）：

1. 接收注册在手势开始前生效；每个 packet 均记录 receiver sequence、
   receipt QPC、input code、raw flags、raw movement 和 button flags。
   movement packet 另记录 cursor success/error 与 screen point；已有
   packet 可附加 button cross-check。无数据时保持 missing/UNKNOWN。
2. `usButtonFlags == 0` 不能证明按钮 down；维护已观察的 DOWN/UP
   transitions，UNKNOWN 不升级为 down。`RI_MOUSE_LEFT_BUTTON_UP` 是
   END witness，读取后不得再让 pending movement 产生下一次写入。
3. 一个有限 owner quantum 内可保留 latest cursor 合并 movement，
   每 quantum 最多一次写入。DOWN/UP、read failure、authority abort
   等 lifecycle 信号不随 movement 丢弃。只记录最后 packet 会失去
   END 或实际 packet 数证据，不能这样实现 coalescing。
4. Drain 设置明确 packet/queue 上限；达到上限须 yield 或 fail closed，
   不创建 resident poll、不无限 drain、不 retry-until-stick。一次
   WM_INPUT 标准读本身足以构成事件来源；不必为本轮引入复杂 buffered API。
5. Receipt QPC 是应用读取时间，不能改称硬件生成时间。跨进程 receiver
   记录 receiver QPC 与 parent arrival QPC；cursor 是当前状态，不给
   raw delta 虚构 screen trajectory。取消、EXIT、写入、UP 都需独立 QPC。

[RAWINPUTHEADER](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-rawinputheader)
规定 `dwType` 与 packet size；`hDevice` 可在 precision touchpad 输入时
为零。因此 device handle 非零不能成为 raw validity 的必要条件；本轮
也不需要设备名称。Raw metadata 或 test tag 是关联线索，不能成为目标
HWND/PID/TID、foreground、desktop、consent 等 authority 的替代品。

## SendInput 并未给出 raw delivery 保证

[SendInput](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput)
保证将合成 INPUT serially 插入 input stream，并报告成功插入数量；受
UIPI 限制，错误值不能可靠标识 UIPI 原因。它没有承诺每次合成 mouse
motion/button 必定产生 `WM_INPUT`，也没有承诺 packet 与 INPUT 一一对应。
Raw Input 文档描述 device input；两者不能拼成未记载的 delivery 保证。
这是合约缺口，**不是已经观察到 SendInput 没有 Raw Input**。

自动 probe 必须分别记录 SendInput request/return、真实 native
ENTER/MOVING/SIZING、raw movement continuity、raw UP 与 cursor progress。
偶然 physical packet 或仅一次后台 packet 不能证明后续 injected
movement 的连续性；需要输入时段、cursor、顺序等实际相关证据。
不得发送伪造 `WM_INPUT`、替代 raw END、添加 hook/timer 或把 driver
计数填成 raw 计数来获得 PASS。

缺少 raw continuation 时，mechanical cancel 可以独立有结论；不得反向
把 cancel 标为失败。输入证据不完整使 takeover 前提 UNKNOWN/BLOCKED、
候选 architecture `UNRESOLVED`，并停在写入门前。此结论只针对当前
自动观察路径，不能推导 physical Raw Input 架构被平台彻底否决。

## WM_CANCELMODE 的保证及所缺实测

[WM_CANCELMODE](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-cancelmode)
用于取消某些模式；`DefWindowProc` 处理会取消标准 scrollbar/menu 内部
处理并释放 mouse capture。文档没有保证一定结束 moving/sizing modal
loop、发出 EXIT，或保持当前 geometry。

[WM_CAPTURECHANGED](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-capturechanged)
发送给失去 capture 的窗口，`lParam` 是获得 capture 的窗口；窗口即使
自行 `ReleaseCapture` 也会收到。接收该消息时不应再次 SetCapture。
它证明 capture 生命周期的一部分，不能独立证明 native loop 已结束。

[WM_EXITSIZEMOVE](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-exitsizemove)
定义在真实 moving/sizing modal loop 退出后向 source 发送一次消息。
本轮必须真实观察 EXIT，不发送 synthetic EXIT；Cancel API return
与 EXIT receipt 分开记录，首个 takeover write 必须晚于真实 EXIT。

[GetCapture](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getcapture)
返回**调用线程**的 capture HWND；controller 调用得到 NULL 不证明
owner/Explorer 无 capture。Owned source 在自己的 UI thread 读取 before/
after；跨线程/进程用
[GetGUIThreadInfo](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getguithreadinfo)
查询 exact source TID，设置 `cbSize`、检查 BOOL 并记录 `hwndCapture`。
不得用 thread id zero 的 foreground 查询悄悄替代冻结的 source TID。

[SendMessageTimeoutW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendmessagetimeoutw)
的函数成功是非零；recipient LRESULT 单独位于 result out 参数。
`WM_CANCELMODE` recipient 返回零与 send 成功相容。调用前清除 LastError；
send 返回零且 error 仍零也算 generic failure。不同线程才受 timeout
约束；同 queue 会直接调用并忽略 timeout。采用 exact HWND、不同 queue
controller、明确 bounded timeout；不用 `SMTO_NOTIMEOUTIFNOTHUNG` 或
`HWND_BROADCAST`。API 成功不能作为 cancel PASS。

本轮的取消 PASS 必须同时具有：真实 ENTER、至少一次正确 MOVING 或
WMSZ_BOTTOM SIZING、仍处于 down 时的一次 cancel、capture lifecycle、
真实 EXIT、随后至少 15 次真实 driver movement 的 geometry stability、
没有继续的 MOVING/SIZING 或 native trajectory POSITION_CHANGED。
记录 cancel issue、native handler、send return、EXIT 各边界，禁止通过
移动边界排除实际晚到的 reassertion。Cancel-only 不调用 SetWindowPos。
满足机械 cancel 后仍需独立证明 raw movement 与 raw UP 的 continuity，
才能进入 cursor-driven free takeover；Move 与 Bottom Resize 分别裁决。

## 独立 receiver 子进程的第一阶段设计审查

Root 选择同一 test EXE 的独立 hidden receiver 子进程；它创建一个
message-only HWND，只注册 mouse `RIDEV_INPUTSINK`，通过匿名 pipe 给
parent 传递 packet 观察。此设计符合上述 process-wide registration
边界，也能严格区别 foreground parent owned source 与 background
receiver。后台证据须同时记录 `RIM_INPUTSINK` 与
`foreground PID != receiver PID`；同进程 receiver HWND 非 foreground
不能自动证明应用在后台。

Pipe 接收必须由独立于 source modal UI 的 parent 线程持续处理。任何
同步 pipe write 都可能阻塞 receiver，因此 packet 数量、传输、队列
要有明确有限预算；不以无限堆积换取完整性。传输错误/overflow 不补数据，
保留事实并使 raw completeness 不通过。第一阶段 receiver 只观察，
parent 只执行一次 exact owned cancel，geometry 写入数为零。

此阶段尚不能宣称 sole writer：保留 legacy delivery 后，target app
仍可能有独立输入处理；需要 cancel 后稳定性及后续真正 takeover 测试。
研究阶段不为任何 unconsented HWND 发 cancel，不注册 keyboard，不实现
`WH_MOUSE_LL`，不改变 Human UAT hard block。只有本轮阶段性实测 gate
通过后，才按 brief 继续 owned free takeover、Magnet、exact nonce
Explorer cancellation 和最小 takeover。

## 首次本地自动观察：raw receipt 早于 cursor/legacy 完成

本节由亲读完整原始日志形成，状态为 `AUTOMATED OBSERVED`，不是人工
操作，也不是文档推定。首次 Debug 日志：
[20260926T074218514Z-Debug-f3ce2ff107c6448193a110520f4e152f.jsonl](../../uat/r1c4b-takeover-owned/20260926T074218514Z-Debug-f3ce2ff107c6448193a110520f4e152f.jsonl)。
QPC frequency 为 `10000000`；22 条记录完整保留，结果仍是
`BLOCKED_BY_RAW_INPUT_CORRELATION`，不能追改为 PASS。

| 实际记录 | 观察及边界 |
| --- | --- |
| seq 4 | 子进程 PID `13452` 的 exact message-only HWND `4784506`，mouse-only `RIDEV_INPUTSINK=256` 注册验证成功。 |
| seq 11–13 | Foreground owned source PID `46784`、HWND `4785818`。Driver 起始 cursor 为 `[2268,850]`，安全目标 HTCLIENT 为 `[1446,874]`。 |
| seq 14–15 | 实际 `RIM_INPUTSINK=1`，foreground PID 仍为 parent、不同于 receiver。Raw flags `11` 是 ABSOLUTE、VIRTUAL_DESKTOP、NOCOALESCE；raw coordinates `[30858,29849]`、test tag matches；device handle 可为空。GetCursorPos 成功，但该 packet 的 cursor 快照仍为 `[2268,850]`。 |
| seq 16–20 | Raw receiver QPC `682649365672`，早于该 SendInput return QPC `682649398946`。Input request 为目标 `[1446,874]` 且 `sent=1`。seq 19 被严格同步 cursor 关联断言阻断；之后 seq 20 才记录 legacy mouse movement 的 screen point `[1446,874]`，QPC `682649422688`。 |
| seq 21–22 | Receiver registration 移除及窗口销毁均成功；仅 owned/guard 窗口销毁，未触碰第三方窗口。没有进入 native 手势，没有 cancel，没有 takeover geometry 写入，没有 raw UP 证据。 |

已证明本机这一次合成 move 路径确实送达了 background Raw Input，
且该 packet 观察时当前 cursor 尚不是 planned point。Raw receiver QPC
记录在 cursor 读取之后，不能称为 device timestamp。Legacy screen point
是后续事件记录，不能据此虚构一次未进行的 GetCursorPos 查询。
未证明完整 raw gesture、UP、cancel、free takeover 或产品时序有效。

由该反例修正测试关联设计（不是放宽 geometry 验收）：

1. Test driver 在 input 日志保存**实际提交的** `INPUT.mi.dx/dy`、完整
   flags、test tag、virtual desktop bounds，以及 SendInput start/return
   QPC。Raw ABS/VIRTUAL coordinates 与 submitted normalized INPUT 比较，
   同时验证最新 receiver sequence、receipt QPC、注册身份和 true
   background；不能用 raw receiver 的当前 cursor 已到目标作为 packet
   delivery 的先决条件。Work area 不等于 virtual desktop，不能从日志
   work-area 高度倒推 normalized INPUT。
2. 保留 raw GetCursorPos success/error、原始 screen snapshot 与 planned
   point 的差异，明确记录 cursor lag。Test tag 用于有限自驱路径的
   sample 关联，不能扩大 source authority，也不能证明 physical device
   的时序与本次合成路径相同。相对 packet 不按绝对 normalized 规则关联。
3. Driver 仍只用原有有限 SendInput pacing：packet 到达后，既定一次
   30 ms test-only pacing，再用 fresh input fence 验证实际 screen point、
   button、foreground、capture/desktop 等。失败立即停止；不增加重试、
   resident observation timer 或产品 polling。Cancel-only 的 cursor
   progress 来自独立 driver 观察，raw continuity 来自实际 packet，
   两种证据必须分别保存。
4. Raw UP 以 `RI_MOUSE_LEFT_BUTTON_UP` 为 witness，且须关联本次 UP 的
   tag、receiver sequence 和 input-start QPC；不要求同 callback 的
   GetAsyncKeyState 已是 false。Async 状态只是当时 cross-check；在
   有限 test pacing 之后的 driver fence 仍独立要求按钮确实 released。
5. 后续 free takeover 必须独立验证由每个真实 WM_INPUT 的当前
   GetCursorPos 产生 geometry target。Background receipt 通过不能
   推定这种 cursor-derived writer 及时、正确或 PRE_RELEASE；更不能以
   submitted INPUT coordinates 替代产品 cursor authority。若事件早于
   cursor 更新导致其最后 target 不准确，保留该反例并停在对应 gate。

## 第二次本地自动观察：local activation 不等于 global foreground

完整亲读第二次 Debug 原始日志：
[20260926T081719278Z-Debug-4bba552a333f493d8a4f9afeee2cc8f2.jsonl](../../uat/r1c4b-takeover-owned/20260926T081719278Z-Debug-4bba552a333f493d8a4f9afeee2cc8f2.jsonl)。
23 条记录结果保持 `BLOCKED_BY_ACTIVATION_FAILURE`；状态为
`AUTOMATED OBSERVED`，不能把后续测试设计倒填成此次通过。

Owned PID `19864`、UI TID `22224`、source HWND `13238740`。
seq 5–6 在 ShowWindow 阶段已观察到 epoch 0 的真实 `WM_ACTIVATE` 和
`WM_SETFOCUS`，当时 `foreground_matches=false`。seq 8 的普通
SetForegroundWindow 返回 false，global foreground 仍为 `854960`。
seq 11、13 的点击前 authority fence（exact owned identity、可见、
HTCLIENT、point root、无 foreign capture/menu/MoveSize、键钮状态）均
通过；seq 12/14/17 的实际 SendInput move/down/up 均返回 `sent=1`。
seq 15/18 在 source 观察到真实 client DOWN/UP；seq 16 global
foreground 已是 source。epoch 1 的 activation/focus callback 未观察到，
seq 20 因此仍为 BLOCKED；临时 topmost 已恢复。Receiver 只完成注册及
安全移除/销毁，未 armed 到正式 raw preflight；没有 native ENTER、
cancel 或 takeover write。该日志没有 GetActiveWindow/GetFocus 的 local
readback，不能将“local 状态未变所以没有新 callback”写成已经证明的根因。

实际补充阅读的官方合约：

| 官方资料 | 与本次 bootstrap 相关的边界 |
| --- | --- |
| [ShowWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-showwindow) | `SW_SHOW` 会显示并激活；`SW_SHOWNOACTIVATE` 显示但不激活。首次调用可能受 STARTUPINFO 影响，需观察 startup facts、actual visibility/local 状态；BOOL 表示此前是否 visible，不是操作成功判据。 |
| [SetActiveWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setactivewindow) | 目标应属于 calling queue；后台调用不把应用带到 global foreground。NULL 不是 documented clear-active contract；Fix A 已撤销该 clear 前提及调用，旧 readback 仅说明第三次历史 run 的结果。 |
| [GetActiveWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getactivewindow) | 返回 calling thread queue 的 active window，不能当作 global foreground；查询别的线程用 GetGUIThreadInfo。 |
| [SetFocus](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setfocus) | 作用于 calling queue；NULL 明确允许，意味着 keystrokes 被忽略。API 会发送正常 KILLFOCUS/SETFOCUS，设置非空 focus 还可激活 receiving window；本次只考虑清除 owned local focus，不设置 source focus。 |
| [GetFocus](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getfocus) | 返回 calling queue 的 focus；NULL 不代表其他队列无 focus。跨线程查 GetGUIThreadInfo，不能在 driver 新 queue 的空值上构造 source proof。 |

[SetForegroundWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setforegroundwindow)
仍是普通第一选择；调用可能被拒绝，即使部分允许条件满足。清 owned
local 状态不能创建 foreground entitlement。Fix A 的真实 test-only
点击继续通过既有安全 fence、intended DOWN/UP 和 final exact global
foreground；fresh activation/focus callbacks 已降为 optional diagnostic。

第二次之后、第三次之前曾设计并执行一次 owned local-clear，以及
fresh epoch 1 callback gate。**该设计已被 Fix A 撤销**：它既不是
RIDEV_INPUTSINK 官方资格，也不是 global foreground 的必要证据。
当时的 HWND/PID/TID、desktop、键钮、无 capture/menu/MoveSize、global
snapshot 和 readback 记录仍说明第三次 run 的安全边界；这些 checks
不能使错误 prerequisite 获得依据。不要将历史 local-clear 设计保留成
未来实现要求，不再寻找 API 来清 active。

## 第三次本地自动观察：NULL active 清理未通过 readback

完整亲读第三次 Debug 原始日志：
[20260926T084255946Z-Debug-ccc8ec31a1af4d08983ab3ca53d2d4f7.jsonl](../../uat/r1c4b-takeover-owned/20260926T084255946Z-Debug-ccc8ec31a1af4d08983ab3ca53d2d4f7.jsonl)。
14 条记录保持 `BLOCKED_BY_LOCAL_ACTIVATION_PREPARATION`；这是本轮
`AUTOMATED OBSERVED` 的 bootstrap 反例，不是 native cancel 反例。

Owned PID `13088`、UI TID `15860`、source HWND `39783966`。
seq 5 记录 `SW_SHOWNOACTIVATE=4` 且 visible；随后 seq 6–7 记录真实
epoch 0 activation/focus、foreground 仍不匹配。seq 8 的普通
SetForegroundWindow 返回 false，global foreground 为 `133064`，
此次明确读取 local active/focus 均为 source。日志不能据此断言
SW_SHOWNOACTIVATE 直接产生了这些 callbacks：callbacks 位于其 show
记录之后、普通 foreground call 的结果记录之前。

seq 10 在 source owner UI thread 记录授权检查成功：owned identity、
desktop、键钮、无 capture/menu/MoveSize、global snapshot 均正常。
`SetFocus(NULL)` 返回 previous source、error 为零，最终 focus readback
为零。`SetActiveWindow(NULL)` 已调用，return 与 error 均为零，但
**GetActiveWindow readback 仍为 source**。Global foreground 保持相同
外部 HWND、owned identity 保持有效；`local_cleared=false`、`success=false`，
因此 seq 11–12 fail closed。这直接确认本机此次 NULL active 清理没有
达到 probe 要求，不能用无 error 推定状态已经清空。

seq 13–14 记录 receiver registration 移除及销毁、source/guard 销毁。
Driver 尚未启动，没有任何 SendInput mouse event、armed raw movement、
native ENTER/MOVING/SIZING、WM_CANCELMODE 或 takeover write。Saved
cursor 没有被移动，不需要还原；`cursor_restored=false` 保持原记录。
不得把本次 blocked 解释为 CancelNativeLoopAndTakeOverInput 已被否决。

原 Pivot 1 在该处停止；Fix A 现在明确授权删除无依据的 local-clear /
fresh-callback 前提并按本文件开头的 global foreground 合约继续 probe。
原三个日志仍保持 BLOCKED，不能重新验收为 Fix A PASS。原 Pivot 1
首次日志只有一个真实 background mouse packet；其完整 movement/UP
continuity、sample 3 cancel observation wakeup、真正 cancel stability、
free takeover、Magnet 及 Explorer 全部 `NOT TESTED / NOT_RUN`。

以下为原 Pivot 1 结束时的历史状态，不预填新的 Fix A 运行结果：

```text
OWNED_NATIVE_CANCEL_MOVE = UNKNOWN
OWNED_NATIVE_CANCEL_RESIZE = UNKNOWN
OWNED_TAKEOVER_MOVE = NOT_RUN
OWNED_TAKEOVER_RESIZE = NOT_RUN
RAW_INPUT_TAKEOVER_ARCHITECTURE = UNRESOLVED
R1C4B_HUMAN_UAT = NOT_READY
```

## 来源与状态

所有链接均为本次实际打开并阅读的 Microsoft 官方 primary sources。
另打开 [HID Architecture](https://learn.microsoft.com/en-us/windows-hardware/drivers/hid/hid-architecture)
确认 TLC 背景；无需为此打开物理 device、实现驱动或读取设备名称。
官方文本/示例 reference-only；没有复制、翻译或改写示例为实现代码。
源码 prior art 与 history 由本轮独立 prior-art 文档及
[SOURCE_PROVENANCE.md](SOURCE_PROVENANCE.md) 统一记录。

本文件状态：`OFFICIAL CONTRACTS REVIEWED / PROBE DESIGN REVIEWED`。
三次局部观察分别标为 `AUTOMATED OBSERVED`；它们不授予任何原生 cancel、
takeover 的 PASS 或 `MANUALLY OBSERVED`，也不使
`R1C4B_HUMAN_UAT = NOT_READY` 改变。
