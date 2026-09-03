# PaneBind R1-C2B Explorer Glue Implementation Report

Report date: 2026-09-03 (Asia/Shanghai; implementation handoff before human
Explorer validation).

## 1. Round and evidence state

```text
Round = R1-C2B - Explorer Feedback Suppression & Glue Session Baseline
Starting main = 8ac18ab07344632e8f0ed87cafe1b85b2b715d06
Branch = codex/r1c2b-explorer-glue-session
Research checkpoint = ea46ab297d149f8416a0cff67d30b32fa71fc015
R0_BASELINE = SEALED
R1C2A_DEBUG_INTERACTIVE_UAT = PASS
R1C2A_RELEASE_INTERACTIVE_UAT = PASS
R1C2B real Explorer runtime = PENDING_UAT
R1C3 = NOT STARTED
```

R1-C2B now has an implemented and automated-tested baseline for one temporary
Glue Move session between exactly two newly created, separately user-consented
Explorer test windows. The human has not yet performed the R1-C2B Debug
Explorer run. Consequently, every real-runtime claim remains
`PENDING_UAT`/`NOT TESTED`; automated and synthetic results are not relabeled as
manual evidence.

The report is committed as part of the implementation handoff. Its own final
commit SHA, push result, clean status, and local/remote divergence are recorded
by the final Git handoff rather than self-referenced inside the commit.

## 2. Prior-art and platform-documentation gate

Detailed findings and provenance are recorded in
[`R1C2B_EXPLORER_GLUE_RESEARCH.md`](../research/R1C2B_EXPLORER_GLUE_RESEARCH.md)
and [`SOURCE_PROVENANCE.md`](../research/SOURCE_PROVENANCE.md).

| Source | Pinned revision | License/use | Applied conclusion |
| --- | --- | --- | --- |
| AltSnap | `5c86416ad21e4b72844a998a746bd3bb0bee5f5d` | GPL-3.0-or-later, reference-only | START/END may be synthesized; event identifiers are not intent or provenance authority |
| AltDrag | `e2740d605b0336a3b391fec26794718864b19521` | GPL-3.0-or-later, reference-only | injected/subclass movement paths remain rejected |
| PowerToys/FancyZones | `19c4d805321db86f3634e6968e14dbf25cbba14a` | MIT, reference-only in R1-C2B | minimal callback-to-owner routing is useful; topology/destroy changes must invalidate live behavior |
| Microsoft Win32 documentation | live pages reviewed 2026-09-03 | documented contracts | out-of-context delivery, installing-thread lifecycle, event meaning, native placement, and geometry contracts |
| PaneBind R1-B/R1-C1/R1-C2A evidence | merged baseline at starting main | local empirical evidence | programmatic placement can emit LOCATION without START/END and without fixed cardinality |

AltSnap/AltDrag code remains GPL reference-only. FancyZones was also inspected
as reference-only for this round. No external implementation code was copied,
adapted, translated, or mechanically rewritten.

```text
R1C2B_PRIOR_ART_GATE = PASS
EXTERNAL_CODE_COPIED = NO
EXTERNAL_CODE_ADAPTED = NO
```

## 3. Platform-neutral behavior coordinator

`src/core/behavior/glue_move_coordinator.h` implements a deterministic
`GlueMoveCoordinator` with these states:

```text
Idle -> Armed -> Active -> Completing -> Completed
                    \--------------------> Aborted
```

The coordinator accepts only platform-neutral values: opaque `WindowId`,
Leader/Follower role, normalized event kind, visible geometry, monotonic event
sequence, session/operation generations, and exact operation outcomes. It has
no HWND, HANDLE, DWORD, Explorer, COM, WinEvent constant, Windows header,
monitor/DPI API, or `SetWindowPos` dependency.

`arm` consumes an actual R1-A `WindowAdjacencyGraph`, requires exactly two
nodes, exactly one relation, and a Leader component containing only Leader and
Follower, then constructs the R1-A `TranslationSession` from that verified
pre-hook baseline. Exact Leader START revalidates and activates it; topology,
membership, roles, and initial rectangles stay frozen until terminal state.

For every Leader LOCATION:

- `Unchanged` is a no-op;
- `Translation` consumes the existing R1-A initial-relative total-delta plan;
- an already current/pending exact target is a no-op; and
- `ResizeOrMixed` aborts immediately.

The Follower target is always recomputed from session-initial geometry. No
incremental follower delta accumulates, so dropped/intermediate/no-op Leader
receipts cannot introduce drift.

Each emitted Follower command carries session generation, operation generation,
source Leader receipt sequence, Follower ID, and exact target visible rectangle.
The bounded Core pending ledger has capacity 64 in the Explorer session. A new
session gets a new generation and cannot consume a prior generation's feedback.

## 4. Feedback state and exact matching

Feedback authority is identity/generation/geometry based, never time based.

### Exact feedback

A Follower LOCATION that belongs to the current session and exactly matches a
pending expected target is acknowledged and suppressed. If it arrives before
the native result has been recorded, it remains provisional; the later exact
native result completes the acknowledgement. No Follower receipt can produce a
Follower move command.

### Duplicate feedback

A repeated LOCATION matching the current acknowledged Follower geometry and
unchanged identity/generation is recorded as duplicate self-feedback and
suppressed without another operation.

### Missing feedback

An exact native receipt and exact post-verification do not require a LOCATION
receipt. At Leader END, an exact final Leader/Follower snapshot may reconcile
such a completed operation. Counters preserve that distinction as missing and
reconciled; the implementation does not pretend that a WinEvent acknowledged
the operation.

### Unexpected feedback

Follower geometry matching neither a valid pending entry nor the exact current
acknowledged target aborts. Any Follower START/END lifecycle, stale or
non-monotonic sequence/generation, multiple ambiguous pending matches, and
illegal transition also aborts. Follower lifecycle is never required and cannot
complete the Glue session.

No rule uses a 50/100 ms ignore window, timestamp proximity, event contiguity,
one request/one event, or mandatory programmatic START/END.

## 5. Explorer Glue event source

R1-C2B adds a narrow event source under
`src/platform/windows/explorer/`. It is additive: R0 `WindowsObserver` was not
refactored into a control bus, and Observer JSONL is not runtime IPC.

The source has no public arbitrary-HWND constructor/factory. Only the private
`ExplorerGlueSession` authority can bind its already-authorized Leader and
Follower identities. For each unique target PID it installs three hook slots:

```text
EVENT_SYSTEM_MOVESIZESTART .. EVENT_SYSTEM_MOVESIZEEND
EVENT_OBJECT_LOCATIONCHANGE .. EVENT_OBJECT_LOCATIONCHANGE
EVENT_OBJECT_DESTROY .. EVENT_OBJECT_DESTROY

flags = WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS
```

The process filter only reduces noise. The callback performs fixed,
allocation-free checks for a supported event, exact role-bound HWND,
`OBJID_WINDOW`, and `CHILDID_SELF` before assigning a receipt sequence or using
a queue slot. Unrelated HWND and accessibility-child noise therefore cannot
exhaust the two-target ring. Owner drain then requires matching hook slot/PID
and live PID/TID/root/capability identity. Noise never enters behavior. DESTROY
is normalized from the already-bound receipt without querying a dead HWND.

The WinEvent callback does only fixed filtering and bounded ingress:

1. verify owner thread, reentrancy, and active source state;
2. reject unsupported, non-target-HWND, and non-window/self envelopes without
   consuming sequence/queue capacity;
3. assign a local monotonic receipt sequence;
4. copy a fixed record to a preallocated 512-entry ring; and
5. notify the owner with `PostThreadMessageW`.

It performs no COM/Shell inventory, live geometry capture, topology solving,
native query, allocation, complex logging, blocking work, or window operation.
Queue overflow does not drop-oldest or continue. Overflow, post failure,
reentrant callback/drain, wrong-thread use, hook install/unhook failure,
hook-receipt mismatch, root or identity drift, sequence exhaustion, and
post-poison receipts fail closed.

## 6. Owner STA, receipt batches, and pre-native watermark

The thread that creates the consent/session aggregate is the sole owner STA and
message-loop thread. It owns Explorer eligibility, event drain, behavior,
translation preparation, native apply, post-verification, terminal state, hook
teardown, and restore. The WinEvent source is installed/uninstalled on that
thread. `MsgWaitForMultipleObjectsEx` plus a waitable timer provides bounded
liveness without a polling event source.

Each drained receipt batch captures one Leader/Follower live snapshot pair
before processing any event that could move Follower. Every geometry receipt in
that batch is interpreted against this frozen pair. This prevents a later
`SetWindowPos` from changing the geometry attributed to an older receipt.

Out-of-context START and LOCATION may already be queued when the owner regains
control. A Leader START therefore permits only:

- the exact armed layout; or
- a size-preserving, same-delta visible/positioning translation from it.

The following LOCATION then uses the same frozen batch geometry and can emit the
initial-relative plan. State/identity drift, inconsistent frame translation, or
resize/mixed geometry aborts.

Before each active Follower native apply, the Windows ledger registers:

```text
behavior operation generation
source Leader receipt sequence
expected visible rectangle
expected positioning rectangle
latest event-source receipt sequence (registration watermark)
```

Registration occurs through the final operation bridge immediately before the
native call, after preparation and before `SetWindowPos`. Matching geometry is
not enough: Follower feedback must also have a receipt sequence strictly greater
than the registration watermark. An older receipt cannot be relabeled as
self-feedback after a later operation happens to write the same geometry.

If a Follower receipt appears later than a Leader LOCATION in the same already-
drained batch, it must be attributable to a previously completed exact pending
operation or the last acknowledged exact geometry before a new native apply may
run. Otherwise the session aborts before that apply. This closes the
Leader-event-backlog laundering case.

## 7. Explorer authority and two-window isolation

Leader and Follower reuse the proven R1-C2A target-consent eligibility prefix,
but their Glue authority is private, additional, temporary, role-specific, and
bounded to one drag session. It cannot be constructed from a raw HWND.

Provisioning is deliberately sequential:

```text
issue Leader from its new nonce location
-> keep Leader session alive
-> begin Follower baseline
-> permanently exclude Leader in that baseline
-> issue one distinct Follower from its own nonce location
-> validate exact pair and same monitor/DPI
-> present separate Glue summary
-> require Y + ENTER
-> issue one Glue consent/authority generation
```

The private R1-C2B bridge records the authorized peer's complete location,
PID/TID, capability, consent generation, and native identity. It is active only
while the pair is bound. A third Explorer remains a blocker. Ordinary R1-C2A
single-translation authorization, `OperationLimitReached`, and public API
semantics remain unchanged; target-confirmation consent is not silently
converted into R1-C2A move consent.

## 8. Temporary layout and native operation path

Before hooks arm, the session records both original visible and positioning
rectangles and plans a zero-gap pair wholly inside one shared work area:

```text
preferred: Leader | Follower
fallback:  Leader above Follower
tolerance: 0
```

The planner preserves each current size, centers the pair, and never changes
z-order or activation. If neither orientation fits, it returns `UnsafeLayout`
instead of resizing. This is a UAT fixture only and is not Snap.

After exact setup verification, the adapter builds the real R1-A graph, then
arms hooks. For an active plan the Follower path is:

```text
live revalidate role-bound Explorer target
-> prepare visible-to-positioning pure translation
-> register expected operation + receipt watermark
-> synchronous SetWindowPos(SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE)
-> capture exact native receipt and post-operation snapshot
-> report exact/mismatch/failure to Core
```

`SWP_ASYNCWINDOWPOS`, `AttachThreadInput`, foreground forcing, synthesized
input, resize, and batch claims of transactional behavior are absent. Native
nonzero alone is not success; actual visible and positioning rectangles and all
live identity/state predicates must match.

The operation record capacity is 512 and reserves terminal restore space; trace
capacity is 4096. Capacity exhaustion is terminal rather than silently losing
evidence.

## 9. Completion, abort, and cleanup

Only exact Leader END enters `Completing`. The owner captures final Leader and
Follower snapshots, verifies final Leader translation and R1-A Follower target,
and reconciles exact completed pending operations. New plans are rejected after
END.

The following conditions abort or block completion:

- Leader resize/mixed geometry;
- unexpected Follower movement/lifecycle;
- queue, trace, pending, or operation capacity exhaustion;
- event-source notification/hook/reentrancy/thread failure;
- target destruction or any identity/location/state/security/monitor/DPI
  invalidation;
- native call failure or exact post-verification mismatch;
- session/operation generation or sequence mismatch;
- final geometry mismatch; or
- bounded runtime timeout.

Every normal terminal path follows this ordering:

```text
mark terminal
-> stop/unhook Glue source
-> drain/discard now-inactive local receipts
-> independently restore Follower exactly
-> independently restore Leader exactly
-> release pair authority
-> leave both user Explorer windows open
```

Setup precedes hook arm and cleanup restore follows unhook, so setup/restore
LOCATION events cannot enter the active state machine. Public setup/arm/run/
cancel paths catch exceptions and enter cleanup. The aggregate is explicitly
owner-thread-affine and must not be transferred across threads.

## 10. Automated verification

The final implementation review found no high- or medium-severity blocker. Both
Debug and Release builds completed, and both complete CTest suites passed:

```powershell
cmake -S . -B out/r1c2b-debug -G "Visual Studio 18 2026" -A x64 -DBUILD_TESTING=ON
cmake --build out/r1c2b-debug --config Debug --parallel
ctest --test-dir out/r1c2b-debug -C Debug --output-on-failure

cmake -S . -B out/r1c2b-release -G "Visual Studio 18 2026" -A x64 -DBUILD_TESTING=ON
cmake --build out/r1c2b-release --config Release --parallel
ctest --test-dir out/r1c2b-release -C Release --output-on-failure
```

| CTest | Debug | Release |
| --- | --- | --- |
| `geometry` | PASS | PASS |
| `core-model` | PASS | PASS |
| `topology` | PASS | PASS |
| `translation` | PASS | PASS |
| `glue-move-coordinator` | PASS | PASS |
| `windows-text-encoding` | PASS | PASS |
| `windows-owned-operations-unit` | PASS | PASS |
| `windows-companion-unit` | PASS | PASS |
| `windows-explorer-unit` | PASS | PASS |
| `windows-explorer-glue-event-source-unit` | PASS | PASS |
| `windows-explorer-glue-session-unit` | PASS | PASS |
| **Total** | **11/11 PASS** | **11/11 PASS** |

The three focused R1-C2B executables contain 24 named deterministic scenarios:
Core 9, event source 9, and session/layout 6. They cover state/generation and
illegal transitions, exact/duplicate/missing/unexpected feedback, bounded
pending behavior, final reconciliation, failure/invalidation/overflow,
process/hook/object filtering, identity/root/thread/reentrancy failures,
DESTROY, notification/unhook poisoning, pre-native watermark ordering,
START+LOCATION batch geometry, and horizontal/vertical/unsafe layout.

The event-source noise stress injects 1,024 unrelated-HWND receipts and 1,024
accessibility-child receipts before a target event. All 2,048 are counted before
sequence/ring consumption, the target receipt remains available, and the source
does not poison or overflow.

The generated Core stress runs 160 generated Leader inputs plus one final
target, issuing 161 exact Follower operations. It inserts duplicates,
unrelated receipts, and Leader no-ops, proves strictly increasing operation
generations, initial-relative targets, maximum pending depth 1 on that path,
zero unexpected feedback, and no recursive Follower command. A separate path
test proves that dropping intermediate Leader positions produces the same final
target.

The preexisting controlled regression harnesses were also run explicitly:

| Controlled harness | Debug `--self-test` | Release `--self-test` |
| --- | --- | --- |
| R1-B Owned-window | PASS / exit 0 | PASS / exit 0 |
| R1-C1 Companion-process | PASS / exit 0 | PASS / exit 0 |

These self-tests control only their own fixtures/companion process. They are not
real Explorer UAT.

## 11. R0, R1-C2A, and scope audit

- R0 observer implementation and semantics are unchanged. It remains optional,
  independent evidence in the UAT runner and never drives Glue.
- R1-C2A user-visible candidate selection, baseline exclusion, one-shot move
  consent, single translation, and restore semantics remain valid. The private
  Glue bridge is additive and only active for a fully authorized pair.
- Existing Explorer unit regressions pass in Debug and Release. No new real
  R1-C2A UAT is required.
- Core remains free of Win32 and Explorer dependencies.
- No preexisting user Explorer was issued a capability, included in topology,
  moved, navigated, or closed during implementation/automated validation.
- No Excel, VS Code, browser, Power BI, Terminal, system window, or other user
  application was controlled.
- No global keyboard/mouse hook, `SendInput`, `mouse_event`, `keybd_event`, DLL
  injection, input attachment, or resident high-frequency polling was added.
- No Snap, Glue Resize, persistent group, dynamic group membership, Ctrl
  activation, or R1-C3 implementation was added.

```text
R0_OBSERVER_SEMANTICS_CHANGED = NO
R0_REVALIDATION_REQUIRED = NO
R1C2A_REVALIDATION_REQUIRED = NO
USER_PREEXISTING_WINDOWS_TOUCHED = NO
OTHER_THIRD_PARTY_CONTROL = NO
GLOBAL_INPUT_CONTROL = NO
```

## 12. Evidence harness and privacy

`panebind-explorer-glue-harness` is interactive-only. The PowerShell runner
starts the independent R0 observer, runs the harness in the inherited foreground
console, waits for the observer to exit naturally, then strictly validates both
JSONL streams. Runtime Glue uses its own event source and has no dependency on
observer stdout.

Raw evidence is written only below ignored `uat/r1c2b/`:

```text
<timestamp>-glue-harness.jsonl
<timestamp>-glue-observer.stdout.jsonl
<timestamp>-glue-observer.stderr.log
leader-<nonce>/
follower-<nonce>/
```

The committed report does not publish full nonce paths, unrelated window
titles/metadata, or raw HWND values. Native keys exist only in ignored evidence
for strict target correlation.

The exact Debug command and human actions are in
[`R1C2B_UAT_HANDOFF.md`](R1C2B_UAT_HANDOFF.md). Release UAT must not be requested
until the user returns a passing Debug evidence set.

## 13. Remaining NOT TESTED risks

Until the human Debug run is reviewed, these are `NOT TESTED` on real Explorer:

- issuance and pairing of two real newly created Explorer top-level windows;
- Follower-baseline exclusion of the live Leader and rejection of a third live
  candidate in the user's current Shell inventory;
- real same-monitor/DPI layout setup, exact adjacency, and two-window restore;
- native Explorer title-bar START/LOCATION/END lifecycle for the Leader;
- live Follower smoothness, event count, event latency, duplicate/missing/
  interleaved feedback mix, and suppression outcome;
- real START+LOCATION already queued in one owner drain;
- real Explorer native apply/post-verification under drag load;
- real timeout, user-moved Follower, Leader resize, navigation, minimize/
  maximize, target destruction, monitor/DPI change, queue overflow, hook failure,
  native failure, and post-verification failure paths (automated only);
- Explorer hang behavior during synchronous placement;
- HWND/PID reuse and late callback behavior after real target destruction;
- vertical fixture fallback on a real desktop and unusual work-area/window-size
  combinations;
- mixed-DPI, multi-monitor, cross-monitor, elevated, UIAccess, AppContainer,
  cross-user, cross-session, and virtual-desktop transitions;
- allocation failure/OOM and long-session sequence/capacity exhaustion;
- measured smoothness, CPU, and memory; and
- all Release real Explorer runtime evidence, which is intentionally deferred
  until Debug PASS.

R1-C3 product activation, Ctrl/global input, Snap, Glue Resize, and persistent
groups are outside this round, not missing R1-C2B acceptance evidence.

## 14. Implementation gate

```text
R1C2B_PRIOR_ART_GATE = PASS
R1C2B_BEHAVIOR_ENGINE_GATE = PASS
R1C2B_FEEDBACK_SUPPRESSION_GATE = PASS
R1C2B_EXPLORER_IMPLEMENTATION_GATE = PASS

R1C2B_IMPLEMENTATION_READY = YES
R1C2B_INTERACTIVE_UAT = REQUIRED
R1C2B_RUNTIME_GATE = PENDING_UAT

R0_OBSERVER_SEMANTICS_CHANGED = NO
R0_REVALIDATION_REQUIRED = NO
R1C2A_REVALIDATION_REQUIRED = NO

USER_PREEXISTING_WINDOWS_TOUCHED = NO
OTHER_THIRD_PARTY_CONTROL = NO
GLOBAL_INPUT_CONTROL = NO

R1C3 = NOT STARTED
```
