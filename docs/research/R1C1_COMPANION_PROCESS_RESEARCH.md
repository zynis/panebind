# PaneBind R1-C1 External Companion-Process Research

Status: **R1-C1 PRIOR-ART GATE PASS**

Review date: 2026-08-26.

## Scope and evidence labels

This gate authorizes only a controller-launched PaneBind companion test
process, its four independent top-level fixture windows, a launch-handshake
capability, pure translation, post-verification, process/window lifetime tests,
and feedback evidence. It does not authorize discovering or controlling any
user-launched application or system window.

- **FACT** is directly supported by pinned source/history or Microsoft
  documentation.
- **INFERENCE** is a conclusion from those facts.
- **PANEBIND DECISION** is an independently designed R1-C1 contract.

No external implementation code was copied, adapted, translated, or
mechanically rewritten.

## Sources and license boundary

| Source | Immutable revision | License / use |
| --- | --- | --- |
| [AltSnap](https://github.com/RamonUnch/AltSnap) | [`5c86416ad21e4b72844a998a746bd3bb0bee5f5d`](https://github.com/RamonUnch/AltSnap/commit/5c86416ad21e4b72844a998a746bd3bb0bee5f5d) | GPL-3.0-or-later; **REFERENCE ONLY** |
| [AltDrag](https://github.com/stefansundin/altdrag) | [`e2740d605b0336a3b391fec26794718864b19521`](https://github.com/stefansundin/altdrag/commit/e2740d605b0336a3b391fec26794718864b19521) | GPL-3.0-or-later; **REFERENCE ONLY** |
| [PowerToys / FancyZones](https://github.com/microsoft/PowerToys) | [`19c4d805321db86f3634e6968e14dbf25cbba14a`](https://github.com/microsoft/PowerToys/commit/19c4d805321db86f3634e6968e14dbf25cbba14a) | MIT; reference-only in R1-C1 |
| Microsoft Learn Win32 documentation | live pages reviewed 2026-08-26 | platform facts paraphrased and cited |

AltSnap and AltDrag remain GPL reference-only. PowerToys remains
reference-only despite its MIT license; future reuse requires a separate
approval and attribution decision.

## AltSnap and AltDrag findings

### Cross-process operations are real, but capability is absent

**FACT.** AltSnap discovers arbitrary desktop windows through point/window
enumeration and moves them from another process. Its single-window worker
eventually calls `SetWindowPos`; its move-only path does not consume the BOOL
as a structured result or post-verify requested versus actual geometry. See
the pinned
[`MoveWindowAsync` / `MoveResizeWindowNow_` path](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L1361-L1549).

**FACT.** AltSnap sticky resize builds a Begin/Defer/End chain for enumerated
neighbor windows, including windows in other processes, but has no launch
handshake, held process handle, session authority, logical generation,
structured End result, or rollback/post-verification contract. See
[`ResizeTouchingWindows`](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L839-L978).

**FACT.** Active move paths perform point-in-time `IsWindow` checks. Process
metadata lookup uses PID/open-process transiently and closes the handle; it
does not retain the process kernel object as identity. See the pinned
[`move lifecycle`](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L5024-L5104)
and
[`worker check`](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L2190-L2197).

**INFERENCE.** This is evidence that ordinary same-integrity cross-process and
cross-thread placement can work. It is not evidence that raw HWND/PID is a
stable capability or that R1-B same-process authority can be widened.

### Application adjustment and feedback

**FACT.** AltSnap can ask an application to adjust a resize through
`WM_SIZING`; application-specific history and issues show that requested
geometry is not guaranteed to become actual geometry. See
[`LetWindowKickBack`](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L1403-L1447),
[issue #374](https://github.com/RamonUnch/AltSnap/issues/374), and
[issue #719](https://github.com/RamonUnch/AltSnap/issues/719).

**FACT.** AltSnap explicitly posts enter/exit-size messages and can synthesize
accessibility START/END events. See
[`StartWindowMove` / `FinishWindowMove`](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L576-L587).
It therefore cannot establish the natural WinEvent lifecycle of a plain
cross-process `SetWindowPos` or Defer batch.

### Injection is a rejected historical path

**FACT.** AltDrag's historical HookWindows path installed a global
`WH_CALLWNDPROC` hook, injected/subclassed third-party windows, and modified
`WM_WINDOWPOSCHANGING`. See
[`altdrag.c`](https://github.com/stefansundin/altdrag/blob/e2740d605b0336a3b391fec26794718864b19521/altdrag.c#L211-L324)
and
[`hooks.c`](https://github.com/stefansundin/altdrag/blob/e2740d605b0336a3b391fec26794718864b19521/hooks.c#L2037-L2160).
AltSnap's pinned README records removal of that injected/two-bitness design;
see its
[`architecture history`](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/README.md#L14-L24).

**PANEBIND DECISION.** Global discovery, injection, subclassing, raw HWND
tokens, `IsWindow`-only identity, synthetic START/END, and time/contiguity-only
feedback attribution are rejected.

## FancyZones findings

### Production cross-process boundary

**FACT.** FancyZones receives global/out-of-context events and processes raw
external HWND values on its owner thread. Its target placement path is
`WorkArea::Snap -> SizeWindowToRect -> SetWindowPlacement`; assigned windows
are updated one at a time. See
[`WorkArea::Snap`](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/WorkArea.cpp#L127-L153)
and
[`SizeWindowToRect`](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/WindowUtils.cpp#L213-L274).

An exact search of the pinned FancyZones tree found no
`BeginDeferWindowPos`, `DeferWindowPos`, or `EndDeferWindowPos` use. FancyZones
therefore does not establish cross-process Defer success, atomicity, rollback,
or message ordering.

### Launch and lifetime lessons

**FACT.** PowerToys components use process handles for selected child/component
lifetime paths, while ordinary target windows remain global raw HWND state.
This is not a launch-handshake capability model. PaneBind must retain the
specific `CreateProcessW` handle rather than reopen authority from PID.

**FACT.** Destroy handling is dispatched to the owner thread and aborts an
active move when its HWND disappears. Production hardening includes
[PR #48569](https://github.com/microsoft/PowerToys/pull/48569),
[PR #48473](https://github.com/microsoft/PowerToys/pull/48473), and
[PR #49985](https://github.com/microsoft/PowerToys/pull/49985).

**FACT.** FancyZones runs PMv2 and contains target-awareness/monitor conversion
logic. [PR #44440](https://github.com/microsoft/PowerToys/pull/44440) records a
mixed-DPI coordinate-context failure. These are risk inputs, not PaneBind
mixed-DPI evidence.

**FACT.** FancyZones documentation and upstream issues record elevation
limitations for ordinary application targets. R1-C1 deliberately runs the
controller and fixture at the same integrity and does not auto-elevate.

**PANEBIND DECISION.** Adopt process-handle lifetime, owner-thread cancellation,
PMv2/coordinate labeling, and post-verification. Reject global discovery as a
capability source, PID/class/title alone as identity, log-only placement
failure, and upstream behavior as PaneBind runtime evidence.

## Official process and IPC contracts

### Launch and stable process identity

[`CreateProcessW`](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw)
returns before child initialization completes. A zero return supports immediate
`GetLastError`; successful `PROCESS_INFORMATION` handles must be closed.
PaneBind supplies an explicit fully qualified `lpApplicationName` and a mutable
quoted command line.

[`PROCESS_INFORMATION`](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/ns-processthreadsapi-process_information)
documents that identifiers can be reused after process/thread objects are
released. The controller therefore retains `hProcess` through the session.
PID and UI TID are validated facts, not permanent authority.

[`WaitForInputIdle`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-waitforinputidle)
returns immediately for a console process/no message queue, returns when any
thread in a multithreaded process becomes idle, and only performs a real wait
once. It is not the R1-C1 readiness gate; a protocol handshake is.

Process liveness is tested with a zero-time wait on the retained process
handle. A signaled process invalidates every token in that session. A new
session receives a new authority even when logical IDs A/B/C/D repeat.

### Restricted anonymous-pipe IPC

Microsoft documents that bidirectional anonymous-pipe communication requires
two pipes and that anonymous pipes are byte streams without message boundaries.
See
[`Interprocess Communications`](https://learn.microsoft.com/en-us/windows/win32/ipc/interprocess-communications),
[`Pipe Handle Inheritance`](https://learn.microsoft.com/en-us/windows/win32/ipc/pipe-handle-inheritance),
and
[`Anonymous Pipe Operations`](https://learn.microsoft.com/en-us/windows/win32/ipc/anonymous-pipe-operations).

R1-C1 uses fixed bounded frames with magic, version, type, and payload size.
The parent creates inheritable child ends, clears inheritance on parent-only
ends with
[`SetHandleInformation`](https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-sethandleinformation),
and allowlists only the child read/write handles through
[`PROC_THREAD_ATTRIBUTE_HANDLE_LIST`](https://learn.microsoft.com/en-us/windows/desktop/api/processthreadsapi/nf-processthreadsapi-updateprocthreadattribute).
`CreateProcessW` uses `bInheritHandles=TRUE` and
`EXTENDED_STARTUPINFO_PRESENT`; the parent closes its copies of child ends
immediately after launch.

The inherited raw handle values and raw HWND values are transport facts, never
public capabilities. IPC is cooperative process-local test plumbing, not a
cryptographic sandbox.

Any response timeout, partial/truncated or oversized frame, read/write failure
while the target remains alive, request/envelope mismatch, or malformed
evidence poisons the protocol session. The controller closes both endpoints,
retires all tokens, and forbids future operation reuse of the byte stream. This
policy is code-reviewed; forced timeout/cancellation races remain NOT TESTED.

### Handshake contract

The first child frame must establish exactly:

```text
protocol magic/version
per-launch session marker echo
launched PID
companion UI TID
four unique logical IDs A/B/C/D
four nonzero raw HWND registration facts
per-window generation
```

The controller rejects a wrong marker/version/PID, zero or duplicate ID/HWND,
unexpected member count, or an already-exited process. No `EnumWindows`,
`FindWindow`, title search, foreground lookup, or point lookup participates.

## Companion capability contract

R1-B `OwnedWindowToken`, `OwnedWindowRegistry`, and `OwnedWindowOperations`
remain unchanged and owned-only. R1-C1 introduces distinct authority:

```text
CompanionWindowToken
    controller-registry authority
  + companion-session authority
  + logical window ID
  + generation
```

The token has no public constructor from native/integer values and no HWND
conversion. A token from session 1 cannot resolve in session 2 even if logical
IDs, PID, TID, or numeric HWND values repeat.

Every resolution immediately before native use requires:

```text
session process handle is nonsignaled
token controller/session authority and generation match
IsWindow(hwnd)
GetProcessId(hProcess) == launch PID
GetWindowThreadProcessId(hwnd) == handshake PID/TID
private companion class matches
GA_ROOT == hwnd
WS_CHILD absent
GW_OWNER == null
session-derived property marker matches
```

[`IsWindow`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-iswindow)
is only a point-in-time check because HWND values can be recycled.
[`GetWindowThreadProcessId`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowthreadprocessid)
provides current native PID/TID facts. The held process handle and session
authority prevent PID reuse from reviving an old token.

The target sets/removes its own property marker. The controller only reads it;
it does not mutate another process's marker.

## Cross-process native operations

Microsoft's
[`SetWindowPos`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowpos)
contract is not limited to same-process windows. R1-C1 keeps
`SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE`. It rejects
`SWP_ASYNCWINDOWPOS` because posted work is not completion evidence and does
not use `AttachThreadInput`.

The Defer contract remains:

- Begin with the validated member count;
- pass forward every latest `HDWP`;
- require a common parent;
- abandon a failed Defer chain without End; and
- call End once, then re-snapshot actual state.

See
[`BeginDeferWindowPos`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-begindeferwindowpos),
[`DeferWindowPos`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-deferwindowpos), and
[`EndDeferWindowPos`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-enddeferwindowpos).
No reviewed page documents database-style transaction or rollback.

R1-C1 reuses only the capability-independent visible-to-positioning pure
translation preparation from R1-B. Owned and companion resolution remain
separate. No generic public arbitrary-HWND executor is introduced.

## Target-side message and uncooperative behavior

[`WM_WINDOWPOSCHANGING`](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-windowposchanging)
allows the target WndProc to modify the proposed `WINDOWPOS`. The fixture's
test-only uncooperative mode deterministically adds five pixels to D's proposed
X coordinate. Native apply may return success while actual geometry differs;
the required result is `PostVerificationFailed` with requested and actual
receipts for all members.

[`WM_WINDOWPOSCHANGED`](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-windowposchanged)
normally leads `DefWindowProc` to generate `WM_MOVE`/`WM_SIZE`. Message
cardinality and order are measured facts, never universal guarantees.

Destroy C is an IPC command executed on the companion UI thread because
[`DestroyWindow`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-destroywindow)
cannot destroy a window created by another thread.

## Integrity and cleanup

The controller and target must have matching baseline integrity/UIAccess/
AppContainer facts. Token information is queried through
[`OpenProcessToken`](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-openprocesstoken)
and
[`GetTokenInformation`](https://learn.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-gettokeninformation).
Elevated, AppContainer, and UIAccess targets are **NOT TESTED**; R1-C1 never
auto-elevates or changes a message filter.

Cleanup first requests graceful IPC shutdown and waits a bounded time on the
exact session process handle. If the fixture fails to exit, the harness may use
[`TerminateProcess`](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-terminateprocess)
only on that retained handle, then must wait again because termination is
asynchronous. It never kills by PID, process name, wildcard, or `taskkill`.
Forced cleanup is a test-fixture safeguard, not normal shutdown or rollback.

A target hung exactly during synchronous native positioning is not forced as a
race test in R1-C1. It remains a future hardening risk; no `TerminateThread`,
sleep race, or asynchronous completion claim is permitted.

## Feedback attribution contract

The existing out-of-context observer measures the companion target as another
process. R1-C1 records START/LOCATION/END, native/observer sequence and time,
PID/HWND, duplicates, missing events, ordering, and interleaving. It does not
assume R1-B results repeat.

An offline attribution candidate needs:

```text
session/token/generation
+ expected target
+ acknowledged actual geometry
+ native operation stage/outcome
```

Time alone, raw HWND alone, START/END, or event adjacency/contiguity is
insufficient. A no-op request need not produce a WinEvent acknowledgement.
R1-C1 does not implement the product Glue suppression state machine.

## Required test model

Deterministic tests cover session/controller authority, cross-session alias,
generation/stale behavior, child-exit invalidation model, token-set preflight,
and shared translation preparation.

The explicit desktop runtime must cover:

```text
launch + restricted-handle handshake
PID/TID/class/root/owner/marker validation
four-window exact 2x2 topology
cross-process SetWindowPos leader
cross-process Defer follower batch
initial-relative repeat/backtrack sequence
destroy C -> mixed batch preflight no-op
whole process exit -> all tokens stale
session 2 rejects session 1 tokens
uncooperative D -> PostVerificationFailed with actual receipts
graceful child cleanup
```

The desktop-dependent runtime remains an explicit `--self-test`, not an
unconditional universal CTest.

## R1-C1 runtime experiment

**WINDOWS RUNTIME INTEGRATION.** The final Debug evidence run launched the R0
observer and `panebind-companion-harness`; the harness in turn launched two
independent `panebind-companion-target` sessions. Raw files remain only under
ignored `uat/r1c1/`, prefix `20260826T005227232Z`.

- Controller PID/TID and both target PID/UI-TID pairs were distinct. Both
  targets matched the controller's medium-integrity RID 8192, UIAccess false,
  AppContainer false, PMv2 baseline.
- Session 1 cross-process startup Defer verified 4/4. Five initial-relative
  leader/follower steps verified `(+30,+20)`, `(+80,+50)`, repeated
  `(+80,+50)`, backtrack `(+40,+10)`, and `(+120,+60)` with no drift.
- The D-only uncooperative batch completed natively, B/C matched requested,
  and D changed proposed positioning X `1600 -> 1605`. The result was
  `PostVerificationFailed`, `verified_count=2/3`, with actual snapshots for all
  three members. A later independent operation restored the fixture; it was
  not called rollback.
- Destroy C ran on the target UI thread. `B + stale C + D` failed in preflight,
  `native_apply_attempted=false`, and B/D were unchanged.
- Graceful process exit retired all session-1 tokens. Session 2 reissued A-D
  under a different authority and rejected the old A token before native use.
- Target-side evidence contained 101 valid records with no overflow/drop:
  CHANGING/CHANGED/MOVE/SIZE/NCDESTROY = `35/31/30/4/1`.

**EXTERNAL OBSERVER EVIDENCE.** Observer JSONL contained 715 valid records,
continuous sequence `1..715`, complete hook/census/shutdown diagnostics, no
queue/drop failure, and empty stderr. Filtering both controller-launched target
PIDs yielded exactly 26 accepted `EVENT_OBJECT_LOCATIONCHANGE` events and no
START/END:

```text
START / LOCATION / END = 0 / 26 / 0
```

The 26 events exactly corresponded to startup A-D (4), four geometry-changing
whole-topology positions (16), D-uncooperative B/C/D (3), and recovery B/C/D
(3). The repeated same target, destroy, and session-2 old-token rejection
produced no location acknowledgement. In this run the target events occupied a
contiguous observer sequence, but contiguity and per-request cardinality remain
observed facts, not contracts.

**PANEBIND DECISION.** A feedback candidate can be attributed offline using
session/token/generation, expected target, and acknowledged actual geometry in
this controlled run. Absence of START/END or a no-op WinEvent does not imply
operation failure. R1-C1 still implements no product suppression state machine.

## Adopted and rejected designs

Adopted:

- explicit CreateProcessW launch authority and retained process handle;
- two anonymous pipes with a restricted inherited handle list;
- fixed bounded binary handshake/protocol;
- separate controller/session/token authorities and generations;
- target-owned private class/property marker;
- repeated full process/window validation before native use;
- shared pure translation preparation with separate capability resolvers;
- synchronous verifiable placement, structured failure, and post-snapshot;
- target-side WndProc evidence and uncooperative geometry test; and
- graceful shutdown with exact-handle-only bounded fallback cleanup.

Rejected:

- widening or renaming R1-B owned capability;
- Enum/Find/foreground/point/title discovery;
- PID, class, title, HWND, or `IsWindow` alone as permanent identity;
- `WaitForInputIdle` as the handshake;
- broad inheritable-handle leakage;
- arbitrary HWND public operations;
- `AttachThreadInput`, default async placement, input injection, DLL injection,
  third-party subclassing, global input, polling, or auto-elevation;
- time/START-END/contiguity-only feedback attribution; and
- native transaction/rollback or universal event-ordering claims.

## Research gate

```text
R1C1_PRIOR_ART_GATE = PASS
ALTSNAP_ALTDRAG_LICENSE = GPL REFERENCE ONLY
POWERTOYS_LICENSE = MIT REFERENCE ONLY FOR R1-C1
EXTERNAL_CODE_COPIED = NO
EXTERNAL_CODE_ADAPTED = NO
THIRD_PARTY_WINDOW_CONTROL_AUTHORIZED = NO
R1C2 = NOT STARTED
```
