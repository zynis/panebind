# R1-C4B Pivot 1 Fix C — END witness and structured diagnostics

研究日期：2026-09-27。起始提交：`10d429706b8afb3be11f308370af89964a5a054a`；分支：`codex/r1c4b-live-magnet`。

`RESEARCH_GATE = PASS` 仅授权本轮独立 test-owned probe 的诊断、WinEvent END witness、受约束 cleanup 及相应 synthetic tests。新 barrier 的真实可靠性、Bottom Resize 原 blocker 的具体原因、重复 gate 均为 `NOT TESTED`，必须由新证据决定；不是产品输入接管或 Explorer 的 research/acceptance PASS。本研究未运行 GUI、发送输入、修改 native implementation 或改写旧证据。

## 1. 保留的事实与未解问题

[Fix B execution report](../reports/R1C4B_PIVOT1_FIXB_EXECUTION_REPORT.md) 的真实 Move 单次完整正例保持有效：`OWNED_NATIVE_CANCEL_MOVE = PASS_WITH_TERMINAL_SETTLEMENT`，Move handoff、takeover、pre-release control 均 PASS；one reconciliation、18 Raw-driven continuation writes、full original intent、Raw UP、exact readback、零 post-END native reassertion。Resize 已真实 ENTER / WMSZ_BOTTOM / cancel / capture release / terminal settlement / held-button EXIT，但旧 compound preflight 在 first write 前失败，native writes 为 0：Resize cancel `UNKNOWN`、takeover `NOT_RUN`，不是 FAIL。

旧 Fix A/B 的报告、JSONL、metadata 和 research 不更改、不追认。原 `handoff_terminal_stability_failed` 不能区分 authority、GUI state、cursor guard、P/V availability 或几何不稳定；本轮必须先完整记录事实，再分项 fail-closed。首次新运行必须是 Debug BottomResize ×1，而不是先重跑 Move。

## 2. 本轮实际检查的 prior art / history / license

以下是本轮重新实际读取，不只是继承旧 provenance；既有 pinned SHA 沿用，未引入新 upstream project。全部 reference-only，无复制、改写、翻译、控制流适配或新增代码归属义务。

| 来源与精确版本 | 本轮实际检查范围 | 可采用的研究结论 / 不可采用的结论 |
| --- | --- | --- |
| [AltSnap](https://github.com/RamonUnch/AltSnap/tree/5c86416ad21e4b72844a998a746bd3bb0bee5f5d)，成熟项目；GPL-3.0-or-later | 本地 clean checkout HEAD；`hooks.c` license header 与 `License.txt` 开头；`NotifySizeMoveStaEnd`、`HandleWinEvent` / `PinWindowProc`、pin destruction 与 movement-end notification 范围 | 生命周期通知与 hook resource owner 必须明确；其 `EVENT_HOOK` pin 分支为条件编译实验路径，其 timer、thunk、synthetic WM/WinEvent 不导入 PaneBind。项目成熟不等于此路径已验证。 |
| AltSnap [issue #572](https://github.com/RamonUnch/AltSnap/issues/572)；[commit 8a5c422928d37d34e63cb8b99d4a74e14cb955f0](https://github.com/RamonUnch/AltSnap/commit/8a5c422928d37d34e63cb8b99d4a74e14cb955f0) | issue body；本地实际 `altsnap.c` history diff（禁用时不释放 hooks DLL） | 跨工具生命周期兼容与 callback code lifetime 是真实维护问题；不是本轮 native END 实证。AltSnap 的 synthetic NotifyWinEvent/WM_EXITSIZEMOVE 绝不能作为 PaneBind 的真实系统 END。 |
| [PowerToys/FancyZones](https://github.com/microsoft/PowerToys/tree/19c4d805321db86f3634e6968e14dbf25cbba14a)，成熟生产参考；MIT | pinned [LICENSE](https://raw.githubusercontent.com/microsoft/PowerToys/19c4d805321db86f3634e6968e14dbf25cbba14a/LICENSE) 全文；[WindowMouseSnap.cpp](https://raw.githubusercontent.com/microsoft/PowerToys/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/WindowMouseSnap.cpp) 全文 | Start/Update/End 与 Abort 是不同生命周期；teardown 不能被当成成功拖动。其 snapping、overlay、transparency 等策略不导入本 probe。 |
| FancyZones [PR #48569](https://github.com/microsoft/PowerToys/pull/48569)；[merge dd26d86580168d2e368701f7b0c4d629dc9cd9ac](https://github.com/microsoft/PowerToys/commit/dd26d86580168d2e368701f7b0c4d629dc9cd9ac) | PR body 与 rendered merge diff：destroy event subscription/routing、drag Abort/reset、modifier-state cleanup | 目标销毁必须结束/清理状态，不可漏掉失败 teardown。PR 的 upstream manual tests 不是 PaneBind observation；未声称本轮重读整个 FancyZones.cpp / FancyZonesApp.cpp。 |

## 3. 官方平台合约

本轮实际读取以下 Microsoft Learn live primary pages；不声称固定文档 revision，也未复制 examples。

- [SetWinEventHook](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwineventhook)：`WINEVENT_OUTOFCONTEXT` 在 client 进程执行，不把 DLL 映射进 source；事件 queued/asynchronous，安装线程必须有 message loop，callback 在该安装线程递送。按 exact process / thread 限制注册 START 到 END 范围。不能加 `WINEVENT_SKIPOWNPROCESS`，也不能加 `WINEVENT_SKIPOWNTHREAD`，否则会屏蔽本轮 owned source。
- [Event Constants](https://learn.microsoft.com/en-us/windows/win32/winauto/event-constants)：`EVENT_SYSTEM_MOVESIZESTART = 0x000A`；`EVENT_SYSTEM_MOVESIZEEND = 0x000B` 表示 window movement/resizing finished。此语义不是“SetWindowPos 可见结果已同步到 DWM”的承诺。
- [WinEventProc](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nc-winuser-wineventproc)：envelope 包含 hook、event、HWND、object/child id、event thread、event-generation milliseconds。`dwmsEventTime` 不是 QPC；callback QPC 是本进程收到通知的时间，不是 OS 内部退出时刻，不包含 source geometry。
- [Out-of-Context Hook Functions](https://learn.microsoft.com/en-us/windows/win32/winauto/out-of-context-hook-functions)：系统异步 marshals event，保证 arrival order；callback 返回后释放对应系统资源。callback 不宜慢或积压。
- [Guarding Against Reentrancy](https://learn.microsoft.com/en-us/windows/win32/winauto/guarding-against-reentrancy-in-hook-functions)：SendMessage、GetMessage / PeekMessage、dialogs、accessibility/COM 等可在 callback 内引起重入，因此 callback 完成顺序不能被简单等同于事件 arrival order。禁止在 hook callback 中做 capture、geometry、COM、message pumping 或 native write。
- [UnhookWinEvent](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-unhookwinevent)：必须由安装 hook 的同一线程卸载；无效/重复 handle、另一线程调用会失败。线程结束自动卸载不替代本轮明确的 installed / removed 记录与 checked return。
- [GetAsyncKeyState](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getasynckeystate)：仅 high bit 表示当前 down；low bit 的“上次以来按下”不能可靠采用。0 还可能由 desktop/access 条件导致，不能单凭全 0 断言 authority。鼠标检查是 physical button，不能忽略 swapped-button mapping。只做 event-triggered / bounded safety snapshot，不变成 poller。
- [SendInput](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput)：合成事件进入全局输入流，不能指定 HWND；UIPI 限制 equal/lower integrity，return/GetLastError 不能确定 UIPI 为阻断原因，也不会重置现有键状态。必须检查插入 count。文档没有承诺每个合成鼠标输入必定产生 WM_INPUT，或 Raw、cursor、async key state 同步更新。

这些官方合约支持一个可实现的观察/诊断方案，不证明新 barrier、cleanup delivery 或接管稳定性已经真实 PASS。原 Raw receiver 和 cursor 时序研究边界继续沿用 [input contracts](R1C4B_PIVOT1_INPUT_CONTRACTS.md)，没有新产品 Raw Input / SendInput 授权。

## 4. END matcher、线程与资源 ownership

每次 run 建立 fresh source、receiver、WinEvent hook。source/UI 身份固定；在 gesture 之前安装并检查非零 handle，之后同一安装线程 explicit Unhook 并记录 success。安装失败、移除失败、消息 envelope 丢失/overflow、receiver 失健康均不能被隐藏为 PASS；所有退出路径都保留 teardown 证据。

Hook callback 只给 envelope 增加 receipt QPC 与单调 sequence，并交给 owner 的有界消息/存储路径；不得在 callback 中开始 handoff、驱动输入、读取窗口/capture、泵消息或写窗口。hook instance/run generation 在安装时固定；owner 根据真正的 START/END envelope 配对，不在 callback 中把“当前全局 gesture”标签直接附给迟到 event 后视为匹配。

有效 END 必须是 exact source HWND、exact source UI thread、当前 hook/run、存在匹配本 gesture 的真实 START。wrong HWND/thread、无 START 的 END、旧 gesture 的 END 不解锁。不人为 SendMessage/NotifyWinEvent 来制造 native/WinEvent START/END。source identity 变化立即使 matcher authority 失效。

新 first-write barrier 同时要求真实 `WM_EXITSIZEMOVE`、匹配 `EVENT_SYSTEM_MOVESIZEEND` 和一次 fresh full proof。先到的 witness 只能缓存，不能写；两者都到后才执行 owner preflight。first native write 的 QPC 严格大于匹配 WinEvent END callback QPC，也必须发生在真实 native END 之后。native EXIT 与 WinEvent callback 属于不同观察路径，不预设其 QPC 差必为正；保留实际 sequence/QPC，记录 native EXIT→WinEvent END、WinEvent END→first write 的 latency / p50 / p95。receipt 时间不能冒充真实 OS exit instant。

## 5. 完整 HandoffPreflightDiagnostic

每次 attempt 无论 PASS/FAIL，先 `record("handoff_preflight", full diagnostic)`，再 fail closed。不可提前 require 丢失后半字段；不可仅记录一个 compound healthy bool。不可用 0 HWND/rect 伪造失败 query 的成功结果：保留 availability、nullable result、error/HRESULT 与采样 sequence/QPC，区分 unavailable 与 mismatch。

| 事实组 | 必须保留字段 |
| --- | --- |
| END | `end_observed`, `end_sequence`, `end_qpc`, `winevent_end_observed`, `winevent_end_sequence`, `winevent_end_qpc` |
| 身份 / 环境 | `own_identity`, `desktop_ready`, `source_visible`, `foreground_hwnd`, `foreground_matches`, `foreground_pid`, `foreground_tid`, `dpi`, `dpi_matches`, `monitor`, `monitor_matches` |
| GUI / capture | `gui_query_succeeded`, `capture_hwnd`, `capture_clear`, `menu_owner_hwnd`, `menu_clear`, `move_size_hwnd`, `move_size_clear`, `gui_flags`, `gui_in_movesize_clear`, `foreign_capture_transferred` |
| cursor / input | `cursor_success`, `cursor`, `cursor_root`, `cursor_root_owned_or_guard`, `buttons_modifiers_clear`, `left_down`, `raw_up_seen` |
| 健康 / lifecycle | `receiver_healthy`, `takeover_healthy`, `native_drag_after_end`, `unowned_geometry_changes` |
| terminal 与 actual | `actual_positioning_available`, `actual_visible_available`, `terminal_positioning`, `terminal_visible`, `actual_positioning`, `actual_visible`, `terminal_positioning_exact`, `terminal_visible_exact` |
| 结论 | `failure_class`，以及允许独立检查全部 failure predicate 的原始事实 |

`buttons_modifiers_clear` 表示除本 gesture 必须继续 held 的 left 之外的 button/modifier fence；不能要求 left clear 同时又要求 left_down。Raw UP 是 gesture lifecycle evidence，GetAsyncKeyState 只是 fresh safety crosscheck，不要求同一 Raw callback 的 async state 已同步。

最低 failure classes：`None`; `MissingNativeEnd`, `MissingWinEventEnd`; `IdentityChanged`, `DesktopUnavailable`, `SourceNotVisible`, `ForegroundChanged`; `GuiQueryFailed`, `CaptureStillOwned`, `MenuStillActive`, `MoveSizeStillActive`; `CursorUnavailable`, `CursorOutsideOwnedInputGuard`; `ButtonReleasedBeforeHandoff`, `RawUpAlreadyObserved`; `ReceiverUnhealthy`, `TakeoverAlreadyUnhealthy`, `ForeignCaptureTransferred`; `DpiChanged`, `MonitorChanged`; `ActualPositioningUnavailable`, `ActualVisibleUnavailable`; `TerminalPositioningMismatch`, `TerminalVisibleMismatch`, `BothTerminalGeometryMismatch`; `NativeDragAfterEnd`, `UnownedGeometryChange`。

若 class 只表达优先级第一项，仍须完整保留其他失败 predicates；“class == CursorOutsideOwnedInputGuard”不等于“只有 guard 失败”。外层 reason 可为 `handoff_preflight_failed`。validator 从字段与前序 envelope/geometry/Raw 独立重算，不信任 harness 的 PASS 或 failure_class；availability 失败不可被降成 exact mismatch。

## 6. Authority / settlement / original intent 的不同职责

authority fresh snapshot 决定是否可写；actual terminal P/V 决定 END 后是否稳定；original START geometry 与 original pointer anchor 决定用户完整 intent。三者不能互相替代。继续沿用 [Fix B END-barrier research](R1C4B_PIVOT1_FIXB_END_BARRIER.md) 中 checked visible→positioning bridge 与 strict exact readback；本轮不引入异步可见宽限、sleep、timer/poll、fixed retry 或 repeated SWP。

BottomResize target：`dy = current_cursor.y - original_HTBOTTOM_down.y`，`target_visible.bottom = original_start_visible.bottom + dy`；left/top/right 冻结。terminal EXIT rect 只是稳定性 baseline，不是新的 intent anchor；不得累加 previous corrected rect。Raw owner 当前 cursor snapshot 可反映合并后的最新位置，不声称是该 Raw packet 生成瞬间的指针点。

native END 已到而 GUI 仍 active：无写地等待匹配 WinEvent END，然后 fresh read 一次；若清除，可用 WinEvent barrier。terminal P exact / V mismatch 可记录 `DWM_VISIBLE_TERMINAL_LAG_CANDIDATE`，不是确诊 DWM bug；匹配 WinEvent END 后一次 fresh capture 仍 mismatch 则 FAIL。匹配 WinEvent END 后 P 变化且没有 PaneBind write / 新 native DRAG，是 END stability FAIL；匹配 END 后真实 MOVING/SIZING 也是 FAIL / STOP。

identity、desktop、foreground、DPI/monitor、receiver/authority failure 为环境 / authority BLOCKED，不扩大为 geometry contract failure。唯一 guard failure 才允许 test-owned `WS_EX_NOACTIVATE` guard 在下次 gesture 前预覆盖整个 planned Move / Bottom Resize trajectory +50px，随后只再运行 BottomResize ×1；不扩权限、不 retry-until-pass。

## 7. CleanupInputDiagnostic 与输入安全

cleanup 与 acceptance 是两个不可混用的通道。失败后先使 acceptance writer/lifecycle 失效；保留独立 cleanup diagnostic。只在 fresh exact source identity、desktop ready、global foreground source、无 foreign capture/menu/movesize、cursor root 为 own source/guard、left high bit down、modifiers clear 全部成立时，允许 exactly one test-only LEFTUP。不足 authority 时 NO INPUT，记录 `cleanup_skipped_no_authority`；不得用 AttachThreadInput、focus/capture 强制转移或人工结束 foreign loop 来补 authority。

记录 `cleanup_up_sent` 与插入 count、cleanup Raw UP observed / not observed、最终 GetAsyncKeyState high bit、capture、cursor（均含 query success）。允许的诊断消息交付/等待不得成为 polling 或重复 LEFTUP；如果 Raw UP 未收到不能伪造 delivery。cleanup UP 从不满足 gesture acceptance Raw UP、END witness 或完整 takeover gate；清理成功也不能将失败 gesture 改为 PASS。

## 8. 测试和执行 gate

synthetic 至少覆盖 brief A–P：compound proof 拆项、wrong WinEvent HWND/thread、END 无 START、native EXIT 缺 WinEvent END、匹配 END 后 GUI clear / 仍 active、仅 guard failure、多 failure 不得走 guard-only、P exact/V mismatch、P mismatch、post-END native drag、first write 太早、Bottom 非参与边变化、full original anchor 正确/错误用 EXIT anchor、cleanup valid UP 但 acceptance 仍 FAIL。还应覆盖 late old-gesture END、hook install/remove failure 与 partial/unavailable diagnostic 不默认为健康。

实际顺序由 main 执行：Debug BottomResize ×1 → 完整 PASS 才 Debug Move ×1 → 两者完整 PASS 才 Debug 5/5 / Release 5/5 → 全部 PASS 才 Debug 20/20 / Release 20/20；每 repetition fresh own resources。首次失败 STOP，保留 first failing evidence；只允许 brief 明确的 sole-guard repair 分支。Resize 需要真实 pre-cancel SIZING、两个 END、held/capture clear、one reconciliation、至少15个 remaining Raw movements、每 event 最多一次写、严格 exact、冻结非参与边、Raw UP、final exact。

此文档只完成研究设计 gate；`FIXC_STRUCTURED_HANDOFF_DIAGNOSTIC`、`FIXC_WINEVENT_END_BARRIER`、initial Resize failure class、owned gate、smoke/formal counts 和 latency metrics 由新 automated/empirical evidence 填写，当前不能宣称 PASS。完整 owned gate 后 STOP；Explorer 仅 `READY_FOR_NEXT_STAGE`，不进入 Explorer/Magnet/Human UAT。产品 Raw Input `NOT_IMPLEMENTED`；产品 SendInput/global mouse hook/DLL injection/polling 均 NONE。`WH_MOUSE_LL` 本轮禁止。
