# R1-C3A — Final Human Ctrl+Move Validation

Review date: 2026-09-09 (Asia/Shanghai). Scope: seal the existing Debug and
Release positive Ctrl+Move evidence. No runtime change or new human run.

## Decision and exact evidence scope

```text
Evaluated implementation/handoff: e56202d03f49721370c704c63e41976907779c2d
Starting main: 8f9921b9550cad1ecee835dbb2193cc6728300f1
Branch: codex/r1c3a-ctrl-move-activation
R1C3A_HUMAN_VALIDATION = PASS
R1C3A_DEBUG_INTERACTIVE_UAT = PASS
R1C3A_RELEASE_INTERACTIVE_UAT = PASS
R1C3A_CTRL_ACTIVATION_RUNTIME_GATE = PASS
R1C3A_REALTIME_FOLLOW_RUNTIME_GATE = PASS
R1C3A_FEEDBACK_SUPPRESSION_RUNTIME_GATE = PASS
R1C3A_RUNTIME_GATE = PASS
HUMAN_SMOOTHNESS_DEBUG = C
HUMAN_SMOOTHNESS_RELEASE = C
SMOOTHNESS_OPTIMIZATION_REQUIRED = YES
```

The accepted functionality is Ctrl+Move activation correctness with the
existing two-Explorer realtime Glue runtime. A smoothness SLA was not defined
for R1-C3A. The user's observation in both configurations is **C: visibly
stepped/laggy**, not merely an END-time jump and not recursive jitter.
This is a KNOWN PRODUCT QUALITY LIMITATION. Functionality acceptance must not
be described as smooth, AquaGlue-equivalent or production-ready.

The human actions and configuration attribution come from the user's final
validation brief. The raw logs independently confirm the behavior below;
they are not a cryptographically embedded build/configuration attestation.
Current source/tests/scripts/CMake trees exactly match the evaluated handoff.

## Dataset, grain and reproducible checks

| Evidence | Debug | Release |
| --- | --- | --- |
| Prefix under ignored `uat/r1c3a/` | `20260909T101910373Z` | `20260909T103941915Z` |
| Harness rows / bytes | 450 / 271246 | 866 / 534465 |
| Observer rows / bytes | 7776 / 6867646 | 8414 / 10176741 |
| Observer stderr bytes | 0 | 0 |
| Raw Glue receipts | 324 | 589 |
| All processing quanta | 18 | 48 |
| Internal trace / operation / reconciliation records | 49 / 19 / 15 | 122 / 43 / 39 |
| Harness UTC interval | 10:19:11.294–10:20:10.914 | 10:39:42.903–10:40:25.766 |

Each JSONL line was parsed as UTF-8. The frozen strict runner validated schema,
continuous harness/Observer/receipt/trace sequences, lifecycle completeness,
unique target/activation records, raw-to-quantum-to-trace correlation, operation
generation linkage, exact geometry/feedback/restore and timing consistency.
No unknown/incomplete evidence was relabeled as a PASS. The raw evidence files
were not edited or committed.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/run-r1c3a-explorer-ctrl-glue-evidence.ps1 -ValidateEvidencePrefix uat/r1c3a/20260909T101910373Z -ValidationHarnessExitCode 0 -ValidationObserverExitCode 0
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/run-r1c3a-explorer-ctrl-glue-evidence.ps1 -ValidateEvidencePrefix uat/r1c3a/20260909T103941915Z -ValidationHarnessExitCode 0 -ValidationObserverExitCode 0
```

Both exited 0 with Ctrl, realtime and timing-consistency gates PASS. Exit-code
arguments describe the supplied successful historical runs, not newly executed
runtime processes. Offline mode retains a default `Debug` display label even
for the Release prefix; that cosmetic output is not configuration evidence.
No script was changed to correct it during this frozen integration round.

Observer hook registration/shutdown and observer shutdown are complete. There
are 6 Debug and 9 Release records with field errors (about 0.077% and 0.107%
of all Observer records), all **non-target**; both target error counts are 0.
Each set includes five OpenProcess/access-denied process-path observations;
remaining records report unrelated identity/snapshot failures, consistent with
window-lifecycle races (not separately reproduced). These are localized
non-target observation limitations, not
queue loss or an acceptance blocker. No new remedy is needed for this seal.

## Human behavior and authority matrix

| Fact | Debug | Release |
| --- | --- | --- |
| Leader START / raw LOCATION / END | 1 / 307 / 1 | 1 / 548 / 1 |
| Leader LOCATION quanta | 15 | 40 |
| Distinct Leader samples | 15 | 39 |
| Active Follower applies / distinct targets | 15 / 15 | 39 / 39 |
| Applies before END receipt delivery | 15 | 39 |
| Internal / external Follower LOCATION | 15 / 15 | 39 / 39 |
| Suppressed / duplicate / missing / reconciled | 15 / 0 / 0 / 0 | 39 / 0 / 0 / 0 |
| Unexpected / recursive operations | 0 / 0 | 0 / 0 |
| Follower no-op decisions | 0 | 1 |
| Setup / active / restore operation records | 2 / 15 / 2 | 2 / 39 / 2 |
| Event queue high-water / capacity | 40 / 512 | 48 / 512 |
| Pending high-water / capacity | 2 / 64 | 2 / 64 |
| Overflow / drop / post/hook failure | None observed | None observed |
| Final geometry / independent restore | exact / exact | exact / exact |

Both use exactly the newly user-provisioned Explorer test pair, immutable
baseline exclusions, nonce locations, live eligibility, and role-bound native
capabilities. Target authority is not granted by pressing Ctrl. Pair fixture
authority is generation 2 after preview 1; Glue prompt/confirmation generations
are both 0, and `glue_consent_confirmed=false`: **no Y+ENTER Glue activation**.

Both have one activation attempt, one accepted activation generation 1,
bound to pair authority, both target identities/consents/capabilities, session
generation 1 and Leader START receipt 1. Callback and owner Ctrl are DOWN;
left Ctrl is true, right Ctrl false. Authority remains callback-delivery state,
not an exact physical drag-start instant or proof of physical-key origin.
The existing per-operation permit and pending-before-native ledger remain in
use; every active apply correlates to this activation and exact self-feedback.

Debug END receipt is 324: first active operation source 13/quantum 2 has
pre/post native receipt watermarks 22/24; last source 322/quantum 16 has
322/323. Release END is 589: first source 11/quantum 2 has 14/23; last source
587/quantum 46 has 587/588. Every accepted active apply's post watermark is
strictly before END in the **same internal delivery sequence**. This does not
measure physical mouse-release or OS event-generation time. Neither log has
discarded post-END Glue receipts.

## Geometry and restore

Both runs use DISPLAY1, work area `[0,0,3072,1824]`, DPI 192, ordinary windows
with visible size 1238 x 871, horizontal zero-gap layout. The initial fixture
visible rectangles are Leader `[298,476,1536,1347]`, Follower
`[1536,476,2774,1347]`. Positioning rectangles differ from visible rectangles
by left -11, right +11, bottom +11; both coordinate sets were verified.

| Run / phase | Leader visible | Follower visible |
| --- | --- | --- |
| Debug pre-test = restored | `[63,111,1301,982]` | `[112,160,1350,1031]` |
| Debug final (delta -212,+315) | `[86,791,1324,1662]` | `[1324,791,2562,1662]` |
| Release pre-test = restored | `[86,791,1324,1662]` | `[135,840,1373,1711]` |
| Release final (delta -171,-79) | `[127,397,1365,1268]` | `[1365,397,2603,1268]` |

Identity, size, monitor/DPI, same-user/session/integrity and exact test-location
checks passed. Both hooks stopped cleanly before independent restore. No
preexisting user window or other application was controlled, no test window
was automatically closed. **Automatic restore is UAT fixture safety only,
not product Glue semantics.**

## Timing and quality interpretation

Milliseconds; ordinary median, nearest-rank p95. QPC frequency 10000000 in
both runs. All recorded durations are finite/nonnegative with validated
ordering and bounded evidence. There is no latency/FPS gate.

| Metric | Debug N | Debug p50 / p95 / max | Release N | Release p50 / p95 / max |
| --- | --- | --- | --- | --- |
| Activation decision | 1 | 195.3543 / 195.3543 / 195.3543 | 1 | 160.6484 / 160.6484 / 160.6484 |
| Follower apply interval | 14 | 297.80185 / 485.225 / 485.225 | 38 | 223.1838 / 452.2333 / 541.7231 |
| Receipt to owner drain | 324 | 157.5476 / 397.4413 / 490.0117 | 589 | 112.974 / 308.6417 / 406.0381 |
| Owner drain to native | 15 | 223.8743 / 418.1807 / 418.1807 | 39 | 149.5724 / 364.538 / 383.8758 |
| Native call | 15 | 9.6256 / 14.2585 / 14.2585 | 39 | 7.6767 / 12.3686 / 13.5829 |
| Native start to postverify | 15 | 66.2722 / 120.9586 / 120.9586 | 39 | 40.0458 / 90.839 / 92.7297 |
| Source receipt to exact postverify | 15 | 328.9357 / 509.4657 / 509.4657 | 39 | 209.8503 / 404.2079 / 416.8396 |

Activation has only one sample per run; its percentile fields are not an
estimated distribution. Receipt-to-owner includes all raw receipts, while
operation metrics use selected Leader source receipts. These populations differ;
their medians **must not be added**. Native-start-to-postverify includes native
call duration. Timings are CPU-side observation/processing intervals, not
compositor presentation, hardware input latency or smoothness measurements.

The evidence supports: **SetWindowPos itself is not the dominant observed
cost in these runs.** For each operation, independently divide native-call
ticks by its source-receipt-to-postverify ticks: Debug shares range 1.593–6.491%
(median 3.127%); Release 1.368–8.951% (median 3.249%). This paired comparison
does not diagnose which individual validation/pump/COM phase dominates and
does not authorize weakening safety checks. Together with subjective C/C, it
is input to future R1-C3B research, not an optimization performed here.

## Evidence hashes and frozen runtime

SHA256, in the same prefix order as above:

```text
Debug harness  51EDA2006B2E06E93B8E5D37D870CE524B081200863B8534C973F9946327B0F9
Debug Observer 51617C0D85E5FB9AC0376D901B2A5A8929102F1946C64C862C3428DFC2E8DE6B
Release harness  5011797652C0A2AE22E1F0275CFBF0F3577FDD732692607BA1BB1FC926E21006
Release Observer 56BA9F8ADBF8D08BF556F8C83EDD9538D8388817886C3C0ED748D2DC2E24D887
Both empty stderr E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855

src tree           0efe3950af57d4ad8b68cd902e14927f58b41e58
tests tree         f524d0c68658a47e30109a77b07e6edb2053100c
scripts tree       7b6ac21e71a62f767b9cb03cd3107e0105a7918a
CMakeLists.txt blob dd20c1ee67d72a8dc9ac8634afd6ef7783cc9c12
```

## Remaining limits and integration gate

Real no-Ctrl/late-Ctrl negative gestures, mid-drag release, right/both Ctrl,
AltGr/remappers, other applications, 3+ real windows, dynamic membership,
persistent groups, Glue Resize, Snap/coexistence, mixed-DPI/multi-monitor,
elevation/UIAccess/AppContainer, alternate/virtual desktops, hung/destroyed
targets during apply, HWND/PID reuse, unusual/vertical work areas, actual
overflow/native/postverify failures and long-duration resource behavior remain
NOT TESTED or outside the accepted human scope. Their automated tests are not
reclassified as human observations.

The read-only evidence review passes with no runtime change required. The
subjective quality limitation remains open. Seal-document regression, PR and
ordinary merge completion are separate integration steps recorded in the
[execution report](R1C3A_EXECUTION_REPORT.md) and final integration handoff.

```text
RUNTIME_TREE_CHANGED_AFTER_UAT = NO
DEBUG_RELEASE_REVALIDATION_REQUIRED = NO
R0_OBSERVER_SEMANTICS_CHANGED = NO
R1C2A_REVALIDATION_REQUIRED = NO
R1C2B_REVALIDATION_REQUIRED = NO
R1C3B = NOT STARTED
```
