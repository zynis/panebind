# R1-C4B Pivot 1 Fix B：END barrier 与原始意图接管研究

2026-09-26；授权基线 `e381b998d9e3eded051fbc859893633e8633ebee`，
分支 `codex/r1c4b-live-magnet`。候选 `CancelWaitEndThenTakeover`。
本文件只建立独立 owned probe 的研究/验收边界，不修改旧 Fix A 报告、
JSONL、metadata、validator 合同或研究记录，也不给产品 READY。

用户 Fix B 正式允许 cancel return 与真实 END 之间的 Windows terminal
settlement；`CANCEL_RETURN_GEOMETRY_RETENTION = NOT REQUIRED`。
旧 `REJECTED_AT_CANCEL_STAGE` 仍是
`RESULT_UNDER_OLD_RETURN_BASELINE_CONTRACT`，不得追认旧日志为新 PASS。
`ConcurrentNativeLoopCorrection` 仍 REJECTED，新的 END-gated 候选待实测。

## Prior art、history 与已有本地观察

沿用并完整读过 [Pivot 1 prior-art 记录](R1C4B_PIVOT1_PRIOR_ART.md)：
AltSnap mature maintained，pin `5c86416ad21e4b72844a998a746bd3bb0bee5f5d`，
GPL-3.0-or-later reference-only；FancyZones mature production，pin
`19c4d805321db86f3634e6968e14dbf25cbba14a`，MIT reference-only。
已记录的 worker/coalescing、recipient-return、hook lifetime、destroy-abort
及 input-delivery history 支持有限 lifecycle/ownership 教训，不证明
PaneBind sole-writer。Fix B 不声称重新 inspect upstream source/diff；
原记录的访问限制仍有效。无 GPL/MIT 实现复制、翻译、适配或机械改写。

本机 Fix A 原始观察及 hash 已在
[原报告](../reports/R1C4B_PIVOT1_FIXA_EXECUTION_REPORT.md) 和
[旧输入合约](R1C4B_PIVOT1_INPUT_CONTRACTS.md) 保留：真实 Move 的 cancel
return 后 0.9841 ms 恢复 initial rect、2.1181 ms 真实 held-button EXIT；
capture 已释放、post-return WM_MOVING 为零，sample 3 在 EXIT 后
40.986 ms 才提交。该记录支持允许 EXIT 前 terminal settlement 的研究，
没有测过 handoff write、15+ raw continuation、Bottom Resize 或 20/20。
其 Raw background preflight PASS 可保留，新的 run 仍逐包检查身份和健康。

## 官方 END 与 native authority

| 实际阅读的 primary source | 合约与本轮边界 |
| --- | --- |
| [WM_EXITSIZEMOVE](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-exitsizemove) | 真实 moving/sizing modal loop 退出后向 source 发送一次。Owned probe 必须取得真实 source callback，不 synthetic 发送；这才是 native END barrier。 |
| [WM_CANCELMODE](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-cancelmode) | DefWindowProc 释放 capture 并取消部分内部模式；没有承诺 send return 时整个 MoveSize operation 已结束或当前 rect 保留。 |
| [SendMessageTimeoutW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendmessagetimeoutw) | Transport completion 与 recipient LRESULT 独立，完成的是 recipient handler；同 queue 会忽略 timeout。不同 queue controller、exact HWND、bounded timeout、单次 cancel，不以 return 代替 END。 |
| [Event Constants](https://learn.microsoft.com/en-us/windows/win32/winauto/event-constants) | `EVENT_SYSTEM_MOVESIZEEND=0x000B` 表示 movement/resizing finished，由系统发送。仅作为未来 Explorer out-of-process END witness 的依据，不是本轮 Explorer 观察。 |
| [SetWinEventHook](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwineventhook) | OUTOFCONTEXT 事件在安装线程交付，需 pump；callback 可 reenter。未来记录 exact native envelope、receipt sequence/QPC，并再次验证 source consent/identity/context；不能从晚到 callback 推定事件时 geometry 快照。 |

Fix B 新 acceptance 是：真实 ENTER 和正确 DRAG、一次 exact cancel、
capture release、held-button 的真实 END，return 后没有新的 native
WM_MOVING/SIZING。Return→END 的 POSITION_CHANGED/P/V settlement 全部记录并
允许；END 后、首次 PaneBind 写入前 fresh actual 必须仍是 END terminal
rect，capture NULL、GUI MoveSize/menu cleared、source exact foreground /
PID/TID / desktop / visible / button / receiver health 均有效。

首个 handoff native-start QPC **严格大于** END receipt QPC，并由 END
之后的 owner work 执行；不要在仍记录 END callback 的中间混入 native
write 使观察边界不清。可在下一实际 Raw movement 或一次 handoff
fresh capture 检查 stability；不添加观察 timer、Sleep、polling 或第二
cancel。任一 END 后 native DRAG、非自身 geometry transition、authority
失配均停止，保存原始证据，不能移动 END 边界排除反例。

未来 Explorer 还需 same authorized HWND/PID/TID、canonical COM identity、
nonce location、integrity、virtual desktop、monitor/DPI；本轮不实现或
运行该阶段。WinEvent END 不授予任何新窗口的控制资格。

## 意图起点与实际 handoff baseline 必须分开

| 保存值 | 用途 |
| --- | --- |
| `intent_start_positioning / intent_start_visible` | 原 native START 的 geometry，属于用户意图数学基线。 |
| `intent_pointer_anchor` | 原 gesture DOWN/start cursor，不能在 cancel 或 EXIT 重置。 |
| `end_terminal_positioning / end_terminal_visible` | END callback 的 actual snapshot，用于 settlement/stability 证据。 |
| `actual_handoff_baseline` | END 后再次 exact capture 的 actual P/V，只描述 Windows 收尾后的实际位置。 |
| `owner_current_cursor` | handoff 或 movement owner quantum 时实际 GetCursorPos，用于 full gesture delta。 |

Move 从原 intent geometry 计算：
`target_p = start_p + (current_cursor - pointer_anchor)`，
`target_v = start_v + 同一个 full delta`。用 checked_difference / checked_add
计算全部边和维度，再检查 native int32 range。禁止 `EXIT rect + post-EXIT
delta`，也禁止 `previous actual/corrected rect + incremental delta`。
若 native START 前已发生第一个触发 movement，必须明确 start snapshot
与 DOWN anchor 的采集顺序；START geometry 不得悄悄采用已经移动后的 rect
并再次叠加同一位移。离线 validator 根据原 anchors 重新计算每个 target。

Bottom Resize 冻结原 positioning left/top/right，仅
`bottom = start.bottom + (current_cursor.y - pointer_anchor.y)`。
固定 participating edge Bottom，非参与边保持；visible target 使用下面
checked frame bridge 或等价严格映射，不把两个坐标域当作同一个 rect。

仅在 END barrier、fresh authority、terminal stability 全部通过时，若
actual baseline 与 full intended geometry 不同，允许**一次**
`HANDOFF_RECONCILIATION` SetWindowPos；相同则记录 no-op，不为凑次数写入。
记录 origin anchor、current cursor/QPC、delta、P/V targets、END/first-write
QPC、native return、immediate/full readback。该 write 属于新手势的唯一
owner，不能启动另一个 native SC_MOVE/SC_SIZE loop。

## 现有 platform frame bridge 的实际检查与复用建议

已完整读取 PaneBind 自有代码：

- [window_rect_adjustment.h](../../src/platform/windows/operations/window_rect_adjustment.h)
  的 `operations::prepare_visible_rect_adjustment(positioning, visible,
  target_visible)` / `RawRectEdges` overload。它独立计算 left/top/right/bottom
  frame insets，以 checked arithmetic 映射到 positioning，检查 positive
  edges、int32 坐标与 width/height。支持 Move 和 edge Resize；成功只提供
  target，没有 HWND 或 native apply authority。
- [window_translation.h](../../src/platform/windows/operations/window_translation.h)
  与 `.cpp` 的 `window_translation::prepare_visible_translation` 是纯 Move
  bridge，拒绝 resize/mixed，并进行 checked translation/native range
  检查；适合验证 full intended visible 与 intended positioning 一致。
- [checked_arithmetic.h](../../src/core/geometry/checked_arithmetic.h) 提供 checked
  add/difference，避免原 anchor 到 current 的溢出。
- [window_rect_adjustment_tests.cpp](../../src/platform/windows/operations/window_rect_adjustment_tests.cpp)
  覆盖 positive/negative frame inset、invalid edges、overflow、native range；
  这里只读已有测试，未把它们称作新的 Fix B 执行结果。

推荐固定 START 的 checked frame relation 并在 END/fresh capture 验证其
仍一致。对 Bottom，可从 original visible Bottom 加同一个 dy，以
`prepare_visible_rect_adjustment(start_p, start_v, intended_v)` 计算 target_p，
再独立对照原 positioning Bottom 公式；left/top/right 必须相同。
Intent 永远来自 original snapshot；frame bridge 只是坐标映射，不使
terminal baseline 或最后 actual 成为下一目标基线。
若 style/monitor/DPI 或 frame relation 改变，停止并记录 context failure，
不假定 cached inset 仍正确。Probe 的 PMv2 coordinate context 必须保持；
不能依赖 bridge 自动修复 DPI virtualization。

## Exact postverify：现有实现与 DWM async 边界

[GetWindowRect](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowrect)
给 positioning screen bounds，受 DPI virtualization，可能含 invisible
resize borders；DWM extended frame 不按相同方式 DPI 调整。
[DwmGetWindowAttribute](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/nf-dwmapi-dwmgetwindowattribute)
读取当前 attribute；
[DWMWA_EXTENDED_FRAME_BOUNDS](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwmwindowattribute)
是 screen-space RECT。实际读过的这些文档没有保证 SetWindowPos return
与 DWM exact 更新同步；这不是已经实测出某次 DWM lag 的结论。
P 与 V 是两次顺序读取，记录各自 QPC/error，不能写成 atomic capture。

基线实际检查结果：

- `rg --files --hidden --no-ignore . | rg 'magnet_postverify_policy'` 无对应
  policy 文件。没有声称读到不存在的 `magnet_postverify_policy.*`。
- [magnet_postverify_diagnostic.h](../../src/platform/windows/operations/magnet_postverify_diagnostic.h)
  的 classify 要求 `actual_p == requested_p`、`actual_v == requested_v`；
  mismatch 分类独立于 capture failure，没有 tolerance/pending/retry state。
- [ExplorerGroupBridge::apply_magnet](../../src/platform/windows/explorer/explorer_session.cpp)
  在一次 SetWindowPos 后做 immediate P/V diagnostic，再 full capture，
  两个实际矩形及 context/receipt/other members 必须 exact。没有 resident
  timer、DWM async confirmation queue 或 correction retry。
- 已读 [Fix 1 official gate](R1C4B_FIX1_MODAL_AUTHORITY.md)、现有 integration
  research 和 [postverify fixture](../../scripts/test-r1c4b-postverify.ps1)，
  它们保持 exact，未建立可复用的 async visible 宽限合同。

因此初版 Fix B owned probe 可严格沿用一次 write、immediate/full exact
readback，mismatch 即停止并如实区分 P/V/error/context；这是 bounded exact
路径。不能将“DWM 可能 async”改成自动 PASS，不能加 epsilon、Sleep、poll、
retry-write 或无上限 capture。若需要 eventual visible confirmation，必须
先明确当前并不存在相应已实现 policy，并另行取得/记录确切 acceptance
决定和 applicable tests；本研究不发明宽限次数/时长或把新 policy 当既有。
Native positioning mismatch 不能等 DWM 变好，且任何 pending observation
不得允许运行下一次 native write 掩盖未确认 operation。

## Raw movement owner quantum 与 completion

[GetCursorPos](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getcursorpos)
返回调用时当前 screen cursor，需要 input desktop 和访问权限。
[RAWMOUSE](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-rawmouse)
规定 relative/absolute/virtual mapping、默认 movement 可合并，raw 不受
相同 mouse-speed 处理，button flags 为 transition。Current cursor 不等于
同一个 raw packet 的历史位置；Fix A 的 lag 事实仍保留。

Owner 只由 actual raw movement 唤醒 bounded quantum，合并 movement 到
latest 可用时再一次 GetCursorPos、从 original anchor 算 target、每 quantum
最多一次 write。保存 receiver seq/QPC、parent ingress QPC、owner quantum/
cursor/write QPC、coalesced count 与实际 cursor；不能把 device timestamp
或提交的 normalized INPUT 当作产品 screen cursor authority。QPC 仅用于
ordering/统计，不凭时间流逝触发新的 movement work。

UP 和 failure/authority lifecycle 不参与 movement 丢弃。Actual raw UP
终结 takeover；不执行 cleanup UP 作为成功 END，不让 queued movement
在 UP 后写入。最后 actual P/V 必须等于最终 cursor-derived target，
无 pending write、无 native modal state、无 reassertion；若 owner 当前
cursor 在最后 movement 仍落后，保留该反例，不能用 SendInput target
补写或 timer 重采样获得 PASS。

## 适用测试与研究 gate

Pure fixtures 至少验证用户指定 A–H：pre-END settlement allowed、post-END
restoration fail、first-write-before-END fail、丢 pre-cancel delta fail、
full original delta pass、native DRAG-after-END fail、raw continuation +
prewrite actual terminal stability pass、双 handoff write fail。另核对
Bottom 非参与边、checked overflow/native range、UP 后写、lag 不能 fake target、
P/V exact mismatch 与 missing context。Synthetic PASS 不代替 native 实验。

执行顺序固定为 Debug Move 一次完整 positive → Debug Bottom Resize 一次
→ fresh owned windows 的 Debug 20/20 + Release 20/20。明确 counterexample
立即停止；Move 未通过不进入 Resize，任一失败不接 Magnet/Explorer。
各 latency p50/p95 使用实际 receipt/native QPC 和真实样本数，不设新 SLA。
Automated geometry/QPC 不能证明 DWM 是否向用户呈现中间 restoration frame：
`VISUAL_TERMINAL_RESTORE_FLICKER = NOT_HUMAN_TESTED`。

`PRIOR_ART_SOURCE_LICENSE_HISTORY = PASS`（沿用记录）；
`OFFICIAL_END_CONTRACT_REVIEW = PASS`；`CHECKED_PLATFORM_BRIDGE_REVIEW = PASS`。
它们只支持上述严格 exact 的独立 test-owned probe 设计，native End/
handoff/free takeover `NOT TESTED`，20/20 未运行，候选 architecture
`UNRESOLVED`。缺失 async policy 单列，不以不存在的策略授予 acceptance。
无 product Raw Input、SendInput、global mouse hook、DLL injection、polling，
无 Human UAT；`WH_MOUSE_LL` 继续 research-only，`R1C4B_HUMAN_UAT = NOT_READY`。
