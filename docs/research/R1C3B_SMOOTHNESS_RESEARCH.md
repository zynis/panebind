# R1-C3B Phase 1 — Profiling Research and Design

Review: 2026-09-09. Starting main `d901094404f04772e1d641842df76d13c551a734`.
Scope: diagnostic-only fine-grained measurement and a human Debug profile
handoff. No safety/cadence/operation rewrite. The
[baseline map](../reports/R1C3B_HOT_PATH_MAP.md) was written before source edits.

## Prior art and history (reference only)

Fixed reviewed source, not an assertion of newest upstream release:

| Project | Exact revision / license |
| --- | --- |
| AltSnap, mature active reference | `5c86416ad21e4b72844a998a746bd3bb0bee5f5d`, GPL-3.0-or-later |
| AltDrag, mature historical reference | `e2740d605b0336a3b391fec26794718864b19521`, GPL-3.0-or-later |
| PowerToys/FancyZones, mature production reference | `19c4d805321db86f3634e6968e14dbf25cbba14a`, MIT |

Source/license files were read from existing local checkouts. No GPL or MIT
code is copied, translated or adapted. No upstream runtime was installed or
run. Provenance records the inspected files and history separately.

AltSnap pinned `hooks.c`:
[worker 695–735](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L695-L735)
coalesces consecutive movement work until another message kind; placement
need not follow every raw input event. [Cadence 5442–5481](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L5442-L5481)
uses event timestamp changes and MoveRate/ResizeRate update counts.
[Configuration 6677–6697](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L6677-L6697)
resolves RezTimer auto modes with display frequency and disables RefreshRate
when RezTimer is selected. The placement wrapper can delay after native work.
[PR #609](https://github.com/RamonUnch/AltSnap/pull/609), implementation commit
`7f4afe59076b70980f71af202f63609ca3ac5745` and follow-up
`7d4c7deb17437a7d5d350fd8074f6285444f4c51` record offloading movement work.

[AltSnap #160](https://github.com/RamonUnch/AltSnap/issues/160) and its comments
describe app-specific Alacritty lag, different rate settings, RezTimer trials,
CPU/latency tradeoffs and differing high-refresh subjective results. The
maintainer rejected an expanding hard-coded per-app rate list. These are
upstream reports, not proof of PaneBind's cause; their Sleep/timer/input-hook
mechanisms are **not** adopted. Mouse event timestamps are not QPC/display
presentation timestamps. AltDrag's pinned `hooks.c:1113–1128` also rate-limits
mouse-driven work. Its [border issue #38](https://github.com/stefansundin/altdrag/issues/38)
illustrates why visible and positioning bounds cannot be substituted blindly.

FancyZones pinned `FancyZonesApp.cpp:96–181`, `FancyZones.cpp:478–530,961–997`,
`WindowMouseSnap.cpp:54–147` and `WindowUtils.cpp:363–390` were inspected.
Lifecycle ingress forwards to an owner; movement updates zone highlight/work
area state; placement at END is not PaneBind's realtime follower contract.
Geometry code explicitly accounts for DWM versus positioning frame bounds.

History inspected:

- [#18568](https://github.com/microsoft/PowerToys/issues/18568) links multi-monitor
  lag to [#18057](https://github.com/microsoft/PowerToys/issues/18057).
  [PR #18106](https://github.com/microsoft/PowerToys/pull/18106), merged commit
  `22786a6bdcbbc6eaeb417a6c6f1f15bb6fb0a550`, fixes a repeated default-layout
  read/reapply/save loop. This supports investigating repeated work using
  evidence, not removing PaneBind validations by analogy.
- [#12135](https://github.com/microsoft/PowerToys/issues/12135) reports remote
  desktop drag/overlay lag, demonstrating environmental context matters; it
  supplies no diagnosis for this same-host Explorer case.
- [PR #48569](https://github.com/microsoft/PowerToys/pull/48569), merged commit
  `dd26d86580168d2e368701f7b0c4d629dc9cd9ac`, aborts destroyed-window drags
  without snapping. Scheduling changes cannot erase invalidation boundaries.

[AquaGlue official help](https://help.nurgo-software.com/article/97-aquasnap-configuration-aquaglue)
remains UX/quality reference only. Its public adjacent-window Ctrl behavior
does not reveal its pipeline, profiler, cache or smoothness implementation.
No AquaGlue equivalence claim is made.

## Official platform contracts rechecked

| API group | Consequence |
| --- | --- |
| [SetWinEventHook](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwineventhook) | OUTOFCONTEXT is asynchronous, delivered on installing message-loop thread; guard reentrancy. Callback receipt time is not native generation time. |
| [PostThreadMessage](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-postthreadmessagew) | Queues without waiting for processing; needs recipient queue; quota/UIPI can fail; thread messages are not window-procedure messages and may be consumed by modal loops. |
| [MsgWaitForMultipleObjectsEx](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-msgwaitformultipleobjectsex), [PeekMessage](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-peekmessagew), [DispatchMessage](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-dispatchmessagew) | Preserve MWMO_INPUTAVAILABLE and eight-message budget. PeekMessage may deliver internal/nonqueued work even when no MSG is retrieved. Direct backlog drain need not await private notification dispatch. |
| [SetWindowPos](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowpos) | Keep synchronous exact operation and NOSIZE/NOZORDER/NOACTIVATE. ASYNC changes completion semantics and is prohibited here. Record native error before instrumentation. |
| [GetWindowRect](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowrect), [DwmGetWindowAttribute](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/nf-dwmapi-dwmgetwindowattribute), [DWM attributes](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwmwindowattribute) | Positioning rect can be DPI-virtualized and contain invisible borders; extended frame is different. Cloak and bounds checks remain separate, HRESULT checked. |
| [GetWindowThreadProcessId](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowthreadprocessid), [GetAncestor](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getancestor) | Read current identity/root, not lifetime guarantees; failures and generation invalidation remain mandatory. |
| [GetDpiForWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getdpiforwindow), [MonitorFromWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-monitorfromwindow), [GetMonitorInfo](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getmonitorinfow) | DPI depends on awareness; monitor follows window intersection; initialize cbSize, validate work area/monitor/DPI against anchors. No mixed-DPI support added. |
| [QPC](https://learn.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancecounter), [QPF](https://learn.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancefrequency), [timing guidance](https://learn.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps) | Cache frequency; same-owner nondecreasing counter; subtract ticks before conversion; avoid overflow and cross-thread ordering assumptions. CPU timing is not DWM presentation. |
| [BeginDeferWindowPos](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-begindeferwindowpos), [DeferWindowPos](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-deferwindowpos), [EndDeferWindowPos](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-enddeferwindowpos) | Future bounded target-set batching only. Use returned handle, abandon on failure, same-parent constraints, messages delivered to each target. No batch placement implementation this phase. |

## Evidence -> independent design -> deterministic tests

The existing accepted logs were reread using the frozen offline validator;
both PASS. Their substage costs and notification dispatch are
NOT RETROACTIVELY RECOVERABLE. The source map identifies where to measure but
does not identify the dominant real stage. Current root cause remains
PENDING_HUMAN_PROFILE. External Observer perturbation is a recorded hypothesis.

Design: a nullable, owner-affine profiler enabled only by a dedicated evidence
profile, with preallocated bounded span/notification storage, numeric stage IDs,
QPC intervals and existing quantum/receipt/operation generations. Nested spans
record inclusive intervals; exclusive durations subtract immediate children
only. Never rank/sum overlapping parents and children as disjoint costs.
Record source-quantum Leader/Follower captures separately from operation-local
prepare/immediate/native/postverify stages. Shared quantum prefixes are not
double-counted in matched-operation percentage aggregates.

Queue/backlog arithmetic uses same-source delivered receipt watermarks, never
OS generation counts. Notification IDs correlate actual successful posting,
inherited already-pending receipts and actual owner private-message retrieval.
When a message has not been retrieved before direct drain (or was consumed by
another modal pump), dispatch is explicitly NOT OBSERVED, not fabricated.
Notification timing needs one opt-in QPC sample at each actual posting edge;
this small addition must be disclosed, not called zero instrumentation cost.
No COM/DWM/geometry/logging/allocation is added in callbacks. Default C2B/C3A
paths have profiling OFF, no new stage QPC calls or profile buffers.

Full validation stages follow the existing order: token/ledger; Shell
observation; inventory/peer/location; native identity; process image file
identity; structure/class/style; state/cloak; virtual desktop; process security;
positioning; DWM visible frame; monitor/DPI; eligibility/snapshot finalize.
Repeated validations keep distinct semantic roles (quantum capture, prepare,
immediate preflight, postverify); none is cached or removed.

Profile overflow, invalid ordering/correlation or malformed data invalidates
TIMING_PROFILE_GATE; it cannot silently overwrite or become a false timing
PASS. It must not mint authority or change existing Glue safety decisions.
Flush/format only after runtime/restore. First human profile keeps external
Observer ON; an OFF comparison parameter is reserved but not accepted as this
phase's independent UAT evidence.

Tests defined before implementation: deterministic injected clocks, nested
ordering/exclusive sums, zero/negative/nonfinite/missing timestamps, capacity
overflow, parent/quantum/operation linkage, pending notification inheritance,
unobserved dispatch, queue-depth/coalescing/watermark math, largest-stage
classification and percentiles. Compare profiler OFF/ON using identical
deterministic behavior inputs and native-operation outcomes; verify identical
Ctrl/plain/late/resize/feedback/END/restore decisions. Measure bounded profiler
overhead in a controlled CPU-only fixture without a timing SLA, and explicitly
retain the lack of real Explorer perturbation evidence until human profiling.

```text
R1C3B_PHASE1_PRIOR_ART_GATE = PASS
R1C3B_HOT_PATH_MAP_GATE = PASS
R1C3B_SAFE_DEDUP_OPTIMIZATION = NONE
SAFETY_CHANGE_REGISTER = EMPTY
R1C3B_ROOT_CAUSE = PENDING_HUMAN_PROFILE
```

Phase 2 candidates only: full/fast predicate split; bound Shell witness with
apartment/lifetime/failure proof; exact geometry versus full eligibility
postverify; last exact Follower result + watermark + generation against user
and manager races. None is proven safe to deploy here. Future Z-order remains
NOZORDER now; component size stays bounded with default product maximum NOT
DECIDED (future 2/4/8 fixtures); multi-monitor/mixed-DPI remains NOT TESTED and
transitions abort. No speculative optimization is made to produce nicer numbers.
