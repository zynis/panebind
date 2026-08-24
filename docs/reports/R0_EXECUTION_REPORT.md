# PaneBind R0 Execution Report

Report date: 2026-08-24 (Asia/Shanghai).

Evidence labels in this report are literal: **IMPLEMENTED**, **AUTOMATED
TESTED**, **MANUALLY OBSERVED**, **NOT TESTED**, and **BLOCKED**. An upstream
issue or test never counts as a local PaneBind observation.

## 1. Project

PaneBind.

## 2. Round

R0 — Prior Art, Event & Geometry Research Baseline.

## 3. Phase

Research / architecture baseline followed by human-validation analysis. The R0
implementation is complete and unchanged by the validation round. The baseline
is not sealed because UAT 07 did not capture its requested Explorer operation;
no R1 work was started.

## 4. Branch

`codex/r0-prior-art-event-geometry`. Development did not occur on `main`.

## 5. Evaluated HEAD SHA

`2e48b967b08e2b0e6cda1ce186b176f31ba576c7` is the clean implementation HEAD
used for the final build, static audit, manifest extraction, tests, and runtime
observations. The report-only sealing commit necessarily follows that evaluated
snapshot; the handoff HEAD is reported by `git rev-parse HEAD` and in the final
handoff response rather than embedded self-referentially here.

## 6. Commit list through evaluated HEAD

```text
fc785cac1e16fc44fdd75c9e371c5aaaf95e0b80 chore: initialize snapweave research baseline
d6618d146adcf5e849cedbd52419e736544cd69c docs: establish project charter and architecture baseline
c7930438003fe19f4259baf0d4bf7120d8727fcd test: add platform-neutral geometry baseline
12048952d1f91fb87a39453d03cd4b4adba2a632 docs: establish prior-art and windows research baseline
9144a9324e49abd761358f93d68152013d214d3f test: strengthen normalized core invariants
2e48b967b08e2b0e6cda1ce186b176f31ba576c7 feat: add windows event observer
```

## 7. Build environment

- Windows 11 Home China, x64, version `10.0.26200`, full build `26200.9168`,
  DisplayVersion `25H2`.
- Visual Studio Community 2026 `18.5.1`.
- MSVC x64 `19.50.35729`.
- Windows SDK `10.0.26100.0`.
- CMake `4.2.3-msvc3`.
- Git `2.53.0.windows.1`.
- Hardware display available to R0: one internal 3072×1920 panel at 200%
  configured scaling.

## 8. Build commands

The CMake executable bundled with Visual Studio was used because `cmake` was
not on the shell `PATH`.

```powershell
& '<VS>\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' `
  -S . -B build -G 'Visual Studio 18 2026' -A x64 -DBUILD_TESTING=ON
& '<VS>\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' `
  --build build --config Debug --parallel

& '<VS>\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' `
  -S . -B out/release -G 'Visual Studio 18 2026' -A x64 -DBUILD_TESTING=OFF
& '<VS>\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' `
  --build out/release --config Release --parallel
```

Debug and Release builds completed without compiler warnings. A tests-disabled
configuration built only the library/observer and registered zero tests as
expected.

## 9. Test command

```powershell
& '<VS>\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' `
  --test-dir build -C Debug --output-on-failure
```

## 10. Automated test results

`3/3` passed in the final run:

- `geometry` — normalized coordinates, size, empty rectangles, positive-area
  intersection/overlap, negative virtual-screen coordinates, edge distances,
  same-axis validation, and tolerance behavior;
- `core-model` — required nonempty session window identity, required nonzero
  process identity, typed normalized event construction, and UTF-8 storage; and
- `windows-text-encoding` — strict UTF-16→UTF-8 conversion, supplementary
  characters, invalid-surrogate rejection, non-ASCII JSON preservation, and
  JSON control-character escaping.

Status: **AUTOMATED TESTED**.

## 11. PRIOR_ART_GATE result

```text
PRIOR_ART_GATE = PASS
```

Both mandatory projects were actually inspected at immutable revisions, their
licenses and histories were recorded, design lessons were consolidated, and
the source-provenance ledger was completed before observer implementation.

## 12. AltSnap reviewed SHA/tag

- SHA: `5c86416ad21e4b72844a998a746bd3bb0bee5f5d`.
- Describe: `1.68-48-g5c86416`.
- Nearest release tag: `1.68` →
  `20307468b832b40998e1e65e96ab6eb28293ee62`.
- License boundary: GPLv3-or-later reference-only.

## 13. AltSnap key findings

- Current AltSnap uses low-level keyboard/mouse hooks that execute back in its
  process; its DLL is not injected into third-party processes.
- Upstream AltDrag's optional `WH_CALLWNDPROC`/subclassing design did inject and
  required a second-bitness helper; AltSnap removed that architecture before
  its first public source revision.
- Move/resize, window policy, candidate enumeration, monitor/DPI/frame
  conversion, native operations, restore state, and UI share a Windows-heavy
  mutable state machine.
- Monitor and window edges enter unified snap comparisons, while occlusion,
  z-order, min/max state, application-adjusted resize, and mixed-DPI behavior
  introduce important failure modes.
- The history contains unresolved hook/input-state and mixed-DPI risks; AltSnap
  has no automated test target at the reviewed revision.

## 14. AltSnap decisions worth adopting

- Separate positioning bounds from visible frame bounds.
- Make edge visibility, filter reasons, min/max constraints, application resize
  response, and callback backpressure explicit evidence.
- Preserve the no-injection decision and derive future regression matrices from
  historical defects.

These are independently designed principles, not adapted GPL code.

## 15. AltSnap designs not adopted and why

- No low-level input control, placement calls, zones, sticky resize, restore
  properties, or synthetic events: all are outside R0.
- No injected legacy HookWindows architecture: it violates project policy.
- No 32 ms polling fallback: high-frequency polling is prohibited.
- No global-HWND monolith or mutable threshold scan: it conflicts with the
  platform-neutral/testable core goal.
- No code was copied, translated, mechanically rewritten, or adapted because
  AltSnap is GPL reference-only.

## 16. FancyZones reviewed SHA/tag

- PowerToys SHA: `19c4d805321db86f3634e6968e14dbf25cbba14a`.
- No exact tag points to the reviewed commit; the SHA is authoritative.
- License: MIT.

## 17. FancyZones key findings

- `FancyZonesApp` is a class under `FancyZones/`, not a directory. It owns hook
  ingress/lifetime and hands events to the Windows-heavy `FancyZonesLib`.
- Out-of-context callbacks post messages to an owning hidden window, serializing
  state away from the reentrant hook callback.
- Location-change subscription is narrowed to active gestures in the product,
  while work-area/topology replacement cancels active state first.
- Work area, layout, native monitor token, persistent identity, virtual desktop,
  storage, overlay, filtering, and operations are distinct production concerns,
  though they are not separated into a portable core.
- The inspected tree has broad native/unit/UI/fuzz test surfaces and valuable
  production fixes for destroy-during-drag, teardown ordering, stale topology
  references, DPI context mismatch, layout cache refresh, and monitor identity.

## 18. FancyZones decisions worth adopting

- Narrow out-of-context WinEvent ingress and owner-thread handoff.
- Explicit topology generation/cancellation and reason-bearing window policy.
- Coordinate-space/DPI provenance and historical-bug regression tests.
- A thin native ingress boundary separate from UI and future operations.

R0 keeps the location hook continuously registered because it is researching
application-originated and potentially unpaired geometry changes. The finite
queue and explicit overflow record keep this decision auditable.

## 19. FancyZones designs not adopted and why

- No keyboard/mouse control, snapping, overlays, placement operations,
  telemetry, persistence, or layout engine: outside R0 or contrary to charter.
- No Win32 registered message or native handle enters core.
- No null `HMONITOR` sentinel for combined topology, guessed virtual desktop,
  volatile monitor-number identity, raw topology pointers, or constant hash.
- No PowerToys code was copied/adapted; MIT reuse would still require a separate
  provenance/notice decision.

## 20. Other prior art reviewed

- AltDrag at `e2740d605b0336a3b391fec26794718864b19521`, GPLv3-or-later,
  inspected only to verify the former injected HookWindows architecture.
- Microsoft Learn Windows desktop documentation: 33 cited official pages for
  enumeration, hooks, callback lifetime/reentrancy, event/object identifiers,
  filtering, process metadata, window/frame geometry, DPI awareness, monitor,
  and work-area semantics.

## 21. License/provenance status

Status: **PASS**. `SOURCE_PROVENANCE.md` records repository, revision/tag,
license, review date, inspected modules/issues/PRs, lessons, subsystem, copied,
adapted, and attribution fields for every external project inspected.

```text
External code copied: NO
External code adapted: NO
R0 code attribution required: NO
```

## 22. Observer supported events

Status: **IMPLEMENTED**.

- `EVENT_SYSTEM_MOVESIZESTART` → typed core `MoveResizeStarted`;
- `EVENT_OBJECT_LOCATIONCHANGE` → typed core `GeometryChanged` after
  `OBJID_WINDOW`/`CHILDID_SELF`/root/identity/policy checks; and
- `EVENT_SYSTEM_MOVESIZEEND` → typed core `MoveResizeEnded`.

Two hook registrations cover exactly those constants with
`WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS`. Hooks are installed before
the census and unhooked on their owner thread. There is no injection.

## 23. Actual event sequences observed

This section records the original execution evidence at `2e48b967...`, before
the deliberate ten-file UAT matrix. Its pre-UAT `NOT TESTED` labels are retained
as historical state and superseded by section 41 and
[`R0_HUMAN_VALIDATION_REPORT.md`](R0_HUMAN_VALIDATION_REPORT.md).

- **MANUALLY OBSERVED:** final Release one-second timed run: PMv2 verified,
  hooks complete, census complete, zero event records, hook shutdown complete,
  observer shutdown complete, exit `0`, `689/689` JSON lines parsed, no overflow,
  and strict sequence ordering. Zero events is reported as zero, not success for
  the three interactive event types.
- **MANUALLY OBSERVED:** an earlier Debug observation interval in which Notepad
  was launched contained raw `EVENT_OBJECT_LOCATIONCHANGE` receipt sequences
  `683..795` (113 receipts). The sample contained duplicated native timestamps and correctly
  rejected non-window object IDs, child/non-root HWNDs, and invisible windows.
  It produced no move/size start or end. This is evidence of location-event
  noise/filter requirements, not an interactive move/resize acceptance test.
- `EVENT_SYSTEM_MOVESIZESTART`: **NOT TESTED** — no manual window gesture was
  performed.
- Interactive top-level `EVENT_OBJECT_LOCATIONCHANGE`: **NOT TESTED** for a
  manually moved/resized target.
- `EVENT_SYSTEM_MOVESIZEEND`: **NOT TESTED** — no manual window gesture was
  performed.
- Balanced-pair/order claims: **NOT TESTED** and not assumed.

## 24. Application observation matrix

The snapshot column is from inspected Release `--enumerate-only` JSON. Window
titles/paths are intentionally omitted from this report because logs can be
sensitive.

| Application | Environment | Snapshot | Move/resize event sequence |
| --- | --- | --- | --- |
| Windows Explorer | Running; 11 accepted windows (7 minimized) | **MANUALLY OBSERVED** | **NOT TESTED** |
| Notepad | Installed; 1 accepted normal window | **MANUALLY OBSERVED** | **NOT TESTED** |
| Windows Terminal | Installed, not running | **NOT TESTED — application not running** | **NOT TESTED** |
| Microsoft Edge | Running; 1 accepted maximized window | **MANUALLY OBSERVED** | **NOT TESTED** |
| Google Chrome | Running; 1 accepted minimized window | **MANUALLY OBSERVED** | **NOT TESTED** |
| VS Code | Running; 3 accepted windows | **MANUALLY OBSERVED** | **NOT TESTED** |
| Excel | Running; 2 accepted minimized windows | **MANUALLY OBSERVED** | **NOT TESTED** |
| Power BI Desktop | Not found in inspected process/AppX/App Paths/uninstall/common-path sources; all-users AppX query denied | **NOT TESTED — environment unavailable** | **NOT TESTED** |

The later UAT matrix manually observed native move/resize lifecycles for
Explorer, VS Code, and Excel. This table remains the original census matrix;
current interaction results are in the human-validation report.

Notepad was launched for the matrix and restored an existing unsaved tab. It
was intentionally left open; no close/termination action was attempted, so no
user data was put at risk.

## 25. PositioningRect vs VisibleRect findings

**MANUALLY OBSERVED:** every one of the 19 accepted target-application windows
in the inspected census had different positioning and DWM visible-frame bounds.
Examples at 200% scaling:

- normal Explorer/VS Code windows commonly had 11-pixel left/right/bottom
  positioning-frame overhang and zero top delta;
- normal Notepad had 10-pixel left/right/bottom deltas;
- maximized Edge had 13-pixel deltas on all four edges; and
- minimized windows used off-screen positioning coordinates around `-32000`,
  with frame deltas that varied.

The native log preserves minimized rectangles as evidence; normalized snapshots
omit minimized window rectangles so they cannot masquerade as current comparable
geometry. No assumption equates the two rectangle sources.

## 26. DPI findings

- The executable's extracted `RT_MANIFEST #1` contains `PerMonitorV2` and
  `asInvoker`; there is no redundant runtime DPI-awareness setter.
- Runtime thread context reported `verified_per_monitor_v2`.
- One 28-candidate census contained 20 Per-Monitor-V2, 7 Per-Monitor, and 1
  system-aware target; all reported target-window DPI `192` on this system.
- `GetDpiForWindow` remains labeled target-window DPI. No DPI-unaware accepted
  target was present, so the documented 96-DPI behavior is **NOT TESTED**.
- A separate DPI-unaware environment probe saw 1536×960 logical desktop/work
  coordinates while the PMv2 observer saw 3072×1920 monitor and 3072×1824 work
  coordinates. This supports, but does not universalize, the requirement to
  label caller awareness and coordinate space.

## 27. Multi-monitor findings

Only one active internal monitor was available:

```text
monitor: (0,0)-(3072,1920)
work area: (0,0)-(3072,1824)
primary: true
configured scaling: 200%
```

Multi-monitor, negative-origin native topology, monitor crossing, mixed DPI,
dock/reconnect, and straddling-window behavior are **NOT TESTED — environment
unavailable**. Negative coordinates are **AUTOMATED TESTED** only in pure core
geometry.

## 28. Architecture decisions made

- Windows is the first adapter, not product identity.
- `src/core/` contains platform-neutral geometry, immutable validated identity,
  optional normalized snapshots, and three typed normalized events.
- Win32 handles/types/constants, raw event metadata, styles, cloaking, raw DPI,
  and process queries remain in `src/platform/windows/`.
- Callback-time PID/window-thread/root identity is queued with each raw receipt;
  owner-thread enrichment requires a match and rechecks PID/thread afterward.
- The callback uses a finite 4096-receipt queue and posts one coalesced owner
  notification; overflow and post failures receive explicit diagnostics.
- Snapshot capture is best-effort and non-atomic; optional fields and typed
  field errors replace fabricated defaults.
- Native UTF-16 is strictly transcoded to UTF-8 and JSON escaped.
- Observation and future operations are different dependency directions. No
  operations interface or behavior engine exists in R0.

## 29. Architecture questions still unresolved

- Whether continuous or move-scoped location subscription gives the best
  coverage/backpressure balance for real applications.
- Whether lifecycle events such as destroy/minimize/restore are necessary to
  close demonstrated state-machine gaps.
- A durable window-generation identity beyond session HWND/PID/thread
  correlation.
- Monitor identity, topology generations, virtual desktops, mixed-DPI unit
  conversion, and future operation-result semantics.
- Event coalescing policy and acceptable queue capacity under actual live
  resize/input load.
- Output redaction/rotation for a future user-facing diagnostic mode; R0 writes
  JSON Lines only to caller-controlled stdout.

## 30. Does core contain any Windows-specific type?

**NO.** Static search found no `HWND`, `HMONITOR`, `RECT`, `POINT`, `DWORD`,
Windows header, or native event constant under `src/core/`.

## 31. Is polling present?

**NO.** There is no resident geometry polling loop or sleep-based event source.
The observer blocks on the Win32 message queue. Bounded buffer growth and a
one-shot user-requested observation timer are not polling.

## 32. Is DLL injection present?

**NO.** Hooks are out of context. Static search found no remote-thread/memory,
injected hook, or third-party subclassing path.

## 33. Is third-party window control present?

**NO.** Static search found no placement/move/resize API, target message send,
or global input hook. The observer queries and logs only.

## 34. Is Glue implemented?

**NO.** Intentionally prohibited in R0.

## 35. Is Snap implemented?

**NO.** Intentionally prohibited in R0.

## 36. Known limitations

- At the original execution snapshot, interactive movement/resizing had not
  been performed. The later UAT matrix supersedes that historical limitation
  for mouse move/resize; see section 41. Keyboard and other unrun paths remain
  unverified.
- Current-desktop hooks and `EnumWindows` do not prove coverage of every app
  model, virtual desktop, elevated target, or protected process.
- Object/child rejection records share the finite event queue with accepted
  receipts. Overflow is honest and explicit, but high-noise workloads still
  need measurement.
- HWND remains ephemeral; PID/thread correlation reduces but cannot eliminate
  same-thread handle-reuse ambiguity.
- Process path is optional and can be denied; emitted paths/titles are sensitive.
- Console-close completion is implemented with a completion event and bounded
  handler wait, but an actual console-close gesture is **NOT TESTED**.
- Linux/macOS compilation is **NOT TESTED**; no fake adapters were created.

## 37. Untested scenarios

- Keyboard move/resize, shell snap, minimize, app-originated geometry, close,
  crash, and hung-target missing-pair sequences. Mouse move/resize and VS Code
  maximize/restore are now manually observed; Explorer maximize/restore test 07
  requires retest.
- 32-bit targets, elevation/integrity boundaries, packaged-host coverage, remote
  desktop, lock/sleep, and virtual desktops.
- Multiple displays, negative native origins, mixed scale, cross-monitor moves,
  straddling/minimized monitor association, dock/KVM/reconnect, and topology
  change during a gesture.
- DPI-unaware target behavior and positioning/DWM unit congruence across mixed
  contexts.
- Queue overflow/reentrancy stress and broken-pipe behavior beyond the nonzero
  exit check.

## 38. Git status

At evaluated implementation HEAD `2e48b967...`:

```text
## codex/r0-prior-art-event-geometry
```

The worktree was clean. Build/output directories are ignored. The report-only
seal is committed afterward and rechecked before handoff.

## 39. Local/remote divergence at the original execution snapshot

No Git remote was configured at the original evaluated implementation snapshot,
so divergence at that point was **N/A**, not zero. Local `main` pointed to
`fc785cac...` and the R0 branch was five commits ahead of it. A later repository
identity round created `origin`; current divergence is reported in the final
handoff rather than rewriting the historical result here.

## 40. Suggested R1 research questions

R1 is not started. Suggested research-only questions are:

1. What event order, duplication, missing-pair, object-ID, and latency behavior
   occurs for each application/state/input path in a deliberate manual lab?
2. Should location hooks be continuously active, gesture-scoped, or hybrid once
   real missing-start/application-originated behavior and queue load are known?
3. Which lifecycle events are minimally necessary for cancel/destroy/minimize
   correctness, and what invariant justifies each?
4. What normalized coordinate contract survives DPI-unaware, system-aware, and
   Per-Monitor targets across mixed-DPI negative-origin displays?
5. What session/generation identity and topology model safely handles HWND reuse,
   monitor reconnect, virtual desktops, and delayed events?
6. What error/result/feedback-suppression contract must a future operations
   adapter satisfy before any third-party window manipulation is authorized?

## 41. Human validation update

The 2026-08-24 validation round evaluated the clean source tree at
`b50a1c9b2d94996f10b4d700e0c9d71f057e140f`. Ten ignored local JSONL files were
parsed without exposing titles, user paths, or raw records. All ten had complete
startup/shutdown diagnostics, continuous observer sequences, and no queue loss
or notification-failure diagnostic.

Results:

- native Explorer, VS Code, and Excel move/resize: **MANUALLY OBSERVED**;
- Explorer AltSnap move/resize: **MANUALLY OBSERVED**, lifecycle-equivalent to
  the corresponding native Explorer sessions;
- VS Code maximize/restore: **MANUALLY OBSERVED** as location/state changes
  without START/END;
- intended Explorer maximize/restore test 07: **RETEST_REQUIRED** because the
  file captured Excel maximize/restore instead of an identifiable Explorer
  target;
- Debug build: **PASS**;
- Release build: **PASS**; and
- automated tests: **PASS (3/3)**.

No implementation code changed. Exact per-test counts, geometry, filtering,
queue/hook review, privacy handling, limitations, and the seal decision are in
[`R0_HUMAN_VALIDATION_REPORT.md`](R0_HUMAN_VALIDATION_REPORT.md).

## R0 acceptance summary

```text
PRIOR_ART_GATE = PASS
R0_IMPLEMENTATION = COMPLETE
R0_AUTOMATED_TESTS = PASS (3/3)
R0_RUNTIME_CENSUS = MANUALLY OBSERVED
R0_INTERACTIVE_MOVE_RESIZE_MATRIX = MANUALLY OBSERVED
R0_MAXIMIZE_RESTORE_MATRIX = PARTIAL / RETEST_REQUIRED (07)
R0_BASELINE = NOT SEALED
RETEST_REQUIRED = 07
REVALIDATION_REQUIRED = 07
R1_STARTED = NO
```
