# PaneBind R1-C2B Human Validation Seal

Seal date: 2026-09-08 (Asia/Shanghai).

## Evaluated implementation and acceptance

The final Debug and Release human runs both evaluated the frozen implementation:

```text
Evaluated implementation SHA = 84fd1ecee5b72e27ba46b08bd851d96a99757515
Branch = codex/r1c2b-explorer-glue-session
Main baseline = 8ac18ab07344632e8f0ed87cafe1b85b2b715d06
Debug evidence prefix = 20260905T084423487Z
Release evidence prefix = 20260905T122247314Z
GLUE_RUNTIME_CODE_CHANGES_AFTER_UAT = NO
DEBUG_RELEASE_REVALIDATION_REQUIRED = NO
```

The completed independent Human Evidence Review is retained without
reinterpretation. Both raw evidence triplets were fully parsed and reviewed;
terminal summaries alone were not the basis for acceptance. The continuation
seals that review and integrates documentation only. No new human UAT is run.

The accepted scope is one temporary Glue Move session between exactly two
newly created, separately user-consented Explorer test windows on one monitor
at the same DPI. The human moves Leader; Follower follows through several
initial-relative plans, its exact self-feedback is suppressed, and both windows
are independently restored. This is a controlled Explorer baseline, not a
general-purpose selection/input or multi-application product release.

## Stage history

| Stage | Preserved result |
| --- | --- |
| Attempt 1, `20260903T044532644Z` | `UnsafeLayout / SAFE_BLOCKED` before Glue consent; zero Glue native operations. Both windows were 1839 x 1074 in a 3072 x 1824 work area: horizontal excess 606, vertical excess 324. The old runner incorrectly required missing PASS-only records. |
| Fix 1, `e0bccc0` | Added non-consuming Layout Readiness Preview and explicit PASS / SAFE_BLOCKED / INVALID_EVIDENCE handling. |
| Attempt 2, `20260905T065805930Z` | Former evidence Gate PASS; safety, final geometry, lifecycle and integrity PASS. 31 raw LOCATIONs became one interpreted sample and one active apply. Realtime evidence insufficient, not total failure. |
| Fix 2, `84fd1ec` | Addressed `BATCH_SNAPSHOT_COLLAPSE` with progressive processing quanta, explicit processing-time samples, bounded owner pumping and stronger multi-step acceptance. |
| Final Debug | Realtime PASS: 270 raw LOCATIONs, 41 Leader quanta, 40 distinct samples, 40 active applies. |
| Final Release | Realtime PASS: 200 raw LOCATIONs, 8 Leader quanta, 8 distinct samples, 8 active applies. |

The [Attempt 2 forensic report](R1C2B_ATTEMPT2_FORENSICS.md) retains its full
event comparison and the limitation that exact historical drain boundaries
were not recorded. Its old PASS has not been deleted or relabeled as FAIL.

## Evidence integrity

| Check | Debug | Release |
| --- | ---: | ---: |
| Harness JSONL records | 595 | 297 |
| Observer JSONL records | 3315 | 5710 |
| Internal trace records | 125 | 28 |
| Accepted raw Glue receipts | 312 | 210 |
| JSONL/schema/physical sequence errors | 0 | 0 |
| Complete hook registration / hook shutdown / observer shutdown | 1 / 1 / 1 | 1 / 1 / 1 |
| Queue overflow / notification failure / incomplete diagnostics | 0 / 0 / 0 | 0 / 0 / 0 |
| Observer stderr bytes | 0 | 0 |
| Target field errors | 0 | 0 |
| Non-target structured field errors | 7 | 7 |

Each stream contains five initial-census access-denied process inspections
and two unrelated identity/destruction-race inspections. Those seven
non-target field errors are not queue, hook or lifecycle failures and were
not used as target evidence. It would be inaccurate to claim zero field errors
across either entire Observer file.

Both harnesses contain unique startup, target chains, pair validation, Glue
authorization, completed setup/arm/run steps, facts, summary PASS and complete
shutdown. Harness and trace physical sequences are continuous. Both existing
runner offline validations returned exit 0. Historical process exit codes
were supplied as offline-validation arguments; they were not recovered as new
process observations from JSONL.

Raw evidence stays under ignored `uat/r1c2b/`; no raw JSONL, nonce full paths,
unrelated titles, or raw HWND values are included in this seal. SHA-256 values
from the accepted review are retained for local audit:

| Prefix / file | SHA-256 |
| --- | --- |
| Debug harness | `7DC99764D73C242BFC61CCF979F35FBAA4C552B61730F33DB7613D07AAEB6831` |
| Debug Observer stdout | `AF01FE18F5226B989CE51623B93BD5A4F8CF63785FB39207CB70AA29272E0A09` |
| Release harness | `C7F915824313A54F39F6714D7C586948CDEC65F94F5C22F69F3AF3D41B160C00` |
| Release Observer stdout | `AFF39F372C78B8481F3A6B4A6908969A324F325F5CEFD647B0DD6FCE17655951` |
| Both empty stderr files | `E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855` |

## Target authority, layout and topology

In both runs, Leader and Follower each have a complete `user_consent` target
chain, one new exact nonce-location candidate, immutable baseline exclusion,
and distinct target identities. Follower provisioning begins after Leader
issuance, and its baseline permanently excludes Leader. No preexisting
Explorer receives authority or enters the component.

Glue authorization is separate: pair preview, Glue prompt, confirmation and
authority generations form `1 < 2 < 3 < 4`. Inputs are recorded as
`interactive_console`, with no synthetic input. Preview binds or consumes no
Glue authority, performs no native apply, and retains no temporary peer state.
Formal begin, confirmation binding and operation preflight revalidate live
identity and eligibility rather than trusting the preview.

Both accepted layouts use work area `[0,0,3072,1824]`, DPI 192 and two
1353 x 804 visible frames. Horizontal required size is 2706 x 804; vertical
required size is 1353 x 1608. Both fit; horizontal is selected. Setup is pure
translation, zero-gap, same monitor/DPI, no resize, no activation and no z-order
change. It is TEST FIXTURE ONLY, not Snap.

The actual R1-A graph has exactly two nodes and one relation. The verified
pre-hook topology and initial rectangles are frozen when armed; exact Leader
START revalidates and activates that `TranslationSession`. LOCATION cannot
start a session. No new member, second follower or dynamic component rebuild
is introduced during the drag.

## Progressive motion and suppression

| Measurement | Debug | Release |
| --- | ---: | ---: |
| Leader START / raw LOCATION / END | 1 / 270 / 1 | 1 / 200 / 1 |
| Quanta containing Leader LOCATION | 41 | 8 |
| All processing quanta | 47 | 12 |
| Distinct sampled Leader geometries | 40 | 8 |
| Active Follower native applies | 40 | 8 |
| Distinct active Follower targets | 40 | 8 |
| Applies before END receipt delivery | 40 | 8 |
| Follower LOCATION, internal / external active interval | 40 / 40 | 8 / 8 |
| Suppressed / duplicate / missing / reconciled | 40 / 0 / 0 / 0 | 8 / 0 / 0 / 0 |
| Recursive operations / unexpected feedback | 0 / 0 | 0 / 0 |
| Queue high-water / capacity | 15 / 512 | 60 / 512 |
| Pending high-water / capacity | 2 / 64 | 2 / 64 |

The 41 and 8 quantum counts refer specifically to quanta containing Leader
LOCATION, not every processing quantum. Debug also has one no-op sample.
Different raw-event/apply ratios are accepted: this proves multiple progressive
steps, not a fixed sampling rate, latency target, FPS or resource SLA.

Raw receipts contain identity, native timestamp and sequence without historical
geometry. A quantum captures current live geometry and labels it
`live_geometry_at_processing_quantum`; only its selected latest Leader trigger
is passed to Core. Later quanta obtain new samples. The owner pump is bounded
to eight messages and yields on target queue readiness; private Glue validation
avoids deliberate Shell readiness waiting while retaining full live checks.
The WinEvent callback still only filters fixed envelopes, records receipts,
assigns sequence, enqueues bounded work and wakes the owner.

All 40 Debug and 8 Release active operations were attempted and exactly
postverified. Size, target identity, nonce location, monitor and DPI were stable.
Native flags remain `SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE`. No input
attachment, asynchronous placement or foreground forcing is used.

Each ACK is uniquely associated with its session, Follower capability,
operation generation, exact requested/actual visible geometry and receipt
watermark. Pending registration precedes `SetWindowPos`, after immediate live
preflight. Feedback only acknowledges/suppresses; it never emits a recursive
Follower command. No time-only, raw-HWND-only, fixed event-count or mandatory
Follower START/END rule supplies authority.

The unchanged Core still supports explicit missing-feedback reconciliation by
exact native result plus exact final snapshot. Missing=0 in these accepted
runs does not remove that tested fallback or fabricate an ACK.

## Before-END evidence and exact cleanup

Debug END is receipt 312; Release END is receipt 210. Every counted operation
has an earlier Leader source sequence, a post-native watermark below its END
receipt, and an operation quantum that does not already contain END. This
means **before END receipt delivery in the same WinEvent source ordering**.
It is not proof of physical native END-generation or mouse-release time.

END takes final captures, verifies the initial-relative R1-A target and
reconciles pending state. The event source is stopped/unhooked before separate
Follower and Leader restore operations. Both windows stay open for the user;
restore is independent verified cleanup, not a native transaction rollback.

All visible rectangles below are `(left, top, right, bottom)`:

| State | Debug Leader | Debug Follower |
| --- | --- | --- |
| Original = restored | `(87,453,1440,1257)` | `(136,502,1489,1306)` |
| Fixture layout | `(183,510,1536,1314)` | `(1536,510,2889,1314)` |
| Final | `(182,884,1535,1688)` | `(1535,884,2888,1688)` |

| State | Release Leader | Release Follower |
| --- | --- | --- |
| Original = restored | `(171,574,1524,1378)` | `(220,623,1573,1427)` |
| Fixture layout | `(183,510,1536,1314)` | `(1536,510,2889,1314)` |
| Final | `(178,948,1531,1752)` | `(1531,948,2884,1752)` |

The final shared deltas are Debug `(-1,+374)` and Release `(-5,+438)`.
Positioning rectangles were independently exact as well; each operation's
native receipt verifies visible and positioning targets and preserved sizes.

## Freeze, safety and regression handoff

The post-UAT seal changes documentation only. Integration must compare the
`src`, `tests`, `scripts` and root CMake blobs/trees with the evaluated SHA:

```text
src = 664ca7e2b200116562b754812e939eda892f65db
tests = f524d0c68658a47e30109a77b07e6edb2053100c
scripts = bf9a3bb38d878480723b14f574924538923cc6be
CMakeLists.txt = dd20c1ee67d72a8dc9ac8634afd6ef7783cc9c12
r0-baseline = 2f2b76dd3358f5dab821cf2a3891401cc9bf2f1f
```

R0 Observer remains an independent external audit recorder, never Glue runtime
IPC. R1-C2A's one-shot default path and consent/operation limits remain
unchanged. Owned, Companion, Explorer single-translation and Glue authorities
remain distinct. Core contains no Win32/Explorer types. No preexisting window,
other third-party application, global input, injection, resident polling or
automatic Explorer-close behavior is added by the seal.

After the seal commit, the integration handoff records actual Debug/Release
builds and CTest, Owned/Companion self-tests, Explorer unit tests and runner
fixtures. Main repeats the required regressions and runtime-tree comparison
after the ordinary merge. The final Git handoff supplies commit, PR, merge SHA,
test outcomes and divergence; these are not predicted by this document.

## Remaining NOT TESTED

- Mixed-DPI, multi-monitor and cross-monitor Glue.
- Elevated Explorer, UIAccess and AppContainer scenarios.
- Hung Explorer, destruction exactly during Follower native apply, and
  application-adjusted WINDOWPOS behavior.
- Three or more real windows and dynamic component membership.
- Glue Resize and Snap.
- Excel, VS Code, browsers and other application eligibility.
- Global Ctrl activation and production selector/interaction UX.
- Final latency, smoothness and resident resource SLA.

These are unvalidated or outside this round. R1-C3 research questions may be
listed in the handoff; no R1-C3 branch or implementation is started here.

## Final human-validation gate

```text
R1C2B_HUMAN_VALIDATION = PASS
R1C2B_DEBUG_INTERACTIVE_UAT = PASS
R1C2B_RELEASE_INTERACTIVE_UAT = PASS
R1C2B_REALTIME_FOLLOW_RUNTIME_GATE = PASS
R1C2B_FEEDBACK_SUPPRESSION_RUNTIME_GATE = PASS
R1C2B_PRIOR_ART_GATE = PASS
R1C2B_BEHAVIOR_ENGINE_GATE = PASS
R1C2B_FEEDBACK_SUPPRESSION_GATE = PASS
R1C2B_EXPLORER_IMPLEMENTATION_GATE = PASS
R1C2B_RUNTIME_GATE = PASS
GLUE_RUNTIME_CODE_CHANGES_AFTER_UAT = NO
DEBUG_RELEASE_REVALIDATION_REQUIRED = NO
R0_OBSERVER_SEMANTICS_CHANGED = NO
R0_REVALIDATION_REQUIRED = NO
R1C2A_REVALIDATION_REQUIRED = NO
CALLBACK_WORKLOAD_EXPANDED = NO
HIGH_FREQUENCY_POLLING = NO
USER_PREEXISTING_WINDOWS_TOUCHED = NO
OTHER_THIRD_PARTY_CONTROL = NO
GLOBAL_INPUT_CONTROL = NO
R1C3 = NOT STARTED
```
