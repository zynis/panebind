# R1-C3A — Ctrl + Move Activation Implementation Report

Report date: 2026-09-09, Asia/Shanghai. This is an automatic implementation
handoff, **not** a human validation seal or product release.

## 1. Baseline, Git and acceptance state

```text
Starting main: 8f9921b9550cad1ecee835dbb2193cc6728300f1
Branch: codex/r1c3a-ctrl-move-activation
Runtime source commit: e74058b127327ded0996052523c17eb628f12d35
Implementation + evidence-runner SHA: bf8be2e47f956d3bc16d2c5cb32f9003a48035f7
Runtime src tree: 0efe3950af57d4ad8b68cd902e14927f58b41e58

R1C3A_GIT_TRANSPORT = PASS
R1C3A_PRIOR_ART_GATE = PASS
R1C3A_CTRL_SAMPLING_GATE = PASS
R1C3A_ACTIVATION_AUTHORITY_GATE = PASS
R1C3A_INTERACTION_IMPLEMENTATION_GATE = PASS
R1C3A_TIMING_INSTRUMENTATION_GATE = PASS
R1C3A_IMPLEMENTATION_READY = YES
R1C3A_DEBUG_INTERACTIVE_UAT = REQUIRED
R1C3A_RUNTIME_GATE = PENDING_UAT
```

Initial `git status`, branch, HEAD, ordinary `git fetch origin`, remote-tracking
main SHA and divergence all matched the requested baseline: clean main, 0/0.
No Git HTTP/TLS/proxy configuration was changed. Before this documentation
commit, another ordinary fetch succeeded: origin/main still `8f9921b...`,
branch 0 behind / 3 ahead. The prior transient transport interruption was not
a technical Ctrl-design failure and did not downgrade the reassessed gates.

Commits:

- `3c6928b`: `docs: research ctrl move activation semantics`
- `e74058b`: `feat: add explorer ctrl move activation boundary`
- `bf8be2e`: `test: validate ctrl activation and timing evidence`
- This containing documentation commit: `docs: record r1c3a debug uat handoff`

The final containing commit SHA, standard push outcome, clean status and
upstream divergence are recorded in the user-facing terminal handoff after
commit/push, not predicted here. The final documentation-only descendant has
the same runtime and runner implementation as the pinned SHAs above. No PR,
main merge, tag or release is authorized in this handoff.

## 2. Research and independent decisions

Full sources/API comparison/probe are in
[research](../research/R1C3A_CTRL_MOVE_ACTIVATION_RESEARCH.md) and
[provenance](../research/SOURCE_PROVENANCE.md).

| Inspected source | Revision / finding |
| --- | --- |
| AltSnap | `5c86416ad21e4b72844a998a746bd3bb0bee5f5d`; GPL-3.0-or-later reference only; modifier tracking versus current high bit, Ctrl/AltGr bug history, event cadence and coalescing |
| AltDrag | `e2740d605b0336a3b391fec26794718864b19521`; GPL-3.0-or-later reference only; historical modifier/lifecycle limitations and rate-limited work |
| PowerToys/FancyZones | `19c4d805321db86f3634e6968e14dbf25cbba14a`; MIT, reference only; owner-thread lifecycle, dynamic Shift/Raw Input/hook architecture, swallowed modifier and destroyed-target history |
| AquaSnap/AquaGlue | Official product/help and 2014-12-02 v1.10.0 announcement: Ctrl+Move/Resize of adjacent windows; closed-source internals and release policy not inferred |
| Microsoft Learn | Read 2026-09-09: async/thread-queue key state, WinEvent delivery, low-level hook, Raw Input, hotkey and QPC/QPF contracts |

No external code was copied, translated, adapted or reused. Mature tools can
coalesce input and place windows less often than raw event receipt; that does
not supply a smoothness SLA or justify tuning this round's frozen cadence.
FancyZones' broader dynamic modifier UI does not require PaneBind to duplicate
its global input tracking. AquaGlue supplies documented UX, not implementation
knowledge. The existing local pinned FancyZones source/license and independently
read history pages were used after its optional promisor-history fetch failed;
no Git object/ref reconstruction occurred.

GetAsyncKeyState was chosen: only the high bit at delivered exact Leader START
is authoritative. Its low bit is unreliable; zero can mean UP or an input
desktop/UIPI failure, and never activates. GetKeyState/GetKeyboardState are
thread-message-queue-relative. RegisterHotKey is a command mechanism; keyboard
hooks and Raw Input would unnecessarily introduce stream tracking. No focus
ownership, AttachThreadInput or injected input is used.

The owned, never-shown NotifyWinEvent probe passed with two same-owner START/END
receipts, three Ctrl high-bit reads only at START, QPC frequency 10000000,
monotonic timestamps and clean unhook. Observed Ctrl was false. It establishes
callback feasibility, **not** physical Ctrl-down or real Explorer acceptance.

## 3. Interaction, authority and safety matrix

| Requirement | Implemented behavior / evidence |
| --- | --- |
| Exact authority edge | Exact preauthorized Leader START + callback aggregate Ctrl-down; one activation generation bound to pair, target capability/consent generations, Glue session and receipt |
| Callback-time semantic | State observed when START callback was delivered, not the physical drag-start instant or physical-key provenance |
| Owner-time sample | One additional START sample, diagnostic only; callback DOWN / owner UP accepts, reversed values reject |
| Left/right Ctrl | Aggregate VK_CONTROL authority; VK_LCONTROL/VK_RCONTROL diagnostics; three reads are not atomic |
| Ctrl before START | Required; one accepted attempt enters existing Core path |
| Ctrl after START / no Ctrl | No mid-drag attach; no events sent to Core; zero active Follower requests/writes; sampled Follower checked against fixture layout through END |
| Ctrl release | Activation latched until the move ends; no dynamic detach. Human mid-drag release is NOT TESTED / NOT SUPPORTED product semantics |
| Follower / other window | Follower START cannot activate; unrelated-window ingress filters before queue; stale/double START/generation mismatch fails closed |
| Ctrl+Resize | Existing ResizeOrMixed classification aborts before an unsafe Follower translation; no Glue Resize |
| Snap / state / monitor / DPI | Existing fail-closed eligibility/classification retained; no competition with Snap or disabling other managers |
| Target authority | Existing nonce, baseline exclusion, unique new Explorer, human ENTER, live eligibility and capability generation preserved |
| Fixture authority | Target prompt explicitly explains pure-translation setup/restore; new named preparation, no fabricated Y+ENTER Glue confirmation |
| Native authority | Existing private permit/prepare/live validation, with matching activation checked at active operation entry and pending-before-native registration |
| UAT restore | Unhook then independently restore both test windows; this is fixture safety, not release behavior |

Implementation is opt-in and additive. Core, original native operation flags,
geometry/translation model, eight-message pump, coalescing, feedback suppression
and END reconciliation remain unchanged. Default R1-C2B harness keeps its
Console activation chain and old JSON schema and does not query Ctrl/QPC.
R1-C2A's one-shot route is unchanged; only its private Glue bridge accepts a
nullable timing output for C3A. No arbitrary HWND factory or new operation type
was introduced.

## 4. Callback audit and timing evidence

Before: owner/reentrancy/poison checks, fixed target/object/event filtering,
bounded receipt enqueue and edge-triggered owner notification.
After in C3A only: one QPC read per accepted receipt, plus three high-bit reads
on exact Leader START. No COM, Shell inventory, DWM snapshot, behavior,
SetWindowPos, JSON, blocking or heap-heavy work was added to the callback.
Owner adds one Ctrl sample at START, never at LOCATION or a timer.

Timestamps: callback receipt; drain start; live sample start/complete; first
quantum policy decision; actual per-operation Core decision; native API
start/return; postverify completion; completed feedback ACK/duplicate suppression.
Activation completion time is stamped after policy evaluation. Native error is
saved before timing calls. A deferred ACK is correlated to its provisional
receipt and completed operation, not assigned a fictitious fresh receipt.
Terminal discarded receipts remain bounded evidence with drain time but no
fake live sample/decision. The timing clock is QPC, not wall clock.

Bounds: 512 ingress slots, 64 pending, 4096 receipts/quanta/trace, 512 operation
records and 8 activation attempts. Overflow explicitly prevents acceptance;
there is no silent drop. Independent synthetic fixtures check overflow and
malformed evidence. Real Explorer queue/latency outcomes are still NOT TESTED
for C3A.

Runner prints min/median/p95/max for apply intervals and all requested latency
stages. Median is ordinary median; p95 is nearest-rank. Fewer than two samples
are labeled insufficient (one activation time is still reported numerically).
There is no P95<16ms, FPS, apply/raw ratio or subjective smoothness gate.
Metrics describe instrumented CPU-side processing, not actual display-present
latency. Actual human smoothness/latency numbers await UAT.

## 5. Automated verification

Windows 10.0.26200, x64; Visual Studio 18 2026 Community; MSVC
19.50.35729.0 (toolset directory 14.50.35717); Windows SDK 10.0.26100.0;
CMake 4.2.3-msvc3; Windows PowerShell runner.

```powershell
$cmakeExe = 'D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ctestExe = 'D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
& $cmakeExe -S . -B out/r1c3a-debug -G 'Visual Studio 18 2026' -A x64
& $cmakeExe -S . -B out/r1c3a-release -G 'Visual Studio 18 2026' -A x64
& $cmakeExe --build out/r1c3a-debug --config Debug --parallel
& $ctestExe --test-dir out/r1c3a-debug -C Debug --output-on-failure
& $cmakeExe --build out/r1c3a-release --config Release --parallel
& $ctestExe --test-dir out/r1c3a-release -C Release --output-on-failure
.\out\r1c3a-debug\src\platform\windows\Debug\panebind-owned-window-harness.exe --self-test
.\out\r1c3a-release\src\platform\windows\Release\panebind-owned-window-harness.exe --self-test
.\out\r1c3a-debug\src\platform\windows\Debug\panebind-companion-harness.exe --self-test
.\out\r1c3a-release\src\platform\windows\Release\panebind-companion-harness.exe --self-test
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c2b-evidence-runner.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c3a-evidence-runner.ps1
```

| Gate | Actual result |
| --- | --- |
| Debug configure/build / CTest | PASS / 12 of 12 PASS |
| Release configure/build / CTest | PASS / 12 of 12 PASS |
| Owned Debug / Release self-test | PASS / PASS, failures=0 |
| Companion Debug / Release self-test | PASS / PASS, failures=0 |
| R1-C2A Explorer tests | PASS both configurations |
| R1-C2B Core/event-source/session tests | PASS both configurations |
| New activation unit | PASS both; low-bit rejection, modifier cross-product, wrong/stale/double START, operation generation, timing ordering and bounded overflow |
| New owned interaction fixture | PASS: no-Ctrl/late-Ctrl/Resize paths zero requests and actual owned native writes; positive 12 progressive applies with 12 exact suppressed feedback events |
| New event-source fixtures | PASS: default opt-out, START-only Ctrl, callback facts retained, missing/non-atomic samples unmodified, bounded overflow |
| Existing runner fixtures | 26 PASS; default C2B contract retained |
| New runner fixtures | 61 PASS including statistical assertions; synthetic fixtures, not human evidence |
| New/old harness `--help` | PASS; correct separate executable names |
| Diff hygiene / Core isolation | PASS; no forbidden Core dependencies, Core tree unchanged |

An initial Debug unreachable-code warning in the profile prompt was fixed with
an explicit constexpr else branch; final rebuild emitted no such warning.
Independent review found the first-policy versus per-operation timing ambiguity;
the latter was added and tested, rather than relabeling the first timestamp.
No R1-C2B regression failure was observed. All raw UAT remains ignored; no
tracked `uat/` files were added. No real Explorer UAT was run by Codex.

## 6. Human handoff and remaining boundaries

Use the exact one-command Debug run and simple human actions in
[R1C3A_UAT_HANDOFF.md](R1C3A_UAT_HANDOFF.md): new Leader, new Follower,
readiness FIT, wait for Ctrl prompt, hold Ctrl, drag Leader about one second,
release mouse, release Ctrl, await restore/evidence. No Y Glue confirmation.
Report `HUMAN_SMOOTHNESS_OBSERVATION` separately; severe stutter becomes a
priority research input for a later R1-C3B, not an unmeasured PASS here.

NOT TESTED: real Debug Ctrl+Move, Release Ctrl+Move, real no-Ctrl/late-Ctrl,
mid-drag Ctrl release, AltGr/remappers/physical input attribution, UIAccess,
elevation/UIPI failure behavior, alternate desktops, RDP, mixed-DPI/cross-monitor
movement, Snap/other-manager coexistence, display latency and subjective
smoothness, long-duration/resource goals. No extra app eligibility, product
window-selection UX, persistent groups or 3+ real windows was implemented.
The inherited R1-C2B empirical limits also remain: hung/destroyed Explorer
during apply, HWND/PID reuse, application-adjusted WINDOWPOS, actual timeout,
queue/hook/native/postverify failure, late delivery after destruction, vertical
fixture fallback/unusual work areas, allocation failure, long-session capacity
exhaustion and broader live feedback mixtures. Automated fixtures do not relabel
those as human observations.

Future architecture questions only: evidence-led input wake/cadence choices;
modifier release detachment; relation selection UX; larger components and one
full follower target set followed by a batched DeferWindowPos transaction.
These are **NOT IMPLEMENTED THIS ROUND**. Mouse deltas must not replace actual
Leader visible geometry. R1-C3B has not started.

```text
CODE_CHANGES = YES
R0_OBSERVER_SEMANTICS_CHANGED = NO
R0_REVALIDATION_REQUIRED = NO
R1C2A_REVALIDATION_REQUIRED = NO
R1C2B_REVALIDATION_REQUIRED = NO
WH_KEYBOARD_LL_USED = NO
RAW_INPUT_USED = NO
GLOBAL_HOTKEY_USED = NO
HIGH_FREQUENCY_INPUT_POLLING = NO
KEYBOARD_CONTENT_COLLECTION = NO
USER_PREEXISTING_WINDOWS_TOUCHED = NO
OTHER_THIRD_PARTY_CONTROL = NO
R1C3B = NOT STARTED
```
