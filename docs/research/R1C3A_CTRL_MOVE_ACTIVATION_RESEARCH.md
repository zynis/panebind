# R1-C3A Ctrl + Move Activation Research

Review date: 2026-09-09 (Asia/Shanghai).

## Prior-art reference gate

```text
R1C3A_PRIOR_ART_REFERENCE_GATE = PASS
EXTERNAL_CODE_COPIED = NO
EXTERNAL_CODE_ADAPTED = NO
```

This section records source and history inspection, not a PaneBind runtime
observation. It supports an independently designed, event-driven activation
boundary. Official platform contracts, focused empirical evidence, the final
design and tests must also pass before implementation is accepted. R1-C2B's
human-validated geometry, scheduling and suppression baseline remains the
starting point; no cadence tuning follows from this research alone.

| Project | Classification and exact inspected revision | License decision |
| --- | --- | --- |
| AltSnap | Mature maintained Windows movement reference; `5c86416ad21e4b72844a998a746bd3bb0bee5f5d` | `hooks.c` license header and `License.txt`: GPL-3.0-or-later; reference only |
| AltDrag | Mature historical comparison; `e2740d605b0336a3b391fec26794718864b19521` | `hooks.c` license header and `LICENSE`: GPL-3.0-or-later; reference only |
| PowerToys / FancyZones | Mature production reference; `19c4d805321db86f3634e6968e14dbf25cbba14a` | Root `LICENSE`: MIT; reference only in this round |

These are fixed inspected revisions, not claims about the newest upstream
release. No GPL algorithm, control flow or implementation is copied,
translated, mechanically rewritten or adapted. No PowerToys code is reused.
Full provenance is recorded in [SOURCE_PROVENANCE.md](SOURCE_PROVENANCE.md).

## AltSnap and AltDrag: modifier intent and move lifetime

**SOURCE FACT.** AltSnap's input model owns a low-level keyboard/mouse gesture,
not merely an observed native title-bar move. Its key handler tracks configured
activation keys and optional combinations; `IsHotkeyDown` also reads async
high bits, and `IsCtrlDown` combines tracked state with a current Ctrl high bit.
This is useful evidence that stored key state and current state are distinct
facts. It does not establish a requirement for PaneBind to track the global
keyboard stream. See pinned
[modifier helpers](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L1913-L1947)
and [key handling](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L2754-L2868).

**HISTORY FACT.** Modifier state has caused real upstream failures:

- [AltSnap PR #689](https://github.com/RamonUnch/AltSnap/pull/689),
  [commit `ce72d731`](https://github.com/RamonUnch/AltSnap/commit/ce72d731563eda654ee2efbd336aa2c2e48525d3),
  added a current high-bit cross-check to address stuck Ctrl behavior.
- [AltSnap PR #695](https://github.com/RamonUnch/AltSnap/pull/695),
  commits [`84249cc3`](https://github.com/RamonUnch/AltSnap/commit/84249cc38d6aa159ee95a945819969cc98100a57)
  and [`f4c7ab3c`](https://github.com/RamonUnch/AltSnap/commit/f4c7ab3c1a1d02451f1cde577a2f92e617ae5f55),
  corrected Ctrl-repeat monitor locking and ensured Ctrl-up processing was
  not hidden behind another key branch. These are dynamic-input-state bugs,
  not proof that a START-latched activation needs repeated Ctrl reads.
- [AltSnap PR #537](https://github.com/RamonUnch/AltSnap/pull/537),
  [commit `6890cabf`](https://github.com/RamonUnch/AltSnap/commit/6890cabfe93f5526491d146626f4874727ace8fc),
  distinguished simulated Ctrl associated with AltGr in its hook path.
  A Ctrl high-bit sample alone is not physical-key provenance. AltGr,
  remappers and synthetic-input attribution remain outside this round's
  accepted product claims; this does not authorize collecting scan codes or
  adding a keyboard hook.
- [AltDrag commit `f614a2b6`](https://github.com/stefansundin/altdrag/commit/f614a2b6a1c89f804c79be19b86185dd5cfb158b)
  removed a movement-time async Shift cross-check that broke mid-drag Shift
  behavior on Windows 8.1/10. The
  [v1.1 release notes](https://github.com/stefansundin/altdrag/releases/tag/v1.1)
  report that fix. Current-state rereading is not universally interchangeable
  with an input fact captured at a different boundary.

**SOURCE FACT.** AltSnap explicitly emits compatibility move lifecycle
notifications and messages; its finish path flushes/finalizes movement before
resetting action state. AltDrag's historical movement path sends enter/exit
messages, has an application exception, and includes synthetic input and an
optional injected `HookWindows` design. These mechanisms are rejected for
PaneBind. See AltSnap's
[lifecycle helper](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L576-L588),
[finish path](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L5245-L5309)
and AltDrag's
[historical enter/exit path](https://github.com/stefansundin/altdrag/blob/e2740d605b0336a3b391fec26794718864b19521/hooks.c#L1841-L1895).

**HISTORY FACT.** [AltSnap issue #572](https://github.com/RamonUnch/AltSnap/issues/572)
reported that AltSnap movement/resizing began activating FancyZones after a
release change. The report links the interaction to unexpected zoning and
modifier-release order. PaneBind must treat externally altered geometry or
lifecycle as observed state, not disable other managers or synthesize its own
START/END to compete with them.

## AltSnap and AltDrag: cadence and coalescing

**SOURCE FACT.** At the inspected AltSnap revision:

- `MoveRate` and `ResizeRate` throttle movement work using a count of incoming
  mouse updates in `LowLevelMouseProc`; the implementation is not a promised
  frame rate or a distance-based accuracy contract.
- `RezTimer=1` accepts work when the event timestamp changes; mode 3 combines
  that opportunity with the configured update count. Auto modes select an
  option using display frequency. This uses mouse-event timestamps; it is
  not evidence of a QPC latency contract.
- `RefreshRate` inserts a configured delay in placement-related work, and
  nonzero resolved `RezTimer` disables that setting. Those sleeps and mouse
  hooks are not proposed for PaneBind.
- The worker coalesces consecutive queued movement messages into the latest
  coordinates, stopping at a different work-message kind. Placement need not
  execute once per raw mouse event.

Source anchors:
[mouse cadence](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L5442-L5481),
[configuration](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L6677-L6697),
[worker](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L695-L735),
[placement delay](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L875).
The inspected [PR #609](https://github.com/RamonUnch/AltSnap/pull/609) and local
[commit `7f4afe59`](https://github.com/RamonUnch/AltSnap/commit/7f4afe59076b70980f71af202f63609ca3ac5745)
record moving mouse work to a worker thread.

AltDrag's older
[mouse update counter](https://github.com/stefansundin/altdrag/blob/e2740d605b0336a3b391fec26794718864b19521/hooks.c#L1113-L1128)
and [movement setup](https://github.com/stefansundin/altdrag/blob/e2740d605b0336a3b391fec26794718864b19521/hooks.c#L1841-L1856)
also show rate-limited work, with a 100 ms update timer in selected modes.
That timer is reference-only and is not allowed as PaneBind's event source.

**PANEBIND INFERENCE.** Raw ingress count, live sample count and native apply
count measure different stages. A lower apply/raw ratio alone proves neither
smoothness nor failure. R1-C3A should preserve R1-C2B's quantum policy, record
bounded monotonic stage timings, and leave cadence decisions to a later
evidence-led round. The source does not justify treating mouse deltas as the
authorized Leader's actual geometry or skipping lifecycle/invalidity edges.
The inspected [touching-window path](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L841-L979)
is gated on resize action and its StickyResize/Shift policy, enumerates touching
windows and can batch placements. It is not evidence of an equivalent
START-latched two-window Glue Move product. Its multi-window placement details
remain reference-only and do not authorize Glue Resize or extra followers.

## FancyZones: input state, lifetime and historical failures

**SOURCE FACT.** FancyZones uses out-of-context move lifecycle ingress and
posts work to its owner window. It subscribes to location changes during the
move interval. `MoveSizeStart` establishes the current snapper; updates change
zone highlights; END finishes zoning and disables drag state. Destroy has an
explicit abort path that avoids snapping a dead window. See pinned
[hook wrapper](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZones/FancyZonesApp.cpp#L96-L181),
[lifetime](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/FancyZones.cpp#L478-L530)
and [owner dispatch](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/FancyZones.cpp#L961-L997).

**SOURCE FACT.** Its input architecture is broader than this round:
`KeyboardInput.cpp` registers keyboard Raw Input with `RIDEV_INPUTSINK`;
`OnKeyboardInput` updates Shift state; `DraggingState` permits dynamic Shift
and mouse toggles. Multi-zone Ctrl uses `KeyState<VK_LCONTROL,VK_RCONTROL>`,
which initializes from async high bits and then uses a low-level key hook.
Its main key handler also supports shortcuts and suppression. These are
inspected implementation facts, not requirements to duplicate them.
Sources:
[KeyboardInput](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/KeyboardInput.cpp),
[DraggingState](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/DraggingState.cpp),
[KeyState](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/KeyState.h),
[GenericKeyHook](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/GenericKeyHook.h).

**HISTORY FACT.** [PR #49985](https://github.com/microsoft/PowerToys/pull/49985),
[commit `d68980a8`](https://github.com/microsoft/PowerToys/commit/d68980a81bb8de144bdec998a114e948bf68c563),
reports two defects exposed by upstream UI tests: swallowed Shift input did
not update the module's own drag state, and a mode transition erased the first
computed zone highlight. The inspected pinned code records Shift before
suppression and switches mode before highlight calculation. It is evidence
for ordering and observable-state tests, not a PaneBind empirical result or
a reason to add input suppression.

**HISTORY FACT.** [PR #48569](https://github.com/microsoft/PowerToys/pull/48569),
[commit `dd26d865`](https://github.com/microsoft/PowerToys/commit/dd26d86580168d2e368701f7b0c4d629dc9cd9ac),
addresses a destroyed dragged window leaving drag state active and stealing
later keys. The lifetime must end on invalidation as well as the normal END.
The unrelated topology report
[#49016](https://github.com/microsoft/PowerToys/issues/49016) was also inspected;
it concerns stale monitor/layout identity after dock/sleep and supplies no
modifier-sampling or latency acceptance evidence.

**PANEBIND INFERENCE.** R1-C3A has narrower authorized semantics: exact
already-authorized Leader START plus the Ctrl state observed at callback
delivery creates at most one activation. A later owner sample is diagnostic;
LOCATION and later Ctrl changes cannot establish authority. No-Ctrl,
wrong-window, repeated START, stale generation and invalid geometry paths
need deterministic evidence. FancyZones' dynamic modifier UI and END-time
zone placement do not replace R1-C2B's progressive Follower operation ledger.

## Inspection method and limits

The review read existing local fixed-HEAD checkouts, source/license files and
AltSnap/AltDrag path history and commit diffs. GitHub issue/PR pages and
immutable history pages were independently inspected. A FancyZones historical
diff requested a missing promisor object and its ordinary Git fetch failed
with a GitHub HTTPS/443 connection error; that source-repository synchronization
was stopped. The already available pinned source and license remained usable;
the history conclusions above use the inspected public PR/commit pages.
No API payload was written into Git objects, no refs were moved to simulate a
fetch, and no PaneBind remote synchronization was substituted.

No upstream program was installed or run, and no third-party window was
manipulated for this source review. Physical Ctrl-before-drag, smoothness,
AltGr/remapping, elevated input boundaries and coexistence behavior are not
claimed as PaneBind observations by this section.

## AquaGlue: documented behavior, not implementation knowledge

**OFFICIALLY DOCUMENTED BEHAVIOR.** Nurgo describes holding Ctrl while moving
or resizing adjacent windows, including moving an adjacent group together.
The official v1.10.0 announcement dates introduction to 2014-12-02.
Sources: [AquaGlue configuration](https://help.nurgo-software.com/article/97-aquasnap-configuration-aquaglue),
[product](https://www.nurgo-software.com/products/aquasnap),
[v1.10.0](https://www.nurgo-software.com/company/news/13-aquasnap/44-aquasnap-v1100-released).

**OUR IMPLEMENTATION HYPOTHESIS / DECISION.** These pages do not reveal its
input API, timing boundary, topology algorithm or release-detachment policy.
PaneBind independently chooses callback-delivery START-latching for one
already-authorized pair. Ctrl+Resize and larger groups are not implemented.
No binary was installed/reverse engineered; no proprietary code was reused.

## Microsoft input contracts and selected boundary

Official pages below were read on 2026-09-09; live revisions are not immutable
source SHAs. No Microsoft implementation samples were copied.

| API / signal | Contract and consequence for this round |
| --- | --- |
| [GetAsyncKeyState](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getasynckeystate) | High bit reports current-down at call time. Low bit is unreliable and never used. Aggregate and left/right Ctrl codes exist. Zero also represents failure, including inactive desktop or UIPI/access restrictions: zero cannot activate and does not prove physical key-up. PaneBind need not own keyboard focus or consume keyboard messages. No registration/global keyboard stream is needed. |
| [GetKeyState](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getkeystate) | State advances with the caller's keyboard message processing; not a substitute for another application's current native-drag modifier state. |
| [GetKeyboardState](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getkeyboardstate) | Reads 256 virtual keys from thread-associated state; another thread consuming messages does not update it. Unnecessary collection; no AttachThreadInput workaround. |
| [SetWinEventHook](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwineventhook) | OUTOFCONTEXT queues asynchronously and delivers on the installing thread, which needs a message loop. Preserve the reentrancy guard. The authority is callback-delivery observation, not physical input-generation time. |
| [WinEvent constants](https://learn.microsoft.com/en-us/windows/win32/winauto/event-constants) | MOVESIZESTART/END cover move and resize; LOCATION covers position, shape or size. Geometry classification, not event type, authorizes translation. No new hook ranges. |
| [WH_KEYBOARD_LL / LowLevelKeyboardProc](https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelkeyboardproc) | Requires an installing-thread message loop and handles a keyboard stream. It runs before the changed key's async state updates; this warning is about keyboard-hook timing, not a blanket WinEvent callback ban. Timeout can silently remove a hook. Not installed. |
| [Raw Input](https://learn.microsoft.com/en-us/windows/win32/inputdev/about-raw-input), [RegisterRawInputDevices](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerrawinputdevices) | Device registration and WM_INPUT processing; background collection needs INPUTSINK. One target per device class/process can interfere with host registration. It is a stream, not a one-call held-state query. Not used. |
| [RegisterHotKey](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerhotkey) | System-wide key-combination command delivered as WM_HOTKEY; registration conflicts and message-loop needs. It does not naturally correlate held Ctrl to an exact native Leader START. Not used. |

Scope remains same-user/session/integrity, active desktop, non-elevated
Explorer. UIPI/elevated PaneBind, UIAccess, AppContainer, alternate desktops,
RDP/remappers/AltGr are NOT TESTED. No focus stealing, suppression or synthetic
input. Three sequential Ctrl reads are not an atomic snapshot; aggregate
VK_CONTROL alone is authority, left/right are diagnostic. No API here proves
physical-key provenance. The owner sample never overrides the callback fact.

## Focused empirical boundary probe

AUTOMATED OBSERVED, not human Explorer UAT: an independently written ignored
probe `out/r1c3a-input-probe/` creates its own never-shown STATIC window,
installs OUTOFCONTEXT limited to its PID/thread, emits owned NotifyWinEvent
START/END and pumps with a bounded message wait. It never sends input or
shows/focuses/manipulates a third-party target. Callback performs only fixed
Ctrl high-bit reads on START and QPC capture.

Commands (Visual Studio 18 Community CMake executable):

```text
cmake -S out/r1c3a-input-probe -B out/r1c3a-input-probe/build -G "Visual Studio 18 2026" -A x64
cmake --build out/r1c3a-input-probe/build --config Debug
out/r1c3a-input-probe/build/Debug/input-probe.exe
```

Exit 0 PASS: two receipts on installing thread 23908; unhook true;
QPC frequency 10000000; START 6148333062434 <= END 6148333066067;
aggregate/left/right Ctrl false. MSVC 19.50.35729.0, SDK 10.0.26100.0,
Windows 10.0.26200, x64. This supports callback feasibility and same-thread
monotonic instrumentation, not physical Ctrl-down/cross-process Explorer
accuracy or realtime Glue acceptance. Those remain human UAT obligations.

## Independent PaneBind design and implementation gate

```text
R1C3A_PRIOR_ART_GATE = PASS
CTRL_STATE_SAMPLING_DESIGN = START_CALLBACK_HIGH_BIT
INPUT_RESEARCH_ESCALATION_REQUIRED = NO
REAL_EXPLORER_CTRL_MOVE = NOT TESTED
```

Source/history, platform contracts, focused probe and the following design/test
obligations were established before implementing the interaction path. No
technical blocker was found for the limited callback-delivery semantics.

```text
nonce/baseline/target confirmation -> exact pair fixture authority
exact Leader START + callback Ctrl -> one activation generation
existing ledger + live validation  -> per-operation native permit
```

The new named Ctrl-fixture preparation binds only target-consented sessions;
it does not manufacture Console Glue prompt/confirmation. Setup/restore use
fixture authority, active Follower operations additionally require matching
activation. Default C2B keeps its actual Console confirmation path.
Core, native permission scope, eight-message budget and coalescing stay intact.

Controller freezes pair ID/generation, both logical IDs, capability and target
consent generations, and Glue session generation. Every START evaluation has
an attempt generation, source receipt and callback/owner samples. Callback
DOWN/owner UP accepts; callback UP/owner DOWN cannot activate. No LOCATION
resamples Ctrl or creates authority. Ctrl-UP stays inactive through END;
setup/restore remain separately reported, not active drag writes. Follower
START cannot activate; unrelated windows filter before queue. Duplicate START,
stale authority, invalid timing/sampling and overflow fail closed.

One activation stays latched through that move lifecycle. UAT requires Ctrl
before mouse press through mouse release. Mid-drag release does not detach;
that product semantic is NOT SUPPORTED / NOT TESTED. ResizeOrMixed, maximize,
monitor/DPI drift and unexpected Follower state retain existing abort paths;
PaneBind does not compete with Snap/managers. Translation, progressive quanta,
pending-before-native, feedback matching and END reconciliation are reused.
Fixture automatic restore is NOT product Glue semantics. No selection UX,
3+ windows or smoothness tuning is added.

## Timing model and deterministic evidence obligations

[QPC](https://learn.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancecounter),
[QPF](https://learn.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancefrequency)
and [Microsoft timing guidance](https://learn.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps)
support a monotonic same-machine domain independent of wall clock. Cache
frequency, subtract integer ticks before conversion. Same-owner measurements
avoid cross-thread +/-1 tick ordering ambiguity. These are CPU/API/observation
intervals, not compositor-present/hardware-input latency or FPS.

Opt-in C3A records accepted receipt QPC; owner drain; live sample start/end;
behavior decision; native start/return; postverify; feedback acknowledgement.
Native error must be captured before timing calls. Preserve receipt sequence,
native timestamp and sample generation. Callback adds one QPC per accepted
receipt and three high-bit reads on Leader START only; never COM, DWM, JSON,
operations, allocations or blocking. Default C2B captures neither Ctrl nor QPC.

Bounds: ring 512, pending 64, receipts/quanta/trace 4096, operations 512,
activation attempts 8. Overflow is diagnostic/fail-closed, not silent success.
Runner must correlate active operations to activation/source/quantum and
validate finite nonnegative monotonic durations. Report min/median/p95/max
apply intervals and median/p95/max receipt-to-drain, drain-to-native,
native duration, native-to-postverify and receipt-to-postverify. Insufficient
samples are labeled. There is no timing SLA or ratio gate.

Tests: callback/owner DOWN/UP cross-product; left/right/both/neither; low-bit
rejection; wrong window/generation/stale authority/duplicate START; plain and
late Ctrl through many LOCATION/END with zero active native writes; one
activation across progression; ResizeOrMixed abort; exact feedback; attempt
overflow; malformed/missing/nonmonotonic timing and percentile fixtures.
Full Debug/Release CTest, owned/companion self-tests and old/new runner fixtures
are required before handing off positive Debug human Ctrl+Move UAT.
