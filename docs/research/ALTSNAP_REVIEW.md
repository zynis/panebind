# AltSnap Prior-Art Review

## Review status

- Review date: 2026-08-24 (Asia/Shanghai).
- Project: [RamonUnch/AltSnap](https://github.com/RamonUnch/AltSnap).
- Immutable revision reviewed: [`5c86416ad21e4b72844a998a746bd3bb0bee5f5d`](https://github.com/RamonUnch/AltSnap/commit/5c86416ad21e4b72844a998a746bd3bb0bee5f5d), committed 2026-07-27 06:58:41 +02:00 on `main`.
- Nearest release: [`1.68`](https://github.com/RamonUnch/AltSnap/releases/tag/1.68), whose tag points to `20307468b832b40998e1e65e96ab6eb28293ee62`; the reviewed head is 48 commits later (`1.68-48-g5c86416`).
- License: the repository carries the [GNU GPL version 3 text](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/License.txt), while the C source headers say GPL version 3 **or later**. GitHub reports `GPL-3.0`. PaneBind therefore treats AltSnap as **GPL reference-only**.
- Review document: **IMPLEMENTED**.
- Code copied: **NO**.
- Code adapted: **NO**.
- External build or runtime behavior: **NOT TESTED**. This is a static source/history/issue review; issue reports are not relabeled as PaneBind manual observations.
- Manual behavior observation: **NOT TESTED**.
- AltSnap research track: **PASS**. The overall `PRIOR_ART_GATE` still depends on the other mandatory review and the consolidated provenance record.

This document uses three evidence labels:

- **Fact** means directly supported by immutable source, repository history, an issue/PR, or official Microsoft documentation.
- **Inference** means a conclusion drawn from those facts but not demonstrated by a runtime experiment in this round.
- **Recommendation** is an independent PaneBind design direction. It is not adapted AltSnap code.

## Classification and fitness as prior art

**Fact.** AltSnap is an active, mature behavioral reference rather than a proof of concept: its public history begins in 2020, it has repeated releases, current development after release 1.68, and a long issue/PR record covering real applications, input devices, monitor configurations, and Windows releases. Popularity was not used as the maturity test. The stronger evidence is the depth of its corrective history, including per-monitor DPI work, hidden-edge snapping, hook/input failure reports, and application-specific resize behavior.

**Fact.** The repository has no automated test directory or test target at the reviewed revision. Most geometry and behavior live in one large Win32 state machine. It is therefore mature operational prior art, but not a validation oracle or a suitable architectural template for PaneBind.

**Recommendation.** Use AltSnap to discover failure modes, policy choices, and required test cases. Do not derive implementation structure or code from it, both because of the GPL boundary and because PaneBind requires a platform-neutral core.

## Architecture

### Source and runtime boundaries

| Unit | Directly observed responsibility | PaneBind interpretation |
| --- | --- | --- |
| `AltSnap.exe` | `altsnap.c` owns process startup, tray/config integration, loading `hooks.dll`, enabling/disabling the keyboard hook, and command routing. `tray.c`, `config.c`, and localization sources are textually included by the executable source. See [loader and hook setup](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/altsnap.c#L41-L103). | Keep lifecycle/UI outside the normalized core, but do not copy the textual-inclusion structure. |
| `hooks.dll` | `hooks.c` owns input state, window selection, filtering, monitor/window enumeration, move/resize, snapping, restore state, z-order actions, timers, and a worker thread. It textually includes [`snap.c` and `zones.c`](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L464-L466). | Valuable as an end-to-end behavior map; too coupled and Windows-specific to become a core boundary. |
| `snap.c` | Stores AltSnap restore flags and dimensions as window properties, with a fixed 16-entry in-process fallback, and rescales stored restore dimensions when the window DPI changes. See [restore state handling](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/snap.c#L43-L202). | Restore provenance and lifetime must be explicit in a future operations layer; do not make the core write HWND properties. |
| `zones.c` | Reads absolute zone rectangles or generates grids over monitor work areas, selects containing or nearest-center zones, and manages a preview. See [zone loading and selection](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/zones.c#L11-L274). | Out of R0 scope. Record the monitor/topology questions, but do not pull zones forward. |
| `unfuck.h` / `nanolibc.h` | Compatibility shims for old Windows/SDK versions, geometry helpers, DWM/DPI calls, window visibility, and a small runtime. | Confirms that Windows compatibility details are numerous and belong in the Windows adapter, not `src/core/`. |
| Build | The default Make target builds exactly `AltSnap.exe` and `hooks.dll`; there is no separate geometry library. See [Makefile targets](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/Makefile#L139-L145). | PaneBind should create explicit seams between native collection, normalized models, and deterministic geometry. |

**Fact.** The shared header includes `windows.h`, and the behavioral state is expressed directly in `HWND`, `HMONITOR`, `RECT`, window styles, messages, and process-global mutable structures. There is no platform-neutral domain layer.

**Inference.** AltSnap's compactness likely helped one maintainer iterate quickly, but it also makes input timing, window policy, frame conversion, snapping, and third-party control interdependent. Its history of cross-cutting regressions is consistent with that coupling; it does not by itself prove that every regression was caused by the architecture.

**Recommendation.** Preserve the behavioral sequence, not the module shape:

```text
native input/event
  -> Windows target/filter adapter
  -> normalized window + monitor snapshot
  -> pure geometry candidate scoring
  -> future behavior decision
  -> future Windows operations adapter
```

The final operations step remains research-only in R0.

## Input, hooks, and the AltDrag-to-AltSnap change

### Current AltSnap hook strategy

**Fact.** Enabling AltSnap loads `hooks.dll`, calls its exported configuration/lifecycle function, then installs a global `WH_KEYBOARD_LL` hook. Pressing an activation key installs `WH_MOUSE_LL`; the mouse hook can instead remain installed when title-bar actions, inactive scrolling, hot-click, or long-click movement require it. See [keyboard activation](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L2731-L2797), [mouse hook installation](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L5647-L5699), and [configuration deciding whether it stays resident](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L6774-L6788).

**Fact.** A `hooks.dll` filename does not imply injection. Microsoft documents that `WH_KEYBOARD_LL` and `WH_MOUSE_LL` callbacks switch back to and run in the installing process, not in the target process: [LowLevelKeyboardProc](https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelkeyboardproc) and [LowLevelMouseProc](https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelmouseproc). At the reviewed revision:

- the only active `SetWindowsHookEx` calls are `WH_KEYBOARD_LL` and `WH_MOUSE_LL`;
- there is no active `WH_CALLWNDPROC` hook;
- searches found no `CreateRemoteThread`, `VirtualAllocEx`, or `WriteProcessMemory` path; and
- `DllMain` only records its own module instance and disables thread callbacks. See [current `DllMain`](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L6797-L6805).

**Conclusion.** Current AltSnap does **not** inject its DLL into third-party processes.

**Fact.** AltSnap has a compile-time `NO_HOOK_LL` fallback that polls cursor/button state on a 32 ms timer, but the macro is not defined in the reviewed source or default Makefiles. The shipped/default source path therefore uses low-level hooks, not polling. PaneBind must not adopt that fallback because R0 explicitly forbids high-frequency polling.

**Fact.** The source contains a WinEvent-hook experiment behind an undefined `EVENT_HOOK` preprocessor guard. It is not part of the reviewed build. Active movement handling does not observe OS move/size WinEvents; AltSnap optionally calls `NotifyWinEvent` to announce moves that **AltSnap itself controls**. See [disabled event-hook block](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L4088-L4119) and [synthetic start/end notification](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L576-L587).

**Recommendation.** AltSnap is not a model for the R0 observer. R0 should continue with out-of-context `SetWinEventHook` observation and no global input behavior.

### Why callback latency matters

**Fact.** Microsoft says low-level hook callbacks must return before `LowLevelHooksTimeout`; Windows 7 and later can silently remove a timed-out hook with no notification. Microsoft recommends a dedicated hook thread that quickly hands work to another thread. The current Microsoft guidance is in the [keyboard](https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelkeyboardproc) and [mouse](https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelmouseproc) callback documentation.

**Fact.** AltSnap added a worker thread in 2025 ([initial worker-thread commit](https://github.com/RamonUnch/AltSnap/commit/7f4afe59076b70980f71af202f63609ca3ac5745)) and currently coalesces consecutive mouse-move work before executing the newest point. See [worker queue/coalescing](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L693-L736). A five-second timer also refreshes a permanently installed mouse hook after cursor movement, expressly citing silent low-level-hook removal. See [rehook timer](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L5715-L5726).

**Fact.** The callbacks still perform some synchronous target selection, blacklist checks, hit testing, and action initialization. [Issue #532](https://github.com/RamonUnch/AltSnap/issues/532) remains open for intermittent loss of window interaction; the maintainer's hook-timeout explanation there is a hypothesis, not a confirmed diagnosis. [Issue #545](https://github.com/RamonUnch/AltSnap/issues/545) remains open for mouse-button state becoming inconsistent, particularly around laggy windows.

**Recommendation.** If a future round authorizes global input, isolate hook ownership on a dedicated message-loop thread, make callback work bounded and allocation-free, version the forwarded input, make recovery observable, and test loss/up-down imbalance. The R0 observer does not need this mechanism.

### What changed from AltDrag

To verify the fork claim, this review also inspected Stefan Sundin's upstream [AltDrag](https://github.com/stefansundin/altdrag) at immutable revision [`e2740d605b0336a3b391fec26794718864b19521`](https://github.com/stefansundin/altdrag/commit/e2740d605b0336a3b391fec26794718864b19521), committed 2020-04-05.

**Fact.** Upstream AltDrag's optional `HookWindows` mode installs a global `WH_CALLWNDPROC` hook, launches a matching 64-bit helper on 64-bit Windows, and uses the injected callback to detect `WM_ENTERSIZEMOVE` and subclass third-party windows during normal title-bar dragging. See upstream [hook installation and bitness helper](https://github.com/stefansundin/altdrag/blob/e2740d605b0336a3b391fec26794718864b19521/altdrag.c#L211-L324), [in-process callback/subclassing](https://github.com/stefansundin/altdrag/blob/e2740d605b0336a3b391fec26794718864b19521/hooks.c#L2084-L2150), and [per-process `DllMain` configuration](https://github.com/stefansundin/altdrag/blob/e2740d605b0336a3b391fec26794718864b19521/hooks.c#L2215-L2428). Microsoft confirms that global non-low-level hooks can inject a DLL and require matching 32/64-bit components: [SetWindowsHookEx](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowshookexw).

**Fact.** AltSnap's first source-bearing commit, [`e69435d`](https://github.com/RamonUnch/AltSnap/commit/e69435db8369943c6ebfcd973fc1bf061f97dee1) from 2020-11-23, already lacks `WH_CALLWNDPROC` and contains only the low-level keyboard/mouse path. Its first README explicitly says HookWindows was removed because of injection risk, complexity, and the need for both bitnesses; the statement remains in the [current README](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/README.md#L14-L24).

**Fact.** The fork was renamed from AltDrag to AltSnap in [`baea101`](https://github.com/RamonUnch/AltSnap/commit/baea101894bda97c593f958b90e00094648bcc80) on 2021-09-09. [Issue #63](https://github.com/RamonUnch/AltSnap/issues/63) records the rationale: the fork had diverged too far to be treated as merely another AltDrag build.

**Historical limitation.** AltSnap was uploaded without preserving AltDrag as Git ancestry, so an exact fork-point/removal diff and a unique "remove HookWindows" commit are not recoverable from the AltSnap repository. The immutable before/after repositories verify the architecture change, but not the precise development sequence or authorship of each removed line.

**Recommendation.** Adopt AltSnap's decision, not its implementation: no injected message hook, no bitness-paired injection helper, and no subclassing third-party windows.

## Move and resize path

### Entry and target selection

**Fact.** The active path is:

```text
low-level keyboard activation
  -> conditional low-level mouse hook
  -> mouse-down action mapping
  -> WindowFromPoint / root-or-MDI selection
  -> process/window/fullscreen/maximized filters
  -> capture placement, monitor work area, DPI, min/max, frame data
  -> worker-thread mouse-move processing
  -> move or resize geometry
  -> edge snap, Aero-like snap, optional zone snap
  -> SetWindowPos / SetWindowPlacement (or hollow preview)
  -> finish and restore-state cleanup
```

The initialization path is visible in [`init_movement_and_actions`](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L5024-L5182). It selects a window under the cursor, normalizes MDI coordinates when enabled, records the nearest monitor's work area, applies blacklist/state filters, reads `WINDOWPLACEMENT`, records origin DPI and min/max limits, and selects the move or resize action.

### Movement

**Fact.** On each accepted move point, AltSnap derives a proposed top-left position from the cursor offset, then applies Aero-like monitor snapping, general edge snapping, and zone snapping before dispatching the target rectangle. See [movement branch](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L2220-L2283).

**Fact.** Live movement uses `SetWindowPos`; hollow-preview mode moves AltSnap-owned preview windows and applies the final rectangle at completion. The operations path sends start/end size-move notifications, can send `WM_SIZING`, chooses synchronous versus `SWP_ASYNCWINDOWPOS` flags based on whether size changes, and uses `WM_SYNCPAINT` with a timeout for some asynchronous moves. See [operation dispatch](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L1403-L1549).

**Recommendation.** None of this control path belongs in R0. A future Windows operations adapter should expose an explicit operation request/result boundary rather than allow input code to call Win32 placement functions directly.

### Resize

**Fact.** Resize selection partitions the clicked window into configurable regions and can use diagonals to select the closest side. The drag path changes only the selected edges, clamps width/height against default DPI-aware system metrics plus the target's `WM_GETMINMAXINFO`, then applies edge and Aero-resize snapping. See [edge selection](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L3747-L3827), [resize calculation](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L2285-L2342), and [min/max acquisition](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/unfuck.h#L1579-L1596).

**Fact.** Merged [PR #723](https://github.com/RamonUnch/AltSnap/pull/723) added a bounded `WM_SIZING` round trip before placement so terminal-like applications can quantize the proposed rectangle to a character grid. The same PR also fixed an out-of-bounds write in snapped-window enumeration.

**Inference.** A resize engine cannot assume the requested rectangle is the rectangle an application will accept. That is supported by AltSnap's corrective history, but PaneBind still needs its own experiment and operation-result model.

**Recommendation.** A future operation result should capture requested bounds, accepted/observed bounds, timeout/failure, and the relevant frame/DPI snapshot. Pure geometry tests must not pretend to model application-owned `WM_SIZING` behavior.

## Snapping

### Target discovery and filtering

**Fact.** Target discovery occurs lazily once per movement state. `EnumDisplayMonitors` appends each monitor's **work area**. When window snapping is enabled, `EnumDesktopWindows` appends eligible top-level windows. See [enumeration control](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L982-L1049).

**Fact.** A window candidate must:

- not be the moving window;
- be visible by AltSnap's stronger definition (`IsWindowVisible`, not DWM-cloaked, and nonzero size);
- not be minimized;
- not have `WS_EX_NOACTIVATE`; and
- have a caption, thick frame, an AltSnap-saved borderless flag, or an explicit `Snaplist` match.

See [`ShouldSnapTo`](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L678-L690) and [visibility helper](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/unfuck.h#L1155-L1184).

**Fact.** Maximized candidates are not generally discarded. For desktop snapping their visible bounds are cropped to the owning monitor's work area to remove maximized invisible overhang; maximized MDI children are skipped. Minimized candidates are always skipped. See [candidate callback](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L739-L789).

**Fact.** The actively manipulated window is independently filtered. `BLMaximized` can reject maximized targets, but its reviewed default is off; fullscreen targets are rejected unless explicitly allowed. Process and window blacklists are checked before movement starts.

### Visible edges and z-order

**Fact.** Enumeration order is used as a z-order proxy. A later candidate fully contained by an already recorded rectangle is skipped. For a partially covered candidate, AltSnap crops each of its four edge segments at endpoints covered by previously seen rectangles and marks edges with no remaining span. General move snapping will not use those fully hidden edges. This behavior was added after [issue #681](https://github.com/RamonUnch/AltSnap/issues/681) in merged [PR #682](https://github.com/RamonUnch/AltSnap/pull/682). The reviewed implementation is in the [candidate callback](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L759-L786) and [move snap checks](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L1092-L1147).

**Fact.** The individual hidden-edge flags are consulted by `MoveSnap`, but the reviewed `ResizeSnap` loop does not carry or check those flags. This is a source observation, not a reproduced defect.

**Recommendation.** Treat occlusion as explicit candidate metadata and test move and resize consistently. Do not infer target eligibility from a single `visible` boolean.

### Distance and screen-versus-window edges

**Fact.** Screen and window rectangles go through the same move/resize comparison loops. Monitors are evaluated first and always allow same-side (inside) alignment. Window level 2 allows outside/opposing-edge snapping; level 3 also allows inside/same-edge alignment. This corresponds to the UI's disabled, screen borders, outside of windows, and inside of windows levels. See [unified move loop](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L1052-L1157) and [unified resize loop](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L1160-L1278).

**Fact.** A candidate edge is eligible only when the perpendicular spans overlap or come within the current tolerance. Horizontal and vertical tolerances are reduced independently after a match, so x and y may snap to different targets. The algorithm is a scan with mutable per-axis thresholds, not a global scored-and-sorted candidate set.

**Fact.** The configured `SnapThreshold` is in pixels (reviewed default 20). The code's comments describe nearest-edge intent, but after a match the mutable threshold is assigned the **signed** coordinate delta rather than an absolute distance. No repository test establishes behavior for equidistant targets or candidates on opposite sides.

**Recommendation.** Define a platform-neutral candidate record such as `{axis, moving_edge, target_edge, signed_delta, absolute_distance, perpendicular_overlap, source_kind, source_id, visibility, priority}`. Rank by absolute distance with an explicit stable tie-break. Test positive/negative deltas, equal distances, monitor/window conflicts, and independent-axis matches. This is an independent model, not an adaptation of AltSnap's scan.

### Multiple windows

**Fact.** Normal snapping considers all eligible candidate windows individually; it does not create a persistent group. A separate "sticky resize" feature snapshots touching, resizable, nonmaximized windows and applies batched `DeferWindowPos` updates to immediate neighbors. See [touching-window discovery and batch resize](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L839-L978).

**Inference.** This immediate-neighbor procedure is useful evidence that min/max constraints, overlap, monitor ownership, and batch failure matter. It is not evidence that a persistent adjacency graph is unnecessary.

**Recommendation.** Do not implement sticky resize, groups, or adjacency propagation in R0. Preserve only deterministic rectangle/tolerance primitives and research questions for a later gate.

### Restore state and native snapping

**Fact.** AltSnap stores its own snap/restore state on target HWNDs and falls back to a 16-entry local table. Native Windows-snapped state is inferred from a mismatch between current bounds and `WINDOWPLACEMENT.rcNormalPosition`; the source itself calls this undocumented behavior. See [restore storage](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/snap.c#L43-L202) and [native snap heuristic](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/unfuck.h#L1557-L1577). [Issue #7](https://github.com/RamonUnch/AltSnap/issues/7) shows the historical difficulty of mixing native and AltSnap restore behavior.

**Recommendation.** Do not make an undocumented heuristic a normalized truth. A future adapter should report observed placement/state fields and confidence/provenance separately; tests must cover Windows-native snapping and restore transitions.

## Monitor, DPI, and frame geometry

### Monitor topology

**Fact.** Drag initialization uses `MonitorFromPoint(..., MONITOR_DEFAULTTONEAREST)` and records `MONITORINFO.rcWork`; each Aero-like snap point reselects the nearest monitor so a drag can cross monitors. General edge snapping enumerates all monitor work areas. Microsoft confirms that `MonitorFromPoint` consumes virtual-screen coordinates and that `MONITOR_DEFAULTTONEAREST` returns the nearest monitor when the point is outside all displays: [MonitorFromPoint](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-monitorfrompoint).

**Fact.** Zones generated as grids also use monitor work areas. Saved absolute layouts are selected by a packed union width/height, and the source comments that proper multi-monitor layout matching is uncertain. See [layout resolution selection](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/zones.c#L418-L475).

**Recommendation.** Give every normalized monitor a stable observation ID, monitor bounds, work area, DPI, and topology revision. Do not identify a multi-monitor layout only by union width/height.

### DPI

**Fact.** AltSnap declares `PerMonitorV2,PerMonitor` in its [manifest](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/AltSnap.exe.manifest#L12-L16). Microsoft recommends setting process DPI awareness in the manifest and documents `PerMonitorV2` as the modern manifest value: [process DPI awareness](https://learn.microsoft.com/en-us/windows/win32/hidpi/setting-the-default-dpi-awareness-for-a-process).

**Fact.** Compatibility helpers dynamically call `GetDpiForWindow`, then fall back to monitor DPI and finally a device context/96 for UI helpers. `GetDpiForWindow` is recorded at movement start and alongside restore dimensions. See [DPI helpers](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/unfuck.h#L723-L813). Microsoft notes that the returned value depends on the **target window's** awareness: [GetDpiForWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getdpiforwindow).

**Fact.** [Issue #413](https://github.com/RamonUnch/AltSnap/issues/413), "Window sizes are not correctly resized when crossing monitors with different DPI scalings," remains open. Merged [PR #415](https://github.com/RamonUnch/AltSnap/pull/415) was progress on that issue, and the 1.61 release notes describe the result as only a partial fix. The issue's experiments report application- and timing-dependent results.

**Conclusion.** A Per-Monitor-V2 manifest is necessary but not sufficient. The AltSnap history does not establish a reliable mixed-DPI cross-monitor movement model.

**Recommendation.** R0 should log, without controlling windows, the event-time window DPI, monitor identity/DPI, positioning rectangle, and visible rectangle before/during/after cross-monitor manual movement. Any later size-preservation policy must state whether it preserves physical pixels, effective-DPI logical size, visible frame size, or application restore size.

### Positioning bounds versus visible bounds

**Fact.** AltSnap deliberately distinguishes the `GetWindowRect` positioning rectangle from `DWMWA_EXTENDED_FRAME_BOUNDS`. `GetWindowRectLL` prefers the DWM visible frame for snap targets; `FixDWMRectLL` calculates per-edge deltas between positioning and visible bounds so placement rectangles can be adjusted. See [frame helpers](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/unfuck.h#L1091-L1153).

**Fact.** Microsoft confirms that `GetWindowRect` is DPI-virtualized and may include invisible resize borders, while `DWMWA_EXTENDED_FRAME_BOUNDS` gives visible bounds and is not DPI-adjusted: [GetWindowRect remarks](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowrect).

**Fact.** `SnapGap` is incorporated by inflating visible target rectangles and adjusting the moving window's frame deltas. This puts policy (desired gap) and native frame conversion in the same helpers.

**Recommendation.** PaneBind should keep separate normalized fields for positioning bounds and visible frame bounds, with source/error metadata. Gap/tolerance is pure behavior policy; frame conversion belongs in the Windows adapter.

## Exclusions, elevation, and special cases

**Fact.** AltSnap has separate lists for processes, windows, snap targets, MDI exceptions, pause/kill, title-bar handling, scrolling, forced resize, size-move notifications, non-client hit testing, and bottommost handling. Items can combine executable, title, and class fields; a leading null pattern enables whitelist mode. See [list schema](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L389-L428), [matching](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L468-L520), and [parser](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L6293-L6393).

**Fact.** The reviewed defaults contain concrete exclusions for shell surfaces, remote-desktop clients, the on-screen keyboard, League of Legends processes, XAML hosts, task switchers, and other known special cases. This is accumulated compatibility policy, not a universal definition of a manageable top-level window.

**Inference.** Separate policy dimensions are worth preserving, but hard-coded application names are a maintenance backstop, not a first-principles filter. The long list is evidence that style/visibility checks alone are insufficient.

**Recommendation.** Normalize observable facts first (`visible`, `cloaked`, `minimized`, styles, owner/root, process identity, rect validity), then apply named policies with a reason code. Keep user exclusions in the Windows/application policy layer, not geometry.

**Fact.** The manifest is `asInvoker` with `uiAccess="false"`. AltSnap can relaunch elevated for controlling elevated windows; ordinary operation intentionally cannot cross the integrity boundary. [Issue #538](https://github.com/RamonUnch/AltSnap/issues/538) discusses but does not result in a `uiAccess=true` reviewed manifest.

**Recommendation.** R0 observation should record access failures and elevation context, never auto-elevate merely to increase coverage. Future control across integrity levels needs its own threat-model and consent gate.

## Reliability and historical decisions

| Evidence | Status at review | Lesson |
| --- | --- | --- |
| HookWindows injection removed before AltSnap's first source import; current README records security, complexity, and 32/64-bit costs. | Current design | Preserve the no-injection product constraint. |
| [Issue #160](https://github.com/RamonUnch/AltSnap/issues/160), GPU-accelerated Alacritty lag during drag/resize. | Closed | Target repaint behavior, `SetWindowPos` flags, input rate, and refresh rate interact. Avoid per-app timing hacks as the primary model. |
| [Issue #388](https://github.com/RamonUnch/AltSnap/issues/388), clicks becoming lost or doubled after an input-state regression. | Closed | Blocked/replayed input and drag-threshold state require sequence tests, not only geometry tests. |
| [Issue #452](https://github.com/RamonUnch/AltSnap/issues/452), movement rate strategy for 1 kHz mice and different display refresh rates. | Closed after several strategies | Event coalescing and bounded work matter; mouse event rate and display refresh are not interchangeable clocks. |
| [Issue #413](https://github.com/RamonUnch/AltSnap/issues/413) plus [PR #415](https://github.com/RamonUnch/AltSnap/pull/415), mixed-DPI cross-monitor size. | Issue still open; release notes say partial fix | Never infer closure from a merged PR titled "Fix". Preserve coordinate-space provenance and run empirical matrices. |
| [Issue #532](https://github.com/RamonUnch/AltSnap/issues/532), intermittent loss of interaction. | Open | Low-level-hook health and native snap/restore state remain uncertain; recovery must be observable. |
| [Issue #545](https://github.com/RamonUnch/AltSnap/issues/545), left-click unavailable until right-click. | Open | Slow synchronous work can desynchronize physical and logical button state; worker offload reduces but does not prove elimination. |
| [Issue #681](https://github.com/RamonUnch/AltSnap/issues/681) / [PR #682](https://github.com/RamonUnch/AltSnap/pull/682), snapping to covered borders. | Closed/merged | Candidate visibility is edge-specific and z-order-dependent. |
| [PR #723](https://github.com/RamonUnch/AltSnap/pull/723), out-of-bounds write plus `WM_SIZING` support. | Merged | Dynamic candidate storage and app-adjusted resize paths require tests and bounded calls. |
| [Issue #414](https://github.com/RamonUnch/AltSnap/issues/414), Explorer keyboard behavior after Alt-drag. | Open | The activation key itself can interact with application UI modes; Windows behavior may be reproducible without AltSnap. Do not attribute all symptoms to geometry code. |

**Fact.** A commit message at [`76114d2`](https://github.com/RamonUnch/AltSnap/commit/76114d263585fa32d55bfa315177d034fc78792a) references `#618`, but GitHub's issue endpoint returned 404 during review. No issue content or conclusion was inferred from that unavailable record.

## PaneBind decisions from this review

### Designs worth adopting as independent concepts

- Distinguish visible frame bounds from OS positioning bounds.
- Treat monitor work areas and physical monitor bounds as different facts.
- Filter minimized, cloaked, zero-size, non-activating, self, and invalid windows before geometric consideration, while retaining reason codes.
- Capture window min/max constraints and recognize that applications can revise a proposed resize.
- Keep low-level callbacks minimal and coalesce high-rate change work when an input-controlled round is eventually authorized.
- Make edge visibility/occlusion a candidate property rather than using whole-window visibility alone.

### Designs worth adapting into a cleaner model

- Replace the mutable threshold scan with deterministic, platform-neutral candidate scoring and explicit tie-breaking.
- Replace global HWND state with immutable/versioned normalized snapshots plus native IDs confined to the adapter.
- Replace HWND properties and undocumented snap inference with an explicitly owned future restore-state model that records provenance and confidence.
- Revalidate a snap target before a future operation; AltSnap's once-per-drag enumeration can become stale if topology or a target changes mid-drag.
- Represent process/window/user exclusions as layered policy decisions, not geometry conditions.

### Designs to avoid

- Any injected `WH_CALLWNDPROC`/subclassing design or paired 32/64-bit injection helper.
- High-frequency polling as an input/event source, including AltSnap's disabled 32 ms fallback.
- A monolithic module where input, filter policy, geometry, native operations, restore state, and UI share mutable globals.
- Synchronous unbounded cross-process calls from a low-level hook callback.
- Treating merged fixes, issue anecdotes, or a Per-Monitor-V2 manifest as proof that mixed-DPI behavior is solved.
- Copying, translating, or mechanically restructuring GPL implementation code into PaneBind.

### Problems PaneBind must test

- Positioning bounds versus visible frame bounds, including asymmetric invisible borders and maximized overhang.
- Negative virtual-screen coordinates and monitors above/left of the primary monitor.
- Different taskbar/work-area layouts, monitor disconnect/reconnect, and topology change during observation.
- 96/non-96 and mixed-DPI transitions, including windows with different awareness modes.
- Minimized, maximized, restored, cloaked, zero-size, borderless, tool, owned, no-activate, and hung windows.
- Completely and partially occluded candidate edges, plus move/resize parity.
- Multiple candidates at equal and near-equal distance, candidates on opposite sides, and x/y matches from different targets.
- Target movement/destruction between snapshot and decision.
- Native Windows snap/restore sequences versus application and tool-owned restore state.
- If a later round authorizes global input: 125/500/1000 Hz mice, slow GPU windows, missed key/button-up, hook timeout/removal, sleep/lock, remote desktop, and integrity-level boundaries.

## R0 boundary and gate conclusion

The AltSnap track satisfies the R0 prior-art requirement: source, license, exact revision, upstream architectural difference, official platform semantics, issue history, and independent design lessons were all inspected and recorded.

This review authorizes **no AltSnap-derived implementation**. In R0, PaneBind may only observe, log, normalize, and calculate. In particular, it must not implement AltSnap's low-level input behavior, `SetWindowPos`/`DeferWindowPos` control, snapping, zones, sticky resize, restore properties, or synthetic move/size notifications.

```text
ALTSNAP_RESEARCH_GATE = PASS
CODE_COPIED = NO
CODE_ADAPTED = NO
RUNTIME_BEHAVIOR = NOT TESTED
```

## Provenance payload for `SOURCE_PROVENANCE.md`

The following payload is intentionally included here rather than editing the shared provenance file from this parallel workstream.

### AltSnap

```text
Project: AltSnap
Classification: Mature, active prior-art reference
Repository: https://github.com/RamonUnch/AltSnap
Commit / Tag / SHA reviewed: 5c86416ad21e4b72844a998a746bd3bb0bee5f5d; describe 1.68-48-g5c86416; nearest release tag 1.68 -> 20307468b832b40998e1e65e96ab6eb28293ee62
License: GNU GPL v3-or-later per source headers; License.txt contains GPLv3; GitHub SPDX metadata GPL-3.0
Date reviewed: 2026-08-24
Files/modules inspected: README.md; License.txt; Makefile; AltSnap.exe.manifest; AltSnap.txt; AltSnap.dni; altsnap.c; hooks.c; hooks.h; snap.c; zones.c; unfuck.h; nanolibc.h; relevant git history
Issues inspected: #7, #63, #160, #388, #413, #414, #452, #532, #538, #545, #644, #681; attempted #618 returned 404
PRs inspected: #415, #682, #723, #748
What was learned: current non-injected low-level hook design; removal of AltDrag HookWindows; move/resize pipeline; unified monitor/window edge candidates; occlusion-aware edge filtering; min/max and app-adjusted resize behavior; frame-bounds distinction; blacklist policy; worker/coalescing history; unresolved mixed-DPI and input reliability risks
Applicable PaneBind subsystem: Windows input research (future); Windows window/monitor/frame adapter; normalized snapshots/events; pure geometry candidate model; filtering policy; reliability test matrix
Code copied: NO
Code adapted: NO
Attribution required: NO for code because no code entered PaneBind; research citations retained. Any future reuse requires a separate GPL compatibility decision and is not authorized by R0.
```

### AltDrag (comparison source)

```text
Project: AltDrag
Classification: Mature historical comparison source
Repository: https://github.com/stefansundin/altdrag
Commit / Tag / SHA reviewed: e2740d605b0336a3b391fec26794718864b19521
License: GNU GPL v3-or-later per source headers; repository LICENSE is GPLv3; GitHub SPDX metadata GPL-3.0
Date reviewed: 2026-08-24
Files/modules inspected: LICENSE; altdrag.c; hooks.c; hookwindows_x64.c; build.bat; AltDrag.ini; repository tree/metadata
Issues/PRs inspected: none directly; AltSnap #63 and current AltSnap README were used for fork history
What was learned: optional HookWindows used global WH_CALLWNDPROC injection, per-process DLL initialization/subclassing, and a second-bitness helper; this is the design AltSnap removed
Applicable PaneBind subsystem: hook/input architecture and no-injection threat boundary
Code copied: NO
Code adapted: NO
Attribution required: NO for code because no code entered PaneBind; research citations retained. Any future reuse requires a separate GPL compatibility decision and is not authorized by R0.
```

## Inspection commands and limitations

Environment: Windows NT 10.0.26200.0, Git 2.53.0.windows.1, GitHub CLI 2.90.0, ripgrep 15.2.0.

Commands used (repeated line-range reads and issue-comment retrievals are collapsed by family):

```powershell
Get-Content -Raw <round-brief>
Get-Content -Raw .\AGENTS.md

git clone --filter=blob:none --no-checkout https://github.com/RamonUnch/AltSnap.git <temp>
git -C <temp> switch --detach origin/main
git -C <temp> rev-parse HEAD
git -C <temp> show -s --format='%H%n%aI%n%cI%n%D%n%s' HEAD
git -C <temp> remote -v
git -C <temp> describe --tags --always --dirty
git -C <temp> show-ref --tags
git -C <temp> for-each-ref refs/tags/1.68
git -C <temp> ls-tree -r --name-only HEAD
git -C <temp> show HEAD:<path>
git -C <temp> grep -n -E <API-and-behavior-patterns> <revision>
git -C <temp> log --oneline --decorate
git -C <temp> log --reverse --format='%H %aI %s'
git -C <temp> log --all --extended-regexp --regexp-ignore-case --grep=<topic>
git -C <temp> show --format=fuller --stat <commit>
rg -n <API-and-behavior-patterns> <reviewed-files>
Get-Content <reviewed-file>  # repeated bounded line-range inspection

gh api repos/RamonUnch/AltSnap
gh api repos/RamonUnch/AltSnap/releases/latest
gh api 'repos/RamonUnch/AltSnap/issues?...'
gh api repos/RamonUnch/AltSnap/issues/<number>
gh api repos/RamonUnch/AltSnap/issues/<number>/comments?per_page=100
gh issue view <number> --repo RamonUnch/AltSnap --comments
gh pr view <number> --repo RamonUnch/AltSnap --json number,state,title,url,createdAt,closedAt,mergedAt,mergeCommit,commits,files,body

gh api repos/stefansundin/altdrag
gh api repos/stefansundin/altdrag/branches/master
gh api repos/stefansundin/altdrag/commits/e2740d605b0336a3b391fec26794718864b19521
gh api 'repos/stefansundin/altdrag/git/trees/e2740d605b0336a3b391fec26794718864b19521?recursive=1'
gh api 'repos/stefansundin/altdrag/contents/<path>?ref=e2740d605b0336a3b391fec26794718864b19521'
```

Official Microsoft pages opened during review:

- [SetWindowsHookEx](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowshookexw)
- [LowLevelKeyboardProc](https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelkeyboardproc)
- [LowLevelMouseProc](https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelmouseproc)
- [GetWindowRect](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowrect)
- [DwmGetWindowAttribute](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/nf-dwmapi-dwmgetwindowattribute)
- [GetDpiForWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getdpiforwindow)
- [MonitorFromPoint](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-monitorfrompoint)
- [GetMonitorInfo](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getmonitorinfow)
- [Setting process DPI awareness](https://learn.microsoft.com/en-us/windows/win32/hidpi/setting-the-default-dpi-awareness-for-a-process)

Limitations and blocked sub-evidence:

- Two direct `git clone --bare` attempts for upstream AltDrag failed due transient GitHub connection resets. GitHub's API then returned the complete non-truncated tree and the inspected file contents at the immutable SHA, so the comparison itself is not blocked.
- AltSnap's checkout produced working-tree encoding warnings for localization/INI files because of `working-tree-encoding=utf-16le-bom`; C source inspection was unaffected, and repository-form content was read with `git show` where needed.
- Issue attachments and test binaries were not downloaded or executed.
- The exact pre-public-history HookWindows removal commit is unavailable, as explained above.
- No AltSnap binary was built or run, and no issue report is presented as a manually observed PaneBind result.
