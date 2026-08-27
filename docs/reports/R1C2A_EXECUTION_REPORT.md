# PaneBind R1-C2A Explorer Eligibility Execution Report

Report date: 2026-08-28 (Asia/Shanghai; updated for user-consented recovery).

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

The initial implementation and blocked report were committed and pushed before
the recovery continuation began. The known committed round history at the
recovery report-writing point is:

```text
ff84e1f docs: research explorer third-party eligibility
eef1f92 feat: add isolated explorer translation harness
71b9ea8 docs: record blocked r1c2a explorer runtime gate
769873d docs: research shell registration attribution recovery
080cca5 feat: recover explorer provisioning attribution
b110f50 docs: record blocked explorer provisioning recovery
560945a docs: research user-consented explorer authority
d606ee1 feat: add user-consented explorer authority
```

Recovery started from `71b9ea88a085e64438f6bc8a704f11279aa2c950`.
That blocked checkpoint is present at
`origin/codex/r1c2a-explorer-single-translation`; the recovery research commit
is `769873daf9620cc04594591994945385bf93927e`. Final recovery implementation
and report SHAs remain a handoff item after the authorized worktree is
committed.

The third recovery continuation started from
`b110f5068c1c252e1f5b0d90315d6d998235adf0`. Its prior-art checkpoint is
`560945a` and its verified implementation checkpoint is `d606ee1`.

The round implemented and automated-tested an Explorer-specific, fail-closed
eligibility/capability model. It did **not** establish a real Explorer
capability in the active desktop. Consequently it performed no third-party
window translation. This distinction is central to the result:

```text
Eligibility/capability model = IMPLEMENTED / AUTOMATED TESTED
Attempts 1/2 real Explorer capability issuance = BLOCKED
Attempt 3 real Explorer capability issuance = PENDING_UAT
Real Explorer translation = NOT TESTED
```

The recovery proved that baseline exclusion can remain complete when existing
entries have opaque location diagnostics. It still did not obtain a creation
object or positive target attribution. Detailed recovery evidence is recorded
in [`R1C2A_PROVISIONING_RECOVERY_REPORT.md`](R1C2A_PROVISIONING_RECOVERY_REPORT.md).

Those statements are the immutable results of automatic Attempts 1 and 2.
Attempt 3 changes only the active UAT authority path: a human creates and
navigates a new Explorer frame and gives two explicit confirmations, while
PaneBind independently proves baseline exclusion, unique exact-location
candidate identity, full live eligibility, and generation freshness. The
interactive implementation/build/test result is **PASS**. Real eligibility and
runtime remain **PENDING_UAT** and must not be inferred from deterministic
tests.

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

The research adopted documented `IShellWindows` read-only inventory. Attempt 2
also researched `CLSID_ShellBrowserWindow`, but its `CoCreateInstance` path is
now blocked history rather than active/default UAT. `ShellExecute`,
`ShellExecuteEx`, direct `explorer.exe` invocation including `/n` or
`/separate`, `start <path>`, titles, class names, basenames, raw HWNDs, and
Shell collection membership are rejected as standalone new-window authority.
No external code was copied or adapted.

```text
R1C2A_PRIOR_ART_GATE = PASS
EXTERNAL_CODE_COPIED = NO
EXTERNAL_CODE_ADAPTED = NO
```

## 3. Historical automatic isolation design (Attempts 1 and 2)

The recovery provisioning path is deliberately narrower than general Explorer
discovery and separates exclusion from positive attribution:

```text
STA COM initialization
-> read-only IShellWindows baseline
-> require a reliable HWND for every entry
-> freeze every baseline HWND, including opaque-location entries, as forbidden
-> one unique empty ignored uat/r1c2a/target-<nonce> directory
-> advise DShellWindowsEvents and establish a subscription generation
-> one CLSID_ShellBrowserWindow creation request
-> retain one provisioning lease only if CoCreate returns IWebBrowser2
-> correlate WindowRegistered(cookie) through FindWindowSW
-> require canonical IUnknown identity with the retained creation object
-> require retained, cookie-resolved, and live HWND equality
-> require that HWND to be absent from the frozen baseline set
-> Navigate2 to the exact directory and put_Visible(TRUE)
-> require one exact live directory file identity
-> run the complete Explorer allowlist
-> issue one ExplorerWindowToken
```

Inventory is never capability. Empty or inaccessible location evidence on an
existing entry makes it `OPAQUE_PREEXISTING`; it does not remove its reliable
HWND from the immutable forbidden set. A baseline/reused HWND, missing or
ambiguous registration identity, unavailable target location, or retained /
cookie / live-HWND mismatch blocks issuance. There is no existing-window
fallback and no path that upgrades a preexisting Explorer frame.

Location authority uses a live filesystem directory handle and `FILE_ID_INFO`
volume/file identity. A title, localized display name, URL string, pathname
string, or opaque digest is not sufficient. Committed evidence contains none
of those sensitive values.

### 3.1 Active user-consented isolation design (Attempt 3)

The active/default path never invokes `CLSID_ShellBrowserWindow` and has no
automatic-launch fallback:

```text
capture immutable baseline forbidden HWND set
-> create empty non-reparse local uat/r1c2a/consent-target-<nonce>
-> print its absolute path
-> human creates one new Explorer top-level window and navigates it
-> human confirms completion
-> fresh read-only inventory
-> require exactly one non-baseline candidate with one exact FILE_ID_INFO entry
-> require full live Explorer allowlist
-> issue consent/session/generation-bound ExplorerWindowToken
-> show sanitized target summary
-> require separate explicit move consent
-> live-revalidate target, eligibility, and generations
-> one SetWindowPos translation and exact post-verification
-> separate live-revalidated restore
-> leave the user-created Explorer open for the human to close
```

Baseline location diagnostics may be valid, empty, or inaccessible, but every
entry must provide a reliable HWND and every value remains forbidden for the
whole run. A baseline frame at the exact nonce location is rejected rather than
promoted. Zero or multiple exact-location candidates block without native
apply.

`Ctrl+N` is a suggested human action, not target evidence. ENTER records an
explicit consent fact, not a credential or security boundary. The harness and
evidence script do not use `SendInput`, keyboard/mouse hooks, `PostMessage`
simulation, UI Automation, foreground forcing, or a selector/picker UI.

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

The recovery adds a separate temporary `ExplorerProvisioningLease`. The lease
retains only this session's successfully created `IWebBrowser2`, canonical COM
identity, nonce-directory identity, subscription generation, and conditional
cleanup authority. It cannot authorize `SetWindowPos`; only complete positive
attribution and the full live allowlist may mint an `ExplorerWindowToken`.

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

At the end of the initial experiments, two hidden unqualified frames and three
empty ignored nonce directories remained. The harness did not force-close a
frame or force-remove a directory after its safe authority/containment proof
failed. No Explorer process was killed or restarted. That initial residual
state is explicitly reported rather than hidden by unsafe cleanup:

```text
SAFE_CLEANUP_NOT_PERFORMED
WINDOW_DESTROY_LIFETIME = NOT TESTED
```

### 7.1 Provisioning recovery evidence

The initial evidence above remains unchanged. Recovery used the ignored prefix
`20260827T114619528Z` and a fixed three-attempt Debug provision-only Gate.

Attempt 1 established a complete exclusion baseline:

```text
total / reliable entries / unique HWND / forbidden HWND = 14 / 14 / 14 / 14
valid / empty opaque / inaccessible opaque location = 10 / 2 / 2
R1C2A_BASELINE_EXCLUSION_GATE = PASS
```

`DShellWindowsEvents` was advised before creation at generation 1. It recorded
no malformed receipt, overflow, wrong-thread callback, post-retirement
callback, generation mismatch, or cookie-lifecycle ambiguity. The sole
`CoCreateInstance(CLSID_ShellBrowserWindow)` then returned `E_FAIL`
(`0x80004005`) at `create_browser_window`. Browser-event subscription was not
reached; registered, revoked, and matching cookie counts were all zero.

No provisioning lease, retained object HWND, cookie-resolved HWND, canonical
COM match, exact target location, or `ExplorerWindowToken` existed. The Shell
connection point was unadvised cleanly. No `Quit` was authorized, and no native
translation was attempted.

A post-attempt read-only inventory contained 15 entries: 11 with accessible
location evidence, three empty-location entries, and one inaccessible entry.
Compared with the pre-attempt aggregate, this is `+1` total, `+1` accessible,
`+1` empty, and `-1` inaccessible; no per-HWND before/after category mapping
was retained. The time-correlated `+1` entry is therefore unqualified, and its
location category is unknown. With no lease or cookie it cannot be canonically
attributed or safely closed, so formal orphan attribution is `UNKNOWN`. The raw
attempt summary's legacy zero-valued orphan
field is preserved as raw evidence but is not accepted as proof; subsequent
code represents this state as `known = false` and a null count.

Provision-only run 1 was `BLOCKED`; provision-only runs 2 and 3 were `NOT RUN`.
Therefore
`PROVISIONING_STABILITY_GATE = BLOCKED`, and neither Debug full Explorer
runtime nor Release Explorer runtime was run. Directory removal also remained
fail-closed. A later read-only audit found four ignored target directories, all
empty and non-reparse, without retroactively granting per-attempt cleanup
authority.

## 8. Precise runtime blocker

The initial attempts did not bind a retained, visible, post-baseline Explorer
HWND to the exact nonce directory. The recovery improved the model and proved
that all 14 pre-attempt Shell HWND values could be excluded even though four
entries had opaque location diagnostics. It then blocked earlier in positive
provisioning: the sole documented CoCreate request returned `E_FAIL` before an
`IWebBrowser2` lease, registration cookie, canonical object identity, or target
HWND existed.

The post-attempt inventory's time-correlated unqualified `+1` entry cannot
repair that missing authority. Its orphan attribution is `UNKNOWN`, not zero
and not owned. Attempting to close it or using it as a target would require the
raw-HWND inference that the Gate explicitly prohibits.

The blocker is therefore not a failed geometry operation. It is the absence of
a proven real capability issuance. The required fail-closed outcome was
honored: no existing Explorer was selected and no native placement was
attempted.

```text
EXPLORER_TEST_TARGET_ISOLATION = BLOCKED
PRECISE_BLOCKER = the sole CLSID_ShellBrowserWindow CoCreate returned E_FAIL before positive attribution authority existed
```

## 9. Automated builds, tests, and regressions

Both Debug and Release configured and built successfully. Both complete CTest
runs passed for the initial implementation and again after the recovery
correlation fixes:

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

`windows-explorer-unit` additionally covers complete exclusion with valid,
empty-location, and inaccessible-location baseline entries; missing reliable
baseline HWND rejection; no/unrelated/one/multiple matching registrations;
preexisting-HWND and wrong-COM-identity rejection; exact location; revocation;
lease cleanup authority; and unknown orphan-attribution encoding. It continues
to cover opaque token construction, independent
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
| Explorer | provision-only BLOCKED before capability/native apply; full runtime NOT RUN | NOT RUN after the Debug provisioning blocker |

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
- The recovery's time-correlated unqualified `+1` Shell entry remained
  unqualified with orphan attribution `UNKNOWN`; it was neither selected nor
  closed.
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

## 12. Attempt 2 handoff matrix (preserved blocked history)

| # | Required item | Result |
| ---: | --- | --- |
| 1 | Starting main SHA | `b5d753e976aa389bb36476d6b2acdc946cddd22f` |
| 2 | Branch | `codex/r1c2a-explorer-single-translation` |
| 3 | Recovery committed checkpoint | `769873daf9620cc04594591994945385bf93927e`; final implementation/report HEAD remains a handoff item |
| 4 | Commits | `ff84e1f` research; `eef1f92` implementation; `71b9ea8` initial blocked report; `769873d` recovery research |
| 5 | Prior-art sources/SHAs | AltSnap `5c86416...`, AltDrag `e2740d6...`, PowerToys `19c4d80...`, Microsoft documentation |
| 6 | Explorer test-target isolation design | baseline exclusion and positive attribution separated, implemented, and deterministic-model tested; real isolation blocked |
| 7 | Preexisting inventory count | recovery attempt: 14 total/reliable/unique/forbidden; location diagnostics 10 valid, 2 empty, 2 inaccessible |
| 8 | Launch mechanism | one documented `CLSID_ShellBrowserWindow` CoCreate; `E_FAIL` at `create_browser_window` |
| 9 | New candidate detection | post-read inventory was 15 with a time-correlated unqualified `+1` entry; its category and orphan attribution are `UNKNOWN` |
| 10 | Existing-window fallback audit | no fallback exists; no preexisting window selected |
| 11 | Process image identity | canonical file-identity validation implemented/model tested; target runtime not tested |
| 12 | Class/root/style eligibility | implemented/model tested; target runtime not tested |
| 13 | Shell location validation | opaque baseline entries remained forbidden; exact target identity still required and was NOT TESTED |
| 14 | Integrity/UIAccess/AppContainer facts | model implemented/tested; no target facts captured |
| 15 | Authority design | non-operational provisioning lease separated from opaque Explorer token; no lease or token issued at runtime |
| 16 | Public HWND audit | PASS; no public raw-HWND operation API |
| 17 | Live revalidation | implemented at issuance/use/verification/restore/cleanup boundaries; real target not reached |
| 18 | Selected safe test delta | NOT TESTED at runtime |
| 19 | Initial visible/positioning geometry | NOT TESTED for a target |
| 20 | Requested geometry | NOT TESTED; no native request |
| 21 | Actual geometry | NOT TESTED; no native request |
| 22 | Post-verification result | NOT TESTED |
| 23 | Restore result | NOT TESTED |
| 24 | Safe close result | no lease, so no `Quit`; `SAFE_CLEANUP_NOT_PERFORMED`; Shell subscription unadvised cleanly |
| 25 | Stale-token result | `WINDOW_DESTROY_LIFETIME = NOT TESTED` |
| 26 | WinEvent START/LOCATION/END | target counts NOT TESTED; six LOCATION events belonged to two unqualified creation-side frames |
| 27 | Feedback attribution conclusion | model inputs defined; runtime target attribution NOT TESTED |
| 28 | Hung-target risk | NOT TESTED |
| 29 | Monitor/DPI | target facts NOT TESTED; no same/mixed/multi-monitor claim |
| 30 | Deterministic tests | `windows-explorer-unit` PASS in Debug and Release |
| 31 | Debug/Release CTest | 8/8 PASS in both configurations |
| 32 | Owned harness regression | Debug PASS / Release PASS |
| 33 | Companion harness regression | Debug PASS / Release PASS |
| 34 | Explorer harness Debug | provision-only attempt 1 BLOCKED; attempts 2/3 and full runtime NOT RUN; native translation count 0 |
| 35 | Explorer harness Release | NOT RUN because provisioning stability blocked |
| 36 | Core isolation | PASS |
| 37 | User-existing Explorer untouched audit | PASS / touched = NO |
| 38 | Other third-party apps untouched audit | PASS / controlled = NO |
| 39 | Global input/injection/polling audit | none present |
| 40 | Raw evidence tracking audit | ignored prefix `20260827T114619528Z`; untracked; legacy orphan zero preserved but formally interpreted as UNKNOWN |
| 41 | Remaining NOT TESTED risks | listed below |
| 42 | R1-C2B questions only | listed below; no implementation started |
| 43 | Git status | authorized implementation/report changes pending commit at report drafting; final status recorded in handoff |
| 44 | Local/remote divergence | blocked checkpoint `71b9ea8` pushed by standard Git transport; recovery final divergence recorded after its commit/push |

## 13. Attempt 2 remaining NOT TESTED risks

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
- attribution and location category of the recovery attempt's time-correlated
  `+1` Shell entry, which remain `UNKNOWN` without a lease/cookie;
- allocation-failure/OOM behavior in Shell/browser event receipt capture and
  recovery diagnostics;
- the global-inventory decision boundary when a baseline entry cannot provide
  even a reliable HWND for exclusion;
- hung Explorer during synchronous placement; and
- Release Explorer desktop runtime.

## 14. Historical R1-C2B architecture questions only

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

These are questions only. The current user-consented R1-C2A runtime Gate must
complete interactive UAT before R1-C2B can begin.

## 15. Attempt 2 gate result (preserved blocked history)

```text
R1C2A_PRIOR_ART_GATE = PASS
R1C2A_BASELINE_EXCLUSION_GATE = PASS
R1C2A_PROVISIONING_GATE = BLOCKED
R1C2A_ELIGIBILITY_GATE = BLOCKED
R1C2A_RUNTIME_GATE = BLOCKED
PROVISIONING_STABILITY_GATE = BLOCKED
PRECISE_BLOCKER = the sole CLSID_ShellBrowserWindow CoCreate returned E_FAIL before positive attribution authority existed
USER_EXISTING_WINDOWS_TOUCHED = NO
OTHER_THIRD_PARTY_CONTROL = NO
R1C2B = NOT STARTED
```

## 16. Attempt 3 implementation and evidence handoff

Attempt 3 is intentionally interactive and is not registered in CTest. The
active CLI contract is:

```text
panebind-explorer-harness.exe --interactive-consent-test
```

The intended evidence wrapper is:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\run-r1c2a-explorer-consent-evidence.ps1 `
  -BuildDirectory out/r1c2a-debug `
  -Configuration Debug `
  -ObserveSeconds 180
```

The human must open a **new** Explorer top-level window, navigate that new
window to the printed absolute nonce path, return and press ENTER, review the
sanitized target summary, then enter `Y` and press ENTER for the one
translation plus immediate restore. The final stale-token close step is
optional. The harness never closes the user-created window.

The wrapper may start only `panebind-observer` and the interactive harness and
save ignored output below `uat/r1c2a/`. It must not create or navigate Explorer,
send keys, close Explorer, or select/control any baseline HWND. Creation and
navigation events are not operation feedback; only the issued target's
translation and restore intervals may contribute START/LOCATION/END evidence.

The implementation must preserve these safety boundaries:

- active/default interactive mode performs no CoCreate/ShellExecute/Explorer
  launch and has no legacy automatic fallback;
- baseline HWND membership is immutable, including opaque preexisting entries;
- one and only one new exact-location candidate may receive a token;
- target confirmation and move authorization are distinct consent generations;
- all identity, state, security, location, monitor, DPI, and generation facts
  are live-revalidated before the single native apply and restore;
- primary apply is exactly one `SetWindowPos` using
  `SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE`, followed by exact verification;
- restore is separate, immediate, verified cleanup; and
- no automatic close, process termination, global/synthetic input, selector UI,
  Glue, Snap, resize, or R1-C2B behavior is introduced.

The user-consent capability implementation and interactive harness build are
complete. Debug and Release builds pass; all 8 CTest entries pass in both
configurations; the expanded `windows-explorer-unit` deterministic consent
matrix passes; and Owned-window plus Companion-process harness regressions pass
in both configurations. Redirected interactive input and both deprecated
automatic CLI modes were also verified to reject before directory, COM, or
Explorer side effects. The before/after `consent-target-*` directory count was
unchanged in that safety check.

No human consent has yet been supplied. Real target issuance, translation,
post-verification, restore, target-correlated observer counts, and optional
manual stale-token evidence therefore remain `PENDING_UAT`/`NOT TESTED`; no
automated result is relabeled as a desktop-runtime observation.

## 17. Current R1-C2A gate state

This state supersedes section 15 only for the active Attempt 3 path; it does not
rewrite the automatic-provisioning failures:

```text
R1C2A_PRIOR_ART_GATE = PASS
R1C2A_BASELINE_EXCLUSION_GATE = PASS
ATTEMPT_1_AUTO_INVENTORY_PROVISIONING = BLOCKED
ATTEMPT_2_SHELL_REGISTRATION_PROVISIONING = BLOCKED
AUTO_PROVISIONING_ON_CURRENT_WINDOWS11 = BLOCKED
ATTEMPT_2_PRECISE_BLOCKER = CLSID_ShellBrowserWindow CoCreate returned E_FAIL before positive attribution authority existed
ATTEMPT_3_USER_CONSENTED_AUTHORITY = CURRENT APPROACH
R1C2A_CONSENT_CAPABILITY_IMPLEMENTATION = PASS
R1C2A_ELIGIBILITY_GATE = PENDING_UAT
R1C2A_RUNTIME_GATE = PENDING_UAT
USER_EXISTING_WINDOWS_TOUCHED = NO
OTHER_THIRD_PARTY_CONTROL = NO
R1C2B = NOT STARTED
```
