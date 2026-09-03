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
