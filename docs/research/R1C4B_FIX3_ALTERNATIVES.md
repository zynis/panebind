# R1-C4B Fix 3 — alternatives after owned modal reassertion

Research date: 2026-09-26. Base:
`167be83db73f62bf6cc0e0691ab88697fc08871c`.
This is a research-only decision record, not a redesign or an implementation.

The current round's owned Move and Bottom Resize probe findings are recorded in
the [Fix 3 execution report](../reports/R1C4B_FIX3_EXECUTION_REPORT.md). Under the
Human Root gate, observed owned reassertion rejects the current live architecture
and stops further interactive runs before Explorer. It does not establish an
observed Explorer result. This document does not manufacture repetition counts,
Explorer observations or alternative-architecture acceptance.

Existing [Fix 1 modal authority research](R1C4B_FIX1_MODAL_AUTHORITY.md) and
[Fix 3 foreground research](R1C4B_FIX3_VERIFIED_FOREGROUND.md) remain applicable.
Improved test foreground acquisition is not a product input-ownership grant.
Repeated SetWindowPos writes, looser postverify, changed 10/16/2000 thresholds or
Sleep/polling are not acceptable responses to an authority conflict.

## Prior-art inspection and provenance

AltSnap is a mature maintained **GPL-3.0-or-later reference only**, pinned at
`5c86416ad21e4b72844a998a746bd3bb0bee5f5d`. The local source inspection covered
[hooks.c](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c),
LetWindowKickBack (around 1404), MoveResizeWindowNow_ (1452), LowLevelMouseProc
(5430) and mouse hook setup (5664), plus the already verified License.txt and GPL
source header. Its placement/input pipeline performs its own movement and sends
synthetic sizing/lifecycle notifications. It installs WH_MOUSE_LL; its optional
no-hook build uses a timer. Those mechanisms are excluded by the current brief.
They cannot be relabeled as native modal-loop authority or copied into PaneBind.
The actual local diff
[`df25d36c6369bb13aa02ec83974e625fc7922c35`](https://github.com/RamonUnch/AltSnap/commit/df25d36c6369bb13aa02ec83974e625fc7922c35)
was re-read: recipient WM_SIZING return handling and application grid adjustment
are distinct from a synthetic notification's transport success. Prior PR 739
inspection is historical; no new PR discussion inspection is claimed here.

PowerToys/FancyZones is a mature production **MIT reference only**, pinned at
`19c4d805321db86f3634e6968e14dbf25cbba14a`. This round's actual pinned
[WindowMouseSnap.cpp](https://raw.githubusercontent.com/microsoft/PowerToys/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/WindowMouseSnap.cpp)
and LICENSE review supports end-of-drag zone placement and a separate Abort path,
not live attraction. Actual
[PR 48569](https://github.com/microsoft/PowerToys/pull/48569) and
[merge diff `dd26d86580168d2e368701f7b0c4d629dc9cd9ac`](https://github.com/microsoft/PowerToys/commit/dd26d86580168d2e368701f7b0c4d629dc9cd9ac)
review supports preserving destroy/abort semantics and not treating a destroyed
window as a successful completed snap. Neither project was run by this research.

No code copied, translated or adapted. Existing license boundaries remain;
future code reuse needs its own explicit provenance/license decision.

## A — adjust the recipient's real WM_MOVING / WM_SIZING RECT

**Official facts.** [WM_MOVING](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-moving)
and [WM_SIZING](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-sizing)
give the receiving window procedure a screen-coordinate RECT; the application
changes that RECT to adjust the drag and returns TRUE. The contract describes
recipient-side participation in the actual interaction.
[SetWindowSubclass](https://learn.microsoft.com/en-us/windows/win32/api/commctrl/nf-commctrl-setwindowsubclass)
does not permit cross-thread subclassing.
[SetWindowLongPtrW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowlongptrw)
states that an application should not subclass a window class created by another
process. An ordinary PaneBind callback pointer is not executable in Explorer.
[SetWindowsHookExW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowshookexw)
requires a DLL hook procedure for a foreign thread; the relevant in-process
message hooks therefore cross the explicit no-injection boundary.
[Hooks Overview](https://learn.microsoft.com/en-us/windows/win32/winmsg/about-hooks)
also distinguishes hook types and their execution contexts; installing a hook
does not generically grant recipient message-editing authority.

[SendMessageW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendmessagew)
supports cross-thread delivery subject to UIPI and system-message marshalling.
Sending an additional WM_SIZING with a caller-supplied RECT does not attach that
RECT to the system's already-running modal interaction. No official equivalence
with rewriting its actual recipient callback was found. Likewise, an
out-of-context WinEvent callback reports a lifecycle/location fact and does not
expose that callback's mutable drag RECT.

**Inference.** A is a valid cooperation model for an application whose WndProc
PaneBind legitimately owns, or for a separately approved cooperating application.
No verified out-of-process route was found that provides the same participation
for Explorer under the no-DLL-injection/no-injected-hook constraints. The owned
probe's WndProc cannot establish external Explorer authority.

`A_FOR_EXPLORER_UNDER_CURRENT_CONSTRAINTS = BLOCKED`.
No subclass, injected callback, synthesized drag message or Explorer experiment
was implemented or run. Any cooperative app/plugin model would require a new
scope and product decision; it is not a continuation authorized by Fix 3.

## B — cancel the native interaction and take over mouse movement

**Official facts.** [WM_CANCELMODE](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-cancelmode)
cancels certain modes; DefWindowProc cancels standard scrollbar/menu processing
and releases mouse capture. Its contract does not describe an atomic transfer
of an Explorer drag session, native cursor anchor or resize-edge state to a
different process.
[SetCapture](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setcapture)
requires a window in the calling thread, limits background capture and explicitly
does not capture input intended for another process.
[ReleaseCapture](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-releasecapture)
releases capture in the current thread; it does not remotely revoke Explorer's
capture by calling it from PaneBind.
[WM_CAPTURECHANGED](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-capturechanged)
notifies the losing window and advises against reacquiring capture in response.

**Inference.** Canceling the native loop and writing geometry are insufficient:
the replacement runtime still needs legitimate continuing physical pointer input,
button release and cancellation ownership. With Explorer foreground and a drag
already started there, current-thread SetCapture is not a proven handoff. Forcing
PaneBind foreground or adding a mouse-capturing surface would change user focus,
event routing and the existing native-drag interaction contract. The current
test-only activation click does not authorize such product behavior.

The researched AltSnap custom movement pipeline illustrates the broader input
ownership machinery involved, but its low-level hook/timer mechanisms are
prohibited here. Global mouse hooks, Raw Input takeover, high-frequency cursor
polling, AttachThreadInput and injected code may not fill this gap.

`B_NATIVE_DRAG_TAKEOVER_UNDER_CURRENT_CONSTRAINTS = UNPROVEN_AND_BLOCKED`.
No WM_CANCELMODE, capture takeover or product input path was tried. Reliability
of Explorer cancellation, initial/final geometry, focus, z-order, Esc, physical
button release, interrupted gestures and monitor/DPI transitions is **NOT TESTED**.
A future explicit opt-in PaneBind-owned drag surface would be a different product
interaction, requiring its own research/design/consent gates; none is implemented.

## C — one snap after native interaction completion

**Official facts.** [WM_EXITSIZEMOVE](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-exitsizemove)
marks exit from the native moving/sizing modal loop. The out-of-context
[EVENT_SYSTEM_MOVESIZEEND](https://learn.microsoft.com/en-us/windows/win32/winauto/event-constants)
reports finished movement/resizing, and is not a button-release payload or a
guarantee of application acceptance of the user's gesture. The pinned FancyZones
source performs its zone placement in MoveSizeEnd and has a separate Abort.
[SetWindowPos](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowpos)
remains an ordinary positioning API; leaving a loop does not itself guarantee
exact application geometry or synchronous DWM visibility.

**Inference.** A single authority-checked correction after an observed complete
native session avoids competing with each subsequent native drag proposal. It
is a plausible bounded alternative, not an empirically accepted Explorer design.
It cannot meet the current live-attraction/pre-END correction goal: the native
window remains unsnapped during the drag and may visibly jump on completion.
Relation formation/acceptance timing and cancellation semantics would change.
An END notification alone must not authorize a snap after an abort, destroy,
maximize/minimize, stale authority or a newly begun gesture.

`C_SNAP_ON_RELEASE = RESEARCH_CANDIDATE_REQUIRES_HUMAN_PRODUCT_DECISION`.
`C_EXPLORER_PLACEMENT_AND_UX = NOT_TESTED`.
Before any separately authorized implementation, the proposed acceptance gate
would need exact nonce/canonical-frame/geometry/context revalidation, healthy
ordered receipts, no newer conflicting lifecycle, one native operation, exact
postverify, and independent abort/cancellation tests. There must be no silent
replacement of live Magnet, no reclassification of existing pre-END failures as
PASS and no unblocking of the human runner on the basis of this research.

## Round boundary

The options above are researched, not selected or implemented. A and B lack a
verified permitted authority path for the current Explorer interaction. C has
the clearest bounded non-injected placement model, but requires an explicit
product decision because its UX and acceptance criteria differ. This comparison
is not an assertion that no other architecture can exist.

No further GUI/Explorer probing, input, repeated correction, feature tuning or
product redesign is authorized by these findings. The Fix 3 execution report
controls the actual owned evidence and current architecture verdict; Explorer
authority and all alternative behaviors remain **NOT TESTED**. Complete the
authorized regression/documentation/Git closure and stop for architecture review.
