# PaneBind R1-C2B Explorer Glue Implementation Report

Report date: 2026-09-05 (Asia/Shanghai; UAT Fix 2 after the legacy-passing
Debug Attempt 2 exposed insufficient progressive Follower-motion evidence).

## 1. Round and evidence state

```text
Round = R1-C2B - Explorer Feedback Suppression & Glue Session Baseline
Starting main = 8ac18ab07344632e8f0ed87cafe1b85b2b715d06
Branch = codex/r1c2b-explorer-glue-session
Fix 1 starting HEAD = 650196507466498f41a6df44b5031733b560098f
Fix 2 starting HEAD = e0bccc0e8ab8870150fa79b9f1a70cdf1c902db5
Research checkpoint = ea46ab297d149f8416a0cff67d30b32fa71fc015
R0_BASELINE = SEALED
R1C2A_DEBUG_INTERACTIVE_UAT = PASS
R1C2A_RELEASE_INTERACTIVE_UAT = PASS
R1C2B_DEBUG_UAT_ATTEMPT_1 = BLOCKED
R1C2B_DEBUG_UAT_ATTEMPT_2_LEGACY_GATE = PASS
R1C2B_DEBUG_UAT_ATTEMPT_2_REALTIME_FOLLOW = INSUFFICIENT_EVIDENCE
R1C2B strengthened real Explorer runtime = PENDING_UAT
R1C2B_UAT_FIX1 automated rerun = PASS
R1C2B_UAT_FIX2 automated rerun = PASS
R1C3 = NOT STARTED
```

Debug Attempt 1 stopped safely at pre-consent `UnsafeLayout`; Fix 1 added
readiness preview and correct blocked-evidence handling. A human then completed
Debug Attempt 2 (`20260905T065805930Z`) on the Fix 1 implementation. Its legacy
runner PASS, safety, exact final geometry, lifecycle, and evidence integrity
remain valid. Its 31 raw Leader LOCATION receipts produced only one active
Follower apply, so real-time follow remains insufficient and is not accepted.
The full immutable-evidence review is in
[`R1C2B_ATTEMPT2_FORENSICS.md`](R1C2B_ATTEMPT2_FORENSICS.md).

Fix 2 preserves the distinction between receipt metadata and processing-time
geometry, introduces fair owner processing quanta, and strengthens the UAT
evidence criterion. No post-Fix-2 human Explorer UAT or Release Explorer UAT has
been run by this work. Historical automated results below are labeled by round;
Fix 2 final verification and Git handoff are recorded separately.

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

For every selected Leader LOCATION sample delivered to Core:

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

## 6. Owner STA, processing quanta, and pre-native watermark

The thread that creates the consent/session aggregate is the sole owner STA and
message-loop thread. It owns Explorer eligibility, event drain, behavior,
translation preparation, native apply, post-verification, terminal state, hook
teardown, and restore. The WinEvent source is installed/uninstalled on that
thread. `MsgWaitForMultipleObjectsEx` plus a waitable timer provides bounded
liveness without a polling event source.

Each nonempty drain forms one processing quantum. Raw receipts retain sequence,
native timestamp, role, quantum ID, and coalescing disposition without geometry.
The owner captures one fully validated live Leader/Follower pair for that
quantum and labels it `live_geometry_at_processing_quantum`. Within a quantum,
multiple Leader LOCATION receipts select the latest meaningful trigger while
preserving lifecycle barriers; coalesced receipts do not enter Core as copies
of that current geometry. A later quantum gets a fresh sample. The pre-apply
Follower sample and existing feedback watermark protections remain in force.

The owner message pump retrieves at most eight messages per quantum and checks
target queue readiness before and after each pump operation, including a
`PeekMessageW` call that returns false after delivering internal callbacks.
Delivered target receipts yield immediately to drain. Receipts delivered during
COM/native validation are handled in a new quantum without waiting for another
empty-to-nonempty notification. Drain clears notification-pending state so a
later empty-to-nonempty transition can notify again. This remains event-driven;
no periodic sampling timer or sleep loop was introduced.

Private Glue validation processes already-delivered Shell receipts using a
zero-deadline readiness check, avoiding the inherited 20 ms readiness wait and
its nested message drain. Full inventory, exact location, identity, security,
state, monitor, DPI, pre-native validation, and exact post-verification remain
mandatory. Ordinary R1-C2A validation retains its existing wait behavior.

Out-of-context START and LOCATION may already be queued when the owner regains
control. A Leader START therefore permits only:

- the exact armed layout; or
- a size-preserving, same-delta visible/positioning translation from it.

The selected LOCATION consumes the current quantum sample and can emit the
initial-relative plan. State/identity drift, inconsistent frame translation,
or resize/mixed geometry aborts. A quantum containing LOCATION and END handles
the coalesced final LOCATION first, allowing its last meaningful translation
before END reconciliation. That final apply does not count as evidence of an
apply before END receipt delivery.

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

Evidence additionally records processing quantum/sample generation and pre-
and post-native receipt watermarks for each active operation. The before-END
criterion requires an earlier source sequence, no END in the operation's
quantum, and a post-native watermark below the eventual END receipt sequence.
It proves owner-side ordering before END callback delivery, not an unrecorded
native END generation time or the exact time the human released the mouse.
Native event timestamps remain source metadata; wall-clock log-write times do
not establish causality.

Callbacks delivered during final post-verification can remain queued after the
active END quantum. After unhook, the owner records this tail as a separate
`inactive_discard` quantum with real receipt metadata, zero sample generation,
no geometry and no Core inputs. It preserves operation watermarks without
counting discarded callbacks as feedback or real-time progress.

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
-> preview exact pair and same monitor/DPI without consuming either session
-> if needed, allow at most three human resize + ENTER readiness checks
-> require a side-effect-free FIT preview
-> formal begin re-inspects and replans the pair
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

`ExplorerGlueConsent::preview_layout_readiness` is a repeatable,
non-consuming readiness operation over the two already-provisioned sessions.
It performs live target/pair validation and geometry calculation only. It does
not bind or consume Glue authority, consume the target sessions, call a native
window operation, or arm an event source. An invalid target may still be
retired by the existing fail-closed live validation; that is not successful
preview authority.

## 8. Temporary layout and native operation path

Before hooks arm, the session records both original visible and positioning
rectangles and plans a zero-gap pair wholly inside one shared work area:

```text
preferred: Leader | Follower
fallback:  Leader above Follower
tolerance: 0
```

The planner preserves each current size, centers the pair, and never changes
z-order or activation. Its safety mathematics are unchanged:

```text
horizontal required = (leader width + follower width) x max(height)
vertical required   = max(width) x (leader height + follower height)
```

If neither orientation fits, it returns `UnsafeLayout` instead of resizing.
This is a UAT fixture only and is not Snap.

Before formal begin, the harness records one to three
`pair_layout_preview` records. Each carries the attempt number and limit,
Leader/Follower visible sizes, work-area size, horizontal and vertical
required/available/excess sizes and fit booleans, same-monitor/DPI facts,
chosen orientation when feasible, and explicit false values for authority
binding/consumption, temporary peer-state retention, native apply, and
event-source arm. Results are `FIT`,
`NEEDS_MANUAL_RESIZE`, or `INVALIDATED`.

For `NEEDS_MANUAL_RESIZE`, the Console prints the live dimensions and exact
horizontal/vertical excess. The human may resize Leader and/or Follower and
press ENTER to request another live check, for at most three total previews.
This is `TEST FIXTURE PREPARATION ONLY`; PaneBind never resizes either window.
Identity, nonce location, state, monitor, and DPI are revalidated on each
attempt. Only a `FIT` preview proceeds. Formal
`ExplorerGlueConsent::begin` then independently inspects and plans again, so a
preview never bypasses the preview-to-authority TOCTOU boundary.

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

Only exact Leader END enters `Completing`. If its quantum also includes Leader
LOCATION, the final selected LOCATION is processed first. The owner captures final Leader and
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

At the pre-Fix-1 implementation HEAD
`650196507466498f41a6df44b5031733b560098f`, review found no high- or
medium-severity blocker. Both Debug and Release builds completed, and both
complete CTest suites passed:

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
| **Pre-Fix-1 total** | **11/11 PASS** | **11/11 PASS** |
| **Fix-1 final rerun** | **11/11 PASS** | **11/11 PASS** |

At that pre-Fix-1 handoff, the three focused R1-C2B executables contained 24
named deterministic scenarios: Core 9, event source 9, and session/layout 6.
They cover state/generation and
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

Fix 1 adds five named deterministic preview/side-effect scenarios, increasing
the focused total from 24 to 29 (Core 9, event source 9, session/layout 11).
Coverage includes horizontal fit, vertical
fallback, neither orientation fitting, monitor mismatch, DPI mismatch, target
invalidation, side-effect-free/non-consuming preview, and a too-large first
attempt followed by a fitting recheck. Ten runner fixtures cover complete
`PASS`, legal setup/restore no-op variants, `SAFE_BLOCKED`, missing/multiple
active lifecycle, legacy downgrade, and contradictory layout/feedback
`INVALID_EVIDENCE`. Attempt 1 offline replay returns the expected exit 2 while
all three source evidence hashes remain unchanged.

```text
R1C2B_UAT_FIX1_DEBUG_BUILD_CTEST = 11/11 PASS
R1C2B_UAT_FIX1_RELEASE_BUILD_CTEST = 11/11 PASS
R1C2B_UAT_FIX1_RUNNER_FIXTURES = 10/10 PASS
R1C2B_UAT_FIX1_ATTEMPT1_OFFLINE_REPLAY = SAFE_BLOCKED / EXIT 2
R1C2B_UAT_FIX1_OWNED_REGRESSION = DEBUG/RELEASE PASS
R1C2B_UAT_FIX1_COMPANION_REGRESSION = DEBUG/RELEASE PASS
R1C2B_UAT_FIX1_R1C2A_REGRESSION = DEBUG/RELEASE PASS
R1C2B_UAT_FIX1_FINAL_SHA_AND_PUSH = RECORDED_IN_FINAL_GIT_HANDOFF
```

Fix 2 verification must cover multiple wakes and live samples; many LOCATIONs
coalesced within one quantum; a new target in the next quantum; LOCATION+END;
duplicate/no-op samples; multiple exact native operations with mixed observed
and missing feedback; notification re-arm; and at least 100 raw LOCATION
receipts over multiple quanta without recursion, queue overflow, or final drift.
Final Fix 2 verification on 2026-09-05: Debug and Release builds PASS; each
complete CTest suite is 11/11 PASS. The focused named scenarios total 39 (Core
9, session 18, event source 12). Added tests use the production quantum helpers
with the actual Core coordinator, and synthetic event ingress with controlled
owned test windows. The 120-raw-receipt stress crosses 24 wakes/quanta and
produces 24 initial-relative operations, max ring depth 5 within capacity 8,
zero recursion/overflow/drift, and exact final geometry. The mixed-feedback
case reconciles two missing receipts after one acknowledged operation.

All 26 Windows PowerShell runner fixtures PASS, including 31-to-1 insufficiency,
too-short drag, repeated samples, missing feedback, false watermark/sample/ACK
rejection, and post-END native callbacks retained as an inactive terminal
quantum. Owned and Companion Debug/Release self-tests each return exit 0 and
summary PASS; the R1-C2A Explorer unit test passes in both configurations.
These are automated results, not post-Fix-2 human Explorer acceptance.

Commands use the same CMake/CTest executables and build directories listed
above. Runner fixtures are executed with:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\test-r1c2b-evidence-runner.ps1
```

## 11. R0, R1-C2A, and scope audit

- R0 observer implementation and semantics are unchanged. It remains optional,
  independent evidence in the UAT runner and never drives Glue.
- R1-C2A user-visible candidate selection, baseline exclusion, one-shot move
  consent, single translation, and restore semantics remain valid. The private
  Glue bridge is additive and only active for a fully authorized pair.
- Existing Explorer unit regressions passed in Debug and Release at the
  pre-Fix-1 handoff and again in the Fix 1 final rerun. No new real R1-C2A UAT
  is required.
- Fix 1 changes neither the platform-neutral `GlueMoveCoordinator` nor its
  feedback-suppression/pending-ledger rules.
- Fix 1 changes neither the narrow Glue WinEvent source nor hook/callback/
  bounded-queue behavior.
- Fix 2 changes owner scheduling, quantum sampling, private Glue Shell readiness
  cadence, and evidence acceptance. The callback workload and R1-A total-delta /
  feedback-ledger semantic core remain unchanged. R1-C2A calls retain the
  original default validation behavior; Fix 2 automated regression is required.
- The horizontal-first, vertical-fallback, zero-gap, pure-translation,
  no-resize layout planner safety rules are unchanged; the preview exposes
  their live inputs and result without weakening them.
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
R1C2B_GLUE_COORDINATOR_CHANGED_BY_FIX1 = NO
R1C2B_EVENT_SOURCE_CHANGED_BY_FIX1 = NO
R1C2B_LAYOUT_PLANNER_SEMANTICS_CHANGED_BY_FIX1 = NO
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
[`R1C2B_UAT_HANDOFF.md`](R1C2B_UAT_HANDOFF.md). Release Explorer UAT is outside
this Fix 2 implementation round.

The runner now treats evidence outcomes as three disjoint results:

| Outcome | Runner exit | Meaning |
| --- | ---: | --- |
| `PASS` | `0` | Complete safety/final/lifecycle evidence plus the strengthened multi-step follow gate passed |
| `SAFE_BLOCKED` / `KNOWN_BLOCKED` | `2` | A supported pre-native zero-side-effect blocker, or an exact-restored safe session with insufficient drag/realtime evidence |
| `INVALID_EVIDENCE` | `1` | JSONL/schema/sequence/lifecycle failed or records contradict the claimed outcome |

A pre-native safe-blocking harness itself exits `1`; after validating that
contract, the runner maps it to its distinct exit `2`. A completed safe session
with insufficient real-time evidence exits `2` directly after exact restore.
A Fix 1 pre-native summary reports
`feedback_suppression_evidence = not_reached`, not a fabricated missing-event
reconciliation. The historical Attempt 1 value
`no_feedback_event_reconciled` is accepted only under its legacy startup
contract and is never interpreted as runtime evidence.

For PASS, the runner scopes external Leader lifecycle to records received after
the drag prompt, so pre-authority human resize START/END does not create a false
failure. It independently reconstructs the centered, zero-gap planner result
from the final FIT preview and original work area, checks the common nonzero
Leader/Follower final delta, closes setup/active/restore operation geometry,
and requires one-to-one command/operation/reconciliation and unique feedback
attribution. The sole markerless compatibility path is offline replay of the
exact Attempt 1 prefix and three fixed SHA-256 hashes; live evidence cannot
downgrade out of the preview contract.

Fix 2 adds `REALTIME_FOLLOW_EVIDENCE_GATE` to the strict safety/final/lifecycle
checks. PASS requires raw Leader START=1, END=1, LOCATION>=3, at least two
distinct sampled Leader geometries, at least two exact active Follower native
applies, at least two distinct active Follower targets, and at least one apply
before END receipt delivery under the quantum/watermark rule in section 6.
All active applies must pass exact post-verification; final Follower geometry
must be exact; recursive operations and unexpected feedback must both be zero.
There is no receipt/apply ratio, FPS target, or final product latency SLA.

Fewer than three raw LOCATION receipts yield `INSUFFICIENT_DRAG_EVIDENCE` after
safe completion. A valid safe session with adequate raw receipts but insufficient
multi-step motion yields `INSUFFICIENT_REALTIME_FOLLOW`; a repeat of 31-to-1
cannot PASS. Both remain `SAFE_BLOCKED`, not malformed evidence. Missing
Follower LOCATION remains legal when exact native receipts and the final
snapshot reconcile it. The runner reports raw Leader count, processing quanta,
distinct Leader samples, Follower applies/targets/before-END count, and
Follower LOCATION/suppressed/duplicate/missing/reconciled counts separately.

## 13. Debug UAT Attempt 1 evidence review

The first real Debug attempt is preserved locally under ignored evidence prefix
`20260903T044532644Z`. Nothing in this review deletes, rewrites, or publishes
those raw files.

| Evidence fact | Verified result |
| --- | --- |
| Harness JSONL | 17 valid records; schema/name valid; physical sequence `1..17`; shutdown complete |
| Observer JSONL | 3,681 valid records; sequence `1..3681`; one complete hook registration, hook shutdown, and observer shutdown; no overflow/drop/post/incomplete diagnostic |
| Observer stderr | Present and empty |
| Leader provisioning | PASS; new `explorer.exe`; target consent and exact unique candidate PASS |
| Follower provisioning | PASS; distinct new `explorer.exe`; immutable baseline excluded Leader; target consent and exact unique candidate PASS |
| Pair validation | `BLOCKED / UnsafeLayout`; consent prefixes, pair distinction, and same monitor/DPI were true; layout was not planned |
| Glue consent/authority/binding | Not reached / not issued / not present |
| Glue event source | Not armed |
| Operation/native apply | Zero operation records; zero active Follower operations; zero Glue native apply |
| Safety | preexisting windows untouched; other third-party control false; global input false; user-window close attempted false |

Both visible frames measured `1839 x 1074` inside a `3072 x 1824` work area.
Horizontal placement required `3678` pixels of width, exceeding the work area
by `606`; vertical placement required `2148` pixels of height, exceeding it by
`324`. The independent Observer saw the human move both target windows during
readiness preparation, before Follower confirmation and pair validation, but
their sizes stayed `1839 x 1074`. Those pre-authority events are not Glue
runtime, feedback suppression, or Follower-operation evidence.

The Observer stream contained 12 structured inspection errors on unrelated
windows (five access-denied process-path lookups, five stale receipt-identity
checks, and two identity changes during snapshot). Neither target had a field
error, root/object mismatch, or destroy receipt. These were not queue, hook,
post, or Observer-lifecycle failures and do not contradict the pre-native
target evidence.

The primary blocker was therefore `UnsafeLayout before Glue consent`. The
secondary runner defect was an unconditional request for PASS-only records such
as `glue_consent_prompt` before branching on `pair_validation=BLOCKED`. Their
absence was expected on this path, but the old runner mislabeled it as malformed
evidence. Fix 1 validates the common provisioning and Observer contracts first,
then validates either the complete PASS suffix or the exact zero-authority,
zero-hook, zero-operation safe-block suffix. Any contradictory suffix remains
`INVALID_EVIDENCE`.

```text
R1C2B_DEBUG_UAT_ATTEMPT_1 = BLOCKED
ATTEMPT_1_PRIMARY_BLOCKER = UnsafeLayout before Glue consent
ATTEMPT_1_NATIVE_GLUE_OPERATION_ATTEMPTED = NO
ATTEMPT_1_GLUE_RUNTIME_EVIDENCE = NOT_REACHED
```

## 14. Attempt 2 acceptance and remaining NOT TESTED risks

Attempt 2 verified post-preview formal Glue consent/binding, same-monitor
layout, Leader START/LOCATION/END, one exact active Follower operation, exact
END reconciliation, and two-window restore under the legacy rule. It did not
prove progressive real-time Follower motion. Raw Leader 31 LOCATION receipts
had 31 distinct Observer processing-time snapshots, while Glue interpreted
one unique Leader geometry and emitted one move request plus 30 no-ops.
This is `BATCH_SNAPSHOT_COLLAPSE`, as proven by evidence and the old code.

Exact drain-cycle count and per-drain Leader/Follower counts are
`NOT_RECORDED / NOT_RECOVERABLE`; queue high-water depth 31 cannot replace
those missing records. The old operation precedes processed END in trace
order (operation trace 4, END trace 35), but native-END-before/after cannot be
established from its absent apply timestamps. Observer recorded a Follower
target LOCATION after Leader END; internal feedback count zero does not mean
Windows never emitted one. Details and hashes remain in the
[forensic report](R1C2B_ATTEMPT2_FORENSICS.md).

```text
ATTEMPT_2_LEGACY_R1C2B_EVIDENCE_GATE = PASS
ATTEMPT_2_SAFETY = PASS
ATTEMPT_2_FINAL_GEOMETRY = PASS
ATTEMPT_2_SESSION_LIFECYCLE = PASS
ATTEMPT_2_EVIDENCE_INTEGRITY = PASS
ATTEMPT_2_REALTIME_FOLLOW = INSUFFICIENT_EVIDENCE / NOT YET ACCEPTED
```

These remain `NOT TESTED` on real Explorer after Fix 2:

- progressive multi-quantum Follower motion under the strengthened gate;
- rejection of a third live candidate in the user's current Shell inventory
  (Follower-baseline exclusion of the live Leader itself passed Attempt 1);
- live Follower smoothness, event count, event latency, duplicate/missing/
  interleaved feedback mix, and suppression outcome;
- exact observed distribution of receipts across processing quanta;
- repeated real Explorer native apply/post-verification during a continuous drag;
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
- all Release real Explorer runtime evidence; no Release UAT is authorized in
  this Fix 2 implementation round.

R1-C3 product activation, Ctrl/global input, Snap, Glue Resize, and persistent
groups are outside this round, not missing R1-C2B acceptance evidence.

## 15. Implementation gate

```text
R1C2B_PRIOR_ART_GATE = PASS
R1C2B_BEHAVIOR_ENGINE_GATE = PASS
R1C2B_FEEDBACK_SUPPRESSION_GATE = PASS
R1C2B_EXPLORER_IMPLEMENTATION_GATE = PASS

R1C2B_UAT_FIX1 = PASS
R1C2B_UAT_FIX2 = PASS
R1C2B_SAFETY_GATE = PASS
R1C2B_FINAL_GEOMETRY_GATE = PASS
R1C2B_REALTIME_FOLLOW_IMPLEMENTATION_GATE = PASS
R1C2B_IMPLEMENTATION_READY = YES
R1C2B_INTERACTIVE_UAT = REQUIRED
R1C2B_RUNTIME_GATE = PENDING_UAT

R1C2B_UAT_FIX1_AUTOMATED_TESTS = PASS
R1C2B_UAT_FIX1_FINAL_SHA_AND_PUSH = RECORDED_IN_FINAL_GIT_HANDOFF
R1C2B_UAT_FIX2_AUTOMATED_TESTS = PASS
R1C2B_UAT_FIX2_FINAL_SHA_AND_PUSH = RECORDED_IN_FINAL_GIT_HANDOFF

R0_OBSERVER_SEMANTICS_CHANGED = NO
R0_REVALIDATION_REQUIRED = NO
R1C2A_REVALIDATION_REQUIRED = NO
R1C2B_GLUE_COORDINATOR_CHANGED_BY_FIX1 = NO
R1C2B_EVENT_SOURCE_CHANGED_BY_FIX1 = NO
R1C2B_LAYOUT_PLANNER_SEMANTICS_CHANGED_BY_FIX1 = NO

USER_PREEXISTING_WINDOWS_TOUCHED = NO
OTHER_THIRD_PARTY_CONTROL = NO
GLOBAL_INPUT_CONTROL = NO

R1C3 = NOT STARTED
```
