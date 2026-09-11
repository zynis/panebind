# R1-C3B Phase 1 — Profiling Implementation Handoff

Date: 2026-09-09 (Asia/Shanghai). This is a profiling implementation and
deterministic verification report, not a real Explorer bottleneck diagnosis.

## Outcome / Git checkpoint

~~~
Starting main = d901094404f04772e1d641842df76d13c551a734
Branch = codex/r1c3b-smoothness-profile
Research/map commit = 5bd74cc
Implementation SHA = db968e2300928c770987304f96219e4ba0dfc861

R1C3B_PHASE1_PRIOR_ART_GATE = PASS
R1C3B_HOT_PATH_MAP_GATE = PASS
R1C3B_PROFILING_IMPLEMENTATION_GATE = PASS
R1C3B_PROFILING_DATA_QUALITY_GATE = PASS
R1C3B_SEMANTIC_EQUIVALENCE_GATE = PASS
R1C3B_SAFE_DEDUP_OPTIMIZATION = NONE
R1C3B_PROFILE_READY = YES
R1C3B_DEBUG_PROFILE_UAT = REQUIRED
R1C3B_ROOT_CAUSE = PENDING_HUMAN_PROFILE
~~~

Initial ordinary fetch passed: clean main at the expected SHA, main/origin 0/0.
No HTTP/TLS/proxy configuration or API object/ref reconstruction was used.
The implementation commit contains source, evidence integration and tests;
this subsequent documentation handoff does not change that implementation.
Final containing HEAD, commit/push result and upstream divergence are recorded
in the terminal user handoff after they actually occur. No PR/merge/tag/release
is authorized in this phase.

## Prior art and frozen empirical starting point

Actual source/license/history inspection and official API links are in
[research](../research/R1C3B_SMOOTHNESS_RESEARCH.md) and
[provenance](../research/SOURCE_PROVENANCE.md). Inspected revisions:

- AltSnap: 5c86416ad21e4b72844a998a746bd3bb0bee5f5d (GPL-3.0-or-later).
- AltDrag: e2740d605b0336a3b391fec26794718864b19521 (GPL-3.0-or-later).
- PowerToys/FancyZones: 19c4d805321db86f3634e6968e14dbf25cbba14a (MIT).
- AquaGlue official public help: UX/quality reference only, no internal claims.
- Microsoft Learn: WinEvent/message scheduling, native placement, identity,
  DWM/geometry, monitor/DPI, QPC/QPF and deferred-placement contracts.

All external code is reference only; nothing copied/adapted. AltSnap cadence
and worker/history plus FancyZones' repeated-layout loop fix support measuring
work and scheduling, not guessing a PaneBind fix or copying timers/hooks.

The two accepted C3A local raw logs were reread with the frozen offline runner;
both PASS. Debug prefix 20260909T101910373Z has 307 raw LOCATION, 15 Leader
quanta/distinct samples/applies. Release 20260909T103941915Z has 548 raw
LOCATION, 40 Leader quanta, 39 distinct samples/applies and one no-op. Both
have exact feedback/final/restore, no recursion and subjective smoothness C.

| Existing C3A metric (ms) | Debug p50 / p95 | Release p50 / p95 |
| --- | --- | --- |
| Activation decision (one sample each) | 195.3543 / 195.3543 | 160.6484 / 160.6484 |
| Apply interval | 297.80185 / 485.225 | 223.1838 / 452.2333 |
| Receipt to owner | 157.5476 / 397.4413 | 112.974 / 308.6417 |
| Owner to native | 223.8743 / 418.1807 | 149.5724 / 364.538 |
| Native call | 9.6256 / 14.2585 | 7.6767 / 12.3686 |
| Native start to postverify | 66.2722 / 120.9586 | 40.0458 / 90.839 |
| Source receipt to exact postverify | 328.9357 / 509.4657 | 209.8503 / 404.2079 |

Release maximum apply interval is 541.7231 ms. Different-grain p50 values are
not additive. Native call is not the dominant observed cost in those runs,
but fine stages are **NOT RETROACTIVELY RECOVERABLE**. The old evidence cannot
identify which Shell/security/geometry stage dominates or prove Observer
perturbation. No new human profile was run.

## Implemented measurement path

The [hot-path map and risk matrix](R1C3B_HOT_PATH_MAP.md) were committed before
source edits. The existing single owner and actual Leader geometry remain
authoritative; no second movement engine is introduced.

~~~
callback -> real notification edge / inherited pending notification
-> owner private-message retrieval OR direct backlog drain
-> drain / quantum -> Leader full capture -> Follower full capture
-> Ctrl/geometry/Core/feedback policy
-> operation prepare -> prepare full validation -> positioning plan
-> immediate full validation -> pending registration
-> SetWindowPos -> full postverify -> exact comparisons -> receipt
-> operation result / feedback ACK -> END captures/reconcile -> fixture restore
~~~

| Measurement | Actual implementation |
| --- | --- |
| Scheduling | Unique post ID, trigger receipt, existing callback QPC, actual post-boundary QPC, actual private-message retrieval QPC when observed; inherited/already-pending receipts share the original ID |
| Missing dispatch | Zero plus dispatch_observed=false means NOT OBSERVED; no fabricated dispatch at drain. Direct drain can precede later dispatch. Such intervals are not forced into negative latency calculations |
| Queue/backlog | Per-quantum first/last/raw/Leader/Follower/coalesced counts, start/end QPC, before/after/end depth and high-water; delivered receipt watermarks attribute arrivals during the prior quantum/native-call/postverify |
| Capture | Separate Leader/Follower containers. Each full validation is split, in original order, into token/ledger, Shell observation, global inventory, exact location/peer witness, native identity, image file identity, structure/class/style, state/cloak, virtual desktop, security, positioning, DWM visible frame, monitor/DPI, eligibility/snapshot finalize |
| Core | Per-receipt Core decision; Leader and Follower statistics printed separately. Classify/initial-relative MovePlan/no-op remain inside unchanged Core |
| Feedback | Platform pending exact-match/watermark guard, Core policy, ACK/duplicate handling and pending removal are measured; callback-to-ACK retains original receipt correlation |
| Prepare / pending | Activation/permit/allowance bookkeeping, fresh prepare validation, positioning calculation, fresh immediate validation and pending-before-native registration retained and measured |
| Native / postverify | Existing SetWindowPos start/return kept; fine native span, full postverify subcategories, exact two-rect/identity comparisons and receipt completion; native LastError preserved across new timing calls |
| Correlation | Span/parent IDs, quantum, role, operation generation and source receipt join existing sampled geometry/MovePlan/native/ACK records; no wall-clock fuzzy join |

All detailed formatting/output/file writing happens after run_until_terminal
has unhooked and restored. No serialization or unbounded allocation was added
to the measured hot path. Existing validation allocations are not silently
removed or mislabeled as profiler allocation.

## Data quality, capacity and interpretation

New records: profile_span, profile_quantum, profile_notification, profile_status.
Storage is preallocated before arming: 32768 spans, 4096 quantum envelopes,
4096 notifications; maximum nested scope depth 32. Existing event queue 512,
pending ledger 64, base traces/receipts/quanta 4096 and operations 512 remain.
Overflow/invalid ordering is sticky diagnostic failure: no overwrite or timing
PASS. Profiling does not override existing Glue correctness/abort decisions.

The runner validates exact capacities/counts, positive integer timestamps,
nonnegative durations, parent/child and sibling ordering, operation/quantum/
receipt linkage, full-validation stage coverage/order, queue/coalescing math,
native/postverify watermarks, notification inheritance and explicit unobserved
dispatch. Actual QPC read count is checked against the emitted profile points.
Every processed Core receipt has a matching decision span.

Inclusive statistics use invocation grain. Exclusive time subtracts only
immediate children; overlapping parents/children are never added as disjoint
stages. Matched-operation percentage totals cover operation-local sequential
exclusive intervals only. Shared source-quantum captures are reported
separately, not counted again per operation. The largest-stage distribution
and TOP OBSERVED HOT STAGES ranking are calculated from input data, not
hard-coded. Parent residuals remain labeled measured exclusive scope time;
they are not assigned a speculative cause.

No p50(A)+p50(B) claim, FPS interpretation, presentation timestamp, fixed latency
threshold or performance acceptance SLA is introduced. External Observer ON
is the first-run mode. Its possible perturbation remains UNRESOLVED; the
reserved OFF runner parameter is rejected until a future controlled comparison.

## Semantic equivalence and duplicate-work decision

Repeated full validation calls were found, but quantum sampling, prepare,
immediate preflight, postverify and END are different semantic instants.
Intervening COM/native work can reenter and deliver events. They do not meet
the same-instant/no-wider-stale-window proof. Snapshot copies/pure-CPU
recalculations are candidates only, with no claimed measured benefit.

~~~
SAFE_DEDUP_APPLIED = NO
SAFETY_CHANGE_REGISTER = EMPTY
SAFETY_CHECKS_REMOVED = NONE
CROSS_QUANTUM_ELIGIBILITY_CACHE = NO
~~~

Diff review preserves all eligibility/location/PID/TID/generation/monitor/DPI/
postverify predicates, native flags, pending-before-native, initial-relative
translation, feedback suppression, END and restore. Core, activation policy,
Ctrl sampler and eight-message/coalescing helper are unchanged. Private
bridge signatures receive only a nullable profiler, default null. C2B/C3A
retain their existing executable/schema/authorization behavior.

The OFF/ON deterministic owned fixture uses identical Ctrl/plain/late/resize
inputs and checks the same requested/native operations, exact suppression and
END outcomes. Synthetic notification tests verify inherited edges without real
keyboard input/live hooks. This proves seeded semantic equivalence, not
identical live batching/cardinality under added timing overhead.

### Callback workload and instrumentation perturbation

Default paths allocate no full profiler buffers and take no new stage QPC
samples; nullable guards and small receipt metadata are not claimed to be
literally zero instruction/memory cost.

Profiling ON adds one QPC at each actual notification post edge and fixed-size
correlation bookkeeping. This is a **small workload expansion**, disclosed
rather than claiming the requested zero-expansion target was fully achieved.
It is needed for actual post timing instead of fabricating it from receipt
time. No COM, DWM, geometry, heap work, JSON, console or file I/O is added to
callbacks. Ctrl still samples only Leader START, not LOCATION.

Controlled CPU-only probe, 2000 iterations/four spans per iteration, 16000
profile QPC reads; frequency 10000000. One observed rerun:

| Build | OFF ticks / ms | ON ticks / ms |
| --- | --- | --- |
| Debug | 538 / 0.0538 | 12514 / 1.2514 |
| Release | 206 / 0.0206 | 4504 / 0.4504 |

Same work/output and valid bounded records in both modes; no timing threshold.
These are tiny synthetic workloads, exclude preallocation, and do not estimate
real Explorer slowdown or Observer contention. Do not extrapolate their ratio.

## Automatic regression and environment

Windows 10.0.26200 x64, Visual Studio 18 2026 Community, MSVC 19.50.35729.0,
Windows SDK 10.0.26100.0, CMake 4.2.3-msvc3, Windows PowerShell.

| Verification | Actual result |
| --- | --- |
| Debug configure/build/CTest | PASS / 13 of 13 |
| Release configure/build/CTest | PASS / 13 of 13 |
| Owned Debug/Release self-test | PASS / PASS, failures 0 |
| Companion Debug/Release self-test | PASS / PASS, failures 0 |
| C2A/C2B/C3A deterministic regressions | PASS in both builds |
| Existing C2B runner fixtures | 26 PASS |
| C3A runner fixtures | 61 PASS |
| New C3B profile fixtures | 47 PASS: end-to-end modes, malformed evidence, dynamic largest-stage/partition assertions and no/late-Ctrl safety |
| Profile executable help / diff hygiene / Core isolation | PASS |

Commands (use the Visual Studio CMake/bin executables when not on PATH):

~~~powershell
cmake -S . -B out/r1c3b-debug -G "Visual Studio 18 2026" -A x64
cmake -S . -B out/r1c3b-release -G "Visual Studio 18 2026" -A x64
cmake --build out/r1c3b-debug --config Debug --parallel
ctest --test-dir out/r1c3b-debug -C Debug --output-on-failure
cmake --build out/r1c3b-release --config Release --parallel
ctest --test-dir out/r1c3b-release -C Release --output-on-failure
.\out\r1c3b-debug\src\platform\windows\Debug\panebind-owned-window-harness.exe --self-test
.\out\r1c3b-release\src\platform\windows\Release\panebind-owned-window-harness.exe --self-test
.\out\r1c3b-debug\src\platform\windows\Debug\panebind-companion-harness.exe --self-test
.\out\r1c3b-release\src\platform\windows\Release\panebind-companion-harness.exe --self-test
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c2b-evidence-runner.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c3a-evidence-runner.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c3b-profile-runner.ps1
~~~

## Human stop point and future boundaries

Use the single command and simple steps in
[Debug profile handoff](R1C3B_PROFILE_UAT_HANDOFF.md); return the prefix and A–E
subjective rating. No real Ctrl/Explorer input was generated by Codex.
Real fine-stage costs, instrumentation/Observer perturbation and bottleneck
classification remain NOT TESTED / PENDING_HUMAN_PROFILE.

Phase 2 full/fast validation, bound Shell witness, postverify split and cached
Follower-state designs remain unproven candidates. Future Z-order requires
normal/topmost/owner/focus/order research; NOZORDER is unchanged. Component
size must be bounded, but the product maximum is NOT DECIDED (future 2/4/8
fixtures). Real component stays two windows. Mixed-DPI/multi-monitor remains
NOT TESTED and transitions abort. No Glue Resize, Snap or wider application
eligibility was implemented.

~~~
R0_OBSERVER_SEMANTICS_CHANGED = NO
R1C2A_REVALIDATION_REQUIRED = NO
R1C2B_REVALIDATION_REQUIRED = NO
R1C3A_REVALIDATION_REQUIRED = NO
WH_KEYBOARD_LL_USED = NO
RAW_INPUT_USED = NO
HIGH_FREQUENCY_POLLING = NO
ZORDER_CHANGE = NO
COMPONENT_SIZE_CHANGE = NO
MIXED_DPI_SUPPORT = NO
USER_PREEXISTING_WINDOWS_TOUCHED = NO
OTHER_THIRD_PARTY_CONTROL = NO
R1C3B_PHASE2 = NOT STARTED
~~~

## Phase 2 continuation — 2026-09-10

The preceding status is the historical Phase 1 handoff. The user's Phase 1
Debug profile 20260909T143539511Z has since completed (subjective C; correctness
and profile validation PASS). Global Shell inventory is the measured primary
hotspot, with long-quanta backlog as its downstream effect.

Phase 2 was attempted, then stopped on an unproved same-HWND Shell-object
multiplicity invariant. Experimental source/runner changes were withdrawn;
there is no optimized implementation SHA or human UAT request. See the
[Phase 2 execution report](R1C3B_PHASE2_EXECUTION_REPORT.md),
[validation contract](../research/R1C3B_PHASE2_VALIDATION_CONTRACT.md) and
[safety change register](R1C3B_PHASE2_SAFETY_CHANGE_REGISTER.md).

Current continuation status: PHASE2_FAST_PATH=BLOCKED,
PHASE2_IMPLEMENTATION_READY=NO, PHASE2_DEBUG_UAT=NOT_READY.

## Human Root amendment implementation — 2026-09-10

The stop above remains historical evidence. Human Root explicitly refined
post-issuance movement authority to the selected top-level native frame, with
the original Shell object retained as an invalidating anchor. Current
[implementation/test results](R1C3B_FRAME_AUTHORITY_EXECUTION_REPORT.md) pass the
amended automatic gates on d8bdc0ba3ec7ef625081eacf49113c8293e51268.
Only [one optimized Debug UAT](R1C3B_PHASE2_DEBUG_UAT_HANDOFF.md) is handed off;
real smoothness remains PENDING_UAT, not an old-rule equivalence claim.

## Phase 3 continuation — 2026-09-12

Phase 2 human Debug prefix 20260910T161142993Z is correctness PASS, subjective
C+ (better than C, not B), with 60 exact applies/suppressions and zero active
inventory. The [Phase 3 report](R1C3B_PHASE3_EXECUTION_REPORT.md) preserves that
result and records VDM service-lifetime reuse only. Runtime SHA
201f3c25cee49f7b2ea7af67063cfaf7df2d5725 passes automatic gates; no desktop
result cache or third-hotspot optimization. The current stop point is
[one Phase 3 Debug UAT](R1C3B_PHASE3_DEBUG_UAT_HANDOFF.md).
