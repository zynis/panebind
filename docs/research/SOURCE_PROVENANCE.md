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

## R1-C1 targeted companion-process review

Review date: 2026-08-26.

### AltSnap / AltDrag

```text
Projects: AltSnap; AltDrag
Classification: Mature active behavioral prior art; mature historical comparison
Repositories: https://github.com/RamonUnch/AltSnap ; https://github.com/stefansundin/altdrag
Commits reviewed: AltSnap 5c86416ad21e4b72844a998a746bd3bb0bee5f5d; AltDrag e2740d605b0336a3b391fec26794718864b19521
License: GNU GPL v3-or-later; REFERENCE ONLY
Files/modules inspected for R1-C1: AltSnap hooks.c, unfuck.h, README.md, altsnap.c; AltDrag hooks.c, altdrag.c, HookWindows implementation; pinned history
Issues/PR/history inspected for R1-C1: AltSnap issues #374, #572, #719; PR #573, #723; initial source/history removing AltDrag HookWindows; prior reviewed move/resize and lifetime history
What was learned: mature cross-process/cross-thread SetWindowPos and Defer behavior exists, but uses global discovery/raw HWND and point-in-time IsWindow rather than launch authority; transient process opens do not prevent PID reuse; requested geometry can be application-adjusted; explicit synthetic START/END prevents natural feedback attribution; injected subclassing is a rejected historical architecture
Applicable PaneBind subsystem: R1-C1 companion capability, process/window lifetime, cross-process placement, post-verification, feedback evidence, and no-injection boundary
Code copied: NO
Code adapted: NO
Attribution required: NO for code because no GPL code entered PaneBind; research citations retained
```

License decision: **GPL REFERENCE-ONLY**. No implementation structure,
control flow, or code was translated or adapted.

### Microsoft PowerToys / FancyZones

```text
Project: Microsoft PowerToys / FancyZones
Classification: Mature production reference
Repository: https://github.com/microsoft/PowerToys
Commit reviewed: 19c4d805321db86f3634e6968e14dbf25cbba14a
License: MIT; reference-only in R1-C1
Files/modules inspected for R1-C1: FancyZonesLib FancyZones, WorkArea, WindowUtils, MonitorUtils, LayoutAssignedWindows; FancyZones/FancyZonesApp; runner/editor launch and process-wait paths; common process/DPI utilities; UI test helpers; root LICENSE
Issues/PR/history inspected for R1-C1: issues #5947, #21849; PRs #44440, #48473, #48569, #49985; commits dd26d86580168d2e368701f7b0c4d629dc9cd9ac, ae9f241ef13737dab6f861767bbfdfca72b78475, d68980a81bb8de144bdec998a114e948bf68c563
What was learned: production cross-process placement/event processing exists but global raw HWND state is not a launch capability; target placement is per-window SetWindowPlacement and provides no Defer precedent; selected component launch paths retain process handles but do not form a window-registration handshake; destroy/topology changes abort stale consumers; elevation and mixed-DPI remain explicit risks
Applicable PaneBind subsystem: R1-C1 process launch/lifetime, external window identity, destroy cancellation, integrity/DPI risk, post-verification, and feedback evidence
Code copied: NO
Code adapted: NO
Attribution required: NO for reference-only review; future copied or substantially reused MIT material requires separate approval and preservation of the Microsoft MIT notice
```

### Microsoft Learn process, IPC, window, and security documentation

```text
Source: Microsoft Learn - Windows desktop/Win32 API documentation
Publisher/repository: Microsoft; https://learn.microsoft.com/en-us/windows/win32/
Version / SHA: N/A - live platform documentation
Terms: Microsoft Learn Terms of Use; facts were cited and paraphrased only
Date reviewed: 2026-08-26
Pages inspected for R1-C1: CreateProcessW; PROCESS_INFORMATION; process creation flags; WaitForInputIdle; WaitForSingleObject/WaitForMultipleObjects; GetExitCodeProcess; TerminateProcess; process termination; CreatePipe; anonymous pipe operations/security/inheritance; SetHandleInformation; Initialize/Update/DeleteProcThreadAttributeList; PROC_THREAD_ATTRIBUTE_HANDLE_LIST; ReadFile/WriteFile/CancelSynchronousIo; OpenProcessToken; GetTokenInformation; mandatory integrity control/UIPI; SetWindowPos; Begin/Defer/EndDeferWindowPos; IsWindow; GetWindowThreadProcessId; GetClassNameW; GetAncestor; GetWindowLongPtrW; GetWindow; SetPropW/GetPropW/RemovePropW; DestroyWindow; WM_WINDOWPOSCHANGING/CHANGED; SetWinEventHook; event constants
Issues/PRs inspected: N/A
What was learned: retained process HANDLE versus reusable PID; CreateProcess readiness limitations; WaitForInputIdle is not a handshake; two stream-framed anonymous pipes and restricted handle inheritance; bounded synchronous I/O/cleanup; per-use PID/TID/class/root/owner/marker validation; same-integrity baseline; cross-thread synchronous placement; application-adjusted WINDOWPOS; no native rollback guarantee; target-thread destruction; out-of-context feedback limitations
Applicable PaneBind subsystem: companion launch/IPC/session authority, process/window lifetime, token resolution, native translation operations, structured receipts, integrity/DPI facts, cleanup, and feedback evidence
Code copied: NO
Code adapted: NO
Attribution required: NO for code reuse; claim-level official-document citations retained
```

Detailed findings and R1-C1 decisions are in
[R1C1_COMPANION_PROCESS_RESEARCH.md](R1C1_COMPANION_PROCESS_RESEARCH.md).

## R1-C2A targeted Explorer eligibility review

Review date: 2026-08-26; provisioning-recovery supplement reviewed
2026-08-27; user-consented recovery supplement reviewed 2026-08-27.

### AltSnap / AltDrag

```text
Projects: AltSnap; AltDrag
Classification: Mature active behavioral prior art; mature historical comparison
Repositories: https://github.com/RamonUnch/AltSnap ; https://github.com/stefansundin/altdrag
Commits reviewed: AltSnap 5c86416ad21e4b72844a998a746bd3bb0bee5f5d; AltDrag e2740d605b0336a3b391fec26794718864b19521
License: GNU GPL v3-or-later; REFERENCE ONLY
Files/modules inspected for R1-C2A: AltSnap window discovery/filtering/blacklist/process/frame/move paths and history; AltDrag historical discovery/injection paths
Issues/PR/history inspected for R1-C2A: existing reviewed Explorer/filter/visible-frame/DPI/application-adjustment issues and commits; R1-C1 movement/injection history
What was learned: mature generic Explorer movement uses raw discovery and reason filters but provides no new-window provenance, Shell location authority, canonical image/session identity, or safe exact-window cleanup; injection remains rejected
Applicable PaneBind subsystem: Explorer-specific eligibility reasons, geometry/post-verification risks, and no-injection/no-global-authority boundary
Code copied: NO
Code adapted: NO
Attribution required: NO for code; citations retained
```

### Microsoft PowerToys / FancyZones

```text
Project: Microsoft PowerToys / FancyZones
Classification: Mature production and UI-test reference
Repository: https://github.com/microsoft/PowerToys
Commit reviewed: 19c4d805321db86f3634e6968e14dbf25cbba14a
License: MIT; reference-only in R1-C2A
Files/modules inspected for R1-C2A: FancyZonesTestHelper Explorer launch/baseline/cleanup; FancyZonesWindowProcessing; WindowUtils; event/lifetime/DPI paths; root LICENSE
Issues/PR/history inspected for R1-C2A: existing reviewed destroy, DPI, elevation, filtering, placement, and topology hardening history
What was learned: HWND baseline set-delta is a useful candidate signal but upstream helper does not prove exact Shell location/image/integrity or reject multiple candidates, and its close-all cleanup is unsafe for PaneBind; production eligibility remains raw-HWND policy rather than capability
Applicable PaneBind subsystem: new-versus-preexisting candidate model, Explorer allowlist reasons, runtime test safety, and prohibited cleanup behavior
Code copied: NO
Code adapted: NO
Attribution required: NO for reference-only review; future MIT reuse requires separate approval/notice
```

### Microsoft Learn Shell, Win32, file, and security documentation

```text
Source: Microsoft Learn Windows Shell/Win32 documentation
Publisher/repository: Microsoft; https://learn.microsoft.com/en-us/windows/win32/
Version / SHA: N/A - live documentation
Terms: Microsoft Learn Terms of Use; facts paraphrased/cited only
Date reviewed: 2026-08-26
Recovery supplement date reviewed: 2026-08-27
User-consented recovery supplement date reviewed: 2026-08-27
Pages inspected for R1-C2A: Developing with Windows Explorer; IShellWindows; DShellWindowsEvents; DShellWindowsEvents.WindowRegistered; DShellWindowsEvents.WindowRevoked; IShellWindows::FindWindowSW; ShellWindowFindWindowOptions; IConnectionPointContainer::FindConnectionPoint; IConnectionPoint::Advise/Unadvise; Architecture of Connectable Objects; IDispatch::Invoke/DISPPARAMS; IUnknown::QueryInterface and Rules for Implementing QueryInterface; IWebBrowser2 Navigate2/HWND/Quit; DWebBrowserEvents2 and NavigateComplete2; ShellExecute/ShellExecuteEx; SHELLEXECUTEINFO; cmd start; SHCreateItemFromParsingName; SHGetIDListFromObject; PathCreateFromUrlW; QueryFullProcessImageNameW; GetWindowsDirectoryW; CreateFileW; GetFileInformationByHandleEx/FILE_ID_INFO; IsWindow; GetWindowThreadProcessId; GetClassNameW; GetAncestor; GetWindowLongPtrW; GetWindow; IsWindowVisible; DwmGetWindowAttribute/DWMWA_CLOAKED/EXTENDED_FRAME_BOUNDS; IsIconic; IsZoomed; IVirtualDesktopManager; OpenProcess/OpenProcessToken/GetTokenInformation; SetWindowPos; GetWindowRect; MonitorFromWindow/GetMonitorInfoW/GetDpiForWindow; Single-Threaded Apartments; CoInitializeEx; MsgWaitForMultipleObjectsEx; BCryptHash and SHA-256 CNG guidance
Issues/PRs inspected: N/A
What was learned: official ShellBrowserWindow creation and ShellWindows inventory; Shell registration/revocation cookies through the DShellWindowsEvents connection point; separation of the Advise connection token from a Shell registration cookie; FindWindowSW cookie resolution with NEEDDISPATCH; canonical IUnknown object identity; no documented total ordering among registration, navigation, visibility, Quit, revocation, and HWND lifetime; NavigateComplete2 frame/asynchrony limitations; documented ShellExecute folder reuse when separate-process folder launch is disabled; ShellExecuteEx hProcess is neither guaranteed nor proof of a unique new HWND; cmd start `/separate` is a 16-bit memory-space option unsupported on 64-bit platforms, not an Explorer-frame contract; no reviewed current Microsoft contract makes explorer.exe `/n` or `/separate` a Windows 11 new-HWND guarantee; tab/frame ambiguity; baseline HWND exclusion independent of positive target location authority; filesystem URL/file identity; process image and kernel-handle lifetime; fail-closed state/security/desktop allowlist; pure single translation and exact post-verification
Applicable PaneBind subsystem: Explorer target isolation, read-only inventory, user-consented capability issuance, live eligibility, process/location identity, safe operation/restore, user-owned close, and feedback evidence
Code copied: NO
Code adapted: NO
Attribution required: NO for code reuse; claim-level citations retained
```

Recovery-specific official URLs:

- <https://learn.microsoft.com/en-us/windows/win32/shell/developing-with-windows-explorer>
- <https://learn.microsoft.com/en-us/windows/win32/api/exdisp/nn-exdisp-ishellwindows>
- <https://learn.microsoft.com/en-us/windows/win32/shell/dshellwindowsevents>
- <https://learn.microsoft.com/en-us/windows/win32/shell/dshellwindowsevents-windowregistered>
- <https://learn.microsoft.com/en-us/windows/win32/shell/dshellwindowsevents-windowrevoked>
- <https://learn.microsoft.com/en-us/windows/win32/api/exdisp/nf-exdisp-ishellwindows-findwindowsw>
- <https://learn.microsoft.com/en-us/windows/win32/api/exdisp/ne-exdisp-shellwindowfindwindowoptions>
- <https://learn.microsoft.com/en-us/windows/win32/api/ocidl/nf-ocidl-iconnectionpointcontainer-findconnectionpoint>
- <https://learn.microsoft.com/en-us/windows/win32/api/ocidl/nf-ocidl-iconnectionpoint-advise>
- <https://learn.microsoft.com/en-us/windows/win32/api/ocidl/nf-ocidl-iconnectionpoint-unadvise>
- <https://learn.microsoft.com/en-us/windows/win32/com/architecture-of-connectable-objects>
- <https://learn.microsoft.com/en-us/windows/win32/api/oaidl/nf-oaidl-idispatch-invoke>
- <https://learn.microsoft.com/en-us/windows/win32/api/oaidl/ns-oaidl-dispparams>
- <https://learn.microsoft.com/en-us/windows/win32/com/rules-for-implementing-queryinterface>
- <https://learn.microsoft.com/en-us/windows/win32/api/unknwn/nf-unknwn-iunknown-queryinterface%28refiid_void%29>
- <https://learn.microsoft.com/en-us/previous-versions/aa752134%28v%3Dvs.85%29>
- <https://learn.microsoft.com/en-us/previous-versions/mt725310%28v%3Dvs.85%29>
- <https://learn.microsoft.com/en-us/previous-versions/aa752140%28v%3Dvs.85%29>
- <https://learn.microsoft.com/en-us/previous-versions/windows/internet-explorer/ie-developer/platform-apis/aa768285%28v%3Dvs.85%29>
- <https://learn.microsoft.com/en-us/windows/win32/com/single-threaded-apartments>
- <https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shellexecutew>
- <https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shellexecuteexw>
- <https://learn.microsoft.com/en-us/windows/win32/api/shellapi/ns-shellapi-shellexecuteinfow>
- <https://learn.microsoft.com/en-us/windows-server/administration/windows-commands/start>

### Microsoft Windows SDK Shell automation declarations

```text
Source: Installed Microsoft Windows SDK
Publisher: Microsoft
Version reviewed: 10.0.26100.0
License/terms: installed Windows SDK terms; declarations inspected for facts only
Date reviewed: 2026-08-27
Files inspected: um/ExDisp.idl; um/ExDisp.h; um/ExDispid.h; um/ocidl.h
Issues/PR/history inspected: N/A; versioned installed SDK declarations
What was learned: exact IWebBrowser2/IShellWindows ABI signatures; ShellWindowFindWindowOptions values; DIID_DShellWindowsEvents and DIID_DWebBrowserEvents2; WindowRegistered/WindowRevoked and NavigateComplete2 dispinterface signatures; DISPIDs 200/201/252; ShellWindows and ShellBrowserWindow source-interface declarations; connection-point method ABI
Applicable PaneBind subsystem: Explorer provisioning lease, Shell registration sink, cookie correlation, navigation readiness, sink lifetime, and exact-object cleanup
Code copied: NO
Code adapted: NO
Attribution required: NO; no SDK implementation or declaration was copied into PaneBind
```

Detailed findings and R1-C2A decisions are in
[R1C2A_EXPLORER_ELIGIBILITY_RESEARCH.md](R1C2A_EXPLORER_ELIGIBILITY_RESEARCH.md).

## R1-C2B targeted Explorer Glue feedback review

Review date: 2026-09-03.

### AltSnap / AltDrag

```text
Projects: AltSnap; AltDrag
Classification: Mature active behavioral prior art; mature historical comparison
Repositories: https://github.com/RamonUnch/AltSnap ; https://github.com/stefansundin/altdrag
Commits reviewed: AltSnap 5c86416ad21e4b72844a998a746bd3bb0bee5f5d; AltDrag e2740d605b0336a3b391fec26794718864b19521
License: GNU GPL v3-or-later; REFERENCE ONLY
Files/modules inspected for R1-C2B: AltSnap hooks.c movement lifecycle, worker queue, sticky resize, disabled WinEvent experiment; AltDrag hooks.c lifecycle messages and historical HookWindows injection
Issues/PR/history inspected for R1-C2B: AltSnap PRs #564, #573, #580, #609; issues #507, #572, #575, #620, #725; commits 2ed9fe3dff49b260c25ce9abbd71f541dbfc1ca0, 1b64b08fb1db262b6f0a180b022243956c8a016e, 45ec7b4343ea4b8c6342cb1974933756367b022b, 7f4afe59076b70980f71af202f63609ca3ac5745, fa5c70a5029f418cac56007053fe67024cbeef86; AltDrag commits 4d90661a757976fdaa0ac31d028f5d9313ea3114, c46747bb184a72f08876a265b9024ddb59b4c073, f877c42afb8cf05eb98ec533834e775112aeea3f, 3d1fa0b22582c32631c5f3ef455b3cc76eb8338c; AltDrag lifecycle compatibility and HookWindows history; AltSnap's later removal of the inherited HookWindows injection design
What was learned: AltSnap explicitly synthesizes START/END and caused FancyZones cross-tool feedback; START timing and END edge cases can create false lifecycle; one worker/coalescing is useful architecture evidence but no generation-bound feedback ledger exists; sticky resize is not Glue Move; AltDrag injection/subclassing and synthetic lifecycle compatibility are rejected
Applicable PaneBind subsystem: event provenance limits, explicit state transitions, callback/owner separation, exact follower feedback attribution, bounded ledger, no-recursion stress, and no-injection boundary
Code copied: NO
Code adapted: NO
Attribution required: NO for code because no GPL code entered PaneBind; research citations retained
```

License decision: **GPL REFERENCE-ONLY**. No implementation structure,
algorithm, control flow, or code was translated or adapted.

### Microsoft PowerToys / FancyZones

```text
Project: Microsoft PowerToys / FancyZones
Classification: Mature production reference
Repository: https://github.com/microsoft/PowerToys
Commit reviewed: 19c4d805321db86f3634e6968e14dbf25cbba14a
License: MIT; reference-only in R1-C2B
Files/modules inspected for R1-C2B: FancyZones/FancyZonesApp hook lifecycle; FancyZonesLib FancyZones owner dispatch; WindowMouseSnap/work-area lifetime; WindowUtils placement; UI test helper; root LICENSE
Issues/PR/history inspected for R1-C2B: PRs #44440, #48473, #48569, #49433, #49985; issue #49016; commits 6c2a99dfd6a12ad98feeda0acbc663aa84865676, ae9f241ef13737dab6f861767bbfdfca72b78475, dd26d86580168d2e368701f7b0c4d629dc9cd9ac, 37d8729ac3eec734f4d000079145d6fcb40db3a5, d68980a81bb8de144bdec998a114e948bf68c563
What was learned: callback-to-owner dispatch and lifecycle-scoped hooks are mature patterns; process skipping is not feedback suppression; pinned code has no bounded receipt queue or expected-operation ledger; destroy/topology replacement must abort and terminate consumers before state replacement; stale generation and mixed-DPI history require whole-session invalidation
Applicable PaneBind subsystem: additive Glue WinEvent source, owner-thread state machine, bounded backpressure, frozen topology, invalidation, setup/cleanup isolation, and DPI/monitor safety
Code copied: NO
Code adapted: NO
Attribution required: NO for reference-only review; any future reuse requires separate approval and preservation of Microsoft MIT notice
```

### Microsoft Learn WinEvent, identity, geometry, and placement documentation

```text
Source: Microsoft Learn Windows desktop/Win32 API documentation
Publisher/repository: Microsoft; https://learn.microsoft.com/en-us/windows/win32/
Version / SHA: N/A - live documentation
Terms: Microsoft Learn Terms of Use; facts paraphrased/cited only
Date reviewed: 2026-09-03
Pages inspected for R1-C2B: SetWinEventHook; UnhookWinEvent; Out-of-Context Hook Functions; Guarding Against Reentrancy; WinEventProc; NotifyWinEvent; Event Constants; SetWindowPos; WM_WINDOWPOSCHANGING; GetWindowThreadProcessId; IsWindow; PROCESS_INFORMATION; GetWindowRect; DwmGetWindowAttribute; DWMWINDOWATTRIBUTE / DWMWA_EXTENDED_FRAME_BOUNDS
Issues/PRs inspected: N/A
What was learned: out-of-context delivery is queued, asynchronous, sequential to the installing message-loop thread, and reentrancy-sensitive; process/thread filters are noise reduction, not authority; unhook is same-thread; native timestamps have no sufficient correlation contract; START/END can be synthetically notified; LOCATION conflates position/shape/size; SetWindowPos success is not exact geometry; ASYNC placement is unsuitable for immediate verification; HWND/PID/TID are point-in-time facts; visible and positioning rectangles remain distinct non-atomic reads
Applicable PaneBind subsystem: hook lifecycle, minimal callback receipt, owner-thread drain, exact identity/generation filter, pending operation registration, feedback reconciliation, synchronous placement, post-verification, and restore
Code copied: NO
Code adapted: NO
Attribution required: NO for code; claim-level official-document citations retained
```

Official URLs reviewed:

- <https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwineventhook>
- <https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-unhookwinevent>
- <https://learn.microsoft.com/en-us/windows/win32/winauto/out-of-context-hook-functions>
- <https://learn.microsoft.com/en-us/windows/win32/winauto/guarding-against-reentrancy-in-hook-functions>
- <https://learn.microsoft.com/en-us/windows/win32/api/winuser/nc-winuser-wineventproc>
- <https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-notifywinevent>
- <https://learn.microsoft.com/en-us/windows/win32/winauto/event-constants>
- <https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowpos>
- <https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-windowposchanging>
- <https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowthreadprocessid>
- <https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-iswindow>
- <https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/ns-processthreadsapi-process_information>
- <https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowrect>
- <https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/nf-dwmapi-dwmgetwindowattribute>
- <https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwmwindowattribute>

Detailed findings and independently selected R1-C2B contracts are in
[R1C2B_EXPLORER_GLUE_RESEARCH.md](R1C2B_EXPLORER_GLUE_RESEARCH.md).

## R1-C2B UAT Fix 2 targeted processing-cadence review

Review date: 2026-09-05. This is an additive reference-only inspection; no
upstream behavior was run or relabeled as a PaneBind empirical result.

### AltSnap / AltDrag

```text
Projects: AltSnap; AltDrag
Classification: Mature active behavioral prior art; mature historical comparison
Repositories: https://github.com/RamonUnch/AltSnap ; https://github.com/stefansundin/altdrag
Commits reviewed: AltSnap 5c86416ad21e4b72844a998a746bd3bb0bee5f5d; AltDrag e2740d605b0336a3b391fec26794718864b19521
License verified: AltSnap hooks.c header and License.txt; AltDrag hooks.c header and LICENSE; GPL-3.0-or-later, REFERENCE ONLY
Files/modules actually reinspected: AltSnap hooks.c WorkerThread and movement work coalescing; AltDrag hooks.c lifecycle message compatibility paths
Issues/PR/history actually reinspected: AltSnap PR #609 and local commit diff 7f4afe59076b70980f71af202f63609ca3ac5745; path-scoped hooks.c history; existing AltDrag historical comparison retained
What was learned: one serialized behavior owner and coalescing consecutive movement work are mature patterns; their code does not supply PaneBind historical WinEvent geometry, bounded processing fairness or operation receipts
Applicable PaneBind subsystem: private Glue owner scheduling, explicit processing-time sample semantics and small callback boundary
Code copied: NO
Code adapted: NO
Attribution required: NO for implementation; research citations retained, GPL implementation structure/control flow must not be translated or adapted
```

Pinned source and history links:

- [AltSnap WorkerThread](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L693-L735)
- [AltSnap worker PR #609](https://github.com/RamonUnch/AltSnap/pull/609)
- [AltSnap worker commit](https://github.com/RamonUnch/AltSnap/commit/7f4afe59076b70980f71af202f63609ca3ac5745)
- [AltDrag pinned hooks.c](https://github.com/stefansundin/altdrag/blob/e2740d605b0336a3b391fec26794718864b19521/hooks.c)

### Microsoft PowerToys / FancyZones

```text
Project: Microsoft PowerToys / FancyZones
Classification: Mature production reference
Repository: https://github.com/microsoft/PowerToys
Commit reviewed: 19c4d805321db86f3634e6968e14dbf25cbba14a
License verified: pinned root LICENSE, MIT; reference-only in Fix 2
Files/modules actually reinspected: FancyZones/FancyZonesApp.cpp hook subscription and forwarding; FancyZonesLib/FancyZones.cpp owner lifecycle/location/destroy dispatch
Issues/PR/history actually reinspected: PR #48569 and immutable commit dd26d86580168d2e368701f7b0c4d629dc9cd9ac through GitHub rendered discussion/diff
What was learned: callbacks forward to an owner, and destroy invalidation must abort rather than finalize movement; lifecycle/invalidation boundaries must survive scheduling/coalescing
Applicable PaneBind subsystem: bounded Glue owner turn and explicit lifecycle barriers; not a native follower feedback matching algorithm
Code copied: NO
Code adapted: NO
Attribution required: NO for this reference-only review; any later code reuse requires separate authorization/provenance and Microsoft MIT notice preservation
```

Inspection limitation: historical `git show` in the existing partial clone
triggered promisor fetch, which failed with GitHub connection resets/timeouts;
that synchronization was stopped. Pinned HEAD source/license were already
available locally. The PR and commit diff were independently inspected as web
sources; no objects were reconstructed, refs moved, or fetch simulated.

- [FancyZones pinned hook wrapper](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZones/FancyZonesApp.cpp#L96-L181)
- [FancyZones pinned owner](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/FancyZones.cpp#L961-L997)
- [Destroy-abort PR #48569](https://github.com/microsoft/PowerToys/pull/48569)
- [Destroy-abort immutable diff](https://github.com/microsoft/PowerToys/commit/dd26d86580168d2e368701f7b0c4d629dc9cd9ac)

### Microsoft Learn message delivery, live geometry and COM documentation

```text
Source: Microsoft Learn Windows desktop/Win32 documentation
Publisher: Microsoft
Revision: live pages reviewed 2026-09-05; no immutable SHA asserted
Terms: Microsoft Learn Terms of Use; facts paraphrased/cited only
Pages actually inspected: SetWinEventHook; Out-of-Context Hook Functions; WinEventProc; GetWindowRect; PeekMessageW; MsgWaitForMultipleObjectsEx; Guarding Against Reentrancy in Hook Functions; Single-Threaded Apartments
Issues/PRs inspected: N/A
What was learned: WinEvent envelope has no geometry; late window query is not event-time history; PeekMessage dispatches nonqueued/internal events before visible MSG retrieval; one MSG is not one callback; MWMO_INPUTAVAILABLE preserves wake opportunity for seen queued input; STA cross-process COM and message pumping permit reentrancy
Applicable PaneBind subsystem: event-driven owner fairness; raw receipts versus processing-time samples; bounded notification rearm; removal of explicit nested Shell waiting only in private Glue path; no callback capture and no polling
Code copied: NO
Code adapted: NO
Attribution required: NO for code; official links retained beside claims
```

- [SetWinEventHook](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwineventhook)
- [Out-of-Context Hook Functions](https://learn.microsoft.com/en-us/windows/win32/winauto/out-of-context-hook-functions)
- [WinEventProc](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nc-winuser-wineventproc)
- [GetWindowRect](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowrect)
- [PeekMessageW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-peekmessagew)
- [MsgWaitForMultipleObjectsEx](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-msgwaitformultipleobjectsex)
- [Guarding Against Reentrancy](https://learn.microsoft.com/en-us/windows/win32/winauto/guarding-against-reentrancy-in-hook-functions)
- [Single-Threaded Apartments](https://learn.microsoft.com/en-us/windows/win32/com/single-threaded-apartments)

Fix 2 independent decisions, limits and research gate are recorded in
[R1C2B_EXPLORER_GLUE_RESEARCH.md](R1C2B_EXPLORER_GLUE_RESEARCH.md).
