# R1-C4B Fix 1 modal-loop authority gate

2026-09-16. Fix 1 and Amendment 001 base:
`f53de7952816bf90550923c0edcd8d9784b247b3`.

## Evidence before design

The immutable local human log is CASE B, not CASE A: source positioning and
visible rectangles both differ from the requested geometry despite native
success. See [forensics](../reports/R1C4B_FIX1_FORENSICS.md). The old full
postverify capture includes capability/COM validation; it is not an atomic
sample at SetWindowPos return. The log contains neither T1 direct geometry nor
a subsequent same-gesture receipt/END. It cannot distinguish app adjustment,
concurrent native movement or modal-loop reassertion. **No timing fix approved.**

## Prior art and history actually inspected

- Mature AltSnap, GPL-3.0-or-later, reference only, local pin
  `5c86416ad21e4b72844a998a746bd3bb0bee5f5d`: License.txt, hooks.c
  LetWindowKickBack / MoveResizeWindowNow_, commit
  `df25d36c6369bb13aa02ec83974e625fc7922c35` and
  [PR 739](https://github.com/RamonUnch/AltSnap/pull/739).
  The refinement distinguishes a timeout API's result from the recipient's
  WM_SIZING return value. Its own movement path and synthesized notifications
  are **not** proof of authority over Explorer's ongoing native drag loop.
  No hooks, timeout/sleep logic, message synthesis or implementation copied.
- Mature Microsoft PowerToys/FancyZones, MIT, reference only, pinned
  [WindowMouseSnap.cpp](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/WindowMouseSnap.cpp)
  and LICENSE; [PR 48569](https://github.com/microsoft/PowerToys/pull/48569),
  merge `dd26d86580168d2e368701f7b0c4d629dc9cd9ac`.
  Destruction abort and ordinary completion are different; zone interaction
  is not proof that an external live correction updates a modal drag RECT.
  No source reused/adapted; no new code attribution obligation.

## Official contract (live documents read 2026-09-16)

| Question | Evidence / bounded conclusion |
| --- | --- |
| A: drag rectangle authority | [WM_MOVING](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-moving) and [WM_SIZING](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-sizing) give the receiving window procedure the screen-coordinate drag RECT through lParam. This is the documented interaction adjustment point, not a claim about undocumented internal state. |
| B: official adjustment | The receiving application changes that RECT and returns TRUE. The probe deliberately does **not** do this, because it measures an independent SetWindowPos correction. |
| C: external placement equivalent? | [SetWindowPos](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowpos) documents size/position/z-order and flags, not replacement of the ongoing modal-loop drag RECT. No equivalence or persistence guarantee found. [WM_WINDOWPOSCHANGING](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-windowposchanging) separately permits app adjustment of WINDOWPOS; BOOL success alone cannot establish exact geometry. |
| D: synchronous DWM? | [GetWindowRect](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowrect) distinguishes DPI-virtualized positioning/invisible borders from visible frame bounds. [DwmGetWindowAttribute](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/nf-dwmapi-dwmgetwindowattribute) and [DWMWA_EXTENDED_FRAME_BOUNDS](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwmwindowattribute) describe querying current attributes, but no synchronous ordering guarantee with SetWindowPos was found. |

`NO SYNCHRONOUS DWM VISIBILITY GUARANTEE FOUND`.
This absence is **not** an empirical finding of DWM lag in this failure.

[WM_ENTERSIZEMOVE](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-entersizemove)
and [WM_EXITSIZEMOVE](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-exitsizemove)
bound the real native modal interaction. Merely sending WM_MOVING/SIZING is
not sufficient. [SendInput](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput)
inserts events into the input stream, subject to UIPI and current key state;
it is not an HWND-addressed placement operation. Amendment 001's explicit
test-only foreground/identity/hit-test fences remain mandatory if a real
Explorer driver is later authorized by the architecture gate.

## Independent diagnostic design and tests

1. Keep attraction/release/speed at **10/16/2000**, all existing exact acceptance,
   one native write, pre-END requirement and raw-reassertion abort unchanged.
2. Separate `MagnetPostverifyDiagnostic` from optional actual capture failure.
   Serialize native result, requested/actual geometry, exact flags, signed edge
   deltas, other-member/context/receipt health and classified failure. Successful
   capture cannot emit default Binding/capture_not_run as a placement failure.
3. Add direct GetWindowRect/DWM observations immediately after native return,
   before full capability capture. Keep their errors/QPC separately. These are
   two sequential reads, not an atomic snapshot and not an acceptance bypass.
4. Link correction to the next **observed** source receipt, LOCATION and END,
   stopping at another START. Offline validation recomputes links and geometry.
   Coalesced samples are labeled by their own receipt. Missing evidence remains
   null/UNKNOWN; prior-raw observation alone is not a causal reassertion proof.
5. Tests precede acceptance: all failure classes, geometry deltas, fake capture
   rejection, missing new-contract diagnostics, chronology isolation and no
   bypass of the new human-runner guard. Legacy logs remain read-only compatible.

## Owned modal probe / execution limitation

`panebind-magnet-modal-loop-probe` is a dedicated diagnostic executable, not a
CTest interaction test. It creates only its own empty top-level window, takes
a new-file evidence path and never accepts an external HWND. Its real
WM_ENTERSIZEMOVE and WM_MOVING/SIZING callbacks schedule one private posted
correction after the first drag callback; it never fabricates those messages.
T0 before, T1 direct positioning/visible after one call, T2 next real drag
callback/proposed RECT and T3 END are recorded, along with post-correction
WM_WINDOWPOSCHANGED observations. A +7 Y pulse for horizontal Move and +7
bottom-edge pulse for bottom Resize allow orthogonal/raw-path comparison.
No timer, polling, retry, input injection, hook or third-party window control.
The UI-owner call isolates same-process modal authority; it does not prove
cross-process Explorer timing even if it eventually captures successfully.

The planned controlled automation could not run: computer-use `list_apps`
reported `native pipe unavailable / os error 2`, repeated after bounded retry
and a fresh JS session. No target was selected, no input sent, no probe or
Explorer launched. Both configuration builds prove compilation, **not modal
behavior**. Per Fix 1 section 12 the human-owned diagnostic artifact is left
for independent review; no modal-loop PASS and no human Explorer retest command.

```text
R1C4B_MODAL_LOOP_OWNED_PROBE = BLOCKED_REQUIRES_HUMAN
R1C4B_SETWINDOWPOS_MOVE_MODAL_AUTHORITY = UNKNOWN
R1C4B_SETWINDOWPOS_RESIZE_MODAL_AUTHORITY = UNKNOWN
R1C4B_IMMEDIATE_POSITIONING_POSTVERIFY = UNKNOWN
R1C4B_IMMEDIATE_DWM_VISIBLE_POSTVERIFY = UNKNOWN
CURRENT_LIVE_MAGNET_NATIVE_ARCHITECTURE = UNRESOLVED
```

This is an observation-path blocker, not proof that the architecture is VALID
or REJECTED. No asynchronous visible-confirmation fix, drag takeover, repeated
write, subclass/injection or release-only substitution is authorized by these
results. If a later real probe proves reassertion, mark REJECTED and research
options A/B/C before any redesign, as the original brief requires.

## Amendment 001 boundary

Required order remains Core -> owned native -> real Explorer automated ->
independent review -> human subjective UAT -> seal. Owned tests are not real
Explorer proof. Stage E (Explorer driver/gate) was **not entered** because
stage D is unresolved. No automated Explorer authority is substituted for
human consent. No `automated_real_explorer` or `real_native_modal_loop=true`
evidence is invented. Debug/Release repetitions: **0/10, 0/10; NOT_RUN**.

Human runner now fails closed even with the review flag. This is a temporary
hard block, **not** an implemented aggregate/artifact verifier. Once the
architecture prerequisite is satisfied, the full explicit test-only driver,
nonce/canonical-frame authority, safety countdown/fences, fresh A/B/C per run,
all scenarios, independent schema validator and SHA-bound aggregate verifier
must be implemented. Only 10/10+10/10 (or disclosed 5/5+5/5 minimum) plus review
may enable human UAT. Do not remove the block merely by supplying a PASS field.

`AUTO_EXPLORER_GATE = NOT_RUN`; `R1C4B_HUMAN_UAT = NOT_READY`.
