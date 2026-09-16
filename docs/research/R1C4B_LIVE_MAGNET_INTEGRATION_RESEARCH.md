# R1-C4B focused live integration research

2026-09-15. Base `0aa13cbcc61a272fdc3ab0089b39d69a66381fe7`.
Read the existing [AquaSnap behavior reference](R1C4_AQUASNAP_BEHAVIOR_REFERENCE.md),
[four-concept model](../architecture/R1C4_MAGNET_RELATION_GLUE.md) and
[frame authority decision](../architecture/R1C3B_FRAME_AUTHORITY_DECISION.md).
No broad re-research or new application scope. All external code is reference
only; no copying, translating or adaptation of implementation code.

## Focused prior art / history

- AltSnap, mature, GPL-3.0-or-later, local pin
  `5c86416ad21e4b72844a998a746bd3bb0bee5f5d`; inspected License.txt,
  hooks.c MoveSnap entry and LetWindowKickBack. [PR 723](https://github.com/RamonUnch/AltSnap/pull/723),
  initial `524332458d1b01244f0fe211bb09d5a32174f911` and later
  `df25d36c6369bb13aa02ec83974e625fc7922c35` / PR 739 history show resize
  requests interacting with application grid constraints. Read the discussion,
  including initial objections, subsequent testing, merge and refinement;
  the initial patch is not misrepresented as the final pinned implementation.
  PaneBind does not adopt its hooks, WM_SIZING synthesis or resize algorithms.
- PowerToys/FancyZones, mature, MIT, pin
  `19c4d805321db86f3634e6968e14dbf25cbba14a`; WindowMouseSnap.cpp and LICENSE.
  [PR 48569](https://github.com/microsoft/PowerToys/pull/48569), merge
  `dd26d86580168d2e368701f7b0c4d629dc9cd9ac`: destruction aborts without a
  snap; interaction lifecycle cannot be inferred from geometry alone.

## Official platform contract

[SetWindowPos](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowpos)
documents NOSIZE/NOZORDER/NOACTIVATE and synchronous versus ASYNCWINDOWPOS.
Use one synchronous call: Move flags 21, Resize flags 20. No focus/z-order/
owner/style operation or synthetic WM_SIZING. The synchronous call may block
on another thread; no hard latency SLA or hung-Explorer support is claimed.

[WM_WINDOWPOSCHANGING](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-windowposchanging)
permits application adjustment and describes DefWindowProc size validation.
[WM_SIZING](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-sizing)
describes interactive sizing notifications, not proof that a requested rectangle
will be accepted unchanged. Therefore native success is insufficient: exact
DWM visible AND GetWindowRect positioning postverification is mandatory.

R0/C3B event and geometry evidence is reused: WinEvent has no resize-edge or
operation-generation payload. Native ticks, sequence/watermark, capability,
gesture/operation ledger and exact geometry identify known self-corrections;
ambiguous feedback cannot be treated as a new solver input. Undelivered events
or the physical origin of arbitrary programmatic movement are not claimed known.

## Independent design / tests before implementation

See [live architecture](../architecture/R1C4B_LIVE_MAGNET.md). Initial attraction
10 / release 16 are user-authorized INITIAL UAT BASELINE, not optimal values or
an upstream hysteresis algorithm. No persistent relation state.

Research gate permits pure coordinator, checked bridge and controlled owned
probe implementation. Before admitting the live Explorer write path, the probe
must pass X/Y/XY Move, four edges/four corners, exact visible/positioning,
clamp rejection and native flags without controlling any third-party window.
Real Explorer behavior remains NOT TESTED until independent review and human UAT.

## Controlled observation result

The owned native probe passed all eleven ordinary Move/Resize variants with
exact visible and positioning rectangles. A deliberately clamped request
returned native success but failed exact geometry as intended; no retry was
made. Two additional cases compose the pure solver and actual owned-source
SetWindowPos with synthetic frozen target geometry: +7 gap and -4 shallow
overlap both reach exact alignment in one call. They are not observations of
Explorer or a second real target window. Debug and Release validation is
recorded in the execution report. Native-integration prerequisite: PASS.
