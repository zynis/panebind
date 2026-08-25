# Source Provenance Register

Review date for R0 entries: 2026-08-24.

This register covers external projects and documentation actually inspected for
PaneBind. A research citation does not authorize code reuse. No external code
was copied or adapted during R0. Any future reuse requires a new, explicit
license-compatibility decision before code enters the repository.

## AltSnap

```text
Project: AltSnap
Classification: Mature, active prior-art reference
Repository: https://github.com/RamonUnch/AltSnap
Commit / Tag / SHA reviewed: 5c86416ad21e4b72844a998a746bd3bb0bee5f5d; describe 1.68-48-g5c86416; nearest release tag 1.68 -> 20307468b832b40998e1e65e96ab6eb28293ee62
License: GNU GPL v3-or-later per source headers; License.txt contains GPLv3; GitHub SPDX metadata reports GPL-3.0
Date reviewed: 2026-08-24
Files/modules inspected: README.md; License.txt; Makefile; AltSnap.exe.manifest; AltSnap.txt; AltSnap.dni; altsnap.c; hooks.c; hooks.h; snap.c; zones.c; unfuck.h; nanolibc.h; relevant git history
Issues/PRs inspected: issues #7, #63, #160, #388, #413, #414, #452, #532, #538, #545, #644, #681; issue #618 attempted but unavailable (404); PRs #415, #682, #723, #748
What was learned: current non-injected low-level hook design; removal of AltDrag HookWindows; move/resize pipeline; unified monitor/window edge candidates; occlusion-aware edge filtering; min/max and application-adjusted resize behavior; positioning/visible-frame distinction; blacklist policy; worker/coalescing history; unresolved mixed-DPI and input reliability risks
Applicable PaneBind subsystem: future Windows input research; Windows window/monitor/frame adapter; normalized snapshots/events; pure geometry candidate model; filtering policy; reliability test matrix
Code copied: NO
Code adapted: NO
Attribution required: NO for code because no code entered PaneBind; research citations retained
```

License decision: **GPL REFERENCE-ONLY**. R0 does not authorize copying,
translation, mechanical restructuring, or adaptation of AltSnap code into the
MIT codebase. See [the detailed review](ALTSNAP_REVIEW.md).

## AltDrag

```text
Project: AltDrag
Classification: Mature historical comparison source
Repository: https://github.com/stefansundin/altdrag
Commit / Tag / SHA reviewed: e2740d605b0336a3b391fec26794718864b19521
License: GNU GPL v3-or-later per source headers; repository LICENSE is GPLv3; GitHub SPDX metadata reports GPL-3.0
Date reviewed: 2026-08-24
Files/modules inspected: LICENSE; altdrag.c; hooks.c; hookwindows_x64.c; build.bat; AltDrag.ini; repository tree/metadata
Issues/PRs inspected: none directly; AltSnap issue #63 and the current AltSnap README were inspected for fork history
What was learned: optional HookWindows used a global WH_CALLWNDPROC hook, per-process DLL initialization/subclassing, and a second-bitness helper; AltSnap removed this injected design before its first public source revision
Applicable PaneBind subsystem: hook/input architecture and no-injection threat boundary
Code copied: NO
Code adapted: NO
Attribution required: NO for code because no code entered PaneBind; research citations retained
```

License decision: **GPL REFERENCE-ONLY**. The historical source was inspected
only to verify the before/after architecture described in the AltSnap review.

## Microsoft PowerToys / FancyZones

```text
Project: Microsoft PowerToys / FancyZones
Classification: Mature production reference
Repository: https://github.com/microsoft/PowerToys
Commit / Tag / SHA reviewed: 19c4d805321db86f3634e6968e14dbf25cbba14a (detached main SHA; no exact tag)
License: MIT; https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/LICENSE
Date reviewed: 2026-08-24
Files/modules inspected: src/modules/fancyzones/FancyZones/FancyZonesApp.{h,cpp}; FancyZones/main.cpp; FancyZonesModuleInterface/dllmain.cpp; FancyZonesLib/FancyZones.{h,cpp}; FancyZonesWindowProcessing.{h,cpp}; WindowUtils.{h,cpp}; WorkArea.{h,cpp}; WorkAreaConfiguration.{h,cpp}; Layout.{h,cpp}; FancyZonesDataTypes.{h,cpp}; MonitorUtils.{h,cpp}; EditorParameters.{h,cpp}; VirtualDesktop.{h,cpp}; AppliedLayouts, AppZoneHistory, CustomLayouts and related data modules; Settings; OnThreadExecutor; ZonesOverlay; project files; native unit tests; legacy and .Next UI tests; editor unit/UI tests; fuzz tests; doc/devdocs/modules/fancyzones.md; root LICENSE
Issues/PRs inspected: issues #1685, #43363, #44058, #49016, #49019; PRs #44440, #48473, #48569, #49433; path-scoped git history through the reviewed SHA
Official documentation inspected: Microsoft Learn FancyZones product documentation and SetWinEventHook documentation
What was learned: use out-of-context event ingress and serialized owner-thread dispatch; scope noisy location events when product semantics permit; cancel state before topology replacement; model monitor/work-area/virtual-desktop identity explicitly; label DPI coordinate spaces; use reason-bearing filters; separate canonical layout data from caches; test teardown, missing events, and topology/DPI transitions; FancyZonesLib is Windows-specific and is not a platform-neutral core template
Applicable PaneBind subsystem: Windows event adapter; window snapshot/filtering; monitor/work-area/DPI adapter; normalized event model; core geometry boundary; lifecycle and test strategy
Code copied: NO
Code adapted: NO
Attribution required: NO for this reference-only review; any future copied or substantially reused MIT material must preserve the PowerToys MIT notice and be approved/recorded separately before entering PaneBind
```

License decision: **REFERENCE-ONLY FOR R0**. MIT compatibility is known, but
R0 independently implements only the documented platform boundary; no
PowerToys code was introduced. See [the detailed review](FANCYZONES_REVIEW.md).

## Microsoft Learn Windows desktop documentation

```text
Source: Microsoft Learn — Windows desktop/Win32 API documentation
Publisher/repository: Microsoft; https://learn.microsoft.com/en-us/windows/win32/
Version / SHA: N/A — live platform documentation; individual page metadata varies
Terms: Microsoft Learn Terms of Use; facts were cited and paraphrased only
Date reviewed: 2026-08-24
Pages inspected: EnumWindows; SetWinEventHook; WINEVENTPROC; UnhookWinEvent; Event Constants; Object Identifiers; Guarding Against Reentrancy; GetAncestor; IsWindow; IsWindowVisible; Extended Window Styles; IsIconic; IsZoomed; GetWindowLongPtrW; GetWindowThreadProcessId; GetWindowTextW; GetClassNameW; QueryFullProcessImageNameW; OpenProcess; CloseHandle; GetWindowRect; DwmGetWindowAttribute; DWMWINDOWATTRIBUTE; setting default DPI awareness; DPI_AWARENESS_CONTEXT; SetProcessDpiAwarenessContext; GetThreadDpiAwarenessContext; AreDpiAwarenessContextsEqual; GetDpiForWindow; GetWindowDpiAwarenessContext; MonitorFromWindow; GetMonitorInfoW; MONITORINFO
Issues/PRs inspected: N/A
What was learned: narrow out-of-context event registration; same-thread hook lifetime and reentrancy; event-specific object filtering; HWND lifetime races; visibility/cloak/tool/minimized distinctions; minimum-rights process metadata; positioning versus visible-frame bounds; PMv2 manifest semantics; target-window DPI semantics; signed virtual-screen monitor/work-area coordinates
Applicable PaneBind subsystem: Windows enumeration/events/filtering/snapshot/geometry/DPI/monitor/process-metadata adapter and normalized event mapping
Code copied: NO
Code adapted: NO
Attribution required: NO for code reuse; inline official-document citations retained
```

Exact page URLs and claim-level citations are retained in
[R0_WINDOWS_EVENT_MODEL.md](R0_WINDOWS_EVENT_MODEL.md).

## R1-A targeted adjacency/topology review

Review date: 2026-08-25.

### AltSnap / AltDrag

```text
Projects: AltSnap; AltDrag
Classification: Mature active behavioral prior art; mature historical comparison
Repositories: https://github.com/RamonUnch/AltSnap ; https://github.com/stefansundin/altdrag
Commits reviewed: AltSnap 5c86416ad21e4b72844a998a746bd3bb0bee5f5d (1.68-48-g5c86416); AltDrag e2740d605b0336a3b391fec26794718864b19521 (v1.1-8-ge2740d6)
License: GNU GPL v3-or-later; REFERENCE ONLY
Files/modules inspected for R1-A: AltSnap hooks.c (ShouldSnapTo, EnumWindowsProc, EnumTouchingWindows, MoveSnap, ResizeSnap, ResizeTouchingWindows); unfuck.h (visible-frame helpers, IsEqualT, IsInRangeT, SegT, AreRectsTouchingT); relevant AltDrag hooks.c snap scan
Issues/PR/history inspected for R1-A: AltSnap issues #59, #95, #347, #413, #445, #507, #566, #620, #681; PRs #415, #682, #723; commits 1dd26c9, 5fdb078, 5fb8307, fa5c70a, 3555048, 8b774a1, 90d0a06; AltDrag issues #1, #7, #32, #38; PRs #99, #134, #136, #187; commit 0055ed1
What was learned: visible-frame snapping; explicit pixel tolerance; mutable signed per-axis threshold risks; tolerance-expanded perpendicular candidate checks; sticky resize's historical move from edge-only matching to inclusive segment contact; immediate-neighbor-only behavior; no connected-component graph; unresolved frame-gap and mixed-DPI behavior
Applicable PaneBind subsystem: visible-geometry adjacency relation; deterministic graph/component solver; translation-session reliability tests; Windows eligibility remains outside core
Code copied: NO
Code adapted: NO
Attribution required: NO for code because no GPL code entered PaneBind; research citations retained
```

License decision: **GPL REFERENCE-ONLY**. R1-A uses independently specified
mathematics and tests. It does not copy, translate, mechanically rewrite, or
adapt AltSnap/AltDrag implementation code.

### Microsoft PowerToys / FancyZones

```text
Project: Microsoft PowerToys / FancyZones
Classification: Mature production reference
Repository: https://github.com/microsoft/PowerToys
Commit reviewed: 19c4d805321db86f3634e6968e14dbf25cbba14a
License: MIT; reference-only in R1-A
Files/modules/tests inspected for R1-A: FancyZonesLib Layout, Zone, WorkArea, WorkAreaConfiguration, FancyZones, MonitorUtils, FancyZonesDataTypes, FancyZonesWindowProcessing, WindowUtils, EditorParameters; AppliedLayouts and CustomLayouts stores; Layout/WorkArea/Zone/WindowProcessing/WorkAreaId unit tests
Issues/PR/history inspected for R1-A: issues #1167, #4962, #28626, #43363, #43386, #44058, #49016; PRs #13703, #19077, #19312, #27005, #28556, #44440, #48473, #49433, #49985; relevant merge commits recorded in R1_ADJACENCY_TOPOLOGY_RESEARCH.md
What was learned: WorkArea/native identity and operations must not become platform-neutral core; cursor sensitivity is not edge tolerance; topology replacement must abort stale consumers; canonical layout refresh prevents mixed-generation state; visual zone targets and positioning operation rectangles are separate contracts; DPI coordinate contexts must be unified before core; reason-bearing Windows eligibility remains caller policy; PaneBind mixed-DPI/multi-monitor runtime behavior remains NOT TESTED
Applicable PaneBind subsystem: immutable geometry snapshot boundary; future topology generation/lifetime contract; visible versus operation geometry separation; graph caller eligibility
Code copied: NO
Code adapted: NO
Attribution required: NO for this reference-only review; any future reuse requires separate approval and PowerToys MIT notice preservation
```

### Microsoft Learn geometry documentation

```text
Sources: GetWindowRect; DwmGetWindowAttribute / DWMWA_EXTENDED_FRAME_BOUNDS; RECT; MONITORINFO; FancyZones product documentation
Publisher: Microsoft Learn
Version / SHA: N/A; live documentation reviewed 2026-08-25
Terms: Microsoft Learn Terms of Use; facts paraphrased and cited only
What was learned: exclusive right/bottom rectangle convention; GetWindowRect DPI virtualization and invisible borders; visible DWM frame distinction; signed virtual-screen monitor/work-area coordinates; documented FancyZones mixed-DPI limitations
Applicable PaneBind subsystem: adapter-to-core visible geometry contract and signed coordinate test domain
Code copied: NO
Code adapted: NO
Attribution required: NO for code reuse; inline citations retained
```

Detailed R1-A findings and decisions are in
[R1_ADJACENCY_TOPOLOGY_RESEARCH.md](R1_ADJACENCY_TOPOLOGY_RESEARCH.md).

## R1-B targeted Windows operations review

Review date: 2026-08-25.

### AltSnap / AltDrag

```text
Projects: AltSnap; AltDrag
Classification: Mature active behavioral prior art; mature historical comparison
Repositories: https://github.com/RamonUnch/AltSnap ; https://github.com/stefansundin/altdrag
Commits reviewed: AltSnap 5c86416ad21e4b72844a998a746bd3bb0bee5f5d (1.68-48-g5c86416); AltDrag e2740d605b0336a3b391fec26794718864b19521 (v1.1-8-ge2740d6)
License: GNU GPL v3-or-later; REFERENCE ONLY
Files/modules inspected for R1-B: AltSnap hooks.c, unfuck.h, License.txt; AltDrag hooks.c, altdrag.c, LICENSE; pinned source history
Issues/PR/history inspected for R1-B: AltSnap issues #76, #160, #374, #572, #719; PRs #77, #573, #723; commits 1b64b08fb1db262b6f0a180b022243956c8a016e, a84f6b1084aa7346a9c847837843f5b221565864, c69135e1efbb9f97bccb8f86c713d319e6e0e835; historical AltDrag HookWindows path
What was learned: move-only SetWindowPos flags; sticky-resize Defer chain without structured error/post-verification; visible/positioning frame corrections; application-adjusted resize; weak IsWindow-only lifetime checks; synthetic interactive messages/WinEvents; async and NOSENDCHANGING application risks; requested geometry can be resisted
Applicable PaneBind subsystem: R1-B Windows owned-window operations, translation bridge, lifetime capability, batch failure model, and feedback evidence
Code copied: NO
Code adapted: NO
Attribution required: NO for code because no GPL code entered PaneBind; research citations retained
```

License decision: **GPL REFERENCE-ONLY**. R1-B independently specifies and
implements its capability, bridge, result, and batch contracts. No source was
copied, translated, mechanically rewritten, or structurally adapted.

### Microsoft PowerToys / FancyZones

```text
Project: Microsoft PowerToys / FancyZones
Classification: Mature production reference
Repository: https://github.com/microsoft/PowerToys
Commit reviewed: 19c4d805321db86f3634e6968e14dbf25cbba14a
License: MIT; reference-only in R1-B
Files/modules inspected for R1-B: FancyZonesLib WorkArea, WindowUtils, FancyZones, MonitorUtils, WorkAreaConfiguration, LayoutAssignedWindows, WindowDrag, WindowMouseSnap; FancyZones/FancyZonesApp; editor/FancyZonesEditor/Utils/NativeMethods.cs; FancyZones.UITests.Next/Utils/FancyZonesTestHelper.cs; common/Display/dpi_aware.cpp; root LICENSE
Issues/PR/history inspected for R1-B: issues #1685, #19440, #49016; PRs #17553, #21565, #28688, #44440, #48473, #48569, #49985; commits 75e966ce1981de26bb7c34f151b7939b88988233, 6d9d4a7112e3db72258989139907619b6bb65729, 3299ecfece9128cfd6791bacee1351b1c9a32c73, 6c2a99dfd6a12ad98feeda0acbc663aa84865676, ae9f241ef13737dab6f861767bbfdfca72b78475, dd26d86580168d2e368701f7b0c4d629dc9cd9ac, d68980a81bb8de144bdec998a114e948bf68c563
What was learned: target placement uses SetWindowPlacement rather than a Defer batch; visible targets require frame adjustment; event callbacks dispatch to an owner thread; destroy/topology changes abort stale consumers; raw HWND state remains weaker than PaneBind's owned token; mixed-DPI failures require explicit coordinate context; placement failures are log-only and not a PaneBind result model
Applicable PaneBind subsystem: R1-B Windows owned-window operations, geometry bridge, lifetime/topology invalidation, DPI/monitor transitions, and feedback evidence
Code copied: NO
Code adapted: NO
Attribution required: NO for reference-only review; future copied or substantially reused MIT material requires separate approval and preservation of the Microsoft MIT notice
```

An exact search of the pinned FancyZones tree found no
`BeginDeferWindowPos`, `DeferWindowPos`, or `EndDeferWindowPos` use. It is not a
batch atomicity or rollback precedent.

### Microsoft Learn Windows operations documentation

```text
Source: Microsoft Learn - Windows desktop/Win32 API documentation
Publisher/repository: Microsoft; https://learn.microsoft.com/en-us/windows/win32/
Version / SHA: N/A - live platform documentation
Terms: Microsoft Learn Terms of Use; facts were cited and paraphrased only
Date reviewed: 2026-08-25
Pages inspected for R1-B: SetWindowPos; BeginDeferWindowPos; DeferWindowPos; EndDeferWindowPos; GetWindowRect; DwmGetWindowAttribute; DWMWINDOWATTRIBUTE; IsWindow; GetWindowThreadProcessId; GetClassNameW; SetPropW/GetPropW/RemovePropW; CreateWindowExW; DestroyWindow; WM_NCCREATE; WM_CREATE; WM_DESTROY; WM_NCDESTROY; WM_WINDOWPOSCHANGING; WM_WINDOWPOSCHANGED; WM_MOVE; WM_SIZE; GetWindowDpiAwarenessContext; AreDpiAwarenessContextsEqual; GetDpiForWindow; PMv2/default DPI-awareness guidance; GetLastError
Issues/PRs inspected: N/A
What was learned: newest HDWP must be carried forward; a failed Defer chain must be abandoned without End; End has no documented rollback/transaction guarantee; visible DWM frame and positioning Window Rect differ; HWND checks are point-in-time and values recycle; creation/destruction message lifetime; PMv2 manifest and context comparison; WINDOWPOS message/application-adjustment behavior
Applicable PaneBind subsystem: owned capability validation, translation bridge, batch application, failure receipts, post-verification, DPI/monitor facts, and harness instrumentation
Code copied: NO
Code adapted: NO
Attribution required: NO for code reuse; claim-level official-document citations retained
```

Detailed findings and independently selected R1-B contracts are in
[R1B_WINDOWS_OPERATIONS_RESEARCH.md](R1B_WINDOWS_OPERATIONS_RESEARCH.md).
