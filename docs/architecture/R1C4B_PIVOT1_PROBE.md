# R1-C4B Architecture Pivot 1 — staged authority experiment

2026-09-26；base 070b05a8f44c8b08503b3643f838a83703495987。
这只是独立 research/test driver，不是 PaneBind 产品架构的实现或 READY 声明。

## Fix B 当前合同：CancelWaitEndThenTakeover

Fix B base `e381b998d9e3eded051fbc859893633e8633ebee` 正式替换旧
cancel-return rect retention 验收；下文 Fix A 及第一阶段描述保留为历史合同，
旧报告/原始 JSONL/metadata/validator/verdict 不改、不追认。
`OLD_CANCEL_RETURN_RECT_CONTRACT = FAILED_AND_SUPERSEDED`。
最新研究见 [END-barrier 合约](../research/R1C4B_PIVOT1_FIXB_END_BARRIER.md)。

新独立 owned CLI `--run-owned-takeover-test --gesture move|bottom-resize`
使用 `r1c4b-takeover-owned/v2`、`end_barrier_v1`。真实 ENTER/DRAG 后只发一次
bounded cancel，允许 return→EXIT 期间 native terminal settlement；真实
WM_EXITSIZEMOVE 仅冻结 actual terminal baseline，下一 owner work fresh 检查
capture/menu/move-size 清空和 geometry 稳定后才允许 handoff。
intent 永远来自真实 DOWN pointer + native ENTER P/V，绝不来自 EXIT rect。

handoff 最多一次 SetWindowPos；随后实际 background WM_INPUT movement
唤醒 coalesced owner quantum，当前 cursor 只读一次、原始 anchor 全量计算、
每 quantum 最多一写。Raw UP terminal，清空 pending movement，无末端补写。
UI 是唯一 geometry writer；driver timer 只负责测试轨迹 pacing，不能唤醒
writer。以 receiver 实际 UP QPC 对照最后 native return，不以 parent 日志
顺序替代。沿用 checked frame bridge 和立即/full P/V strict exact，不新增
DWM 宽限、timer 重试或操作风暴。END 后 native DRAG 或无归属 geometry
change 均 FAIL。先 Debug Move×1，只有完整 PASS 才 Bottom Resize×1；
两者都 PASS 才开始独立 Debug/Release 20/20 repetition gate。

显式 runner `scripts/run-r1c4b-end-handoff.ps1` 每次仅运行一个 fresh owned
gesture，原始证据仍 ignored local；不自动进入下一项、Explorer、Pure Magnet
或 Human UAT。验收及停止结果由新的 Fix B 执行报告记录。

## Fix A 当前覆盖：global foreground proof，local 状态仅诊断

Fix A base `94242959568b28d782d1a89a6f78c95c2dd8b098`；新 probe 使用
`verified_global_foreground_v2`。已删除 local-clear 函数及调用，不使用
SetActiveWindow(NULL)/SetFocus(NULL)，不要求 local active/focus 为空或本 epoch
新 activation/focus callbacks。旧 v1 仅作历史证据解码，不用于新实验验收。
下面的 Raw/native/cancel 分阶段模型不变；当前结果见
[Fix A 执行报告](../reports/R1C4B_PIVOT1_FIXA_EXECUTION_REPORT.md)。

Fix A 实测：背景 Raw movement/UP PASS；单 cancel 后 capture 已释放，source
在 APIreturn后、EXIT前回到初始rect，EXIT时LB仍held。当前预先固定的
cancel-return rect retention门 FAIL，按本轮合同停止后续阶段；没有post-EXIT
reassertion、没有Raw失败，也不是loop无法退出。允许terminal restoration、
改用EXIT-final rect为anchor须新的明确合同决定，不在本轮改baseline追认PASS。

[实际 prior art/history/license](../research/R1C4B_PIVOT1_PRIOR_ART.md) 与
[官方合约](../research/R1C4B_PIVOT1_INPUT_CONTRACTS.md) 的有限 probe gate 已通过。
官方缺少 SendInput→WM_INPUT 送达保证与 WM_CANCELMODE→modal EXIT 保证；
因此本轮依真实记录分阶段，不由 API transport success 推导 authority。

## 第一阶段：background input prerequisite 与 cancel-only

独立 panebind-magnet-takeover-probe，schema r1c4b-takeover-owned/v1；
旧 Fix 3 probe/schema/evidence 不变。

Parent 新建空白 top-level source，另建同进程 NOACTIVATE 空白 guard
覆盖有限鼠标轨迹。唯一输入 driver 发有限 tagged mouse SendInput，
其 timer 只用于测试轨迹 pacing。主 UI 等待 message/event，不注册 input hooks。
source identity/desktop/foreground/按钮/cursor/捕获每次重新验证，未知即停止。

新 probe 使用 SW_SHOWNOACTIVATE 显示 source，先尝试普通 SetForegroundWindow。
直接成功后要求 fresh exact HWND/PID/TID、global foreground、desktop/session、
visible、空 capture/menu/MoveSize、键钮清及非 topmost。调用被拒时，只用既有
strict verified activation click：fresh point/root/HTCLIENT/foreign-GUI/input
fences、实际 source DOWN/UP receipts、move/down/up 成功、NOTOPMOST 恢复及
同样 fresh global foreground proof。所有 callbacks 只作 diagnostic。
`source_thread_local_active/focus` 从真正 owner queue 查询，非 NULL 也不阻断。
不 AttachThreadInput、不伪造消息、不改变旧 Fix 3 helper/benchmark。

同一 EXE 创建独立 hidden receiver 子进程；它没有 target HWND 或 geometry-writing
authority，只创建 message-only window、mouse-only TLC 0x01/0x02 INPUTSINK，
并读回注册。后台证明同时要求实际 RIM_INPUTSINK 与 foreground PID≠receiver PID。
不注册 keyboard，不用 NOLEGACY/CAPTUREMOUSE，不读取设备名或其他窗口内容。

Receiver 仅在 armed 生命周期读取实际 WM_INPUT 的 HRAWINPUT。movement 触发一次
GetCursorPos；button flags 及事件触发的 async high bit 分开记录。原始 movement
不积分成 screen cursor。标准读取先于 DefWindowProc cleanup，不造 WM_INPUT。
独立 parent pipe reader 持续接收固定 packet；保留 receiver sequence/QPC 与
parent arrival QPC，2048 packet/4096 log limits，最多64 messages 一次 drain。
数量/传输/identity 错误不补造数据、使证据不通过。没有 observation timer。

先在 source 的安全 client point 做有限 move、held move、UP preflight，
按真实 background movement 和 raw UP 放行。缺失时只标输入证据 BLOCKED、
architecture UNRESOLVED；cancel UNKNOWN、takeover NOT_RUN，不再输入。
偶然用户 movement 不能代替计划 cursor/QPC 的相关证明。清理自建窗口、
移除 mouse registration、正常关闭空 receiver 子进程，保留原始失败原因。

首次本地预检已观察到 tagged RIM_INPUTSINK absolute packet 早于 SendInput
return / source legacy cursor update；该 callback 的 GetCursorPos 仍是旧位置。
原 run 保持 BLOCKED，不追认通过。输入送达关联因此使用实际构建的
INPUT normalized dx/dy/flags、原 virtual-screen bounds、tag、API QPC 和 fresh
receiver watermark，而不是假设 Raw snapshot 等于该 packet 的历史屏幕位置。
仍原样记录真实 cursor snapshot 与是否滞后；绝不拿 normalized Raw 坐标驱动 geometry。
Raw UP 的 transition 独立于同 callback 的 async cross-check；之后仍由既有
有限 input pacing 后的 fresh driver fence 验证实际屏幕点和已松开状态。
这仅修正测试送达门，**不修复、不证明**未来 WM_INPUT→cursor-derived writer
的时序 authority。该同步假设已有反例，后续 free takeover 需独立验证。

只有 preflight 成功，才制造真实标题 Move、HTBOTTOM/WMSZ_BOTTOM Resize。
每项20有限轨迹 samples，在第2个已观察 native callback 后由独立 controller
thread 向 exact source 发送一次 bounded WM_CANCELMODE。记录 UI capture、消息、
transport 与 recipient LRESULT、真实 CAPTURE_CHANGED/EXIT。
余下18个 movement 只观察。第3个既定 sample 是唯一 cancel-observation
wakeup，不是额外输入或重试：捕获已释放、exact foreground/identity/desktop、
无菜单或 foreign capture、命中空 source/guard 时，暂时只允许 exact source
自己的 MoveSize 遗留状态。该 sample 后一次 bounded EXIT wait；真实 EXIT
及 fresh GUI clear 后，第4–20个 sample 恢复严格 capture/menu/move-size 全空。
不凭孤立 timeout 否决模型；Fix A 允许在真实 START/DRAG、单 cancel、实际
第三 MOVE/Raw 送达、完整等待 deadline 后，fresh source 仍在 MoveSize 且
foreground/desktop/cursor/buttons/receiver/GUI 全部可信时，认定无法退出的
协议反例。缺少可靠上下文仍 UNKNOWN。API return 后的真实 native DRAG 或原轨迹实际写入
都是反例，不能从确认时刻开始才计数而排除第3个 sample。
该阶段 takeover geometry writes=0；
setup/visibility placement 不是 takeover writer。native drag RECT 完全不修改。

机械 cancellation 与完整 cancellation verdict 分开：
native ENTRY/DRAG、单 cancel、capture release/EXIT、余下15+ actual cursor 轨迹稳定
构成机械事实；真实 raw continuation/up 另外必需。取消后继续 native DRAG 或
实际 geometry 沿原轨迹变化是明确反例；缺少 raw 不倒推 cancel FAIL。

补充官方 [GetMessageW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getmessagew)、
[SendMessageW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendmessagew)
及 [message queues](https://learn.microsoft.com/en-us/windows/win32/winmsg/about-messages-and-message-queues)
已实际读取：sent handler 可以在等待 queued message 的过程中被 dispatch；
发送 API 返回不是该 message-retrieval invocation 已返回的保证。内部 native
modal loop 的具体机制仍是待实测假说，不能由文档直接认定。source 和 guard
命中差异、单次 sample3 stimulus 与真正 EXIT 都必须记录，不能造消息替代它们。
失败前退休 raw armed scope，cleanup 的 UP/关闭不补成成功 gesture END。

## 后续阶段只能依实测授权

本轮实际结果见 [执行报告](../reports/R1C4B_PIVOT1_EXECUTION_REPORT.md)。
三次开发观察均在 native START 之前停止。最后一次 local readback 明确显示：
普通 foreground setter denied 时 local active/focus 为 source；清 focus 成功，
SetActiveWindow(NULL) 返回 NULL/diagnostic error0，但 active 仍是 source。
因此初始化失败，零 activation click/Raw/native cancel。没有继续试验其他激活
绕过方式；不能将这类 bootstrap blocker 写成 cancellation FAIL。
原 Raw/cursor 同步假设已有反例，最后的 NULL active 清理候选也未获证明。
上述旧前置门由 Fix A 明确撤销；不是 Raw Input eligibility 或 foreground
authority 合约。旧日志保持原判，但当前 probe 不再执行它们，也不寻找替代清理 hack。

取消和 input prerequisite 都 PASS 后才实现 free Move/Bottom Resize takeover：
冻结原始 cursor/rect anchor，只由 WM_INPUT movement wakeup，在有界 owner
quantum 中合并 latest cursor，一次最多一写；UP terminal，pre-release write
不能晚于已接收 UP，双矩形即时/最终 exact。不得从上次 corrected geometry 累加。
先完成 Debug/Release 各20/20 free Move/Resize，才接既有 Pure Magnet。
再完成 owned Magnet pre-release scenarios，才进入 exact test-created Explorer。
本文件不实现这些尚未通过前置 gate 的阶段，也不预填 PASS。

任何明确 cancel FAIL 按 brief 拒绝并停止。输入证据不足则停止在 UNRESOLVED，
不实现 WH_MOUSE_LL 或虚拟 HID，不让 Human 替代 driver。full C4B automated
product gate 属于下一轮；本轮 human runner 永远 NOT_READY，PR/merge/tag/release均NO。
