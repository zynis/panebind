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
across the cancel-to-END gap. The only identified overlay candidate covers the
reachable virtual desktop for the short authorized gesture. That would
temporarily intercept mouse input over *pre-existing other applications*.
This is materially broader than exact three-Explorer-window control, so it is
**not authorized by this research note**. The user has been asked explicitly
whether to authorize that bounded-in-time but broad-in-space experiment. No
product overlay or Explorer cancellation may be enabled before the choice and
owned evidence. Neither `ClipCursor` nor `BlockInput` is substituted silently.

Current live gates: product input isolation `UNPROVEN`; Explorer Move Magnet
`NOT_RUN`; same-entry three-window UAT `NOT_RUN`. The pure core Move intent
model and owned-only, non-invasive diagnostics may proceed independently.
