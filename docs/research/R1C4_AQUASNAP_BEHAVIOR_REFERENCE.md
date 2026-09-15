# R1-C4 behavior reference and research gate

Reviewed 2026-09-15. Pure C4B and topology-neutral C4A only. No binary inspection,
reverse engineering, source copying, translation or adaptation.

## AQUASNAP_OFFICIAL_BEHAVIOR

| Official source | Documented behavior, not internal algorithms |
| --- | --- |
| [Product](https://www.nurgo-software.com/products/aquasnap) | Separates docking, magnetic alignment, stretching, Ctrl movement/resize; low-memory/CPU native-code claim is the vendor's, not a PaneBind measurement. |
| [AquaMagnet](https://help.nurgo-software.com/article/96-aquasnap-configuration-aquamagnet), 2020-05-21 | Screen edges, outer/inner window edges, adjacent-window corners; configurable attraction field, default 10 pixels; progressive attenuation during fast movements and configurable speed threshold. |
| [AquaGlue](https://help.nurgo-software.com/article/97-aquasnap-configuration-aquaglue), 2020-02-04 | Ctrl plus Move/Resize acts on adjacent windows. |
| [AquaSnap](https://help.nurgo-software.com/article/94-aquasnap-configuration-aquasnap), 2020-12-02 | Smart Docking considers already docked windows' sizes, not necessarily halves/quarters; separate from window-to-window Magnet. |
| [AquaStretch](https://help.nurgo-software.com/article/95-aquasnap-configuration-aquastretch), 2017-08-30 | Double-click an edge/corner to extend toward the closest window or screen; separate from continuous resize alignment. |
| [Appearance](https://help.nurgo-software.com/article/100-aquasnap-configuration-appearance), 2017-08-30 | Docking preview rectangles and screen-border position glyphs; does not establish a relation-lock UI. |
| [v1.16.0](https://www.nurgo-software.com/company/news/13-aquasnap/71-aquasnap-v1-16-0-released), 2015-12-10 | Added inner-edge and adjacent-corner snapping and stacked-window movement. Public release history, not source history. |

[AquaSnap EULA](https://www.nurgo-software.com/company/eula/80-aquasnap):
proprietary, official observable-behavior reference only. No public commit,
internal algorithm, hysteresis values or persistence scheme asserted.

## Mature open-source and history

- AltSnap GPL-3.0-or-later, REFERENCE ONLY, pinned
  `5c86416ad21e4b72844a998a746bd3bb0bee5f5d`: inspected hooks.c
  MoveSnap/ResizeSnap/enumeration and License.txt. Separate movement/edge resize
  and cached enumeration are observed lessons, not sources of PaneBind code.
  [PR 682](https://github.com/RamonUnch/AltSnap/pull/682) fixes issue 681:
  merge `c8e141656a8ae17c308a660caf47f020bd29f31a`, commit
  `0b52dafa7e05aeb9987a7d40da43e7a0668b64d8` addresses hidden borders.
  Also read [issue 681](https://github.com/RamonUnch/AltSnap/issues/681): a
  partially visible background window exposes some but not all eligible edges.
  Its reproduction was inspected, not executed on the user's desktop.
  Read `397b84b2db114de979d428c3cec0cddec2a339ae` allocation-before-write
  fix diff. Lessons: adapter eligibility/occlusion and bounded safe storage.
- PowerToys/FancyZones MIT, mature production reference, REFERENCE ONLY,
  `19c4d805321db86f3634e6968e14dbf25cbba14a`: inspected
  FancyZonesLib/WindowMouseSnap.cpp and LICENSE. Start/update/end/abort and
  work-area selection are distinct from magnetic geometry.
  [PR 48569](https://github.com/microsoft/PowerToys/pull/48569), merge
  `dd26d86580168d2e368701f7b0c4d629dc9cd9ac`: destroyed windows abort without
  snapping; stale drag state can swallow keys. Preserve C4A lifecycle and
  feedback isolation, do not add another input hook.

## Official platform and empirical boundary

[WinEvent constants](https://learn.microsoft.com/en-us/windows/win32/winauto/event-constants)
describe movement/size changes, not participating resize edges.
[GetWindowRect](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowrect)
distinguishes DPI-virtualized positioning bounds from DWM visible bounds.
Retain current PMv2 visible-coordinate authority; no native types/reads/writes
in Core. Future inputs must share the same coordinate space.

PANE_BIND_DESIGN_INFERENCE: the user-specified equations and existing R1-A
adjacency support deterministic pure experiments; live Explorer timing,
target freshness, occlusion and native Magnet correction remain NOT TESTED.
See [original design/test plan](../architecture/R1C4_MAGNET_RELATION_GLUE.md).
Research gate for this bounded pure implementation: PASS. Live gate NOT OPENED.
Current C4A human log is forensic evidence, not Magnet or AquaSnap UAT.
