# PaneBind R1-C2B Explorer Glue Feedback Research

Status: **PRIOR-ART GATE PASS; IMPLEMENTATION READY; HUMAN UAT PENDING**

Review date: 2026-09-03.

## Scope and evidence boundary

R1-C2B researches one temporary Glue Move session between exactly two newly
created, separately user-consented, Explorer test windows. It covers event
ingress, owner-thread serialization, frozen topology, follower-operation
feedback attribution, abort/completion, and exact cleanup restore. It does not
authorize generic third-party control, Glue Resize, Snap, persistent groups,
global input, or R1-C3 behavior.

Evidence labels in this document are:

- **FACT** — directly supported by a pinned upstream source/history item,
  official Microsoft documentation, or a named PaneBind observation;
- **INFERENCE** — a bounded conclusion from those facts; and
- **PANEBIND DECISION** — the independent fail-closed contract selected for
  this round.

No external implementation code was copied, adapted, translated, or
mechanically rewritten.

## Sources and license boundary

| Source | Revision | License / use |
| --- | --- | --- |
| [AltSnap](https://github.com/RamonUnch/AltSnap) | [`5c86416ad21e4b72844a998a746bd3bb0bee5f5d`](https://github.com/RamonUnch/AltSnap/commit/5c86416ad21e4b72844a998a746bd3bb0bee5f5d) | GPL-3.0-or-later; **REFERENCE ONLY** |
| [AltDrag](https://github.com/stefansundin/altdrag) | [`e2740d605b0336a3b391fec26794718864b19521`](https://github.com/stefansundin/altdrag/commit/e2740d605b0336a3b391fec26794718864b19521) | GPL-3.0-or-later; **REFERENCE ONLY** |
| [PowerToys / FancyZones](https://github.com/microsoft/PowerToys) | [`19c4d805321db86f3634e6968e14dbf25cbba14a`](https://github.com/microsoft/PowerToys/commit/19c4d805321db86f3634e6968e14dbf25cbba14a) | MIT; reference-only in R1-C2B |
| Microsoft Win32 documentation | live pages reviewed 2026-09-03 | facts paraphrased and cited |
| PaneBind R1-B / R1-C1 / R1-C2A evidence | merged reports at starting main `8ac18ab` | local empirical evidence |

GPL sources remain reference-only. FancyZones is also reference-only in this
round; any future code reuse needs a separate provenance/attribution decision.

## Existing PaneBind empirical baseline

R1-B programmatic placement produced LOCATION feedback without natural
START/END. R1-C1 companion placement likewise recorded
`START/LOCATION/END = 0/26/0`. R1-C2A's two real Explorer runs each reliably
separated primary `0/1/0` and restore `0/1/0`; creation/navigation LOCATION
events preceded operation markers, and unrelated events interleaved between
primary and restore.

**PANEBIND DECISION.** WinEvent is asynchronous observation and behavior input,
not native-operation success. A follower operation succeeds only by its native
receipt and exact post-verification. Feedback logic must support missing,
duplicate, delayed, and interleaved LOCATION without requiring START/END or a
fixed event count.

## AltSnap and AltDrag

### Synthetic lifecycle and cross-tool feedback

**FACT.** AltSnap's pinned
[`NotifySizeMoveStaEnd`](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L576-L588)
posts `WM_ENTERSIZEMOVE` / `WM_EXITSIZEMOVE` and explicitly calls
`NotifyWinEvent(EVENT_SYSTEM_MOVESIZESTART/END)`. Its programmatic movement
path can therefore manufacture the same event identifiers observed for native
title-bar movement.

The behavior was added for external tiling tools in
[PR #564](https://github.com/RamonUnch/AltSnap/pull/564) / commit
[`2ed9fe3`](https://github.com/RamonUnch/AltSnap/commit/2ed9fe3dff49b260c25ce9abbd71f541dbfc1ca0),
then [issue #572](https://github.com/RamonUnch/AltSnap/issues/572) reported that
it unintentionally activated FancyZones. [PR #573](https://github.com/RamonUnch/AltSnap/pull/573) /
commit [`1b64b08`](https://github.com/RamonUnch/AltSnap/commit/1b64b08fb1db262b6f0a180b022243956c8a016e)
made notification configurable.

**FACT.** [Issue #575](https://github.com/RamonUnch/AltSnap/issues/575) showed
that sending START before the drag threshold created false sessions.
[PR #580](https://github.com/RamonUnch/AltSnap/pull/580) / commit
[`45ec7b4`](https://github.com/RamonUnch/AltSnap/commit/45ec7b4343ea4b8c6342cb1974933756367b022b)
delayed START until actual movement, while the pinned
[`Escape path`](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L2823-L2843)
can still produce an END edge case. START/END existence is therefore neither
user-intent proof nor a complete lifecycle contract.

**PANEBIND DECISION.** Only an exact, live Leader capability can start a Glue
session. `LOCATION before START`, `END before START`, a second START, and a
Follower START are explicit invalid transitions. A synthetic fixture may call
`NotifyWinEvent`, but must label those events `SYNTHETIC` and cannot substitute
for human Explorer evidence.

### Worker serialization and missing ledger

**FACT.** AltSnap's pinned
[`WorkerThread`](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L693-L735)
serializes movement work through a thread queue, coalesces consecutive mouse
movement, and suppresses new mouse-movement work while finish is queued or
executing. [PR #609](https://github.com/RamonUnch/AltSnap/pull/609)
further moved mouse work off the hook path. This supports a small callback and
one behavior owner, but AltSnap has no session generation, expected-geometry
receipt ledger, bounded feedback queue, overflow abort, or exact
post-verification.

**FACT.** Its multi-window implementation is sticky resize of directly touching
windows, not group move. The open request for moving adjacent groups remains
[issue #507](https://github.com/RamonUnch/AltSnap/issues/507); broader
same-axis/non-direct StickyResize propagation remains an open request in
[issue #620](https://github.com/RamonUnch/AltSnap/issues/620).
The pinned WinEvent consumer experiment is disabled code, not a production
feedback-suppression precedent.

**FACT.** AltDrag's pinned implementation sends lifecycle window messages but
does not call `NotifyWinEvent`. Historical compatibility exceptions and its
injected `WH_CALLWNDPROC`/subclass path show why synthetic lifecycle and DLL
injection are unsuitable PaneBind foundations.

**PANEBIND DECISION.** PaneBind independently implements a generation-bound,
bounded receipt ledger and initial-relative R1-A planning. No AltSnap/AltDrag
control flow, sticky algorithm, worker design, or injection mechanism is
copied or adapted.

## PowerToys / FancyZones

### Callback-to-owner separation

**FACT.** The pinned FancyZones wrapper installs global out-of-context WinEvent
hooks. LOCATION is armed only after START and, on successful `UnhookWinEvent`,
is unhooked in END callback before the END message reaches the hidden owner
window; see
[`FancyZonesApp.cpp`](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZones/FancyZonesApp.cpp#L96-L160).
The backend hidden window serializes lifecycle, topology, display, destroy, and
configuration work; see its
[`owner handler`](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/FancyZones.cpp#L961-L997).

On the normal successful-unhook path, this lifecycle prevents final Snap
placement feedback from re-entering the drag LOCATION hook, but it is not a
receipt-based suppression scheme and does not cover live follower movement
while a Leader drag remains active.
`WINEVENT_SKIPOWNPROCESS` excludes only PowerToys' process; it does not exclude
feedback caused when PowerToys moves another process's window.

**FACT.** The callback forwards only a window handle through `PostMessageW`;
the return is not checked, and the pinned subtree has no explicit monotonic
receipt sequence, bounded event queue, overflow diagnostic, operation
generation, or expected-geometry acknowledgement ledger. LOCATION/END handlers
also rely on current snapper state rather than revalidating an exact role-bound
capability.

**PANEBIND DECISION.** Adopt only the architectural lesson that callbacks do
minimal ingress and an owner thread owns behavior. PaneBind must preserve event
envelopes, check notification failure, use a bounded queue, revalidate exact
Leader/Follower roles, and fail closed on overflow or ambiguity.

### Invalidation and topology history

**FACT.** [PR #48569](https://github.com/microsoft/PowerToys/pull/48569) /
commit [`dd26d865`](https://github.com/microsoft/PowerToys/commit/dd26d86580168d2e368701f7b0c4d629dc9cd9ac)
added destroy handling so a dragged window's destruction aborts rather than
snapping a dead handle.

**FACT.** [PR #48473](https://github.com/microsoft/PowerToys/pull/48473) /
commit [`ae9f241`](https://github.com/microsoft/PowerToys/commit/ae9f241ef13737dab6f861767bbfdfca72b78475)
fixed a dangling work-area pointer by ending its consumer before topology
replacement. [PR #49985](https://github.com/microsoft/PowerToys/pull/49985) /
commit [`d68980a`](https://github.com/microsoft/PowerToys/commit/d68980a81bb8de144bdec998a114e948bf68c563)
then changed replacement from normal completion to abort. [PR #49433](https://github.com/microsoft/PowerToys/pull/49433) /
commit [`37d8729`](https://github.com/microsoft/PowerToys/commit/37d8729ac3eec734f4d000079145d6fcb40db3a5)
fixed an existing WorkArea that refreshed custom-layout shape from the live
store while retaining stale scalar policy from the applied-layout snapshot.

**FACT.** [PR #44440](https://github.com/microsoft/PowerToys/pull/44440) /
commit [`6c2a99d`](https://github.com/microsoft/PowerToys/commit/6c2a99dfd6a12ad98feeda0acbc663aa84865676)
fixed mixed-DPI coordinate interpretation across components.

**PANEBIND DECISION.** Freeze one value-owned topology and generation at
Leader START. Navigation, destroy, state/security, monitor/DPI, work-area, or
capability changes abort rather than complete. Stop/unhook the consumer before
cleanup restore or topology replacement; never retain raw topology pointers.

## Microsoft Win32 contracts

### Out-of-context delivery and lifecycle

[`SetWinEventHook`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwineventhook)
documents that `WINEVENT_OUTOFCONTEXT` does not inject into the event process;
events are queued asynchronously and delivered sequentially on the installing
thread, which must have a message loop. It also warns that callback processing
can be reentered and complete out of sequence.

[`UnhookWinEvent`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-unhookwinevent)
must be called from the installing thread and may fail for an invalid, already
removed, or cross-thread hook. Successful unhook prevents later callback calls,
but already copied local receipts still require generation-aware drain/discard.

**PANEBIND DECISION.** A dedicated owner STA installs process-filtered START/END
and LOCATION hooks after setup. The callback only validates the hook instance,
copies a fixed event envelope including `hwnd`, `idObject`, `idChild`, event
thread, and native timestamp, assigns a local monotonic receipt sequence,
pushes to a bounded queue, and posts one drain notification. It performs no
COM, geometry, topology, logging, blocking wait, or window operation.
Reentrancy, overflow, post failure, wrong-thread use, or unhook failure poisons
the session.

### Event meaning and synthetic events

Microsoft's [event constants](https://learn.microsoft.com/en-us/windows/win32/winauto/event-constants)
describe START/END as move-or-resize lifecycle and LOCATION as a change of
location, shape, or size. They do not distinguish Move from Resize or specify
one-request-one-event cardinality. [`NotifyWinEvent`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-notifywinevent)
allows accessible servers to signal predefined events, confirming that the
identifier alone is not provenance.

The [`WinEventProc`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nc-winuser-wineventproc)
timestamp is only documented as a DWORD millisecond event-generation time. No
epoch, uniqueness, relation to `GetTickCount64`, callback latency, or cross-hook
total order is specified.

**PANEBIND DECISION.** Native timestamp is diagnostic only. Local receipt
sequence, exact role/token/session generation, retained process identity, and
captured geometry drive behavior. Process/thread hook filters reduce noise but
never grant authority. Only an exact target root-window receipt with
`idObject == OBJID_WINDOW` and `idChild == CHILDID_SELF` can normalize to a
Leader/Follower geometry event. Caret, accessible child, non-root, and other
object receipts are ignored or recorded diagnostically and never drive Glue.

### Geometry and native apply

[`SetWindowPos`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowpos)
nonzero return is API success, not exact geometry proof. `SWP_NOSIZE`,
`SWP_NOZORDER`, and `SWP_NOACTIVATE` preserve the intended boundaries;
`SWP_ASYNCWINDOWPOS` posts in cross-input-queue cases and is rejected for exact
post-verification. Applications may modify a request during
[`WM_WINDOWPOSCHANGING`](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-windowposchanging).

[`GetWindowRect`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowrect)
returns a DPI-virtualized screen rectangle with exclusive right/bottom and may
include invisible resize borders. `DWMWA_EXTENDED_FRAME_BOUNDS`, read through
[`DwmGetWindowAttribute`](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/nf-dwmapi-dwmgetwindowattribute),
is a distinct visible screen-space rectangle and is not DPI-adjusted. These
queries are not an atomic historical event snapshot.

[`GetWindowThreadProcessId`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowthreadprocessid)
provides point-in-time PID/TID facts. It does not prevent handle reuse or the
TOCTOU documented for [`IsWindow`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-iswindow);
[`PROCESS_INFORMATION`](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/ns-processthreadsapi-process_information)
also documents identifier reuse after the corresponding object is released.

**PANEBIND DECISION.** Register the pending follower identity before native
apply. Use the existing visible-to-positioning bridge, synchronous
`SetWindowPos` flags, live eligibility, retained process identity, and exact
post-read receipt. Restore has its own preflight/native result/post-verification
and is not rollback.

## Platform-neutral Glue behavior contract

The core coordinator owns no native handle or OS concept. It accepts normalized
role events, immutable `WindowId`, visible geometry, session/operation
generation, and operation outcomes. Its explicit states are at least Idle,
Armed, Active, Completing, Completed, and Aborted.

Only exact Leader START can transition Armed to Active. At that transition the
Windows layer has already revalidated both targets and supplies an R1-A graph
whose Leader component is exactly `{Leader, Follower}` with one relation. The
coordinator creates an R1-A `TranslationSession`; topology, membership, roles,
and initial geometry remain frozen.

For each Leader LOCATION:

- `Unchanged` is ignored;
- `Translation` consumes the existing R1-A total-delta plan;
- a target already represented by current/pending exact geometry is a no-op;
- `ResizeOrMixed` aborts; and
- no incremental follower delta is accumulated.

Each emitted follower operation receives an operation ID before the Windows
layer calls `SetWindowPos`. The bounded pending ledger records session,
operation, source leader sequence, follower identity/generation, expected
visible/positioning geometry, native outcome, actual geometry, and exact
post-verification.

Follower LOCATION matching a pending expected/actual geometry is acknowledged
and suppressed. A repeated current acknowledged geometry is duplicate
self-feedback and suppressed. Any other Follower geometry or Follower START is
unexpected interaction and aborts. Missing feedback does not fail an exact
native operation; END can reconcile completed pending entries by operation
receipt and exact final snapshot without pretending an event was acknowledged.

On exact Leader END the coordinator enters Completing, rejects new plans,
verifies final Leader translation and final Follower R1-A target, reconciles
pending feedback, and completes. Queue overflow, generation mismatch,
invalidation, timeout, native/post-verification failure, final mismatch, or
illegal transition aborts. Completed/aborted generations never execute late
feedback.

## Setup and cleanup isolation

The UAT fixture records both original geometries, then uses a separately
consented Glue authority to make a zero-tolerance horizontal or vertical
two-window layout wholly inside one work area, without resize, activation,
z-order change, or cross-monitor movement. This is **TEST FIXTURE LAYOUT ONLY**,
not Snap.

Only after exact layout verification and R1-A adjacency construction does the
Glue event source arm. On completion or abort, it stops and explicitly unhooks
before exact independent restore of both windows, preventing setup/cleanup
feedback from entering active behavior. User-created Explorer windows are
never automatically closed.

## Rejected designs

- driving Glue from R0 Observer JSONL or changing R0 Observer semantics;
- raw HWND, PID/TID, class, process filter, START/END, or timestamp as authority;
- feedback suppression by a 50/100 ms window or callback proximity;
- requiring one event per native request, mandatory follower START/END, or an
  empty pending ledger at END;
- running COM, geometry capture, behavior, logging, or `SetWindowPos` inside a
  hook callback;
- silent queue drop, overwrite-oldest, or retry-until-success;
- incremental follower movement from previous follower position;
- dynamic topology/component membership during the drag;
- treating invalidation as successful END;
- `SWP_ASYNCWINDOWPOS`, `AttachThreadInput`, foreground forcing, input hooks,
  injection, polling, Snap, Glue Resize, or generic third-party capability; and
- automatically closing either user-created Explorer window.

## Research gate

```text
R1C2B_PRIOR_ART_GATE = PASS
ALTSNAP_ALTDRAG_LICENSE = GPL REFERENCE ONLY
POWERTOYS_LICENSE = MIT REFERENCE ONLY FOR R1-C2B
EXTERNAL_CODE_COPIED = NO
EXTERNAL_CODE_ADAPTED = NO
R0_OBSERVER_CONTROL_BUS = REJECTED
R0_OBSERVER_SEMANTICS_CHANGED = NO
R1C2A_SEMANTICS_CHANGED = NO
R1C2B_IMPLEMENTATION = NOT STARTED AT THIS CHECKPOINT
R1C3 = NOT STARTED
```

## R1-C2B UAT Fix 2：处理节奏与实时证据补充

复核日期：2026-09-05。此节为 Fix 2 的定向研究 checkpoint；上文初始
checkpoint 与 Attempt 2 的旧 gate PASS 均保留。新的真实 Explorer UAT
尚未执行，调度策略的实际多步跟随效果仍须真人 UAT 验收。

### 实际检查的源码、历史与许可

**FACT.** 本轮重新读取现有本地 Git checkout 的 immutable HEAD 与许可：

| 来源 | 本轮实际检查 | 适用结论与边界 |
| --- | --- | --- |
| AltSnap `5c86416ad21e4b72844a998a746bd3bb0bee5f5d` | `hooks.c` 文件许可头、`WorkerThread`（约 693–735 行）、`License.txt`；本地 `git show 7f4afe59076b70980f71af202f63609ca3ac5745` 与 [PR #609](https://github.com/RamonUnch/AltSnap/pull/609) | 工作线程合并连续 movement 消息、随后执行最后一个位置；历史明确将鼠标移动工作移出 hook 路径。这是小 callback 与合并工作的参考，不能证明 PaneBind 的实时性。GPL-3.0-or-later，REFERENCE ONLY。 |
| AltDrag `e2740d605b0336a3b391fec26794718864b19521` | `hooks.c` 许可头、`WM_ENTERSIZEMOVE`/`WM_EXITSIZEMOVE` 兼容路径和 `LICENSE` | 继承历史仍涉及 lifecycle window messages；不提供 WinEvent 历史几何或 PaneBind feedback ledger。GPL-3.0-or-later，REFERENCE ONLY。 |
| FancyZones `19c4d805321db86f3634e6968e14dbf25cbba14a` | `FancyZones/FancyZonesApp.cpp` 96–181 行、`FancyZonesLib/FancyZones.cpp` owner handler 961–997 行、根 `LICENSE`；[PR #48569](https://github.com/microsoft/PowerToys/pull/48569) 与 [commit dd26d865](https://github.com/microsoft/PowerToys/commit/dd26d86580168d2e368701f7b0c4d629dc9cd9ac) 的网页 diff | 回调转交 owner，destroy 必须 abort；调度合并不能跨越或丢弃生命周期/失效边界。MIT，本轮仅参考，不复制代码。 |

AltSnap 的 worker 历史为
[`7f4afe59076b70980f71af202f63609ca3ac5745`](https://github.com/RamonUnch/AltSnap/commit/7f4afe59076b70980f71af202f63609ca3ac5745)。
其现有合并循环不是 PaneBind 的 bounded WinEvent scheduling 模板；本轮
独立从 PaneBind 队列、authority 和测试要求设计 quantum，不翻译或改写 GPL
控制流。FancyZones 的 late placement/unhook 也不能替代 active follower
操作的精确反馈匹配。

检查方法限制：FancyZones 既有 partial clone 的旧 commit `git show` 触发
promisor fetch 后遇到 GitHub 连接失败，该同步工作已停止。其 pinned HEAD
源码与许可可本地读取；上述 PR 和 immutable commit 的网页 diff 已实际
检查。未用 API 重建 Git objects、移动 refs 或模拟 fetch。

### 官方平台契约

**FACT.** [SetWinEventHook](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwineventhook)
和 [Out-of-Context Hook Functions](https://learn.microsoft.com/en-us/windows/win32/winauto/out-of-context-hook-functions)
说明 out-of-context notifications 异步排队、在安装 hook 的消息循环线程交付；
慢 callback 会增加 USER 资源压力。[WinEventProc](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nc-winuser-wineventproc)
的参数只有事件、对象身份、线程和生成时间，没有窗口 rectangle。
[GetWindowRect](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowrect)
读取指定窗口的 rectangle，没有按 WinEvent 时间取历史状态的接口。

**INFERENCE.** 晚处理 receipt 后读取的 live geometry 不能被逐条贴回历史
LOCATION 并称为 event-time observation。多个 receipt 在同一处理 quantum
共用一次采样时，证据必须明确只有一个 processing-time sample；只有后续
quantum 再读取几何，才是新的采样机会。即使后续几何相同，也不应制造第二个
native apply。

**FACT.** [PeekMessageW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-peekmessagew)
自身会分派 pending nonqueued messages，并可能处理 system internal events，
而后才返回一个可见 MSG 或 false。因此“一次 PeekMessage/DispatchMessage”
不等于“只交付一个 WinEvent callback”。仅在可见 MSG 的循环体内检查 ring
会遗漏 `PeekMessage == FALSE` 前已发生的 callback ingress。
[MsgWaitForMultipleObjectsEx](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-msgwaitformultipleobjectsex)
的 `MWMO_INPUTAVAILABLE` 能唤醒已经看过但尚未移除的 queued input；bounded
pump 之后仍有消息时，不应仅等待一条全新的消息。

**FACT.** [Guarding Against Reentrancy](https://learn.microsoft.com/en-us/windows/win32/winauto/guarding-against-reentrancy-in-hook-functions)
列出消息检索、SendMessage 与跨进程 COM 调用造成重入的风险；
[Single-Threaded Apartments](https://learn.microsoft.com/en-us/windows/win32/com/single-threaded-apartments)
说明 STA 通过消息分派 COM 调用，而且发起 ORPC 或泵消息时可以被重入。

**INFERENCE.** 将复杂 capture 搬到 callback 会扩大重入窗口；把它保留在
owner 也不意味着整个 capture 是不可重入的原子区间。同步 COM inspection
期间到达的 WinEvent 只能进入有限 ring，不能递归执行 Glue behavior。bounded
外层 MSG quantum 不构成 COM 调用时长、callback 数量或最终 smoothness SLA。

### PaneBind 现有代码审计与独立决策

**FACT.** Starting HEAD `e0bccc0e8ab8870150fa79b9f1a70cdf1c902db5`
的 `ExplorerGlueSession::run_until_terminal_impl` 在一次 wake 后用无界
`while (PeekMessageW(...))` 排空消息，然后才回到 event-source drain。
`drain_event_source` 则为一次 drained batch 仅 capture 一次 Leader/Follower，
把同一 rectangle 交给每个 LOCATION receipt。此组合支持
`BATCH_SNAPSHOT_COLLAPSE` 的代码因果链；Attempt 2 的具体 receipt、drain、
geometry 和 operation 对应关系以
[执行报告](../reports/R1C2B_EXECUTION_REPORT.md) 的原始证据解析为准。

**FACT.** `explorer_session.cpp::validate_native_target` 的 user-consent
分支还会显式等待 Shell activity：20 ms deadline、外层 64 次预算。
`ExplorerConsentTargetObservation::pump_until_activity` 内部按自身 message
budget 读取可用消息。它不是每次绝对无限的 pump，但对一次 Glue
capture/preflight/postverify 引入了不必要的 nested wait/drain 机会。
`ExplorerGlueEventSource::drain_owner_queue` 已在 drain 开始时清除
`notification_pending_`，现有 empty→non-empty 通知机制本身没有证据表明
丢失 rearm；应补回归证明，不应未经证据重写 callback。

**PANEBIND DECISION.** Fix 2 采用以下有限调整：

1. 主 owner 每次 wake 最多处理小固定数量的可见 MSG（实现选择 8）；每次
   PeekMessage/Dispatch 后，若 target ring 有数据，立即让出给 drain。
   `PeekMessage == FALSE` 时仍允许下一轮 drain 处理刚刚进入的 receipts。
   等待继续使用事件与现有 session deadline，不新增 resident timer 或 polling。
2. 仅 Glue 已绑定 authority 的私有路径跳过上述主动 Shell activity waiting，
   保留 canonical identity、当前 HWND/location/inventory、订阅健康、导航失效、
   security、monitor/DPI 与 native postverify 检查。R1-C2A 默认路径保持原义。
   COM 本身仍可能重入，不以禁用检查换取频率。
3. 一次有 Leader LOCATION 的 processing quantum 至多产生一个明确标注为
   `live_geometry_at_processing_quantum` 的 Leader 样本。记录 raw receipt
   sequence/native timestamp 与 quantum/sample generation 的映射；选择本
   quantum 最新有效 Leader LOCATION 驱动 R1-A total-delta。先前 receipt
   仍保留为 coalesced 输入，不伪造各自的历史 geometry。
4. START/END、Follower lifecycle 和 destroy 是必须保留的边界。LOCATION 与
   END 同批时先处理最后一次可观察 translation，再做 END reconciliation；
   ResizeOrMixed 和 identity/navigation invalidation 仍 abort。END 同批的
   final apply 不应单凭其较小的 source LOCATION sequence 宣称“已在原生 END
   发生前执行”。
5. raw receipt 顺序、sample 顺序、behavior/native operation 顺序分别记录。
   新 gate 用多 quantum、两个 distinct samples/targets、两个 exact active
   applies 及至少一个可明确排在 END 之前的 apply 共同验收；同批 final catch-up
   本身不能证明 realtime progression。native timestamp 仅诊断，不当作统一
   monotonic clock，也不建立 apply/LOCATION 比率。
6. 不改 pending identity、exact feedback matching 或 missing reconciliation
   语义。多个 exact native receipts 中部分没有 LOCATION 仍可在 final exact
   时 reconcile；不制造 feedback，不重试 native apply 来提高计数。

### 研究 gate 与待验证项

针对上述独立设计的研究 gate 为 PASS；其验收测试必须覆盖 multiple wake
cycles、一个 quantum 内的合并、next-quantum 新几何、END 同批、same-sample
no-op、notification rearm、mixed missing feedback，以及 100+ raw receipts
跨多个 quantum 的有界队列/no recursion/no drift/final exact。具体测试结果
由本轮执行报告记录，不由源码研究替代。

```text
R1C2B_FIX2_PRIOR_ART_GATE = PASS
EXTERNAL_CODE_COPIED = NO
EXTERNAL_CODE_ADAPTED = NO
CALLBACK_COMPLEX_CAPTURE = REJECTED
EVENT_TIME_GEOMETRY_FROM_LATE_SNAPSHOT = REJECTED
FIXED_RATE_POLLING = REJECTED
REALTIME_FOLLOW_RUNTIME_EVIDENCE = PENDING_UAT
R0_OBSERVER_SEMANTICS_CHANGED = NO
R1C3 = NOT STARTED
```
