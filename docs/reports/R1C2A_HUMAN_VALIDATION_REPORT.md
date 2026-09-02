# PaneBind R1-C2A Human Validation Report

Report date: 2026-09-03 (Asia/Shanghai).

## 1. Scope and evaluated implementation

This report seals the final human validation for the narrowly scoped R1-C2A
Explorer baseline. Both interactive runs evaluated the Explorer runtime at:

```text
Implementation SHA = 2c9b5480312dbf201112c2821fdb4cbc9659ea45
Branch = codex/r1c2a-explorer-single-translation
Debug evidence prefix = 20260902T161116173Z
Release evidence prefix = 20260902T161740730Z
```

The Release prefix is unambiguous: it is the only consent evidence triplet
after the known Debug prefix. The user also confirmed that this later run used
the Release build. Raw JSONL, native identifiers, and nonce paths remain under
ignored `uat/r1c2a/` and are not committed.

The accepted claim is intentionally narrow:

> With two explicit human confirmations, immutable baseline exclusion,
> Explorer-specific eligibility, and live identity revalidation, PaneBind can
> translate one newly created Explorer top-level test window once and restore
> it exactly.

This does not authorize existing Explorer windows, other applications,
continuous Glue, resize, Snap, global input, or R1-C2B behavior.

## 2. Evidence quality and integrity

The unit of analysis is one runner-produced evidence triplet: harness JSONL,
Observer JSONL, and empty Observer stderr. Both selected triplets were parsed
independently rather than accepted from terminal success text alone.

| Check | Debug | Release |
| --- | ---: | ---: |
| Harness records | 15 | 15 |
| Observer records | 6,919 | 3,439 |
| JSONL parse errors / empty lines | 0 / 0 | 0 / 0 |
| Schema violations | 0 | 0 |
| Observer sequence errors | 0 | 0 |
| Hook registration complete | 1 | 1 |
| Hook shutdown complete | 1 | 1 |
| Observer shutdown complete | 1 | 1 |
| Queue overflow diagnostics | 0 | 0 |
| Queue notification failures | 0 | 0 |
| Incomplete diagnostics | 0 | 0 |
| Observer stderr bytes | 0 | 0 |

Each harness contains exactly one passing baseline, candidate selection,
target confirmation, move confirmation, primary marker/operation, restore
marker/operation, and final summary. Both summaries report one primary native
apply, one restore native apply, no automatic close, no existing-user-window
control, and no other third-party control.

Each Observer stream also contains five unrelated, invisible initial-census
records whose process-path inspection returned Win32 access-denied diagnostics.
They are rejected census noise, not incomplete Observer diagnostics, and never
entered candidate or operation attribution. The target operation records
themselves contain no field error.

Data-quality conclusion: the two evidence triplets are complete, internally
consistent, and suitable for the R1-C2A eligibility and runtime decision with
high confidence. The optional stale-token result is a separate failed check,
documented below; it does not contaminate the successful translation/restore
grain and is not a Runtime Gate requirement.

## 3. Human consent and generation chain

Both runs record `interactive_console` as the input source for the two distinct
human confirmations and `user_consent` as the capability authority.

```text
baseline_generation
< target_prompt_generation
< target_confirmation_generation
< eligibility_generation
< consent_token_generation
< move_prompt_generation
< move_confirmation_generation

Observed in Debug and Release: 1 < 2 < 3 < 4 < 5 < 6 < 7
```

The first confirmation authorized target correlation and token issuance only.
The second `Y + ENTER` confirmation authorized one primary native attempt.
Redirected, piped, or file input was not accepted as consent.

## 4. Target isolation

For both runs:

- baseline inventory was complete;
- every baseline Shell entry had a reliable window identity and entered the
  immutable forbidden set;
- the candidate was a new non-baseline top-level window;
- exactly one Shell entry matched the exact nonce `FILE_ID_INFO`;
- authority was `user_consent`;
- the candidate was in normal state; and
- the final summary reported `user_existing_windows_touched = false`.

No baseline Explorer window was navigated, moved, closed, or issued a
capability. No automatic-provisioning failure fell back to an existing window.

## 5. Explorer eligibility

The two issued targets produced the same sanitized eligible profile:

| Fact | Debug | Release |
| --- | --- | --- |
| Process | `explorer.exe` | `explorer.exe` |
| Class | `CabinetWClass` | `CabinetWClass` |
| Root/top-level identity | PASS | PASS |
| Visible / cloak | `true` / `0` | `true` / `0` |
| Minimized / maximized | `false` / `false` | `false` / `false` |
| Process/window identity | valid and stable | valid and stable |
| Shell location / file identity | exact and stable | exact and stable |
| Monitor | `\\.\DISPLAY1` (primary) | `\\.\DISPLAY1` (primary) |
| DPI / awareness | 192 / Per-Monitor V2 | 192 / Per-Monitor V2 |

The raw schema does not separately serialize integrity, UIAccess, or
AppContainer values. Their aggregate eligibility is established by successful
token issuance and repeated live validation at this implementation SHA, whose
hard allowlist requires compatible user/session, medium integrity, no
elevation, `UIAccess = false`, and `AppContainer = false`. This is a code-gate
inference, not a claim that those individual values appeared as raw fields.

## 6. Primary translation

Both Debug and Release selected the deterministic same-monitor delta:

```text
dx = -80
dy = -50
```

Both runs recorded identical geometry:

| Geometry | Initial | Requested primary | Actual primary |
| --- | --- | --- | --- |
| Visible rect | `[1154,116,2993,1190]` | `[1074,66,2913,1140]` | `[1074,66,2913,1140]` |
| Positioning rect | `[1143,116,3004,1201]` | `[1063,66,2924,1151]` | `[1063,66,2924,1151]` |

For both runs:

```text
native_apply_attempted = true
native_translation_count = 1
exact_receipt = true
size_preserved = true
identity_stable = true
location_stable = true
monitor_and_dpi_stable = true
```

The implementation uses one `SetWindowPos` call with
`SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE`. No resize, z-order change,
activation, foreground forcing, or `AttachThreadInput` path exists.

## 7. Restore

Restore is an independent cleanup operation, not native or transactional
rollback. In both runs it performed exactly one separately live-validated
native apply and passed exact post-verification:

| Geometry | Requested restore | Actual restore | Initial |
| --- | --- | --- | --- |
| Visible rect | `[1154,116,2993,1190]` | `[1154,116,2993,1190]` | `[1154,116,2993,1190]` |
| Positioning rect | `[1143,116,3004,1201]` | `[1143,116,3004,1201]` | `[1143,116,3004,1201]` |

```text
restore_native_apply_count = 1
restore_exact_receipt = true
identity/location/monitor/DPI remained valid
```

## 8. Optional stale-token step

The optional stale-token branch was attempted in both selected runs, but both
records report:

```text
window_destroy_lifetime = FAIL
native_apply_attempted = false
directory_cleanup = DEFERRED
```

Therefore `WINDOW_DESTROY_LIFETIME` is **not validated** and must not be called
`MANUALLY OBSERVED` or `PASS`. Importantly, the failed optional check made no
native apply. The round brief explicitly excludes this optional lifetime check
from the Runtime Gate, so this failure remains a documented risk rather than a
translation/restore blocker.

## 9. Target WinEvent evidence

Only events matching both the issued candidate window identity and candidate
process were counted. Each full evidence stream contained four target
`LOCATION_CHANGE` events and no target START/END. Two LOCATION events occurred
before the operation marker during creation/navigation and were excluded from
operation feedback.

Monotonic operation markers and exact resulting geometry reliably separate the
remaining two events:

| Run / phase | MOVESIZESTART | LOCATION_CHANGE | MOVESIZEEND |
| --- | ---: | ---: | ---: |
| Debug primary | 0 | 1 | 0 |
| Debug restore | 0 | 1 | 0 |
| Release primary | 0 | 1 | 0 |
| Release restore | 0 | 1 | 0 |

For every phase, the accepted LOCATION snapshot exactly matches that phase's
requested and actual visible/positioning geometry. In both runs, two
non-target LOCATION records were interleaved between the target primary and
restore events. The global event stream is therefore not contiguous even
though target attribution is reliable.

## 10. Feedback attribution conclusion

The combined R1-B, R1-C1, and R1-C2A evidence supports these conclusions:

1. Programmatic Explorer translation did not naturally emit START or END in
   either run.
2. It emitted LOCATION feedback for both primary and restore.
3. These runs observed one LOCATION per native request, but that cardinality is
   not a Windows contract and must not be fixed in a suppression design.
4. Duplicate, missing, or interleaved events remain required design cases; the
   current runs directly demonstrate unrelated interleaving.
5. Operation marker, capability/token/session generation, target process/window
   identity, and expected/actual geometry were sufficient for reliable offline
   attribution in these controlled runs.
6. Those inputs are not by themselves a completed runtime suppression engine;
   future logic still needs bounded expiry, repeated/missing-event handling,
   invalidation, and native/post-verification failure policy.

Time-only, raw-HWND-only, mandatory START/END, event contiguity, and
one-request-one-event assumptions remain rejected.

## 11. User-data and scope safety

```text
USER_EXISTING_WINDOWS_TOUCHED = NO
OTHER_THIRD_PARTY_CONTROL = NO
GLOBAL_INPUT = NO
DLL_INJECTION = NO
RESIDENT_POLLING = NO
EXPLORER_PROCESS_KILL = NO
AUTO_CLOSE_ATTEMPTED = NO
```

The user created and later owned the test window. PaneBind did not call
`IWebBrowser2::Quit`, send `WM_CLOSE`, kill/restart Explorer, synthesize input,
or control another application.

## 12. Automatic-provisioning history

The human-validation PASS does not rewrite the failed automatic approaches:

```text
Attempt 1: inventory/difference automatic provisioning = BLOCKED
Attempt 2: CLSID_ShellBrowserWindow registration recovery = BLOCKED
Attempt 2 CoCreate result = E_FAIL
AUTO_PROVISIONING_ON_CURRENT_WINDOWS11 = BLOCKED
```

The only active successful real-Explorer UAT path is
`--interactive-consent-test`. Deprecated automatic harness modes and legacy
evidence scripts reject before Shell creation or native window side effects.
There is no automatic-to-existing-window fallback and no hidden CLSID call in
the consent path.

## 13. Final automated regression

After the documentation and runner wait-state UX update, the frozen
implementation passed:

| Check | Debug | Release |
| --- | --- | --- |
| Build | PASS | PASS |
| CTest | 8/8 PASS | 8/8 PASS |
| Owned-window harness | PASS | PASS |
| Companion-process harness | PASS | PASS |

The modified PowerShell evidence runner retains its UTF-8 BOM and parses with
zero Windows PowerShell 5.1 AST errors. The UX-only change adds progress text
around the existing natural Observer wait; it changes no duration, process
lifecycle, JSONL, evidence rule, or validation Gate.

The Explorer interactive UAT was not rerun because no Explorer runtime,
eligibility, consent, translation, post-verification, or restore code changed
after the two human runs.

## 14. Remaining NOT TESTED

- mixed-DPI, multi-monitor, and cross-monitor runtime;
- elevated Explorer, UIAccess target, and AppContainer target rejection in a
  real desktop run;
- hung Explorer and destruction exactly during native apply;
- Explorer application-adjusted `WINDOWPOS` behavior beyond this environment;
- reliable stale-token rejection after manual destruction;
- other third-party applications;
- continuous Glue and feedback-suppression runtime;
- global Ctrl/input behavior; and
- allocation/OOM and forced COM `Unadvise` failure paths.

## 15. Final human-validation Gate

```text
R1C2A_HUMAN_VALIDATION = PASS
R1C2A_DEBUG_INTERACTIVE_UAT = PASS
R1C2A_RELEASE_INTERACTIVE_UAT = PASS

R1C2A_PRIOR_ART_GATE = PASS
R1C2A_BASELINE_EXCLUSION_GATE = PASS
R1C2A_CONSENT_CAPABILITY_IMPLEMENTATION = PASS
R1C2A_ELIGIBILITY_GATE = PASS
R1C2A_RUNTIME_GATE = PASS

AUTO_PROVISIONING_ON_CURRENT_WINDOWS11 = BLOCKED
USER_CONSENTED_TARGET_AUTHORITY = PASS

USER_EXISTING_WINDOWS_TOUCHED = NO
OTHER_THIRD_PARTY_CONTROL = NO
R1C2B = NOT STARTED
```
