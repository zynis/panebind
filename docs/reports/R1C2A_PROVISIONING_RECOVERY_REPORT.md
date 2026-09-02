# PaneBind R1-C2A Explorer Provisioning Recovery Report

Report date: 2026-09-03 (Asia/Shanghai; final Attempt 3 disposition added).

## 1. Scope and relationship to the initial attempt

This report records automatic provisioning Attempt 2 of the same R1-C2A round.
It does not replace or reinterpret Attempt 1 blocked evidence in
[`R1C2A_EXECUTION_REPORT.md`](R1C2A_EXECUTION_REPORT.md).

The initial attempt remains material evidence:

- the later aggregate contained 13 unique Shell HWND values, including two
  entries with empty location evidence and one inaccessible entry;
- hidden, unqualified `CabinetWClass` frames were observed;
- no frame could be bound to a retained creation object and exact nonce
  location;
- no `SetWindowPos` call was made; and
- no preexisting user window or other third-party application was controlled.

The recovery did not weaken target eligibility. It separated two authorities:

```text
Baseline exclusion = prove every preexisting Shell HWND is forbidden
Positive attribution = prove one new object belongs to this CoCreate/session
```

An empty or inaccessible location may make a baseline entry
`OPAQUE_PREEXISTING`, but it cannot make that reliably obtained HWND eligible.
The same condition remains disqualifying for the positive target.

## 2. Git and checkpoint state

```text
Branch = codex/r1c2a-explorer-single-translation
Recovery starting HEAD = 71b9ea88a085e64438f6bc8a704f11279aa2c950
Blocked checkpoint push = PASS
Remote blocked checkpoint = 71b9ea88a085e64438f6bc8a704f11279aa2c950
Recovery research commit = 769873daf9620cc04594591994945385bf93927e
```

The checkpoint was pushed through standard Git transport before recovery work
started. The final recovery implementation/report commit and its push are left
for the round handoff; this report does not claim an uncommitted worktree as a
final Git state.

## 3. Recovery model exercised

The recovery model adds the following fail-closed stages:

1. retain one `IShellWindows` object and advise `DShellWindowsEvents`;
2. enumerate every baseline entry through that same object and require a
   reliable HWND for exclusion, while treating unavailable location facts as
   diagnostics;
3. freeze all baseline HWND values in an immutable
   `forbidden_preexisting_hwnds` set for the complete session;
4. perform exactly one
   `CoCreateInstance(CLSID_ShellBrowserWindow, ..., IWebBrowser2)` request;
5. retain a temporary provisioning lease only if that object is created;
6. correlate `WindowRegistered(cookie)` through `FindWindowSW` and canonical
   `IUnknown` identity;
7. require lease HWND, cookie-resolved HWND, and live-eligibility HWND equality,
   with the HWND absent from the frozen baseline set;
8. require exact live target directory file identity before token issuance;
9. allow no native translation from the provisioning lease; and
10. use `IWebBrowser2::Quit` only when cleanup authority belongs to the exact
    retained creation object.

The Shell event sink performs small receipt capture and has explicit
`Advise`/`Unadvise`, generation, malformed-receipt, overflow, wrong-thread, and
post-retirement diagnostics. No enumeration/sleep loop or high-frequency
polling was introduced as the attribution mechanism.

## 4. Evidence set

The ignored evidence prefix was:

```text
20260827T114619528Z
```

Only sanitized counts, states, API/stage identifiers, and HRESULT values are
reported. No Explorer title or filesystem path is reproduced here.

The fixed stress Gate was three Debug provision-only runs within Attempt 2. It
stopped fail-closed after run 1 blocked; provision-only runs 2 and 3 were
deliberately not used as retry-until-pass opportunities.

## 5. Baseline exclusion result

Provision-only run 1 within Attempt 2 captured this baseline before the sole
creation request:

| Fact | Value |
| --- | ---: |
| Total Shell entries | 14 |
| Entries with reliable HWND | 14 |
| Reliable unique HWND values | 14 |
| Frozen forbidden HWND values | 14 |
| Valid location diagnostics | 10 |
| Empty-location opaque entries | 2 |
| Inaccessible-location opaque entries | 2 |

Every Shell entry supplied a reliable HWND, including all four opaque-location
entries. Therefore all 14 HWND values entered the immutable forbidden set:

```text
R1C2A_BASELINE_EXCLUSION_GATE = PASS
```

This PASS proves exclusion completeness only. It does not identify or authorize
a target.

## 6. Shell event subscription and creation result

The Shell registration subscription was established before creation:

```text
DShellWindowsEvents advised = true
subscription generation = 1
malformed receipts = 0
receipt overflow = 0
wrong-thread callbacks = 0
post-retirement callbacks = 0
generation mismatch = false
cookie lifecycle ambiguity = false
```

The one allowed creation request then failed:

```text
API = CoCreateInstance(CLSID_ShellBrowserWindow)
stage = create_browser_window
HRESULT = 0x80004005 (E_FAIL)
```

Because no `IWebBrowser2` object was returned, the browser-event subscription
stage was not reached. The Shell subscription observed no registration or
revocation receipts before the attempt terminated:

| Correlation fact | Result |
| --- | --- |
| `WindowRegistered` cookies | 0 |
| `WindowRevoked` cookies | 0 |
| Matching cookies | 0 |
| `FindWindowSW` resolution | NOT REACHED |
| CoCreate object HWND | NOT AVAILABLE |
| Cookie-resolved HWND | NOT AVAILABLE |
| Canonical `IUnknown` match | NOT TESTED |
| Exact target location | NOT TESTED |

Zero registration receipts are not positive attribution. The recovery
correctly refused to infer ownership from a raw HWND, class name, inventory
delta, title, timing, or previously observed hidden frame.

A separate read-only inventory after the failed attempt contained 15 entries:

| Post-attempt diagnostic | Value |
| --- | ---: |
| Total Shell entries | 15 |
| Accessible location entries | 11 |
| Empty-location entries | 3 |
| Inaccessible-location entries | 1 |

Relative to the 14-entry pre-attempt baseline, total entries increased by one;
aggregate categories changed by `+1` accessible, `+1` empty, and `-1`
inaccessible. No per-HWND before/after category mapping was retained, so the
new entry's category is not proven. The time-correlated `+1` entry is therefore
unqualified. The failed CoCreate returned neither a lease nor a registration
cookie, so it cannot be canonically attributed to this run or closed by this
harness.

## 7. Lease, token, cleanup, and native-operation result

No successful creation object meant no provisioning lease and no cleanup
authority:

```text
ExplorerProvisioningLease = NOT CREATED
ExplorerWindowToken = NOT ISSUED
native Quit attempted = false
native translation count = 0
```

The Shell connection point was unadvised cleanly. The browser connection point
did not exist and therefore was not advised or unadvised. No frame attribution
could be established for this failed run:

```text
Shell event lifecycle = CLEAN
Orphan attribution = UNKNOWN
SAFE_CLEANUP_NOT_PERFORMED
```

The raw attempt summary encoded an attributable-orphan count of zero. That
legacy value is not accepted as proof of absence because no lease/cookie
authority existed. The evidence file remains unchanged; the subsequent code
corrects the representation to `known = false` with a null count. The formal
result in this report is therefore `UNKNOWN`, consistent with the post-attempt
`+1` unqualified entry. The harness did not call raw `WM_CLOSE`, kill/restart
Explorer, or close that unqualified entry.

Directory cleanup also remained fail-closed. Although the final inventory read
completed, the run could not prove the full containment/identity/unused
predicate, so the directory was not removed. This is preserved as an explicit
cleanup limitation rather than bypassed. A later read-only filesystem audit
found four ignored target directories; all four were empty and non-reparse.
That later observation does not retroactively grant per-attempt removal
authority.

## 8. Provision-only stability and runtime Gates

| Fixed attempt | Result | Decisive fact |
| ---: | --- | --- |
| Debug provision-only 1 | BLOCKED | sole CoCreate returned `E_FAIL` at `create_browser_window` |
| Debug provision-only 2 | NOT RUN | fixed Gate stopped after attempt 1 failed |
| Debug provision-only 3 | NOT RUN | fixed Gate stopped after attempt 1 failed |

```text
PROVISIONING_STABILITY_GATE = BLOCKED
```

The required 3/3 provisioning proof was not available. Consequently the Debug
full Explorer runtime and Release Explorer runtime were not run. There was no
safe delta selection, `SetWindowPos`, post-verification, restore, observer
attribution, safe Quit, or stale-token runtime attempt.

## 9. Build, deterministic test, and regression result

The recovery worktree passed both configuration builds and complete test
suites:

```text
Debug build = PASS
Release build = PASS
Debug CTest = 8/8 PASS
Release CTest = 8/8 PASS
windows-explorer-unit = PASS (Debug and Release)
```

Existing operation harness regressions also passed:

| Harness | Debug | Release |
| --- | --- | --- |
| Owned-window | PASS | PASS |
| Companion-process | PASS | PASS |

These results validate deterministic baseline exclusion, registration
correlation, lease/token separation, and prior owned/companion behavior. They
do not convert the blocked real Explorer provisioning attempt into a PASS.

## 10. Safety audit

```text
USER_EXISTING_WINDOWS_TOUCHED = NO
OTHER_THIRD_PARTY_CONTROL = NO
GLOBAL_INPUT = NO
DLL_INJECTION = NO
POLLING_EVENT_SOURCE = NO
EXPLORER_PROCESS_KILL = NO
NATIVE_TRANSLATION_COUNT = 0
```

The recovery used no existing-window fallback, secondary Explorer launch
mechanism, arbitrary-HWND close, or retry-until-pass loop.

## 11. Attempt 2 remaining risks and blocked evidence

The following remain `NOT TESTED` or unresolved:

- successful `CLSID_ShellBrowserWindow` creation in the active Shell state;
- positive registration-cookie, canonical COM identity, three-way HWND, and
  exact target-location attribution;
- provisioning lease cleanup through exact-object `Quit` and revocation/HWND
  invalidation;
- attribution and location category of the time-correlated post-attempt `+1`
  Shell entry (`UNKNOWN`, not zero);
- 3/3 provisioning stability;
- target token issuance, translation, post-verification, restore, feedback
  attribution, and stale-token rejection;
- Release Explorer desktop runtime;
- allocation-failure/OOM behavior in receipt capture and recovery diagnostics;
- hung or non-responsive Shell/Explorer COM calls and synchronous placement;
  and
- the policy boundary for a genuinely incomplete global inventory, especially
  when a baseline Shell entry cannot provide even a reliable HWND for safe
  exclusion.

The first recovery attempt failed at the official creation API before positive
attribution could begin. Relaxing target evidence, falling back to an existing
Explorer, or repeating until success would not resolve that blocker safely.

## 12. Attempt 2 final recovery Gate result

```text
R1C2A_PRIOR_ART_GATE = PASS
R1C2A_BASELINE_EXCLUSION_GATE = PASS
R1C2A_PROVISIONING_GATE = BLOCKED
R1C2A_ELIGIBILITY_GATE = BLOCKED
R1C2A_RUNTIME_GATE = BLOCKED
PROVISIONING_STABILITY_GATE = BLOCKED
EXPLORER_TEST_TARGET_ISOLATION = BLOCKED
USER_EXISTING_WINDOWS_TOUCHED = NO
OTHER_THIRD_PARTY_CONTROL = NO
R1C2B = NOT STARTED
```

## 13. Attempt 3 successor: user-consented target authority

Attempt 3 started from
`b110f5068c1c252e1f5b0d90315d6d998235adf0`; research is checkpointed at
`560945a`, and the verified implementation is checkpointed at `d606ee1`.

Attempt 2 remains permanently blocked by the recorded
`CoCreateInstance(CLSID_ShellBrowserWindow) -> E_FAIL` result. Attempt 3 does
not repair, retry, weaken, or fall back from that path. The CLSID path and its
exact-object `Quit` cleanup model remain diagnostic history and are not
active/default UAT behavior.

The current path replaces automatic creation with an explicit human authority
fact while preserving stronger native proof:

```text
immutable forbidden set containing every reliable baseline HWND
+ human creates and navigates a new Explorer top-level frame
+ explicit target-creation confirmation
+ exactly one non-baseline candidate at the nonce directory FILE_ID_INFO
+ full live Explorer process/window/security/state/monitor/DPI eligibility
+ matching session/capability/consent generations
= one test-only ExplorerWindowToken
```

The harness creates only the empty, local, non-reparse ignored
`uat/r1c2a/consent-target-<nonce>` directory and prints its absolute path. It
does not launch or navigate Explorer. `Ctrl+N` is merely a suggested human
action, and ENTER is a consent record rather than a password, credential, or
security boundary. A baseline HWND remains forbidden even if the user
accidentally navigates it to the exact nonce directory.

After unique target issuance, the harness displays sanitized facts and requires
a second explicit move consent. It then live-revalidates identity, exact
location, eligibility, monitor/DPI, and all generations before exactly one
same-monitor `SetWindowPos` translation with
`SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE`. Exact post-verification and a
separate immediate live-revalidated restore are required. Native success alone
cannot pass the Gate.

The user-created target is never automatically closed: no
`IWebBrowser2::Quit`, `WM_CLOSE`, Shell close command, Explorer process
termination, or restart is authorized. Optional manual closure may support a
stale-token check; if skipped, `WINDOW_DESTROY_LIFETIME = NOT TESTED` is
acceptable. No global/synthetic input, UI Automation, foreground forcing,
selector UI, polling event source, Glue, Snap, resize, or R1-C2B behavior is
introduced.

The active CLI/evidence contracts are:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\run-r1c2a-explorer-consent-evidence.ps1 `
  -BuildDirectory out/r1c2a-debug `
  -Configuration Debug `
  -ObserveSeconds 180
```

The wrapper invokes `panebind-explorer-harness.exe
--interactive-consent-test`; raw observer and harness JSONL remain ignored
under `uat/r1c2a/`.

The interactive path is not CTest. The capability implementation and harness
build pass Debug/Release verification; all 8 CTest entries, the expanded
Explorer deterministic consent matrix, and Owned/Companion Debug/Release
regressions pass. Redirected input and deprecated automatic modes reject before
side effects. Human Debug and Release UAT subsequently passed against the
unchanged Explorer implementation. Full sanitized evidence is in
[`R1C2A_HUMAN_VALIDATION_REPORT.md`](R1C2A_HUMAN_VALIDATION_REPORT.md).

The optional stale-token branch was attempted but did not validate destruction
or stale rejection in either final run. Both attempts remained fail-closed with
no native apply; this non-gating result remains a recorded risk.

The final Attempt 3 state is:

```text
ATTEMPT_1_AUTO_INVENTORY_PROVISIONING = BLOCKED
ATTEMPT_2_SHELL_REGISTRATION_PROVISIONING = BLOCKED
AUTO_PROVISIONING_ON_CURRENT_WINDOWS11 = BLOCKED
ATTEMPT_3_USER_CONSENTED_AUTHORITY = PASS
R1C2A_CONSENT_CAPABILITY_IMPLEMENTATION = PASS
R1C2A_DEBUG_INTERACTIVE_UAT = PASS
R1C2A_RELEASE_INTERACTIVE_UAT = PASS
R1C2A_ELIGIBILITY_GATE = PASS
R1C2A_RUNTIME_GATE = PASS
USER_CONSENTED_TARGET_AUTHORITY = PASS
USER_EXISTING_WINDOWS_TOUCHED = NO
OTHER_THIRD_PARTY_CONTROL = NO
R1C2B = NOT STARTED
```
