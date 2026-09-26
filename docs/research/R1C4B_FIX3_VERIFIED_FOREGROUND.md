# R1-C4B Fix 3 — verified foreground acquisition research

Review date: 2026-09-26. Base:
`167be83db73f62bf6cc0e0691ab88697fc08871c`.

The Human Root Fix 3 brief explicitly authorizes a test-only verified synthetic
activation click after an ordinary SetForegroundWindow attempt is denied. Fix 2's
foreground prerequisite is a test bootstrap limitation, not evidence of a failed
native modal-loop authority model. This record establishes the research/design
gate; it does not claim successful input or modal interactions.

## Prior art, licensing and actual inspection boundary

AltSnap is a mature maintained reference, GPL-3.0-or-later and **REFERENCE ONLY**.
The local checkout `out/r1c2a-priorart/AltSnap` was verified at
`5c86416ad21e4b72844a998a746bd3bb0bee5f5d`. The inspection covered
[License.txt](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/License.txt),
the hooks.c GPL header, the
[foreground helpers](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L2603),
ActionLower's topmost handling and CreatePinWindow's nonactivating indicator.
The foreground helper uses synthetic Ctrl before bringing the window forward.
That historical solution is expressly prohibited in Fix 3 and is not reused.

Actual local history/diff inspection:

- [`3c69ef9e650c8cf66031550183038980e7109d81`](https://github.com/RamonUnch/AltSnap/commit/3c69ef9e650c8cf66031550183038980e7109d81),
  2022-07-28: introduces a Ctrl-based foreground workaround.
- [`865a51619975fe02fe9d13549d3e88abdcd6256f`](https://github.com/RamonUnch/AltSnap/commit/865a51619975fe02fe9d13549d3e88abdcd6256f),
  2025-08-28: removes synthetic Ctrl when focusing its own menu windows.
- [`400eebf04dc651f76b2c1148c63fee7a4b039d8c`](https://github.com/RamonUnch/AltSnap/commit/400eebf04dc651f76b2c1148c63fee7a4b039d8c),
  2026-07-14: adds WS_EX_NOACTIVATE to the topmost indicator. The lesson is that
  visibility/z-order and activation are distinct concerns.
- Merge `bdb881019397195a09da9e00a844612253b67030` identifies
  [PR 756](https://github.com/RamonUnch/AltSnap/pull/756). The merge message and
  implementation diff were read locally; the PR web discussion could not be
  fetched, so no claim is made about its discussion or review rationale.

Microsoft PowerToys/FancyZones is a mature production reference, MIT and
**REFERENCE ONLY**, pinned at
`19c4d805321db86f3634e6968e14dbf25cbba14a`. The actual source inspection covered
[WindowMouseSnap.cpp](https://raw.githubusercontent.com/microsoft/PowerToys/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/WindowMouseSnap.cpp)
and its pinned
[LICENSE](https://raw.githubusercontent.com/microsoft/PowerToys/19c4d805321db86f3634e6968e14dbf25cbba14a/LICENSE).
MoveSizeUpdate updates highlights, MoveSizeEnd performs zone placement and Abort
has a separate cleanup path; this is not evidence of live native drag correction
authority. The actual
[PR 48569](https://github.com/microsoft/PowerToys/pull/48569) review and
[merge diff `dd26d86580168d2e368701f7b0c4d629dc9cd9ac`](https://github.com/microsoft/PowerToys/commit/dd26d86580168d2e368701f7b0c4d629dc9cd9ac)
covered destroyed-window ingress/dispatch, abort cleanup and overbroad key
swallowing. A stranded drag must not steal subsequent focused-application input,
and an abort must not be relabeled as successful END.

Attempts to inspect pinned WindowUtils.cpp, ZonesOverlay.cpp and WorkArea.cpp
returned cache-miss errors. They were **not re-inspected in Fix 3**. Existing
earlier-round inspection records remain historical and are not relabeled.

No external implementation was copied, translated, mechanically rewritten or
adapted. GPL code remains reference-only; no new code attribution obligation is
created. Any future MIT reuse still requires a separate provenance/attribution
decision. The applicable subsystem is the dedicated automated test driver, not
PaneBind product activation or movement behavior.

## Official platform contracts

Microsoft Learn live pages were read on 2026-09-26; no immutable documentation
revision is claimed and only contract facts are paraphrased.

| Contract | Consequence for the independently implemented test driver |
|---|---|
| [SetForegroundWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setforegroundwindow) may be denied, even when stated eligibility conditions hold. AllowSetForegroundWindow requires a caller with foreground-setting ability. | Attempt the normal API once and record its result. No Alt/Ctrl hack, settings changes, AttachThreadInput or reliance on AllowSetForegroundWindow. |
| [SetWindowPos](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowpos) distinguishes HWND_TOPMOST, HWND_NOTOPMOST and SWP_NOACTIVATE. SWP_NOZORDER ignores hWndInsertAfter. Owned popups can follow an owner's topmost change. | Temporary exact-window TOPMOST with NOMOVE/NOSIZE/NOACTIVATE/SHOWWINDOW is only a visibility fence. Verify the result and remove TOPMOST before the drag. Do not claim it establishes foreground. |
| [GetClientRect](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getclientrect) and [ClientToScreen](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-clienttoscreen) provide the client bounds and screen point. | Derive the owned activation point from the window's actual client area. |
| [WindowFromPoint](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-windowfrompoint) finds the window at a point but excludes hidden/disabled windows; [GetAncestor](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getancestor) GA_ROOT walks the parent chain. | Freshly require the point's root to equal the exact authorized HWND, including after moving the cursor. |
| [WM_NCHITTEST](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-nchittest) identifies client/caption/border/system areas; screen coordinates can be negative. | Require HTCLIENT for the owned activation click. Use signed screen coordinates and never target caption, resize or system controls for owned bootstrap. |
| [GetGUIThreadInfo](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getguithreadinfo) reads a specified/foreground GUI thread; [GUITHREADINFO](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-guithreadinfo) exposes capture, menu, move/size and corresponding state flags. | Require successful fresh queries, stable foreground identity and clear capture/menu/move-size state before activation input. A query is a thread snapshot, not proof that all other desktop threads lack capture. Fail closed on uncertainty. |
| [SendInput](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput) returns inserted-event count, is subject to UIPI and retains existing key state. [MOUSEINPUT](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-mouseinput) defines absolute/virtual-desktop coordinates and dwExtraInfo. | Use tagged test mouse input only, equal-integrity checks and modifier/button fences. A partial count blocks; return success does not prove receipt or activation. |
| [WM_MOUSEACTIVATE](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-mouseactivate) occurs when an inactive window is clicked; its recipient can decline activation or discard the click. | An ordinary activation click is feasible, but its result must be observed, never assumed. |
| [WM_ACTIVATE](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-activate) includes WA_CLICKACTIVE and can be asynchronously delivered across input queues. [WM_SETFOCUS](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-setfocus) follows gained keyboard focus. | Signal an event from the owned WndProc's actual activation/focus messages. Use a bounded wait, then fresh exact foreground comparison. No foreground polling loop. |

No official guarantee was found that a synthetic mouse click necessarily obtains
foreground. The event-driven observed result is therefore a mandatory empirical
gate. The brief's exact identity, desktop, integrity, visibility and input fences
are additional test authority constraints, not new Windows API guarantees.

## Design and test gate

`RESEARCH_GATE = PASS_FOR_TEST_ONLY_VERIFIED_ACTIVATION_CLICK`.
This authorizes implementation and execution of the brief's bounded owned test;
it does not authorize a product foreground helper or imply empirical PASS.

The special activation path is isolated from normal modal drag injection:

1. Prove exact test-created identity, active/unlocked desktop and integrity;
   attempt ordinary SetForegroundWindow and verify any claimed success.
2. On denial, make only the exact window temporarily visible/topmost without
   activation. Derive and validate a safe client point, root/hit-test and clear
   foreign capture/menu/move-size state with no modifiers/buttons down.
3. Send one tagged absolute move, then revalidate actual cursor, point ownership,
   hit-test and input state before LEFTDOWN/LEFTUP. No keyboard input belongs to
   this activation sequence.
4. Observe real activation/focus messages and wait on their event handle with a
   fixed deadline. Freshly require foreground==owned; remove temporary TOPMOST
   and again confirm foreground and visibility.
5. Only then use the unchanged normal `fence()` / `inject()` path, which continues
   to require owned foreground for every real drag input boundary.

Model/unit coverage must distinguish direct API success, click success, foreign
root, nonclient hit, foreign capture, partial injection, missing activation event
and wrong final foreground. Negative preflight cases require zero button clicks.
Raw evidence must preserve each proof, API/input result and actual event; a model
PASS is never evidence of a native interaction.

The real owned probe must record WM_ENTERSIZEMOVE, WM_MOVING/WM_SIZING and
WM_EXITSIZEMOVE, with immediate positioning/visible postverify, next real sample
and END. Synthetic WM_MOVING/WM_SIZING messages do not satisfy this gate. Debug
and Release formal batches are 20 fresh processes/windows each, with first
failure preserved and no retry-until-pass. The separate Move/Resize authority
classifications govern whether Explorer bootstrap is permitted.

Only stable owned authority (or rigorously explained DWM_ASYNC_ONLY) permits
the brief's exact new-frame Explorer bootstrap. Owned immediate rejection or
reassertion requires architecture rejection and stops Explorer work. Explorer
bootstrap must independently prove canonical COM/nonce/HWND/security authority,
fresh point safety and at least 100 px setup work-area margin; an arbitrary
Explorer client center is not proof of no navigation/selection side effects.
Only successful minimal Explorer Move/Bottom Resize authority permits the full
10 Debug / 10 Release gate. Human runner stays blocked through this round and
requires the separate independent review after a full automated PASS.

`PRODUCT_SENDINPUT_DEPENDENCY = NONE` and `PRODUCT_FOREGROUND_FORCE = NONE` are
mandatory boundaries. No helper is linked into ExplorerLiveMagnetSession,
Magnet/Glue Core or the product runtime. Parameters 10/16/2000, single native
correction and exact/fail-closed diagnostics remain unchanged. No empirical
result is supplied here; actual evidence, repetition counts and architecture
verdict belong in the [Fix 3 execution report](../reports/R1C4B_FIX3_EXECUTION_REPORT.md).
