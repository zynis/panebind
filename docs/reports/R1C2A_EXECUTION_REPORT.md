# PaneBind R1-C2A Explorer Eligibility Execution Report

Report date: 2026-08-26 (Asia/Shanghai).

## 1. Round, branch, and evaluated state

```text
Round = R1-C2A - Allowlisted Explorer Eligibility & Single-Translation Baseline
Starting main = b5d753e976aa389bb36476d6b2acdc946cddd22f
Branch = codex/r1c2a-explorer-single-translation
Research commit = ff84e1f183e2e6b679f38db2cc1ce3a342df3d6f
R0_BASELINE = SEALED
R1A_ALGORITHM_BASELINE = MERGED_TO_MAIN
R1B_OPERATIONS_BASELINE = MERGED_TO_MAIN
R1C1_COMPANION_BASELINE = MERGED_TO_MAIN
```

The implementation and this report were evaluated in the authorized round
worktree after the research commit. Their final commit SHA and the final pushed
HEAD are recorded in the round handoff after those changes are committed. The
known committed round history at report-writing time is:

```text
ff84e1f docs: research explorer third-party eligibility
```

The round implemented and automated-tested an Explorer-specific, fail-closed
eligibility/capability model. It did **not** establish a real Explorer
capability in the active desktop. Consequently it performed no third-party
window translation. This distinction is central to the result:

```text
Eligibility/capability model = IMPLEMENTED / AUTOMATED TESTED
Real Explorer capability issuance = BLOCKED
Real Explorer translation = NOT TESTED
```

No Glue, global input, resize, Snap, multi-window operation, generic
third-party registry, or R1-C2B implementation was added.

## 2. Prior-art gate and provenance

Detailed findings are in
[`R1C2A_EXPLORER_ELIGIBILITY_RESEARCH.md`](../research/R1C2A_EXPLORER_ELIGIBILITY_RESEARCH.md)
and the updated
[`SOURCE_PROVENANCE.md`](../research/SOURCE_PROVENANCE.md).

| Source | Pinned revision | License/use | R1-C2A conclusion |
| --- | --- | --- | --- |
| AltSnap | `5c86416ad21e4b72844a998a746bd3bb0bee5f5d` | GPL-3.0-or-later, reference-only | generic movement does not prove launch provenance or location authority |
| AltDrag | `e2740d605b0336a3b391fec26794718864b19521` | GPL-3.0-or-later, reference-only | injection/subclass history remains rejected |
| PowerToys/FancyZones | `19c4d805321db86f3634e6968e14dbf25cbba14a` | MIT, reference-only in this round | baseline/new-HWND delta is useful but insufficient by itself |
| Microsoft Win32/Shell documentation | live pages reviewed 2026-08-26 | documented contracts | Shell inventory, Shell browser creation, filesystem identity, security, geometry, and lifetime source |

The research adopted documented `IShellWindows` read-only inventory and
`CLSID_ShellBrowserWindow` creation, while rejecting ordinary `ShellExecute`,
titles, class names, basenames, raw HWNDs, and Shell collection membership as
standalone authority. No external code was copied or adapted.

```text
R1C2A_PRIOR_ART_GATE = PASS
EXTERNAL_CODE_COPIED = NO
EXTERNAL_CODE_ADAPTED = NO
```

## 3. Explorer test-target isolation design

The implemented provisioning path is deliberately narrower than general
Explorer discovery:

```text
STA COM initialization
-> complete read-only IShellWindows baseline and baseline HWND set
-> one unique empty ignored uat/r1c2a/target-<nonce> directory
-> one CLSID_ShellBrowserWindow creation request
-> bounded retained-object HWND readiness
-> Navigate2 to the exact directory and put_Visible(TRUE)
-> complete post-navigation inventory
-> require exactly one new HWND
-> require retained-object HWND == sole new HWND
-> require every baseline HWND/location fingerprint unchanged
-> require one exact live directory file identity
-> run the complete Explorer allowlist
-> issue one ExplorerWindowToken
```

Inventory is never capability. A zero-HWND delta, a baseline/reused HWND,
multiple new HWNDs, an unavailable or ambiguous location, a changed baseline,
or a retained-object mismatch blocks issuance. There is no existing-window
fallback and no path that upgrades a preexisting Explorer frame.

Location authority uses a live filesystem directory handle and `FILE_ID_INFO`
volume/file identity. A title, localized display name, URL string, pathname
string, or opaque digest is not sufficient. Committed evidence contains none
of those sensitive values.

## 4. Explorer allowlist and live validation

The Explorer-specific model requires all of the following before issuance and
again before every operation stage:

- valid root, non-child, unowned top-level window;
- visible, uncloaked, non-minimized, and non-maximized state;
- current virtual desktop;
- stable PID/TID and a live retained process handle;
- exact file identity for the installed Windows `explorer.exe`, not a basename;
- fail-closed observed Explorer class allowlist;
- same user, Windows session, and medium integrity;
- non-elevated, UIAccess false, and AppContainer false;
- unique Shell entry at the exact test-directory file identity; and
- stable monitor, DPI, geometry, and post-baseline provenance.

The reason-bearing model distinguishes inventory/creation/readiness failures,
preexisting/reused/ambiguous candidates, baseline or location changes,
process/image/thread/class/top-level/style/state/security failures, monitor/DPI
changes, stale authority, translation rejection, native failure, and
post-verification failure. These predicates and rejection precedence are
automated-tested. They were not claimed as real-target facts because no target
passed issuance.

## 5. Explorer capability and public API audit

`ExplorerWindowToken` is distinct from both `OwnedWindowToken` and
`CompanionWindowToken`. It contains private controller authority, private test
session authority, logical identity, and generation. It has no default, HWND,
or integer constructor and no native-handle conversion.

The public Explorer operation surface accepts only the opaque token:

```text
ExplorerWindowOperations::capture(ExplorerWindowToken)
ExplorerWindowOperations::apply_single(ExplorerWindowToken, target_visible)
ExplorerWindowOperations::restore(ExplorerWindowToken)
ExplorerTestSession::close_test_window(ExplorerWindowToken)
```

Raw HWND and Shell COM values remain inside the Windows Explorer adapter. A
separate internal diagnostic read exposes sanitized native correlation facts
to the harness but cannot issue authority or perform an operation. No generic
third-party token, registry, manager, or arbitrary-HWND executor exists.

Every capture/apply/restore/close re-inventories the exact Shell location and
revalidates the full token, window, process, state, security, monitor, and DPI
facts. Navigation or identity/state change invalidates the target before a
later native call.

## 6. Single-translation contract

The implemented test-fixture delta selector tries, in deterministic order:

```text
(+80,+50), (-80,-50), (+50,-50), (-50,+50)
```

It accepts only the first translation that keeps the complete visible frame
inside the original monitor work area. These values are test configuration,
not product movement policy.

The adapter reuses the R1-B/R1-C1 capability-neutral `window_translation`
bridge:

```text
target visible - current visible = checked (dx,dy)
target positioning = current positioning + checked (dx,dy)
```

Resize, mixed geometry change, empty rectangles, arithmetic overflow, and
native-coordinate overflow fail before native apply. The sole authorized
primary placement is one `SetWindowPos` with:

```text
SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
```

Success requires exact requested-versus-actual visible and positioning
geometry, unchanged size, and stable window/process/location/monitor/DPI
identity. Restore is a separately authorized cleanup translation, not
rollback. Graceful close may use only the exact retained Shell object after a
fresh complete eligibility proof; otherwise cleanup is not attempted and the
token-destroy lifetime remains untested.

The design and deterministic models are implemented and tested. Runtime never
reached safe-delta selection, snapshot capture, `SetWindowPos`,
post-verification, restore, or close.

## 7. Runtime isolation attempts and raw evidence

A preliminary read-only Shell inventory observed 11 unique Shell browser HWNDs
before the experiments. That aggregate is diagnostic only; each issuing run
still had to prove its own complete baseline/post pair and exact retained
candidate.

Three Debug evidence prefixes under ignored `uat/r1c2a/` were validated:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\run-r1c2a-explorer-evidence.ps1 `
  -BuildDirectory out/r1c2a-debug -Configuration Debug `
  -ObserveSeconds 30 -HarnessHoldSeconds 2
```

| UTC prefix | Observer records | Harness records | Harness result/reason | Native translations |
| --- | ---: | ---: | --- | ---: |
| `20260826T071646856Z` | 687 | 7 | `BLOCKED / inventory_unavailable` | 0 |
| `20260826T073434624Z` | 1,108 | 8 | `BLOCKED / shell_window_creation_failed` (`E_FAIL`) | 0 |
| `20260826T075612785Z` | 706 | 7 | `BLOCKED / inventory_unavailable` | 0 |

Every observer JSONL file was valid, had a continuous sequence, and contained
complete startup/census/hook-shutdown/observer-shutdown evidence. All three
reported no queue overflow, dropped event, post-notification failure, or
Observer failure. Every harness JSONL file was valid and its stderr file was
empty.

The middle prefix came from the pre-readiness worktree binary and is not
evidence for the final worktree's creation-stage diagnostics. It nevertheless
records a material safety observation: one provisioning path containing
exactly one `CoCreateInstance` attempt reported `E_FAIL` at a Shell stage that
the old diagnostic did not preserve, while two additional hidden
`CabinetWClass` top-level frames appeared. Six
`EVENT_OBJECT_LOCATIONCHANGE` records were observed for those two frames. They
were invisible and lacked a retained-object plus exact unique location
binding, so they are **unqualified frames, not Explorer test targets**.

No capability was issued in any run. Neither unqualified frame received
`Navigate2`, `put_Visible`, `SetWindowPos`, restore, or close from the harness.
The six location events are creation-side observations and are not translation
feedback. Therefore target `MOVESIZESTART / LOCATION_CHANGE / MOVESIZEEND`
counts are **NOT TESTED**, not `0 / 0 / 0`.

A later aggregate read-only inventory observed 13 unique Shell browser HWNDs;
two entries had an empty location witness and one entry was inaccessible. That
aggregate cannot bind either new hidden frame to a retained object or the nonce
directory and cannot prove an unchanged per-run baseline. It therefore cannot
be used to manufacture a capability.

Two hidden unqualified frames and three empty ignored nonce directories remain
after the experiments. The harness did not force-close a frame or force-remove
a directory after its safe authority/containment proof failed. No Explorer
process was killed or restarted. This residual state is explicitly reported
rather than hidden by unsafe cleanup:

```text
SAFE_CLEANUP_NOT_PERFORMED
WINDOW_DESTROY_LIFETIME = NOT TESTED
```

## 8. Precise runtime blocker

The active Explorer/Shell environment did not provide the complete evidence
needed to bind exactly one retained, visible, post-baseline Explorer HWND to
the exact nonce directory while proving all baseline fingerprints unchanged.
The latest executed Debug attempt stopped at `inventory_unavailable`. Later
final-worktree changes only hardened post-baseline absolute-deadline checks;
they were built and automated-tested but were not used to repeat the decisive
desktop blocker. The older
middle attempt returned `E_FAIL` and produced two hidden, unqualified frames,
which demonstrates ambiguity rather than authority.

The blocker is therefore not a failed geometry operation. It is the absence of
a proven real capability issuance. The required fail-closed outcome was
honored: no existing Explorer was selected and no native placement was
attempted.

```text
EXPLORER_TEST_TARGET_ISOLATION = BLOCKED
PRECISE_BLOCKER = complete unique real Explorer capability issuance was not proven
```

## 9. Automated builds, tests, and regressions

Both Debug and Release configured and built successfully. Both complete CTest
runs passed:

Environment:

- Visual Studio 18 2026 generator;
- MSVC 19.50.35729.0;
- Windows SDK 10.0.26100.0; and
- CMake/CTest 4.2.3-msvc3.

Final build/test commands were:

```powershell
cmake -S . -B out/r1c2a-debug -G "Visual Studio 18 2026" -A x64 -DBUILD_TESTING=ON
cmake --build out/r1c2a-debug --config Debug --parallel
ctest --test-dir out/r1c2a-debug -C Debug --output-on-failure

cmake -S . -B out/r1c2a-release -G "Visual Studio 18 2026" -A x64 -DBUILD_TESTING=ON
cmake --build out/r1c2a-release --config Release --parallel
ctest --test-dir out/r1c2a-release -C Release --output-on-failure
```

```text
1/8 geometry                         PASS
2/8 core-model                       PASS
3/8 topology                         PASS
4/8 translation                      PASS
5/8 windows-text-encoding            PASS
6/8 windows-owned-operations-unit    PASS
7/8 windows-companion-unit           PASS
8/8 windows-explorer-unit            PASS
TOTAL                                8/8 PASS (Debug)
TOTAL                                8/8 PASS (Release)
```

`windows-explorer-unit` covers opaque token construction, independent
controller/session authority, generation and stale semantics, single-primary
and restore gates, shell-stage mapping, new-versus-baseline inventory delta,
preexisting/reused/ambiguous candidates, compound baseline location
fingerprints, exact location, the allowlist rejection reasons, safe-delta/work
area selection, shared translation and overflow rejection, and exact
post-verification/identity invalidation.

Existing runtime regressions remained intact:

| Harness | Debug | Release |
| --- | --- | --- |
| Owned-window | PASS | PASS |
| Companion-process | PASS | PASS |
| Explorer | BLOCKED before capability/native apply | NOT RUN after the Debug isolation blocker |

Release Explorer runtime was intentionally not run to repeat creation side
effects after the decisive Debug isolation blocker. This does not affect the
Release build or 8/8 Release CTest result.

## 10. Feedback, monitor/DPI, and hung-target status

The planned attribution key combines session/token/generation, operation ID,
expected and actual geometry, HWND/PID/TID diagnostics, and matching WinEvent
geometry. Time-only matching, raw-HWND-only matching, mandatory START/END,
event contiguity, and one-request-one-event assumptions remain rejected.

Because no real target was issued, target feedback attribution, target
visible/positioning geometry, process/security facts, safe delta, monitor,
DPI, native apply, and acknowledgement behavior are all **NOT TESTED**. No
single-monitor, mixed-DPI, multi-monitor, or cross-monitor runtime claim is
made.

Hung Explorer behavior during synchronous cross-process placement remains
**NOT TESTED**. The round did not hang, terminate, restart, or otherwise stress
the real Explorer process to manufacture evidence.

## 11. Safety, boundary, and tracking audit

- No preexisting Explorer HWND was upgraded, navigated, moved, restored, or
  closed.
- No Excel, VS Code, browser, terminal, Power BI, or other third-party window
  was controlled.
- The two hidden frames were creation-side Shell effects and never received a
  PaneBind window operation.
- No global keyboard/mouse hook, `SendInput`, input attachment, DLL injection,
  process kill, shell restart, or resident/high-frequency polling was added.
- No Glue, leader/follower, adjacency runtime, feedback suppression, Snap, or
  resize behavior was implemented.
- `src/core/` remained free of HWND/HANDLE/COM/Explorer/Win32 policy; Explorer
  policy stays under the Windows adapter.
- Raw evidence and nonce directories remain ignored under `uat/r1c2a/` and are
  not tracked by Git.

```text
USER_EXISTING_WINDOWS_TOUCHED = NO
OTHER_THIRD_PARTY_CONTROL = NO
GLOBAL_INPUT_CONTROL = NO
DLL_INJECTION = NO
RESIDENT_POLLING = NO
CORE_WIN32_ISOLATION = PASS
RAW_RUNTIME_LOGS_TRACKED = NO
```

## 12. Required final handoff matrix

| # | Required item | Result |
| ---: | --- | --- |
| 1 | Starting main SHA | `b5d753e976aa389bb36476d6b2acdc946cddd22f` |
| 2 | Branch | `codex/r1c2a-explorer-single-translation` |
| 3 | Final HEAD | recorded in the final handoff after implementation/report commits |
| 4 | Commits | research `ff84e1f`; implementation/report commits recorded in final handoff |
| 5 | Prior-art sources/SHAs | AltSnap `5c86416...`, AltDrag `e2740d6...`, PowerToys `19c4d80...`, Microsoft documentation |
| 6 | Explorer test-target isolation design | implemented and deterministic-model tested; real isolation blocked |
| 7 | Preexisting inventory count | 11 unique in preliminary read-only inventory |
| 8 | Launch mechanism | documented `CLSID_ShellBrowserWindow`; runtime did not reach a valid retained frame |
| 9 | New candidate detection | exact one-new-HWND + retained HWND + exact location model tested; runtime not proven |
| 10 | Existing-window fallback audit | no fallback exists; no preexisting window selected |
| 11 | Process image identity | canonical file-identity validation implemented/model tested; target runtime not tested |
| 12 | Class/root/style eligibility | implemented/model tested; target runtime not tested |
| 13 | Shell location validation | file identity and live inventory implemented/model tested; runtime blocker was unavailable/ambiguous authority |
| 14 | Integrity/UIAccess/AppContainer facts | model implemented/tested; no target facts captured |
| 15 | ExplorerWindowToken design | opaque controller + session + logical ID + generation |
| 16 | Public HWND audit | PASS; no public raw-HWND operation API |
| 17 | Live revalidation | implemented at issuance/use/verification/restore/cleanup boundaries; real target not reached |
| 18 | Selected safe test delta | NOT TESTED at runtime |
| 19 | Initial visible/positioning geometry | NOT TESTED for a target |
| 20 | Requested geometry | NOT TESTED; no native request |
| 21 | Actual geometry | NOT TESTED; no native request |
| 22 | Post-verification result | NOT TESTED |
| 23 | Restore result | NOT TESTED |
| 24 | Safe close result | `SAFE_CLEANUP_NOT_PERFORMED` |
| 25 | Stale-token result | `WINDOW_DESTROY_LIFETIME = NOT TESTED` |
| 26 | WinEvent START/LOCATION/END | target counts NOT TESTED; six LOCATION events belonged to two unqualified creation-side frames |
| 27 | Feedback attribution conclusion | model inputs defined; runtime target attribution NOT TESTED |
| 28 | Hung-target risk | NOT TESTED |
| 29 | Monitor/DPI | target facts NOT TESTED; no same/mixed/multi-monitor claim |
| 30 | Deterministic tests | `windows-explorer-unit` PASS in Debug and Release |
| 31 | Debug/Release CTest | 8/8 PASS in both configurations |
| 32 | Owned harness regression | Debug PASS / Release PASS |
| 33 | Companion harness regression | Debug PASS / Release PASS |
| 34 | Explorer harness Debug | BLOCKED before capability/native apply |
| 35 | Explorer harness Release | NOT RUN |
| 36 | Core isolation | PASS |
| 37 | User-existing Explorer untouched audit | PASS / touched = NO |
| 38 | Other third-party apps untouched audit | PASS / controlled = NO |
| 39 | Global input/injection/polling audit | none present |
| 40 | Raw evidence tracking audit | ignored and untracked |
| 41 | Remaining NOT TESTED risks | listed below |
| 42 | R1-C2B questions only | listed below; no implementation started |
| 43 | Git status | authorized implementation/report changes pending commit at report drafting; final status recorded in handoff |
| 44 | Local/remote divergence | starting main matched `origin/main`; final branch divergence recorded after standard-transport push |

## 13. Remaining NOT TESTED risks

- successful issuance of a real isolated, visible Explorer target;
- canonical image/class/topology/location/security facts on an issued target;
- safe delta and exact initial/requested/actual geometry;
- one cross-process `SetWindowPos`, post-verification, and exact restore;
- safe graceful close and stale-token rejection after actual destruction;
- target START/LOCATION/END cardinality and feedback attribution;
- normal same-monitor/DPI placement, mixed-DPI, multi-monitor, and
  cross-monitor behavior;
- user navigation/close/minimize/maximize/monitor/DPI competition during an
  active capability;
- elevated, UIAccess, AppContainer, cross-user, or cross-session Explorer;
- Explorer tab/frame reuse variations and numeric HWND/PID reuse;
- hung Explorer during synchronous placement; and
- Release Explorer desktop runtime.

## 14. R1-C2B architecture questions only

1. Which documented Shell mechanism, if any, can reliably yield exactly one
   retained, visible new frame under current tabbed Explorer behavior without
   navigating or selecting an existing user frame?
2. How should bounded readiness distinguish transient empty/inaccessible Shell
   location entries from a permanently incomplete inventory without weakening
   the unchanged-baseline requirement?
3. Can a future fixture safely retain cleanup authority when
   `CoCreateInstance` reports failure yet Explorer creates multiple hidden
   frames, or must those frames always remain untouched?
4. Once real translation evidence exists, what acknowledgement/suppression
   state machine handles missing, duplicate, interleaved, and creation-side
   location events without time-only or START/END assumptions?
5. What supervision boundary can address hung synchronous native placement
   without injection, `TerminateThread`, Explorer termination, or false async
   completion?

These are questions only. The blocked R1-C2A runtime Gate must be resolved
before R1-C2B can begin.

## 15. Gate result

```text
R1C2A_PRIOR_ART_GATE = PASS
R1C2A_ELIGIBILITY_GATE = BLOCKED
R1C2A_RUNTIME_GATE = BLOCKED
PRECISE_BLOCKER = real Explorer capability issuance was not proven
USER_EXISTING_WINDOWS_TOUCHED = NO
OTHER_THIRD_PARTY_CONTROL = NO
R1C2B = NOT STARTED
```
