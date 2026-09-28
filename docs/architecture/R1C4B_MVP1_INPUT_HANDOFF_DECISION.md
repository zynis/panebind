# R1-C4B MVP1 — Move handoff decision (research gate)

Date: 2026-09-28. Starting implementation: `5459133b00740e74f6be2f752eb96227760d370b`.
This note is a decision boundary, not an Explorer implementation or runtime PASS.
The accepted Fix H owned free-takeover replay remains historical evidence only.
Existing [Pivot 1 prior art](../research/R1C4B_PIVOT1_PRIOR_ART.md) and
[source provenance](../research/SOURCE_PROVENANCE.md) cover the inspected
AltSnap and FancyZones references; no upstream code is copied or adapted here.

## Established constraints

The old `ExplorerLiveMagnetSession` samples native `LOCATION_CHANGE` and calls
`ExplorerGroupBridge::apply_magnet` while Explorer's native move loop can still
be active. Fix 3 rejected that concurrent correction path. The product path
must classify Move before cancellation, retire on a real physical UP, wait for
native `MOVESIZEEND`, and use fresh exact source/target/context and actual
geometry before the first write. An END event is a barrier, not the end of the
whole pointer gesture. Cursor-derived intent and native/self geometry receipts
must be separate streams.

`WM_CANCELMODE` can cause default processing to release capture, but its return
is not a native-loop-exit or write-authority proof
([Microsoft](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-cancelmode)).
`EVENT_SYSTEM_MOVESIZEEND` reports move/size completion; it does not by itself
prove physical button state, input routing or exact post-END authority
([Microsoft](https://learn.microsoft.com/en-us/windows/win32/winauto/event-constants)).
The owned source's private `WM_EXITSIZEMOVE` callback cannot be projected onto
cross-process Explorer.

Raw mouse input has button transitions but no foreign target HWND or historical
screen point. `MSG.pt/time` can supply the cursor position/time when `WM_INPUT`
was posted, not the Explorer-received DOWN; WinEvent and Raw receiver queues
have no documented common gesture ID or cross-queue total order
([MSG](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-msg),
[RAWMOUSE](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-rawmouse),
[SetWinEventHook](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwineventhook)).
The candidate association must therefore be compared with owned
`WM_NCLBUTTONDOWN` ground truth, then observed read-only against newly created,
exactly consented Explorer frames. Ambiguous, missing or already-UP input must
not authorize cancellation or a source write.

## Input-isolation boundary

After foreign capture is released, `RIDEV_NOLEGACY` only suppresses legacy
mouse messages for the registering application, and background `SetCapture`
cannot capture input meant for another process
([RAWINPUTDEVICE](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-rawinputdevice),
[SetCapture](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setcapture)).
The existing Fix D shield is test-only, built **after** END around a frozen
synthetic trajectory. It cannot cover the cancel-to-END gap or an arbitrary
physical cursor path.

A PaneBind-owned, nonactivating, hit-test-opaque but nearly visually transparent
layered overlay installed **before** the single bounded cancel is a candidate
for an owned experiment. A zero-alpha or `WS_EX_TRANSPARENT` window would pass
mouse input through, while nonzero alpha without that style can participate in
hit testing
([Layered Windows](https://learn.microsoft.com/en-us/windows/win32/winmsg/window-features#layered-windows),
[WM_MOUSEACTIVATE](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-mouseactivate)).
Its actual hit, foreground, native capture, legacy UP, destruction and abnormal
cleanup must be observed; creation/style/API success is insufficient. Raw UP
must immediately retire pending writes, but Raw and legacy UP delivery have no
documented cross-queue order, so destroying the overlay on Raw UP alone is not
a proven safe teardown.

To cover an unrestricted real cursor path, a spatially bounded corridor is
insufficient: it can be escaped between Raw delivery and repositioning, or
across the cancel-to-END gap. A candidate overlay covers the reachable virtual
desktop for the short gesture. That temporarily intercepts mouse input over
*pre-existing other applications*; it does not control those HWNDs or block
their background Raw Input. This is one research candidate, not a proof that
no other product architecture is possible.

**2026-09-28 human-root amendment:** The user explicitly authorized this
full-virtual-screen, PaneBind-owned, nearly transparent, nonactivating overlay
for one already authenticated ordinary Move gesture, including setup before
the bounded native cancel. The authorization is spatially broad for input hit
routing but grants geometry writes only to the exact three consented temporary
Explorer frames. It does not authorize `ClipCursor`, `BlockInput`, hooks,
injection, permanent Explorer style/z-order changes, or a resident desktop
shield. The prior scope-decision blocker is removed, **not** the empirical
gate. Test-owned hit/foreground/UP/teardown evidence must precede Explorer.

The product design must cap each isolation lifetime at 30 seconds, retire
pending geometry writes as soon as a reliable Raw UP is observed, and have an
escape path independent of a stalled geometry writer. Normal teardown requires
actual evidence of where the legacy UP went; Raw UP alone is not a safe
cross-queue teardown barrier. Premature UP, setup failure, explicit stop,
foreground/desktop invalidation, parent exit and deadline must retire the
gesture without a later cancel or source write. A deadline/failure escape may
remove the overlay without a legacy-UP witness to restore desktop usability,
but records the handoff safety as `UNKNOWN` or `FAIL`, never `PASS`.

The minimal pure [MoveHandoffGate](../../src/core/behavior/move_handoff_gate.h)
models only this ordering; Win32 identity, hit, end and authority proofs remain
adapter duties. In particular, the owned test receiver currently performs a
synchronous pipe write from its `WM_INPUT` procedure. It is not a safe sole
owner of a full-desktop overlay: a stuck parent/pipe reader could also stall
its UI message pump. A per-gesture, short-lived overlay helper must own the
shield HWND on its own UI thread, with a separate event/deadline escape path
and no blocking writer IPC in that UI thread. The Raw receiver must revoke the
writer generation before any potentially blocking evidence transfer. A native
placement already in flight at physical UP cannot be undone merely by setting
a flag; this must be fault-tested and reported separately from the guarantee
that **no new placement is issued after UP**. These are design requirements,
not yet observed product results.

Current live gates after authorization: product input isolation `UNPROVEN`;
Explorer Move Magnet `NOT_RUN`; same-entry three-window UAT `NOT_RUN`.

## Owned test execution boundary found in this continuation

The new opt-in owned overlay experiment is intentionally **not runnable yet**:
its synthetic-input CLI fails before creating a window or sending input. It
would send a test LEFTDOWN before the candidate overlay is ready. If, in that
gap, the owned source loses foreground/capture or the desktop changes, neither
its exact-self cleanup route nor the accepted Fix H test fence proves that a
synthetic LEFTUP can be safely delivered without reaching a pre-existing
window. The Fix H 100/100 result is a controlled normal-path result, not a
guarantee of recovery after arbitrary external interference. Destroying an
overlay first and releasing afterward worsens the ambiguity. This is a
test-execution safety blocker, not empirical evidence that the layered overlay
itself fails. No owned overlay hit/UP/teardown, Explorer cancellation, or
product geometry write has been observed under the amendment.

The read-only Explorer anchor diagnostic is also gated behind the required
owned stage. It retains the previous human-created nonce frame provisioning
because the R1-C2A research rejected `ShellExecute` and `explorer.exe /n` or
`/separate` as guaranteed-new-window authority; merely invoking them could
navigate or reuse a pre-existing user window. No such fallback is enabled.
