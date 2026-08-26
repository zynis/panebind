# PaneBind R1-C1 Companion-Process Execution Report

Report date: 2026-08-26 (Asia/Shanghai).

## 1. Round and scope

```text
Round = R1-C1 - External Companion-Process Operations & Feedback Baseline
Starting main = 505ecb16d3a3b66e56879962fe876f513d5717f1
Branch = codex/r1c1-companion-process-operations
Evaluated implementation HEAD = 63d240c
R0_BASELINE = SEALED
R1A_ALGORITHM_BASELINE = MERGED_TO_MAIN
R1B_OPERATIONS_BASELINE = MERGED_TO_MAIN
R1C2 = NOT STARTED
```

Commits through the evaluated implementation are:

```text
c39b1a6 docs: define remote git integrity policy
5ba01dc docs: research r1c1 companion process semantics
63d240c feat: add companion process operations harness
```

The final report commit is recorded in the round handoff after this document is
committed and the branch is pushed.

R1-C1 controls only four windows created by a PaneBind companion process that
the test controller launches itself. The companion is a fixture, not evidence
that arbitrary third-party applications are eligible or safe. Explorer,
Office, editors, browsers, terminals, system windows, and every user-launched
window remained prohibited and unreachable from capability issuance.

No Glue behavior, Ctrl interaction, global input, Snap, Glue Resize, tray,
autostart, persistent group, third-party eligibility, or R1-C2 code was added.

## 2. Remote Git integrity governance

The root `AGENTS.md` now requires standard Git transport for fetch/pull/push and
remote-tracking synchronization. GitHub APIs may independently verify remote
state but cannot normally reconstruct objects or simulate fetch. Persistent
standard-transport failure is a blocker unless a future round explicitly
authorizes another recovery method. The rule is prospective and does not
rewrite already verified history.

## 3. Prior-art gate and provenance

Detailed findings are in
[`R1C1_COMPANION_PROCESS_RESEARCH.md`](../research/R1C1_COMPANION_PROCESS_RESEARCH.md)
and the updated
[`SOURCE_PROVENANCE.md`](../research/SOURCE_PROVENANCE.md).

| Source | Pinned revision | License/use | Result |
| --- | --- | --- | --- |
| AltSnap | `5c86416ad21e4b72844a998a746bd3bb0bee5f5d` | GPL, reference-only | cross-process behavioral precedent only |
| AltDrag | `e2740d605b0336a3b391fec26794718864b19521` | GPL, reference-only | injection/subclass counterexample |
| PowerToys/FancyZones | `19c4d805321db86f3634e6968e14dbf25cbba14a` | MIT, reference-only | process/lifetime/DPI lessons; no Defer precedent |
| Microsoft Learn | live pages reviewed 2026-08-26 | documented contracts | launch/IPC/lifetime/native-operation source |

```text
R1C1_PRIOR_ART_GATE = PASS
EXTERNAL_CODE_COPIED = NO
EXTERNAL_CODE_ADAPTED = NO
```

## 4. Executables and process topology

Two new executables are built:

```text
panebind-companion-harness.exe --self-test
    -> CreateProcessW
panebind-companion-target.exe
    -> dedicated IPC command thread
    -> dedicated UI/message-loop thread
    -> independent top-level A/B/C/D windows
```

The harness resolves the target only as the fixed executable beside itself and
passes a fully qualified, nonempty `lpApplicationName`. It does not use a shell
or process-name search. The target does not enumerate/read/control other
windows or read user files.

## 5. Restricted inherited-pipe protocol

The controller creates two anonymous pipes for bidirectional communication.
Parent-only ends have inheritance cleared. `STARTUPINFOEX` with
`PROC_THREAD_ATTRIBUTE_HANDLE_LIST` allowlists only the child command-read and
response-write ends; `bInheritHandles=TRUE` and
`EXTENDED_STARTUPINFO_PRESENT` are used together. Parent copies of child ends
close immediately after launch, and the child clears their further-inheritance
bits.

The binary protocol is bounded and versioned:

```text
40-byte header
magic = PBC1
version = 1
message kind
payload size <= 64 KiB
128-bit session marker
request ID
```

Exact read/write loops handle partial transfers. The handshake, command
results, and evidence payloads have fixed layouts. Target WndProc never writes
to a pipe; it records into a bounded in-process evidence buffer drained only by
an IPC query.

```text
IPC_AFTER_TIMEOUT_POLICY = SessionPoisoned / RetireSession
```

A timeout, truncated/oversized frame, live-child read/write failure, response
envelope/request mismatch, or malformed evidence closes both protocol
endpoints, retires every token, and prevents subsequent capture/apply reuse.
Shutdown can then only await EOF-driven exit or clean up the exact retained
fixture process handle. The poison policy is code-reviewed; forced timeout and
`CancelSynchronousIo` races are not runtime-forced.

## 6. Launch handshake and issuance

Handshake is the only window-capability source. It must echo the launch marker
and report exactly:

```text
launched PID
one UI TID
four unique logical IDs A/B/C/D
four unique nonzero raw HWND registration facts
generation per window
session-derived marker value per window
```

The controller rejects wrong magic/version/size/marker/PID, extra/missing or
duplicate IDs/HWNDs, an exited process, unexpected class, wrong PID/TID,
non-root/child/owned windows, or marker mismatch. No Enum/Find/title/
foreground/point discovery exists.

## 7. Session and process identity

`CompanionSession` retains the exact `CreateProcessW` process handle until
session teardown. The primary thread handle closes after launch. PID is a
validated property, not authority. Every session has a random 128-bit marker
and a process-monotonic controller-registry authority.

The baseline also queries both process tokens and requires equal:

```text
integrity RID
TokenUIAccess
TokenIsAppContainer
```

Final evidence recorded controller and target as medium integrity RID 8192,
UIAccess false, AppContainer false.

## 8. CompanionWindowToken

`CompanionWindowToken` is separate from every R1-B owned type and contains:

```text
private controller-registry authority
128-bit companion-session authority
logical A/B/C/D ID
generation
```

It has no default/public native constructor and no HWND conversion. R1-B
`OwnedWindowToken`, `OwnedWindowRegistry`, and `OwnedWindowOperations` public
headers/names/authority are unchanged.

Unit and runtime tests prove that another controller/session cannot resolve an
otherwise equal logical ID/generation and that session 2 rejects session-1 A
before native apply.

## 9. Per-use window validation

Immediately before capture/native apply, resolution verifies:

```text
process handle remains nonsignaled
GetProcessId(handle) == launched PID
token controller/session authority + generation
IsWindow
GetWindowThreadProcessId == handshake PID/TID
class == PaneBind.R1C1.CompanionWindow
GA_ROOT == HWND
WS_CHILD absent
GW_OWNER == null
session-specific property name/value
```

Unexpected failure retires the affected generation; process exit retires the
whole session. `contains()` and token enumeration also perform live native
resolution rather than trusting the ledger alone.

## 10. Shared translation preparation

Only capability-neutral translation preparation was extracted from R1-B into
`window_translation`. Both adapters reuse it, while owned and companion
resolvers remain separate.

```text
target visible - current visible = checked (dx,dy)
target positioning = current positioning + checked (dx,dy)
```

Visible/positioning sizes remain unchanged. Empty, resize/mixed, arithmetic
overflow, and Win32 int-range overflow fail before native apply. No generic
public arbitrary-HWND executor was created.

## 11. Cross-process native operation contract

Leader placement uses cross-process/cross-thread `SetWindowPos`. Followers use
`BeginDeferWindowPos`, latest-returned `DeferWindowPos` handles, and one
`EndDeferWindowPos`.

```text
flags = SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
```

The complete member set is resolved, captured, translated, duplicate/parent
checked, target-step armed, and resolved again before native apply. Async
placement and `AttachThreadInput` are absent.

## 12. Structured results and rollback boundary

The companion result records operation/batch ID, session/token/generation,
PID/TID, stage/status, requested/deferred/verified counts, native-attempt and
outcome-known flags, diagnostics, and every member's before/requested/actual
geometry, monitor, and DPI.

Stages distinguish preflight, target-arm, single apply, Begin, Defer, End, and
post-verification. Defer failure abandons the chain without End. Native success
never substitutes for requested-versus-actual verification.

Only preflight is all-or-nothing. Native operation is not described as
transactional or rollback-capable. Fixture recovery is a separate operation.

## 13. Exact 2x2 and R1-A consumption

Session 1 startup cross-process Defer produced exact visible topology:

| Window | Visible rectangle | Positioning rectangle |
| --- | --- | --- |
| A | `[1198,683,1476,882]` | `[1187,683,1487,893]` |
| B | `[1476,683,1754,882]` | `[1465,683,1765,893]` |
| C | `[1198,882,1476,1081]` | `[1187,882,1487,1092]` |
| D | `[1476,882,1754,1081]` | `[1465,882,1765,1092]` |

The controller built the real R1-A `WindowAdjacencyGraph`, verified four side
relations and one four-node component, created `TranslationSession(A)`, and
consumed each returned follower `target_visible_rect`. It did not reimplement
adjacency or planning math.

## 14. Scripted cross-process sequence

The tested initial-relative leader sequence was:

```text
(+30,+20)
(+80,+50)
(+80,+50) repeated
(+40,+10) backtrack
(+120,+60)
```

All five leader SetWindowPos receipts and all follower Defer receipts reached
verified success. The repeated target caused no drift, the backtrack matched
the initial-relative plan, and topology remained unchanged. Final geometry was:

| Window | Final visible | Final positioning |
| --- | --- | --- |
| A | `[1318,743,1596,942]` | `[1307,743,1607,953]` |
| B | `[1596,743,1874,942]` | `[1585,743,1885,953]` |
| C | `[1318,942,1596,1141]` | `[1307,942,1607,1152]` |
| D | `[1596,942,1874,1141]` | `[1585,942,1885,1152]` |

## 15. Uncooperative target result

Test-only D mode legally modified `WM_WINDOWPOSCHANGING::WINDOWPOS.x` by five
pixels. The batch returned:

```text
status = PostVerificationFailed
native_apply_attempted = true
native_outcome_known = true
requested/deferred/verified = 3/3/2
```

| Window | Requested visible | Actual visible |
| --- | --- | --- |
| B | `[1611,751,1889,950]` | same |
| C | `[1333,950,1611,1149]` | same |
| D | `[1611,950,1889,1149]` | `[1616,950,1894,1149]` |

Target evidence independently recorded D proposed/effective positioning X as
`1600 -> 1605`, operation step 12. All three actual snapshots were retained.
The later successful restore was explicitly a cleanup operation, not rollback.

## 16. Window destruction test

The controller requested destroy C through IPC; the target UI thread executed
`DestroyWindow`, and `WM_NCDESTROY` removed the session marker. C retired from
the controller registry.

`B valid + C stale + D valid` then returned `StaleToken/Preflight`,
`deferred_count=0`, `native_apply_attempted=false`; B and D before/after
snapshots were identical.

## 17. Whole-process and cross-session lifetime

Session 1 acknowledged graceful shutdown, exited, and signaled its retained
process handle without termination fallback. All four session-1 tokens became
stale and operations failed after exit.

Session 2 launched a new target, reissued logical IDs A/B/C/D under a distinct
128-bit authority, resolved its local A, and rejected session-1 A in preflight
with no native apply. Session 2 also exited gracefully.

Actual numeric PID/HWND reuse was not forced and remains **NOT TESTED**. It is
not required for the process-handle/session-authority Gate.

## 18. Target-side WndProc evidence

Final session-1 target evidence contained 101 records, no overflow/drop:

| Message | Count |
| --- | ---: |
| `WM_WINDOWPOSCHANGING` | 35 |
| `WM_WINDOWPOSCHANGED` | 31 |
| `WM_MOVE` | 30 |
| `WM_SIZE` | 4 |
| `WM_NCDESTROY` | 1 |

Records include logical ID, generation, operation step, sequence/tick, PID/TID,
proposed/effective X, geometry flags, and D test offset. Cardinality and order
are one-run evidence, not universal Win32 guarantees.

## 19. External WinEvent evidence

The final evidence command used the ignored runner with Debug binaries,
12-second observation, and 2-second hold. Raw prefix:

```text
uat/r1c1/20260826T005227232Z
```

Evidence quality:

```text
observer JSONL records = 715 valid
observer sequence = continuous 1..715
hook/census/shutdown = complete
queue/drop/notification failures = none
harness JSONL records = 313 valid
harness summary = PASS / failures 0
observer/harness stderr = empty
```

Filtering the two controller-launched target PIDs:

```text
MOVESIZESTART = 0
LOCATION_CHANGE = 26
MOVESIZEEND = 0
```

The 26 events exactly map to startup A-D (4), four changing whole-topology
positions (16), uncooperative B/C/D (3), and recovery B/C/D (3). Repeated
same-target, destroy, and session-2 old-token rejection produced no location
acknowledgement. Observed event contiguity is not a contract.

## 20. Feedback attribution conclusion

Within this controlled run, an event candidate can be attributed offline using:

```text
session/token/generation
+ expected target
+ actual geometry
+ native stage/outcome
```

Time only, raw HWND only, mandatory START/END, assumed adjacency/contiguity, or
one-request-one-event are rejected. No product Glue feedback-suppression state
machine was implemented.

## 21. Integrity, DPI, and monitor environment

```text
controller/target integrity RID = 8192 / 8192
UIAccess = false
AppContainer = false
display = \\.\DISPLAY1
monitor = [0,0,3072,1920]
work area = [0,0,3072,1824]
DPI = 192
DPI awareness = PerMonitorV2
```

Elevated/UIAccess/AppContainer targets, mixed-DPI, multi-monitor, and
cross-monitor movement remain **NOT TESTED**. R1-C1 never elevates or changes a
message filter.

## 22. Cleanup contract

Normal cleanup is an IPC shutdown request followed by a bounded process-handle
wait. The fallback can call `TerminateProcess` only on the exact retained
fixture handle, then waits again because termination is asynchronous. It never
kills by PID/name/wildcard and never uses `taskkill`.

Both final sessions exited gracefully, so forced termination was not used.
Hung target WndProc during synchronous native placement and exact
destroy-during-End remain **NOT TESTED**.

## 23. Automated and runtime tests

Deterministic `windows-companion-unit` covers:

- opaque/separate companion token surface;
- controller and cross-session authority;
- generation, stale, native-value reuse model, and retire-all;
- duplicate logical/native facts and mixed stale token-set preflight;
- second-session logical reissue without old-token alias;
- independent top-level predicate;
- D-only bounded uncooperative selection;
- marker logical/generation distinction; and
- shared translation success/resize/overflow.

Final Debug and Release CTest:

```text
1/7 geometry                         PASS
2/7 core-model                       PASS
3/7 topology                         PASS
4/7 translation                      PASS
5/7 windows-text-encoding            PASS
6/7 windows-owned-operations-unit    PASS
7/7 windows-companion-unit           PASS
TOTAL                                7/7 PASS
```

The desktop-dependent companion runtime is deliberately an explicit
`--self-test`, not a universal CTest. Debug and Release both returned exit 0
with `self_test_summary=PASS`.

The R1-B owned harness also passed after translation preparation factoring.

## 24. Build environment and commands

Environment:

- Visual Studio 18 2026;
- MSVC 19.50.35729.0;
- Windows SDK 10.0.26100.0; and
- bundled CMake 4.2.3.

Final x64 build directories were `out/r1c1-debug` and
`out/r1c1-release-final`. Both completed without compiler warnings.

```powershell
cmake -S . -B out/r1c1-debug -G "Visual Studio 18 2026" -A x64 -DBUILD_TESTING=ON
cmake --build out/r1c1-debug --config Debug --parallel
ctest --test-dir out/r1c1-debug -C Debug --output-on-failure

cmake -S . -B out/r1c1-release-final -G "Visual Studio 18 2026" -A x64 -DBUILD_TESTING=ON
cmake --build out/r1c1-release-final --config Release --parallel
ctest --test-dir out/r1c1-release-final -C Release --output-on-failure

panebind-companion-harness.exe --self-test
```

## 25. Safety and isolation audit

- `src/core/` was not modified and contains no Windows process/window types.
- Companion public operations accept tokens, never raw HWND.
- R1-B owned public header and authority remain unchanged.
- No `EnumWindows`, `EnumDesktopWindows`, `FindWindow`, foreground/point/
  desktop discovery, or shell enumeration is present.
- No `SendInput`, mouse/key APIs, global hook, `AttachThreadInput`, remote
  thread/memory, injection, taskkill, or polling was added.
- Raw `uat/r1c1` and all `out/` artifacts are ignored and untracked.

```text
THIRD_PARTY_WINDOW_CONTROL = NO
GLOBAL_INPUT_CONTROL = NO
DLL_INJECTION = NO
RESIDENT_POLLING = NO
CORE_WIN32_ISOLATION = PASS
```

## 26. Remaining NOT TESTED and hardening risks

- real numeric PID/HWND reuse;
- exact destroy/exit during `EndDeferWindowPos`;
- hung target WndProc during synchronous SetWindowPos/End;
- formal wall-clock boundedness if `CancelSynchronousIo` itself races/fails;
- forced response-timeout/partial-frame poison path runtime behavior;
- forced `TerminateProcess` fallback runtime path;
- elevated/UIAccess/AppContainer and cross-Windows-session targets;
- mixed-DPI, multi-monitor, cross-monitor, topology change during apply; and
- cross-environment WinEvent ordering/cardinality stability.

## 27. R1-C2 research questions only

1. What eligibility and consent model could ever authorize a real user window?
2. How is external process image/signature/elevation/virtual-desktop identity
   bound without a cooperative handshake?
3. How should hung native calls be supervised without async completion lies,
   `TerminateThread`, injection, or unsafe user-process termination?
4. Which protocol/session failures poison pending receipts and feedback state?
5. How are elevation/UIPI, AppContainer, mixed-DPI, monitor changes, minimize,
   maximize, z-order, and focus represented?
6. What feedback-suppression state machine handles duplicates, missing/no-op
   events, interleaving, and unknown native outcomes?

No R1-C2 branch or implementation exists.

## 28. Gate result

```text
R1C1_PRIOR_ART_GATE = PASS
R1C1_CAPABILITY_BASELINE = PASS
R1C1_RUNTIME_GATE = PASS
THIRD_PARTY_WINDOW_CONTROL = NO
R1C2 = NOT STARTED
```
