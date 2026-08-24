# Source Provenance Register

Review date for R0 entries: 2026-08-24.

This register covers external projects and documentation actually inspected for
SnapWeave. A research citation does not authorize code reuse. No external code
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
Applicable SnapWeave subsystem: future Windows input research; Windows window/monitor/frame adapter; normalized snapshots/events; pure geometry candidate model; filtering policy; reliability test matrix
Code copied: NO
Code adapted: NO
Attribution required: NO for code because no code entered SnapWeave; research citations retained
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
Applicable SnapWeave subsystem: hook/input architecture and no-injection threat boundary
Code copied: NO
Code adapted: NO
Attribution required: NO for code because no code entered SnapWeave; research citations retained
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
Applicable SnapWeave subsystem: Windows event adapter; window snapshot/filtering; monitor/work-area/DPI adapter; normalized event model; core geometry boundary; lifecycle and test strategy
Code copied: NO
Code adapted: NO
Attribution required: NO for this reference-only review; any future copied or substantially reused MIT material must preserve the PowerToys MIT notice and be approved/recorded separately before entering SnapWeave
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
Applicable SnapWeave subsystem: Windows enumeration/events/filtering/snapshot/geometry/DPI/monitor/process-metadata adapter and normalized event mapping
Code copied: NO
Code adapted: NO
Attribution required: NO for code reuse; inline official-document citations retained
```

Exact page URLs and claim-level citations are retained in
[R0_WINDOWS_EVENT_MODEL.md](R0_WINDOWS_EVENT_MODEL.md).
