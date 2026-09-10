# R1-C3B Phase 2 — blocked proof review

Review 2026-09-09/10 (Asia/Shanghai). Outcome: NOT READY; no optimized UAT
handoff. Implementation experiment withdrawn after a countermodel exposed an
unproved safety invariant. This is not a Git transport failure or a rejection
of the Phase 1 performance diagnosis.

## Starting and final repository state

- Branch: codex/r1c3b-smoothness-profile; no new branch.
- Starting and final HEAD: 14e0c6818894c6912d3b3cb4d5a922f427b4e6ce.
- origin/main: d901094404f04772e1d641842df76d13c551a734.
- Starting working tree: clean. Ordinary git fetch origin: PASS.
- main comparison: 0 behind / 3 ahead; upstream comparison: 0 / 0.
- Existing branch commits: 5bd74cc (research/map), db968e2 (Phase 1 profiling),
  14e0c68 (Phase 1 handoff). No Phase 2 commit or push, PR, merge or tag.
- Final worktree: documentation-only changes, intentionally uncommitted at the
  architectural stop point. No source/test/CMake/runner diff or staged change.
- Raw uat/ untouched and ignored. Drafts under ignored
  out/r1c3b-phase2-rejected-draft/ are NOT accepted implementation evidence.

## Phase 1 input and evidence quality

The user-completed Debug prefix 20260909T143539511Z was replayed with:

~~~powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/run-r1c3b-smoothness-profile.ps1 -ValidateEvidencePrefix uat/r1c3b/20260909T143539511Z -ValidationHarnessExitCode 0 -ValidationObserverExitCode 0
~~~

Exit 0 / PASS. Harness file 1,111,243 bytes, Observer stdout 5,672,802 bytes,
stderr 0. Harness SHA256:
8BAC49C1F2D7AC4F113651C123A50EBAE5208E0EF7EF3E5A8891825797A62322.
The data-quality workflow kept invocation, operation-local exclusive and
quantum grain separate; no parent/child durations were added twice and no
synthetic timing was relabeled as a real speedup.

Human C; Ctrl/realtime correctness PASS. Leader START/LOCATION/END 1/365/1,
18 Leader processing quanta, 16 distinct samples and 16 distinct Follower
applies/targets, all before END. Internal/external Follower LOCATION 16/16;
suppressed/duplicate/missing/reconciled 16/0/0/0, recursive/unexpected 0/0,
exact final geometry and restore.

Primary cause GLOBAL_SHELL_INVENTORY_REVALIDATION: 92 calls, p50 40.22775 ms,
p95 106.9213 ms, max 127.9998 ms; 82.9303798959% of operation-local exclusive
aggregate, largest in 16/16 operations. Full validation p50 46.6152 ms,
p95 122.8728 ms. Quantum p50 218.5243 ms, max queue 51.

Repeated enumeration entails global Shell COM traversal, HWND/location reads
and filesystem identity work, not a constant-cost token lookup. Capture,
prepare, immediate pre-native and postverify each have distinct TOCTOU roles.
Their global-selection work cannot simply be deleted with the entire check.

Notification-to-dispatch p50 about 1030 ms is not owner processing wait:
362 inherited pending notifications, 20 drain-before-dispatch cases, zero
dispatch-before-drain observations. Long validation quanta explain the queue
backlog hypothesis. Message loop/fairness/coalescing were not changed.

## Safety decision and requested questions

Detailed [threat model and countermodel](../research/R1C3B_PHASE2_VALIDATION_CONTRACT.md)
and [safety change register](R1C3B_PHASE2_SAFETY_CHANGE_REGISTER.md) are authoritative.

| Question group | Finding / disposition |
| --- | --- |
| Provisioning uniqueness | Identifies the newly user-confirmed non-baseline exact target. This is separate from an issued object's identity. |
| Canonical binding / unrelated duplicate B | Canonical A + private token/generation prevents B on another HWND from inheriting authority merely by sharing a location. This does not prove a one-object/one-frame relation. |
| Ongoing uniqueness | Global exact-location uniqueness for distinct frames may be selection-only; current same-HWND Shell-entry multiplicity is a separate existing rejection predicate. The candidate failed to preserve it. |
| Navigation / HWND / quit / stream / generation threats | Candidate direct location, HWND, canonical identity, browser receipts and private generations handled the modeled cases, but cannot establish absence of another Shell object sharing H. Real Explorer invalidation behavior was NOT TESTED. |
| Full/active split | Rejected candidate retained boundary inventory and removed active inventory. Actual delivered behavior remains Phase 1 full inventory at every existing point. |
| Before/after calls | Actual code unchanged: the active call sites remain. Zero active global inventory NOT ACHIEVED. A modeled 80-to-0 request fixture is not implementation acceptance. |
| Location / canonical / token | Original direct filesystem identity, canonical object and generation-scoped ledger checks remain unchanged. Binding entry counts are snapshots, not live evidence. |
| Process/security/state/desktop | All existing image, process-instance, PID/TID, user/session/integrity, elevation, UIAccess/AppContainer and desktop checks unchanged. |
| Geometry/monitor/DPI | Positioning and visible bounds, size preservation, exact native postverify, PMv2 and same-monitor/DPI anchors unchanged. |
| Feedback | Pending-before-native, operation/session/target generations, source watermarks, exact geometry correlation and suppression unchanged. |
| Invalidation | No weakened contract delivered. Observed invalidation must still stop active writes; no authority resurrection or replacement by location. |
| Epoch fallback | No proved shared immutable interval across current COM/native-separated validation points; exact-object epoch cannot certify global membership/location. BLOCKED, not time-cached. |

## Experiment, withdrawal and checks

The draft introduced an opt-in executable, fresh bound proof, both-target
receipt checkpoints and phase/mode audit records. It compiled in Debug. During
adversarial review, a shared-frame countermodel held canonical A, H, location,
generation and A's delivered browser stream unchanged while current inventory
entry count for H became 2. Existing full validation rejected; proposed exact-
object proof accepted. This is AUTOMATED MODEL EVIDENCE, not a live Explorer
tab observation. Official HWND documentation supports treating frame and tab
identity as distinct, but does not prove the modern host's event ordering.

The focused draft Explorer unit test ultimately exited 0 and printed the
countermodel reproduction. The initial run exited 1 for two test-fixture owner
Ctrl-availability mistakes, corrected in the fixture; both outputs/drafts are
retained locally. Other modeled checks included both-role navigation, direct
location mismatch without an event, HWND/canonical/generation/stream errors,
duplicate-window ledger/activation rejection and native-predicate equivalence.
These passing subsets do NOT establish the missing frame invariant.

All experimental tracked edits were removed using patches, verifying each
resulting Git content hash against HEAD. New experimental source/runner files
were archived as .draft under ignored out/ then removed from active paths.
The generated optimized Debug executable was quarantined there with suffix
.exe.disabled. No user file/window/log was removed; drafts are recoverable.

Baseline restoration check: Debug rebuild PASS; CTest 13/13 PASS, 0 failed,
reported test duration 1.45 seconds. These are restored Phase 1 tests, not
Phase 2 acceptance. Exact commands:

~~~powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build out/r1c3b-debug --config Debug --parallel
& 'D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --test-dir out/r1c3b-debug -C Debug --output-on-failure
~~~

Windows 10.0.26200, VS 18 x64 / MSBuild 18.5.4, Windows SDK 10.0.26100.0;
existing CMake multi-configuration Debug build directory. Release binaries were
not built from the experimental changes. Release/native Owned/Companion and 26/61/47 runner fixture suites
were not repeated for acceptance: there is no accepted Phase 2 implementation.
Those prior-round results are not relabeled as new regressions. New Phase 2
runner fixtures/comparison were incomplete when the safety stop was reached.

## Human handoff / remaining review

Debug optimized UAT command: NONE / NOT READY. Do not run the quarantined draft
or repeat Phase 1 UAT. No real Ctrl+Move/Explorer/Release UAT was performed by
the agent. The standard future interaction sequence (new Leader/Follower, FIT,
Ctrl before roughly one-second drag, mouse up, Ctrl up, restore; A-E plus full
summary) is unchanged, but is not requested now.

Before implementation resumes, review whether the selected target must remain
the sole Shell object in its frame, and identify an evidenced event-driven
target-frame witness or a genuinely valid inventory epoch. Do not infer that
IE tab documentation is a current-host Explorer observation. No new runtime
control or callback expansion is authorized by this blocked handoff.

~~~text
R1C3B_PHASE1_ROOT_CAUSE = GLOBAL_SHELL_INVENTORY_REVALIDATION
R1C3B_PHASE2_VALIDATION_CONTRACT_GATE = BLOCKED
R1C3B_PHASE2_THREAT_MODEL_GATE = BLOCKED
R1C3B_PHASE2_FAST_PATH = BLOCKED
R1C3B_CONSENT_BOUND_FAST_PATH = BLOCKED
R1C3B_INVENTORY_EPOCH_DEDUP = BLOCKED
R1C3B_ACTIVE_GLOBAL_INVENTORY_TARGET = NOT_ACHIEVED
R1C3B_SEMANTIC_EQUIVALENCE_GATE = BLOCKED
R1C3B_SAFETY_GATE = BLOCKED
R1C3B_PHASE2_IMPLEMENTATION_READY = NO
R1C3B_PHASE2_DEBUG_UAT = NOT_READY
R1C3B_SMOOTHNESS_RUNTIME_GATE = PENDING_UAT
R0_OBSERVER_SEMANTICS_CHANGED = NO
R1C2A_REVALIDATION_REQUIRED = NO
R1C2B_REVALIDATION_REQUIRED = NO
R1C3A_REVALIDATION_REQUIRED = NO
CALLBACK_WORKLOAD_EXPANDED = NO
HIGH_FREQUENCY_POLLING = NO
WH_KEYBOARD_LL_USED = NO
RAW_INPUT_USED = NO
SWP_ASYNCWINDOWPOS = NO
ZORDER_CHANGE = NO
COMPONENT_SIZE_CHANGE = NO
MIXED_DPI_SUPPORT = NO
USER_PREEXISTING_WINDOWS_TOUCHED = NO
OTHER_THIRD_PARTY_CONTROL = NO
~~~
