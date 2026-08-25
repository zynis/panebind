# PaneBind R1-B Windows Owned-Window Operations Research

Status: **R1-B PRIOR-ART GATE PASS**

Review date: 2026-08-25.

## Scope and evidence labels

This review authorizes a Windows operations adapter and runtime harness that
can move only top-level windows created, registered, and owned by the current
PaneBind harness process. It does not authorize control of Explorer or any
other third-party window, product Glue behavior, global input handling, Snap,
resize conversion, injection, or polling.

- **FACT** is directly supported by pinned source, repository history, or
  official Microsoft documentation.
- **INFERENCE** is a conclusion drawn from those facts.
- **PANEBIND DECISION** is an independently designed R1-B contract.

No external implementation code was copied, adapted, translated, or
mechanically rewritten.

## Sources and license boundary

| Source | Immutable revision | License / use |
| --- | --- | --- |
| [AltSnap](https://github.com/RamonUnch/AltSnap) | [`5c86416ad21e4b72844a998a746bd3bb0bee5f5d`](https://github.com/RamonUnch/AltSnap/commit/5c86416ad21e4b72844a998a746bd3bb0bee5f5d), `1.68-48-g5c86416` | GPL-3.0-or-later; **REFERENCE ONLY** |
| [AltDrag](https://github.com/stefansundin/altdrag) | [`e2740d605b0336a3b391fec26794718864b19521`](https://github.com/stefansundin/altdrag/commit/e2740d605b0336a3b391fec26794718864b19521), `v1.1-8-ge2740d6` | GPL-3.0-or-later; **REFERENCE ONLY** |
| [PowerToys / FancyZones](https://github.com/microsoft/PowerToys) | [`19c4d805321db86f3634e6968e14dbf25cbba14a`](https://github.com/microsoft/PowerToys/commit/19c4d805321db86f3634e6968e14dbf25cbba14a) | MIT; reference-only in R1-B |
| Microsoft Learn Win32 documentation | live pages reviewed 2026-08-25 | platform facts paraphrased and cited |

GPL code remains reference-only. FancyZones is MIT, but this round also uses
it only as a behavioral and production-history reference. Any future copied or
substantially reused MIT material requires a separate provenance decision and
preservation of the Microsoft MIT notice.

## AltSnap and AltDrag targeted findings

### Native operations and error handling

**FACT.** AltSnap's single-window path eventually calls `SetWindowPos`.
Move-only calls use `SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE |
SWP_NOSIZE`; some paths add `SWP_ASYNCWINDOWPOS`. The reviewed path does not
consume the `SetWindowPos` return value. See the pinned
[`MoveWindowAsync` / `MoveResizeWindowNow_` path](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L1361-L1549).

**FACT.** AltSnap's sticky-resize path uses `BeginDeferWindowPos`, repeated
`DeferWindowPos`, and `EndDeferWindowPos` for immediate neighbors and the
leader. It passes forward the returned `HDWP`, but does not expose Begin,
Defer, or End failure stages, inspect the final `BOOL`, roll back, or
post-verify actual geometry. Members are enumerated before apply without an
owned capability, PID/class/generation revalidation. See
[`EnumTouchingWindows` / `ResizeTouchingWindows`](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L839-L978).

**FACT.** AltDrag's reviewed move/resize paths use `MoveWindow`, generally
after `IsWindow` / `GetWindowRect`, and do not establish a batch or structured
failure contract. Some paths reread geometry because applications can adjust a
requested size. See its pinned
[`hooks.c`](https://github.com/stefansundin/altdrag/blob/e2740d605b0336a3b391fec26794718864b19521/hooks.c#L632-L703).

**INFERENCE.** These mature projects show realistic native-operation choices
and failure modes, but neither is evidence for owned-window capability,
preflight all-or-nothing, native rollback, or requested-equals-actual.

### Visible and positioning geometry

**FACT.** AltSnap's `FixDWMRectLL` reads both `GetWindowRect` and
`DWMWA_EXTENDED_FRAME_BOUNDS`, derives per-edge offsets, and later applies
those offsets when converting a visible resize result back to native
positioning geometry. See
[`GetWindowRectLL` / `FixDWMRectLL`](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/unfuck.h#L1091-L1153).

**FACT.** AltSnap uses a bounded `WM_SIZING` request before some resize
operations so an application can adjust the proposed rectangle. The history
in [PR #723](https://github.com/RamonUnch/AltSnap/pull/723) records
grid-constrained terminal behavior. This is resize prior art, not a translation
contract.

**PANEBIND DECISION.** R1-B handles translation only. It does not copy the
per-edge resize solver or synthesize `WM_SIZING`. A target visible rectangle
must have the same size as the current visible frame and share one checked
four-edge delta. That delta translates the current positioning rectangle.

### Lifetime, feedback, and historical failures

**FACT.** AltSnap checks `IsWindow` at several drag lifecycle points, but its
worker/batch path has no PID, private class, property marker, logical token, or
generation capability. This remains a check-to-use race and cannot prevent a
recycled handle from denoting a different window.

**FACT.** AltSnap explicitly posts `WM_ENTERSIZEMOVE` / `WM_EXITSIZEMOVE` and
can explicitly call `NotifyWinEvent(EVENT_SYSTEM_MOVESIZESTART/END)`. See the
[`StartWindowMove` / `FinishWindowMove` helpers](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L576-L587).
[Issue #572](https://github.com/RamonUnch/AltSnap/issues/572) and
[PR #573](https://github.com/RamonUnch/AltSnap/pull/573) record the resulting
FancyZones interaction and the option that followed.

This refines source attribution for R0 AltSnap evidence: the observed
START -> LOCATION -> END lifecycle remains valid manual evidence, but it is not
evidence that `SetWindowPos` naturally emits START/END. R0 revalidation is not
required. The R1-B harness must not synthesize interactive WinEvents.

**FACT.** AltSnap history records several application-specific effects:

- [issue #76](https://github.com/RamonUnch/AltSnap/issues/76) and
  [PR #77](https://github.com/RamonUnch/AltSnap/pull/77) show that `SWP_NOSIZE`
  and asynchronous placement can alter dialog repaint behavior;
- [issue #160](https://github.com/RamonUnch/AltSnap/issues/160) records lag and
  invalidation with asynchronous resize;
- [issue #374](https://github.com/RamonUnch/AltSnap/issues/374) shows that
  `SWP_NOSENDCHANGING` is a deliberate escape hatch that changes application
  participation rather than a harmless optimization; and
- [issue #719](https://github.com/RamonUnch/AltSnap/issues/719) records an
  application resisting external positioning, reinforcing the need for
  post-verification.

**FACT.** AltDrag's historical HookWindows path installed a global
`WH_CALLWNDPROC` hook and subclassed third-party windows to modify
`WM_WINDOWPOSCHANGING`. See
[`hooks.c`](https://github.com/stefansundin/altdrag/blob/e2740d605b0336a3b391fec26794718864b19521/hooks.c#L2037-L2160).

**PANEBIND DECISION.** Injection, third-party subclassing, synthetic
START/END, default asynchronous placement, and `SWP_NOSENDCHANGING` are
rejected.

## FancyZones targeted findings

### Placement path and geometry boundary

**FACT.** At the pinned revision, `WorkArea::Snap` converts a zone rectangle
through `AdjustRectForSizeWindowToRect` and calls `SizeWindowToRect`. See
[`WorkArea::Snap`](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/WorkArea.cpp#L127-L153).
`SizeWindowToRect` uses `WINDOWPLACEMENT` / `SetWindowPlacement`, logs failures,
and performs two placements for scaling behavior; its caller receives no
structured requested-versus-actual receipt. See
[`WindowUtils.cpp`](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/WindowUtils.cpp#L213-L274).

**FACT.** `AdjustRectForSizeWindowToRect` reads both `GetWindowRect` and
`DWMWA_EXTENDED_FRAME_BOUNDS` and expands the desired visible zone rectangle by
the current frame margins before native placement. See
[`WindowUtils.cpp`](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/WindowUtils.cpp#L364-L400).

**FACT.** An exact search of the pinned `src/modules/fancyzones` tree found no
`BeginDeferWindowPos`, `DeferWindowPos`, or `EndDeferWindowPos` use. Assigned
windows are updated one at a time through `Snap`; see
[`UpdateWindowPositions`](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/WorkArea.cpp#L242-L248).
FancyZones therefore provides no evidence for Defer atomicity, rollback,
failure stages, or cross-window ordering.

### Event/operation separation and stale state

**FACT.** FancyZones' WinEvent callback posts messages to an owner-thread
hidden window; the owner-thread handler performs gesture/update/snap work. See
[`WinEventProc`](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/FancyZones.cpp#L252-L286)
and the
[`owner-thread handler`](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/FancyZones.cpp#L961-L997).

**FACT.** Location-change observation is installed for an active interactive
move/size and removed at its end. A UI-test helper explicitly notes that a
programmatic `SetWindowPos` custom-titlebar move does not produce the
`EVENT_SYSTEM_MOVESIZESTART` FancyZones needs for drag handling. See
[`FancyZonesApp.cpp`](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZones/FancyZonesApp.cpp#L96-L160)
and the pinned
[`FancyZonesTestHelper`](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZones.UITests.Next/Utils/FancyZonesTestHelper.cs#L530-L537).
This is upstream evidence, not a substitute for PaneBind runtime observation.

**FACT.** Destroy is dispatched to the owner thread and aborts an active move
for the destroyed HWND. Topology replacement now aborts move/size before
reclaiming state. Production fixes include
[PR #48569](https://github.com/microsoft/PowerToys/pull/48569),
[PR #48473](https://github.com/microsoft/PowerToys/pull/48473), and
[PR #49985](https://github.com/microsoft/PowerToys/pull/49985).
The assigned-window collection still keys raw `HWND` values and uses
`IsWindow` during cleanup; it is not an owned-capability precedent.

**FACT.** Relevant geometry and DPI history includes
[PR #17553](https://github.com/microsoft/PowerToys/pull/17553) (restore size and
monitor DPI), [PR #28688](https://github.com/microsoft/PowerToys/pull/28688)
(cross-monitor state preservation), and
[PR #44440](https://github.com/microsoft/PowerToys/pull/44440) (mixed-DPI
coordinate-context mismatch). This establishes risk, not a PaneBind
mixed-DPI runtime result.

**PANEBIND DECISION.** Preserve callback/operation separation for future
behavior, invalidate destroyed identities and stale generations, and keep
monitor/DPI facts in receipts. Do not copy FancyZones' placement path,
double-apply behavior, or log-only failure model.

## Official Windows contracts

### Positioning APIs and batch semantics

[`SetWindowPos`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowpos)
returns `BOOL` and supports flags that independently suppress size, move,
z-order, activation, redraw, and messages. R1-B selects:

```text
SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
```

- `SWP_NOSIZE` makes the translation-only boundary explicit;
- `SWP_NOZORDER` prevents a z-order change and makes `hWndInsertAfter`
  irrelevant; and
- `SWP_NOACTIVATE` prevents activation/focus stealing.

`SWP_NOOWNERZORDER` is unnecessary when z-order is already suppressed.
`SWP_NOSENDCHANGING` is rejected because it suppresses a message this round
must study and prevents normal application participation.
`SWP_ASYNCWINDOWPOS` is rejected because asynchronous dispatch would undermine
immediate post-verification.

[`BeginDeferWindowPos`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-begindeferwindowpos)
allocates an internal multiple-window-position structure. The requested member
count is supplied up front so allocation failure is discovered early.

[`DeferWindowPos`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-deferwindowpos)
updates that internal structure and can return a different `HDWP`; every next
call must use the newest return. If it returns `NULL`, Microsoft directs the
caller to abandon the operation and not call `EndDeferWindowPos`.

[`EndDeferWindowPos`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-enddeferwindowpos)
applies the stored operations in a single screen-refreshing cycle and returns
success/failure. It sends `WM_WINDOWPOSCHANGING` and
`WM_WINDOWPOSCHANGED` to each identified window.

No reviewed official page documents database-style transaction, rollback,
no-partial-effect behavior on End failure, which windows have changed after a
failure, cross-window message ordering, or reuse/retry of an Ended `HDWP`.

**PANEBIND DECISION.** Preflight is all-or-nothing: all members are validated
before Begin. Native apply is explicitly *not* described as transactional. A
Defer failure poisons the chain and skips End. End is attempted at most once.
After any native failure, all resolvable owned members are re-snapshotted and
reported; cleanup layout is a later operation, not rollback.

### Positioning rect, visible frame, and DPI

Microsoft documents that
[`GetWindowRect`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowrect)
returns an exclusive-edge screen rectangle, is DPI-virtualized according to
the caller's awareness, and can include invisible resize borders. PaneBind
names this the `positioning_rect`.

[`DwmGetWindowAttribute`](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/nf-dwmapi-dwmgetwindowattribute)
with
[`DWMWA_EXTENDED_FRAME_BOUNDS`](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwmwindowattribute)
returns the extended visible frame. That rectangle is not DPI-adjusted. A DWM
query failure is a preflight failure, not permission to substitute
`GetWindowRect` and silently change geometry meaning.

Microsoft recommends declaring process DPI awareness in the
[application manifest](https://learn.microsoft.com/en-us/windows/win32/hidpi/setting-the-default-dpi-awareness-for-a-process).
R1-B reuses the repository's audited manifest resource containing
`PerMonitorV2`. Runtime validation uses
[`GetWindowDpiAwarenessContext`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowdpiawarenesscontext)
and
[`AreDpiAwarenessContextsEqual`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-aredpiawarenesscontextsequal),
not raw context-bit comparison.

### Window identity and lifetime

[`IsWindow`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-iswindow)
warns that a window can be destroyed after the check and that HWND values are
recycled. It is a point-in-time fact, never an ownership proof.

[`GetWindowThreadProcessId`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowthreadprocessid)
provides creator thread and process identity. R1-B compares both with their
registration values and requires the process to be the current process.

[`CreateWindowExW`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-createwindowexw)
sends creation messages before it returns. A token becomes live only after
CreateWindowEx succeeds and registry validation/marking completes.

[`DestroyWindow`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-destroywindow)
sends `WM_DESTROY` and `WM_NCDESTROY`; a thread cannot destroy a window created
by another thread. At `WM_NCDESTROY` entry, PaneBind immediately invalidates
the logical generation and removes its private property. This is a PaneBind
invariant; it does not claim that the numeric HWND has already become invalid
at that exact instruction.

The capability validation is therefore:

```text
token registered and generation matches
AND IsWindow(hwnd)
AND PID == current process and registered PID
AND creator TID == registered TID
AND exact private harness class matches
AND private property marker matches the live registration
```

Create, apply, invalidate, and destroy are serialized on the harness UI
thread. Native messages can still re-enter the WndProc; post-apply generation
and geometry checks remain mandatory.

### Message contract

- [`WM_WINDOWPOSCHANGING`](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-windowposchanging)
  is sent before a change and lets a handler modify the proposed `WINDOWPOS`.
- [`WM_WINDOWPOSCHANGED`](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-windowposchanged)
  is sent after the change. `DefWindowProc` normally emits `WM_MOVE` and
  `WM_SIZE` from it.
- [`WM_MOVE`](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-move)
  and [`WM_SIZE`](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-size)
  are lifecycle evidence, not authoritative geometry snapshots.

Microsoft does not specify batch member callback order, strict continuity,
interleaving absence, exact cardinality, or whether a no-op target emits a
particular message sequence. The harness records message entry before calling
`DefWindowProc`, copies `WINDOWPOS` values rather than retaining pointers, and
draws ordering conclusions only from runtime evidence.

## PaneBind operations contract

### Owned capability

The public apply and capture APIs accept `OwnedWindowToken`, never raw `HWND`.
Only the registry boundary accepts an HWND immediately after the harness has
created it. Registration itself validates the current process, creator thread,
fixed private class, independent unowned top-level shape, and installs a
private per-registration property marker. The token contains a private
process-monotonic registry authority plus an opaque logical identity and
generation. It has no native conversion and cannot be caller-forged into a
live capability. Authority is part of equality and resolution, so equal local
ID/generation values from two registries cannot alias each other.

The four harness windows are independent, unowned top-level windows. Destroying
one cannot cascade through an owner relationship. Old tokens never regain
authority when an HWND numeric value is recycled.

### Translation bridge

For each request:

```text
change = classify_geometry_change(current_visible, target_visible)
require change == Translation or Unchanged

target_positioning =
    current_positioning translated by the same checked (dx, dy)
```

All four visible edges must share one delta; visible size cannot change;
positioning size remains unchanged. Every subtraction/addition and every
conversion to native `int` coordinates is checked. The visible target is never
passed directly as a positioning target.

### Preflight and failure stages

Before any native call, every member must pass token/lifetime/ownership checks,
geometry capture, pure-translation classification, arithmetic/range checks,
same-parent validation, duplicate rejection, and nonempty/count checks. Any
failure returns without Begin and therefore moves no member.

The result distinguishes at least:

```text
PreflightFailure
BeginFailure
DeferChainFailure
EndFailureStateUnknown
PostVerificationMismatch
SuccessVerified
```

It records batch identity, requested/deferred/verified counts, whether native
apply was attempted, whether native outcome is known, native diagnostic when
documented, and per-member requested/before/actual geometry, DPI, monitor, and
generation. `DwmGetWindowAttribute` errors remain HRESULT-domain diagnostics;
they are not read through stale `GetLastError` state.

## Feedback-suppression inputs, not behavior

The R1-B operation receipt can provide future R1-C with:

- operation batch ID;
- exact token and generation set;
- expected visible and positioning targets;
- before/actual snapshots;
- apply stage and completion status; and
- an observation time/sequence boundary supplied by the future event adapter.

Time alone, assumed follower contiguity, raw HWND alone, or the absence of
interactive START/END is insufficient. R1-B does not implement a Glue event
loop or suppression state machine.

## Owned-window runtime experiment

**WINDOWS RUNTIME INTEGRATION.** The final Debug experiment ran the R0
observer and the R1-B harness as separate processes. Raw evidence remains only
under ignored `uat/r1b/`, prefix `20260825T094555450Z`; no raw record is
tracked by Git.

- The observer JSONL contained 695 valid records with a continuous sequence
  from 1 through 695. Hook registration, initial census, hook shutdown, and
  observer shutdown were complete. There was no queue-overflow,
  notification-failure, dropped-event, target field-error, or identity/DPI
  failure diagnostic.
- Restricting evidence to class `PaneBind.R1B.OwnedWindow` yielded exactly 20
  events: five `EVENT_OBJECT_LOCATIONCHANGE` events each for A, B, C, and D;
  `EVENT_SYSTEM_MOVESIZESTART` and `EVENT_SYSTEM_MOVESIZEEND` were absent.
- The 20 events correspond to the initial 2x2 placement and four geometry-
  changing scripted positions. The repeated `(+80,+50)` target produced no
  location WinEvent because actual geometry did not change.
- A nonzero leader `SetWindowPos` produced harness messages
  `WM_WINDOWPOSCHANGING -> WM_WINDOWPOSCHANGED -> WM_MOVE`, with no `WM_SIZE`.
- In each nonzero follower batch in this run, B/C/D first received their
  `WM_WINDOWPOSCHANGING` entries, then each received
  `WM_WINDOWPOSCHANGED -> WM_MOVE`. Their observer location events were
  contiguous and shared one native timestamp in each follower batch.
- A repeated no-op leader request and repeated no-op follower batch produced
  `WM_WINDOWPOSCHANGING` but no `WM_WINDOWPOSCHANGED`, `WM_MOVE`, or WinEvent.
- No cross-window ordering, contiguity, timestamp equality, or message
  cardinality is promoted to an API contract; these are one-machine observed
  facts only.

**PANEBIND DECISION.** Natural programmatic placement is a geometry-feedback
source but not an interactive move/resize session. Future feedback suppression
must match a receipt's authority/token/generation and acknowledged geometry;
it must not require START/END or depend only on timing/contiguity.

## Adopted and rejected designs

Adopted:

- fixed-class, current-process owned tokens with generation and property marker;
- R1-A visible target consumed without reimplementing its move-plan math;
- checked visible-delta to current-positioning translation;
- complete preflight before Begin;
- `Begin/Defer/EndDeferWindowPos` with the newest returned handle;
- minimal no-size/no-z-order/no-activation flags;
- structured failure stages and requested-versus-actual verification;
- PMv2 manifest and explicit runtime DPI/monitor evidence; and
- natural WndProc/WinEvent observation without synthetic START/END.

Rejected:

- a public raw-HWND operation API or HWND-cast token;
- registry-only trust without live PID/TID/class/property checks;
- direct visible-rect placement or R1-B resize conversion;
- GPL implementation/control-flow/structure adaptation;
- FancyZones double `SetWindowPlacement` / void error path;
- default async placement, `SWP_NOSENDCHANGING`, z-order, activation, topmost,
  or foreground side effects;
- native transaction/atomic rollback claims;
- injection, third-party subclassing, global input, polling, or synthetic
  interactive lifecycle events; and
- speculative R1-C feedback state machinery.

## Research gate

```text
R1B_PRIOR_ART_GATE = PASS
ALTSNAP_ALTDRAG_LICENSE = GPL REFERENCE ONLY
POWERTOYS_LICENSE = MIT REFERENCE ONLY FOR R1-B
NATIVE_ROLLBACK_GUARANTEE = NONE DOCUMENTED
EXTERNAL_CODE_COPIED = NO
EXTERNAL_CODE_ADAPTED = NO
THIRD_PARTY_WINDOW_CONTROL_AUTHORIZED = NO
R1C = NOT STARTED
```
