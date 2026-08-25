# PaneBind R1-B Owned-Window Operations Execution Report

Report date: 2026-08-25 (Asia/Shanghai).

## 1. Round, branch, and scope

```text
Round = R1-B - Owned-Window Operations Harness
Starting main = b911056c48282b24fcec0fae66994577be4258e3
Branch = codex/r1b-owned-window-operations
R0_BASELINE = SEALED
R1A_ALGORITHM_BASELINE = MERGED_TO_MAIN
R1C = NOT STARTED
```

R1-B implements a Windows-only native-operations boundary and a four-window
test harness. The adapter can act only on opaque capabilities issued for
independent, unowned, current-process windows of the fixed private harness
class. It is not a general third-party window API. Explorer, Office, editors,
browsers, terminals, system windows, and every other existing user window were
outside the executable control path.

Implemented behavior is pure translation only. Glue runtime, Glue Resize,
Snap, input hooks, zones, persistent groups, tray/settings, and R1-C feedback
suppression remain unimplemented.

## 2. Prior-art gate and provenance

Detailed findings are recorded in
[`R1B_WINDOWS_OPERATIONS_RESEARCH.md`](../research/R1B_WINDOWS_OPERATIONS_RESEARCH.md)
and the updated
[`SOURCE_PROVENANCE.md`](../research/SOURCE_PROVENANCE.md).

| Source | Pinned revision | License/use | R1-B result |
| --- | --- | --- | --- |
| AltSnap | `5c86416ad21e4b72844a998a746bd3bb0bee5f5d` | GPL-3.0-or-later, reference-only | targeted operations/history inspected |
| AltDrag | `e2740d605b0336a3b391fec26794718864b19521` | GPL-3.0-or-later, reference-only | historical move/injection comparison inspected |
| PowerToys/FancyZones | `19c4d805321db86f3634e6968e14dbf25cbba14a` | MIT, reference-only in R1-B | placement/lifetime/DPI history inspected |
| Microsoft Learn Win32 documentation | live pages reviewed 2026-08-25 | facts paraphrased/cited | native contract source |

FancyZones at the pinned SHA has no `BeginDeferWindowPos`,
`DeferWindowPos`, or `EndDeferWindowPos` call in its FancyZones tree, so it is
not treated as evidence for batch atomicity or rollback. Microsoft documents
no transactional rollback guarantee for End. AltSnap and AltDrag remain GPL
reference-only.

```text
R1B_PRIOR_ART_GATE = PASS
EXTERNAL_CODE_COPIED = NO
EXTERNAL_CODE_ADAPTED = NO
```

## 3. Owned capability and public API audit

The public operation surface is:

```text
OwnedWindowOperations::capture(OwnedWindowToken)
OwnedWindowOperations::apply_one(WindowTranslationRequest)
OwnedWindowOperations::apply(span<WindowTranslationRequest>)
```

None accepts or returns a raw `HWND`. `OwnedWindowToken` has no default/public
value constructor and no conversion to a native handle. Its equality and
resolution include:

```text
private process-monotonic registry authority
+ logical registration ID
+ generation
```

The authority field closes a cross-registry alias found during review: two
registries can both have local token `1/generation 1`, but neither can resolve
the other's token. Unit tests reproduce this equal-local-ID condition and
verify bidirectional rejection.

`OwnedWindowRegistry::register_window(HWND)` is the sole raw-HWND capability
issuance boundary. It verifies before issuing:

```text
IsWindow
PID == current process
creator TID == current/registered UI thread
class == PaneBind.R1B.OwnedWindow
GetAncestor(GA_ROOT) == hwnd
WS_CHILD absent
GW_OWNER == null
no conflicting private property
private generation property successfully installed and read back
```

Every capture/apply resolves the token again and repeats the live PID/TID,
class, independent-top-level, marker, authority, and generation checks. This
is a process-local architecture capability boundary, not a security sandbox
against malicious code already executing in the harness process.

Static and call-path audit found no way for a caller-supplied third-party HWND
to enter `capture`, `apply_one`, or `apply`. The harness itself creates exactly
four `parent=null`, unowned top-level windows and registers only those four.

## 4. Token and HWND lifetime

The harness WndProc calls `invalidate_window(hwnd)` at `WM_NCDESTROY` entry,
before forwarding to `DefWindowProc`. Invalidation retires the ledger entry and
removes the property marker. The old token then remains stale even if Windows
later recycles the same numeric HWND value.

Runtime result:

```text
Destroy follower C = PASS
C token active after WM_NCDESTROY = NO
stale C apply_one stage = Preflight
stale C native_apply_attempted = false
```

Forced numeric HWND-value reuse is **NOT TESTED** because reuse cannot be made
reliable. The deterministic ledger test retires a native value, reissues that
same value, verifies a higher/new token generation, and proves that the old
token remains unresolved.

## 5. Visible-to-positioning translation bridge

The R1-A output remains a `target_visible_rect`. The adapter never passes that
rectangle directly to a native positioning API. Preflight computes:

```text
change = current_visible -> target_visible
require unchanged or one equal four-edge translation
dx = target_visible.left - current_visible.left
dy = target_visible.top  - current_visible.top

target_positioning = current_positioning translated by (dx, dy)
```

Visible and positioning sizes must remain nonempty and unchanged. All
subtraction/addition and all conversions into native `int` coordinates are
checked. Resize, mixed change, empty geometry, arithmetic overflow, and native
range overflow fail before any native call.

In the final runtime, A's initial visible and positioning rectangles show the
distinction directly:

```text
initial A visible     = [1198,683,1476,882]
initial A positioning = [1187,683,1487,893]
final A visible       = [1318,743,1596,942]
final A positioning   = [1307,743,1607,953]
delta                 = (+120,+60) for both rectangles
```

Requested and actual visible/positioning rectangles matched exactly for all
successful operations. Width and height were preserved.

## 6. Native operation choices and flags

Leader translations use `SetWindowPos`; follower batches use:

```text
BeginDeferWindowPos
DeferWindowPos x N (always carrying forward the latest returned HDWP)
EndDeferWindowPos
```

Both paths use:

```text
SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
```

`SWP_NOSIZE` enforces the translation boundary, `SWP_NOZORDER` prevents
z-order changes, and `SWP_NOACTIVATE` prevents activation. `SWP_NOOWNERZORDER`
is redundant when z-order is already suppressed. `SWP_NOSENDCHANGING` is not
used because it would suppress a message under study and change normal window
participation. Async, redraw suppression, show/hide, topmost, and foreground
flags are not used.

## 7. Preflight and native failure model

Every batch member is fully resolved, captured, bridged, range-checked, and
re-resolved before `BeginDeferWindowPos`. The complete set must also be
nonempty, duplicate-free, count-representable, and share a native parent.

The structured result distinguishes:

```text
Preflight
SingleApply
BeginBatch
DeferBatch
EndBatch
PostVerification
```

It reports process-wide monotonic operation batch ID, status, requested /
deferred / verified counts, native-apply attempted, native-outcome known,
diagnostic domain/code/API, and per-window requested/before/actual geometry,
DPI, monitor, size, and verification flags.

If `DeferWindowPos` returns null, the chain is abandoned and End is not called.
End and single-apply failures are not described as rollback-capable. After any
attempted native failure, all still-resolvable members are re-snapshotted. The
harness may later perform cleanup, but cleanup is not called rollback.

Native Begin/Defer/End failure injection and post-verification mismatch are
**NOT TESTED at runtime**; their structured branches are implemented, compiled,
and based on the documented contracts.

## 8. Harness and initial topology

`panebind-owned-window-harness.exe --self-test` creates four visually labeled,
independent top-level windows:

```text
A B
C D
```

The executable reuses the repository's audited PerMonitorV2 manifest resource.
It shows windows with no activation, captures positioning and DWM visible-frame
rectangles, and uses a four-member owned batch to place exact visible frames:

| Window | Initial visible rectangle | Initial positioning rectangle |
| --- | --- | --- |
| A | `[1198,683,1476,882]` | `[1187,683,1487,893]` |
| B | `[1476,683,1754,882]` | `[1465,683,1765,893]` |
| C | `[1198,882,1476,1081]` | `[1187,882,1487,1092]` |
| D | `[1476,882,1754,1081]` | `[1465,882,1765,1092]` |

The harness then builds the real R1-A `WindowAdjacencyGraph` with explicit
test tolerance `0`, verifies four side adjacencies and one four-node connected
component, constructs `TranslationSession(A)`, and consumes every returned
`PlannedTranslation::target_visible_rect`. It does not reimplement MovePlan
math.

## 9. Batch success and requested-versus-actual

The startup four-member batch result was:

```text
operation_batch_id = 1
requested = 4
deferred = 4
verified = 4
status = succeeded / post_verification
```

For final delta `(+120,+60)`:

| Window | Requested/actual visible | Requested/actual positioning |
| --- | --- | --- |
| A | `[1318,743,1596,942]` | `[1307,743,1607,953]` |
| B | `[1596,743,1874,942]` | `[1585,743,1885,953]` |
| C | `[1318,942,1596,1141]` | `[1307,942,1607,1152]` |
| D | `[1596,942,1874,1141]` | `[1585,942,1885,1152]` |

All successful receipts reported visible target verified, positioning target
verified, size preserved, and DPI/monitor stable. The four-edge adjacency
relations remained identical after every whole-topology translation.

## 10. Repeated and non-monotonic sequence

The scripted leader positions, all relative to the immutable R1-A session
origin, were:

```text
(+30,+20)
(+80,+50)
(+80,+50) repeated
(+40,+10) backtrack
(+120,+60)
```

Each leader request used `apply_one(SetWindowPos)`. Each R1-A follower plan
used one B/C/D deferred batch. Operation IDs 2 through 11 all reached verified
success. The repeated target remained unchanged, the backtrack reached the
correct initial-relative target, and the final rectangles equaled initial plus
`(+120,+60)` with no accumulated drift.

## 11. Invalid-member preflight

After destroying C, the harness submitted:

```text
B valid / C stale / D valid
```

Result:

```text
operation_batch_id = 12
status = stale_token
stage = preflight
requested = 3
deferred = 0
native_apply_attempted = false
B before == B after
D before == D after
```

This is the PaneBind prevalidation all-or-nothing guarantee. It is not a claim
about native transactional rollback.

## 12. Harness WndProc evidence

The WndProc records messages at entry, before `DefWindowProc` can synchronously
generate nested messages.

Observed for a nonzero leader move:

```text
A: WM_WINDOWPOSCHANGING
A: WM_WINDOWPOSCHANGED
A: WM_MOVE
WM_SIZE = 0
```

Observed for one nonzero follower batch:

```text
B CHANGING -> C CHANGING -> D CHANGING
-> B CHANGED -> B MOVE
-> C CHANGED -> C MOVE
-> D CHANGED -> D MOVE
```

Observed for the repeated same target:

```text
leader: A CHANGING only
followers: B CHANGING -> C CHANGING -> D CHANGING only
```

Post-reset message totals before final cleanup were:

| Window | CHANGING | CHANGED | MOVE | SIZE | NCDESTROY |
| --- | ---: | ---: | ---: | ---: | ---: |
| A | 5 | 4 | 4 | 0 | 0 |
| B | 5 | 4 | 4 | 0 | 0 |
| C | 6 | 5 | 4 | 0 | 1 |
| D | 5 | 4 | 4 | 0 | 0 |

C includes its destroy lifecycle. These cardinalities and cross-window order
are observed facts, not Win32 guarantees.

## 13. External Observer feedback

The final evidence command was:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\run-r1b-observer-evidence.ps1 `
  -BuildDirectory out/r1b-debug `
  -Configuration Debug `
  -ObserveSeconds 12 `
  -HarnessHoldSeconds 2
```

Final process exit codes were observer `0`, harness `0`. Local raw evidence is
under ignored `uat/r1b/`, prefix `20260825T094555450Z`.

Evidence quality:

```text
observer JSONL valid records = 695
observer sequence = continuous 1..695
hook registration = complete
initial census = complete
hook shutdown = complete
observer shutdown = complete
queue overflow/drop = none
PostThreadMessage failure = none
target field/identity/DPI errors = none
harness JSONL valid records = 314
harness self-test = PASS / failures 0
stderr files = empty
```

Filtering to `PaneBind.R1B.OwnedWindow`:

| Native event | A | B | C | D | Total |
| --- | ---: | ---: | ---: | ---: | ---: |
| `EVENT_SYSTEM_MOVESIZESTART` | 0 | 0 | 0 | 0 | 0 |
| `EVENT_OBJECT_LOCATIONCHANGE` | 5 | 5 | 5 | 5 | 20 |
| `EVENT_SYSTEM_MOVESIZEEND` | 0 | 0 | 0 | 0 | 0 |

The 20 location events cover startup placement plus four geometry-changing
script positions. The repeated same target emitted no location event. For each
nonzero follower batch, B/C/D location events were contiguous and shared one
native event timestamp in this run; no unrelated event interleaved in observer
sequences 674 through 693. This is not promoted to a contract.

Programmatic placement therefore produced geometry feedback without an
interactive START/END session. This differs from R0 human move/resize evidence.
It also refines AltSnap attribution: AltSnap can explicitly synthesize
START/END, so its valid R0 lifecycle evidence must not be attributed solely to
`SetWindowPos`.

## 14. Feedback-suppression research input

R1-B provides future R1-C with:

- process-wide operation batch ID;
- registry authority + logical token + generation identity;
- exact expected visible and positioning target per member;
- before and acknowledged actual geometry;
- DPI/monitor before and after;
- structured native stage/outcome; and
- external observer sequence/timestamp evidence.

The evidence rejects suppression based only on time, raw HWND, assumed
follower contiguity, or mandatory START/END. A likely R1-C contract must retain
an in-flight expected token/target set, acknowledge actual geometry, invalidate
on generation/topology change, and handle missing/repeated/interleaved events.
No suppression state machine or Glue loop was implemented in R1-B.

## 15. DPI and monitor environment

Final runtime facts:

```text
display = \\.\DISPLAY1
monitor rect = [0,0,3072,1920]
work area = [0,0,3072,1824]
DPI = 192
window DPI awareness = PerMonitorV2
```

All operations remained on this one display and at one DPI. Cross-monitor and
mixed-DPI operation behavior are **NOT TESTED**. Signed core tests and
FancyZones history do not convert these risks into a pass.

## 16. Build and automated tests

Environment:

- Visual Studio 18 2026;
- MSVC 19.50.35729.0;
- Windows SDK 10.0.26100.0; and
- bundled CMake 4.2.3.

The shell PATH did not contain CMake, so the actual Visual Studio bundled path
was invoked:

```powershell
$cmakeExe = 'D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ctestExe = 'D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'

& $cmakeExe -S . -B out/r1b-debug -G 'Visual Studio 18 2026' -A x64 -DBUILD_TESTING=ON
& $cmakeExe --build out/r1b-debug --config Debug --parallel
& $ctestExe --test-dir out/r1b-debug -C Debug --output-on-failure

& $cmakeExe -S . -B out/r1b-release -G 'Visual Studio 18 2026' -A x64 -DBUILD_TESTING=ON
& $cmakeExe --build out/r1b-release --config Release --parallel
& $ctestExe --test-dir out/r1b-release -C Release --output-on-failure
```

Final Debug and Release results:

```text
1/6 geometry                         PASS
2/6 core-model                       PASS
3/6 topology                         PASS
4/6 translation                      PASS
5/6 windows-text-encoding            PASS
6/6 windows-owned-operations-unit    PASS
TOTAL                                6/6 PASS
```

Both Debug and Release builds completed without compiler warnings. Both Debug
and Release harness self-tests returned `0` with `self_test_summary=PASS`.
The desktop-dependent harness is intentionally not registered as universal
CTest; it is an explicit `WINDOWS RUNTIME INTEGRATION` command.

## 17. Boundary audit

- `src/core/` contains no `windows.h`, HWND/HMONITOR/HDWP, Win32 event/message
  constant, or Windows API.
- Native calls are confined to `src/platform/windows/operations/` and the
  self-owned harness lifecycle.
- The harness does not enumerate windows, accept a raw HWND argument, or look
  up another process/window.
- No `SendInput`, `mouse_event`, `keybd_event`, remote thread/memory write,
  global input hook, DLL injection, or polling was added.
- The existing R0 observer code and semantics were not changed.
- Raw `uat/r1b/` and all `out/` build artifacts remain ignored and untracked.

```text
THIRD_PARTY_WINDOW_CONTROL = NO
GLOBAL_INPUT_CONTROL = NO
DLL_INJECTION = NO
HIGH_FREQUENCY_POLLING = NO
CORE_WIN32_ISOLATION = PASS
RAW_RUNTIME_LOGS_TRACKED = NO
```

## 18. Remaining risks and NOT TESTED items

- Mixed-DPI and multi-monitor/cross-monitor movement: **NOT TESTED**.
- Reliable numeric HWND recycling in a live runtime: **NOT TESTED**; logical
  stale/reissue/authority behavior is deterministic-unit-tested.
- Begin/Defer/End native failure injection and End partial-outcome behavior:
  **NOT TESTED**; no rollback is claimed.
- Window destruction exactly during a native End call: **NOT TESTED**.
- An application modifying `WM_WINDOWPOSCHANGING` and causing a post-verify
  mismatch: **NOT TESTED** in this cooperative owned WndProc.
- Monitor/work-area/DPI topology change during one batch: **NOT TESTED**.
- Cross-thread owned HWND operation: deliberately rejected, not a supported
  success scenario.
- Elevation/UIPI and third-party application constraints: deliberately outside
  this owned-only round and not tested.
- One-run follower event continuity/order is not guaranteed by Win32 and must
  not become R1-C suppression truth.

## 19. R1-C architecture questions only

1. What exact in-flight receipt identity should match feedback: operation ID,
   authority/token/generation, expected target, and which observer boundary?
2. When is a follower event acknowledged: first exact actual geometry, final
   exact geometry, or a bounded set of intermediate accepted snapshots?
3. How should suppression handle no-op requests that produce CHANGING but no
   WinEvent, repeated location events, missing events, or interleaving?
4. How does an End failure with unknown native outcome cancel or reconcile an
   in-flight expected set after re-snapshot?
5. Which topology-generation, destroy, DPI, monitor, minimize/maximize, and
   visible-frame changes invalidate a pending receipt?
6. How are user-driven leader events distinguished from PaneBind follower
   geometry without relying on START/END alone?
7. What evidence-backed expiry rule replaces fragile time-only suppression?
8. Which owner thread serializes observation, topology/session state,
   operations, receipts, and invalidation without hook-callback reentrancy?

No answer was implemented in R1-B.

## 20. Gate result

```text
R1B_PRIOR_ART_GATE = PASS
R1B_OPERATIONS_BASELINE = PASS
R1B_RUNTIME_GATE = PASS
THIRD_PARTY_WINDOW_CONTROL = NO
R1C = NOT STARTED
```
