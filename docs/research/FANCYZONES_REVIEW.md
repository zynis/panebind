# Microsoft PowerToys / FancyZones prior-art review

## Review record and conclusion

`FANCYZONES_RESEARCH_TRACK = PASS`

This result covers only the mandatory FancyZones track. The aggregate R0
`PRIOR_ART_GATE` also depends on the AltSnap review and the completed source
provenance ledger.

| Field | Reviewed value |
|---|---|
| Project | Microsoft PowerToys, FancyZones |
| Classification | Mature, actively maintained production reference |
| Repository | [microsoft/PowerToys](https://github.com/microsoft/PowerToys) |
| Immutable revision | [`19c4d805321db86f3634e6968e14dbf25cbba14a`](https://github.com/microsoft/PowerToys/tree/19c4d805321db86f3634e6968e14dbf25cbba14a) |
| Revision date | 2026-08-23 15:43:22 -05:00 |
| Tag | No tag points at the reviewed commit; the SHA is authoritative |
| Review date | 2026-08-24 |
| License | [MIT](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/LICENSE), Copyright Microsoft Corporation |
| Inspection method | Blob-filtered Git clone, detached checkout at the SHA, source/test inspection, path-scoped `git log`, and review of linked issues, PRs, and Microsoft documentation |
| Code copied | NO |
| Code adapted | NO |
| Local PowerToys build/tests | NOT TESTED; this was a source/history review, not a PowerToys validation run |
| Blocker | None |

The most reusable FancyZones lesson is not a snapping algorithm. It is the
event and lifetime discipline accumulated around a real Windows daemon:
capture native events cheaply, leave the reentrant callback immediately,
serialize state changes on an owning thread, make topology replacement cancel
in-flight gestures first, and treat DPI coordinate space and monitor identity
as explicit data. FancyZones is not a suitable platform-neutral core template:
`FancyZonesLib` deliberately combines Win32 types, window operations,
persistence, overlays, settings, and behavior.

Throughout this review:

- **FACT** means verified in the pinned source, repository history, an official
  Microsoft document, or the linked issue/PR state.
- **INFERENCE** means an architectural interpretation of those facts.
- **RECOMMENDATION** is a PaneBind decision proposed from that evidence.
- An issue reporter's runtime observation is not presented as a PaneBind
  observation.

## Literal source tree at the reviewed revision

The reviewed tree is
[`src/modules/fancyzones`](https://github.com/microsoft/PowerToys/tree/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones).
The literal project paths relevant to this review are:

- `FancyZones/` — the `PowerToys.FancyZones.exe` wrapper. The
  `FancyZonesApp` class is in
  [`FancyZones/FancyZonesApp.h`](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZones/FancyZonesApp.h)
  and
  [`FancyZones/FancyZonesApp.cpp`](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZones/FancyZonesApp.cpp).
  There is no `FancyZonesApp/` directory at this SHA.
- `FancyZonesLib/` — the C++ backend, including event dispatch, work areas,
  layouts, filtering, persistence, overlays, and window operations.
- `FancyZonesModuleInterface/` — the PowerToys Runner-facing DLL that starts
  and stops the separate FancyZones executable.
- `editor/FancyZonesEditor/` — the WPF layout editor executable.
- `FancyZonesEditorCommon/` — editor data contracts and file I/O shared by
  managed editor-related projects.
- `FancyZonesCLI/` — command-line layout operations.
- `FancyZonesTests/UnitTests/` — native C++ unit tests.
- `FancyZones.UITests/` and `FancyZones.UITests.Next/` — legacy and migrated
  backend UI/integration tests, both present at this revision.
- `FancyZonesEditor.UnitTests/`, `FancyZonesEditor.UITests/`, and
  `FancyZonesEditor.UITests.Next/` — managed editor tests.
- `FancyZones.FuzzTests/` — managed custom-layout deserialization fuzz target.

The repository's own
[`doc/devdocs/modules/fancyzones.md`](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/doc/devdocs/modules/fancyzones.md)
describes conceptual `Editor`, `Runner`, and `Settings` areas, but those are not
the literal directory names at the pinned revision. Source-tree evidence is
used here when the overview and tree differ.

## Component boundaries and process model

### Verified flow

```text
PowerToys Runner
  -> FancyZonesModuleInterface DLL
  -> PowerToys.FancyZones.exe / FancyZonesApp
       -> native hooks and process lifecycle
       -> IFancyZones / IFancyZonesCallback boundary
  -> FancyZonesLib hidden message window
       -> serialized event/state handling
       -> WorkAreaConfiguration -> WorkArea -> Layout
       -> filtering, persistence, overlay, and window operations

PowerToys.FancyZonesEditor.exe
  <-> JSON files + named/update events
  <-> FancyZonesLib reload/refresh handlers
```

**FACT.**
[`FancyZonesModuleInterface/dllmain.cpp`](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesModuleInterface/dllmain.cpp)
is the Runner adapter. It starts `PowerToys.FancyZones.exe`, passes the Runner
PID, signals named toggle/exit events, waits briefly on shutdown, and retains a
process handle. The executable's
[`main.cpp`](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZones/main.cpp)
enforces a single instance, watches the parent process, initializes logging,
policy, COM/WinRT and tracing, constructs `FancyZonesApp`, then runs a message
loop.

**FACT.** `FancyZonesApp` is a thin native ingress/lifecycle shell. The COM-like
interfaces in
[`FancyZonesLib/FancyZones.h`](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/FancyZones.h#L10-L60)
keep hook ownership outside the `FancyZones` implementation while exposing
callbacks for WinEvents and keyboard input.

**FACT.** `FancyZonesLib` is a backend boundary, not a pure core boundary. Its
project includes `FancyZones.cpp`, `WorkArea`, `Layout`, monitor and virtual
desktop discovery, settings and JSON stores, window filtering/properties,
mouse and keyboard snapping, overlay rendering, and telemetry. Public types
and algorithms use `HWND`, `HMONITOR`, `RECT`, `POINT`, `GUID`, and Win32
messages. `WorkArea` itself owns a hidden overlay window and exposes `Snap` and
`Unsnap`; see
[`WorkArea.h`](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/WorkArea.h#L11-L84).

**FACT.** The visual layout editor is a separate process. Before launch,
`FancyZonesLib` serializes monitor/work-area parameters, launches
`PowerToys.FancyZonesEditor.exe`, and observes its termination; see
[`FancyZones::ToggleEditor`](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/FancyZones.cpp#L801-L850).
The editor and backend have separate read/write handlers over shared
configuration files. Registered file-update messages cause the backend to
reload stores and refresh active layouts
([dispatch](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/FancyZones.cpp#L998-L1019)).

**INFERENCE.** The process split keeps the always-running hook/backend path
independent of the comparatively heavy editor UI and makes editor replacement
possible. Shared files and update events also create multiple cached views,
which require explicit refresh and canonical-source rules.

**RECOMMENDATION.** PaneBind should adopt the thin native ingress and explicit
UI/backend boundary, but not call a Windows-heavy library its core. R0's
platform-neutral geometry and normalized events must remain in `src/core/`;
native handles, hooks, monitor discovery, DPI contexts, and future window
operations remain in the Windows adapter.

## Window-event processing

### Hook set and event lifetime

**FACT.** `FancyZonesApp::InitHooks` installs a low-level keyboard hook and
seven single-event WinEvent hooks:

- `EVENT_SYSTEM_MOVESIZESTART`
- `EVENT_SYSTEM_MOVESIZEEND`
- `EVENT_OBJECT_NAMECHANGE`
- `EVENT_OBJECT_UNCLOAKED`
- `EVENT_OBJECT_SHOW`
- `EVENT_OBJECT_CREATE`
- `EVENT_OBJECT_DESTROY`

Every WinEvent hook uses `WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS` with
process and thread filters set to zero
([source](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZones/FancyZonesApp.cpp#L82-L113)).
`EVENT_OBJECT_LOCATIONCHANGE` is different: it is subscribed only after a
move/size start and unhooked at move/size end
([source](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZones/FancyZonesApp.cpp#L126-L159)).
This limits a noisy global event to an active gesture rather than using
continuous polling.

Microsoft's
[`SetWinEventHook` documentation](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwineventhook)
confirms that out-of-context callbacks are not mapped into event-generating
processes, are queued across the process boundary, require a message loop on
the installing thread, and can still be reentered. No FancyZones WinEvent DLL
is injected into third-party processes.

### Callback-to-owner-thread handoff

**FACT.** The WinEvent callback does not perform snapping. `FancyZones` maps
native events to registered private window messages and calls `PostMessageW`
on its hidden tool window
([source](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/FancyZones.cpp#L252-L286)).
Create/show/uncloak and destroy are forwarded only for `OBJID_WINDOW`.
Virtual-desktop work is likewise posted because the source explicitly notes
that the WinHook callback is reentrant
([source](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/FancyZones.cpp#L470-L476)).

The hidden-window procedure serially dispatches start, update, end, create,
destroy, display/work-area, settings, and file-change messages
([source](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/FancyZones.cpp#L852-L1019)).
On start it captures the dragged `HWND`, creates `WindowMouseSnap`, and enables
drag state. Location changes update the active snapper from the physical
cursor and monitor. End finalizes and clears the snapper. Destroy of the
dragged window aborts instead of snapping a dead handle.

**INFERENCE.** This is an event-envelope/queue boundary even though the
envelope is a Win32 registered message rather than a platform-neutral type.
It minimizes work in the hook callback and establishes one owning thread for
mutable gesture state.

**RECOMMENDATION.** PaneBind R0 should preserve the pattern with a smaller,
explicit adapter translation:

1. WinEvent callback validates the event and records native metadata.
2. The Windows adapter queues an immutable normalized event.
3. One observer-owned thread takes a fresh snapshot and logs it.
4. Core receives no Win32 constant or handle.

The approved R0 start/location/end set remains sufficient for the first
observer. FancyZones' extra lifecycle events are production evidence, not
automatic authorization to add them in R0. If destroy is later added, document
the concrete invariant it protects. R0 should apply `OBJID_WINDOW`/child
filtering to location events even though FancyZones only applies that filter
to lifecycle events.

## WorkArea, layout, topology, and monitor identity

**FACT.** `WorkAreaConfiguration` owns an
`unordered_map<HMONITOR, unique_ptr<WorkArea>>`. Lookups can use a monitor, the
cursor, or a window. A null monitor key is a sentinel for one work area
spanning every monitor
([source](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/WorkAreaConfiguration.cpp#L6-L66)).

**FACT.** A `WorkArea` combines:

- a `WorkAreaId` (monitor identity plus virtual-desktop GUID);
- an immutable work-area rectangle;
- the current computed `Layout`;
- assigned-window state;
- a hidden overlay window and renderer; and
- snap/unsnap/update operations.

Layout selection is persisted separately from custom layout definitions.
`WorkArea::CalculateZoneSet` resolves the applied layout, refreshes custom
layout scalar values from the canonical custom-layout store, then recomputes
zones for the work-area rectangle
([source](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/WorkArea.cpp#L271-L355)).
`Layout` separates layout data from a computed zone map and validates invalid
work-area sizes and zone counts, but still accepts Win32 `RECT`, `POINT`, and
`HMONITOR` and consults global settings
([source](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/Layout.cpp#L86-L214)).

**FACT.** Display and work-area messages trigger topology reconciliation.
FancyZones re-enumerates monitors, compares count, runtime handle, device ID,
serial number, virtual desktop, and work-area rectangle, and rebuilds work
areas when any relevant value changes
([source](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/FancyZones.cpp#L1100-L1324)).
Monitor discovery combines Win32 display-device enumeration with WMI serial
numbers. If display association is temporarily incomplete it retries up to
100 times at 30 ms intervals
([source](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/MonitorUtils.cpp#L222-L296),
[reconciliation](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/MonitorUtils.cpp#L369-L413)).
That is a bounded recovery loop, not FancyZones' steady-state event source.

**FACT.** The pinned `FancyZonesDataTypes.h` contains two unresolved identity
risks: differing device-instance IDs can compare equal when monitor numbers
match, and `hash<WorkAreaId>` always returns zero
([source](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/FancyZonesDataTypes.h#L116-L220),
[hash](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/FancyZonesDataTypes.h#L248-L258)).
Open issue
[#49016](https://github.com/microsoft/PowerToys/issues/49016) reports wrong
saved-layout selection after dock/sleep and connects it to those exact
constructs plus stale persisted entries. The constructs are source-confirmed;
the reported runtime symptom was **NOT TESTED** by this review.

**RECOMMENDATION.** PaneBind should model these as distinct concepts:

- ephemeral native monitor token, valid only for the current enumeration;
- persistent monitor identity with explicit confidence/fallback provenance;
- normalized work-area geometry;
- virtual-desktop identity or `unknown`, never a magic first desktop; and
- a topology generation used to invalidate gesture references.

Do not use a null native handle to mean "combined topology" in core. Do not
make equality depend on a volatile display number. Equality and hashing must
use the same fields and have invariant tests. A topology replacement must
invalidate or cancel every object that refers into the old topology before
destruction.

## DPI and coordinate spaces

**FACT.** The FancyZones process enables Per-Monitor DPI Awareness V2 at
startup
([call](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZones/FancyZonesApp.cpp#L17-L25),
[implementation](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/common/Display/dpi_aware.cpp#L130-L133)).
Runtime work areas use monitor `rcWork`. When sizing another process's window,
FancyZones considers that window's DPI-awareness level and adjusts/clamps
coordinates for DPI-unaware windows on mixed-DPI systems
([source](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/WindowUtils.cpp#L32-L106)).

**FACT.** The editor boundary intentionally uses a dedicated DPI-unaware
thread to obtain virtualized coordinates that its WPF windows consume. Merged
PR [#44440](https://github.com/microsoft/PowerToys/pull/44440), fixing issues
[#43363](https://github.com/microsoft/PowerToys/issues/43363) and #43386,
documents a production failure caused by mixing C++ backend virtual
coordinates with PerMonitorV2 editor interpretation. The fix made position
and dimension handling use a consistent coordinate context. Microsoft's
[FancyZones product documentation](https://learn.microsoft.com/en-us/windows/powertoys/fancyzones)
still documents limitations for spanning mixed-DPI monitors and small edge
gaps for DPI-unaware target applications.

**INFERENCE.** "DPI aware" is not a complete geometry type. Backend physical
coordinates, DPI-virtualized coordinates, WPF device-independent units,
monitor rectangles, work-area rectangles, and a target window's placement
coordinates can all differ.

**RECOMMENDATION.** Every Windows-adapter rectangle crossing into PaneBind
must state its semantic space and source: positioning versus visible bounds,
physical versus virtualized coordinates, monitor versus work area, and the DPI
used for interpretation. Normalize once at the adapter boundary. Never infer
`DPI = 96`; never copy the editor's DPI-unaware compatibility bridge into
platform-neutral geometry.

## Window filtering and exclusions

**FACT.**
[`FancyZonesWindowProcessing::DefineWindowType`](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/FancyZonesWindowProcessing.cpp#L9-L76)
returns a reason-bearing classification, not just a boolean. It rejects, in
order, minimized windows, non-visible windows, tool windows, non-root windows,
most popup windows, owned/child windows unless enabled, excluded applications,
and windows outside the current virtual desktop. The popup heuristic permits
resizable/captioned application popups while filtering menu/notification-like
popups. Automatic placement additionally rejects windows marked as launched
by PowerToys Workspaces; manual placement does not.

**FACT.** User exclusion strings are uppercased and stored as lines. Matching
uses the executable name portion of the process path and falls back to a
case-insensitive window-title substring
([settings](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/Settings.cpp#L222-L244),
[shared matcher](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/common/utils/excluded_apps.h#L6-L70)).
Default exclusions include the SystemApps path, desktop/shell/taskbar windows
and classes, the Office splash class, FancyZones Editor, CoreWindow, and
SearchUI
([source](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/WindowUtils.cpp#L145-L189)).
The public documentation explicitly warns that a partial application name can
match multiple executables.

**INFERENCE.** A reason enum makes policy observable and testable, while a
single boolean would hide whether exclusion came from visibility, ownership,
style, user policy, or virtual desktop. Conversely, title-substring matching
is broad and titles can change during a window's lifetime.

**RECOMMENDATION.** R0 observation should capture raw facts separately from a
policy result and log a stable exclusion reason. Keep native style/class/path
inspection in the Windows adapter. Do not promote FancyZones' exact blacklist
or title-substring behavior into core without PaneBind-specific product
evidence and tests. Permission/path lookup failures should be explicit rather
than treated as a fabricated identity.

## Production hardening and history inspected

| Evidence | Verified state at review | Lesson for PaneBind |
|---|---|---|
| [PR #48569](https://github.com/microsoft/PowerToys/pull/48569), merge commit [`dd26d865`](https://github.com/microsoft/PowerToys/commit/dd26d86580168d2e368701f7b0c4d629dc9cd9ac) | Merged 2026-06-23. Added the missing destroy subscription/dispatch, aborts a destroyed window's in-flight drag, always clears drag state, and narrows swallowed keys. The PR states that this hook path had no unit-test harness and was manually verified. | Event completeness is defined by state-machine invariants, not by the happy-path start/end pair. Cancellation and cleanup must be idempotent and tested separately from commit/end. |
| [PR #48473](https://github.com/microsoft/PowerToys/pull/48473), merge commit [`ae9f241e`](https://github.com/microsoft/PowerToys/commit/ae9f241ef13737dab6f861767bbfdfca72b78475) | Merged 2026-07-01. Fixed overlay-thread teardown ordering, joining an unstarted thread, a condition-variable shutdown race, and a dangling `WorkArea*` when topology/configuration was cleared during a drag. | Stop workers before recycling their resources; cancel gestures before topology replacement; avoid raw borrowed pointers that can outlive a topology generation. |
| [PR #44440](https://github.com/microsoft/PowerToys/pull/44440) / [issue #43363](https://github.com/microsoft/PowerToys/issues/43363) | Merged 2026-01-09. Mixed-DPI editor overlays were shifted/clipped because two sides interpreted coordinates in different DPI contexts. | Coordinate-space metadata belongs in contracts and tests, not comments or caller assumptions. |
| [PR #49433](https://github.com/microsoft/PowerToys/pull/49433) / [issue #44058](https://github.com/microsoft/PowerToys/issues/44058) | Merged 2026-08-03 with a regression test. Active work areas refreshed shape but retained stale spacing/sensitivity/count from a persisted snapshot; the fix reloads canonical custom-layout data. | Name the canonical source of each field and test live refresh. Multiple caches require versioning/invalidation, not partial reloads. |
| [Issue #1685](https://github.com/microsoft/PowerToys/issues/1685) and current [`WindowUtils.cpp`](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/WindowUtils.cpp#L213-L259) | Closed as fix committed. Current code still performs a bounded 5 x 100 ms placement-state retry to avoid breaking minimize-to-tray applications. | Window state can be transitional after events. This bounded actuator compatibility workaround is not evidence for using polling as PaneBind R0's event source. |
| [Issue #49016](https://github.com/microsoft/PowerToys/issues/49016) | Open. Source-confirmed identity/hash constructs; reported dock/sleep symptom NOT TESTED here. | Persistent monitor identity needs explicit stability rules, pruning, and equality/hash tests. |
| [Issue #49019](https://github.com/microsoft/PowerToys/issues/49019) | Open. The source does use desktop-name change as a signal and then reads the current ID from registry, falling back to the first desktop ([source](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/VirtualDesktop.cpp#L99-L132)); the reporter's timing symptom was NOT TESTED here. | A signal that identity may have changed is not necessarily a settled identity value. Represent unknown/transitional states and do not persist a guessed fallback. |

Microsoft's application-compatibility section also records applications for
which `EVENT_SYSTEM_MOVESIZESTART` is not raised and cases that require
elevation. That is official evidence that WinEvent coverage is application- and
integrity-dependent, not proof that PaneBind observed those applications.

## Test architecture and limits

**FACT.** A static source count at the pinned SHA found 283 native
`TEST_METHOD` declarations under `FancyZonesTests/UnitTests`, plus 21 migrated
backend UI test methods, 10 editor unit test methods, 31 migrated editor UI
test methods, and the concurrently present legacy UI suites. These are source
counts, not executed results.

The inspected native tests cover:

- persisted applied/default/custom layouts, hotkeys, app-zone history, and
  JSON handling;
- layout generation, invalid dimensions/counts/spacing, overlap selection,
  zone bitmasks, and combined ranges;
- `WorkArea` creation, default/parent layout selection, snap/unsnap bookkeeping,
  resizable versus non-resizable windows, and the #44058 live-refresh
  regression;
- work-area identity cases including reconnects, same monitor models, missing
  serials, differing virtual desktops, handles, and instance IDs;
- window filtering for minimized, invisible, tool, non-root, popup, child,
  default/user exclusion, Workspaces-launched, and processable windows; and
- keyboard snapping within and across work areas.

Representative sources are
[`WindowProcessingTests.Spec.cpp`](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesTests/UnitTests/WindowProcessingTests.Spec.cpp),
[`WorkArea.Spec.cpp`](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesTests/UnitTests/WorkArea.Spec.cpp),
[`WorkAreaIdTests.Spec.cpp`](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesTests/UnitTests/WorkAreaIdTests.Spec.cpp),
and
[`Layout.Spec.cpp`](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesTests/UnitTests/Layout.Spec.cpp).

The migrated UI suite includes real Explorer drag outcomes, exclusions,
keyboard snap/restore, quick layouts, and window cycling
([`CoreBehaviorTests.cs`](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZones.UITests.Next/CoreBehaviorTests.cs),
[`DragWindowTests.cs`](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZones.UITests.Next/DragWindowTests.cs)).
The fuzz project targets custom-grid JSON deserialization
([source](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZones.FuzzTests/FuzzTests.cs)).

**LIMITS.** The native "unit" suite uses real test windows for some filtering
paths, and at least some cases return early when the CI desktop cannot make an
owner window visible. UI tests need an interactive desktop and include retry
and settling logic. PR #48569 explicitly records no unit harness for its
destroy-mid-drag hook path. No evidence found in the inspected tests makes the
full hook ordering, topology changes during a gesture, mixed-DPI physical
hardware matrix, elevated targets, or virtual-desktop registry timing a
deterministic unit-tested surface.

**RECOMMENDATION.** PaneBind should keep pure geometry and normalized event
tests independent of an interactive desktop, then add adapter tests and a
clearly labeled manual matrix for native semantics. Required future edge
cases include destroy/cancel without end, duplicate/child location events,
topology change during a gesture, negative-origin monitors, taskbar work-area
change, mixed DPI, non-DPI-aware targets, missing start/end events, elevated
targets, and monitor reconnect identity. R0 must report unrun cases as
`NOT TESTED`, never inherit upstream claims as local results.

## Decisions for PaneBind R0

### Worth adopting

- Out-of-context WinEvent hooks with no third-party DLL injection.
- Event-driven observation and a message loop; no continuous geometry polling.
- Dynamic subscription to noisy location changes only during a move/resize
  gesture, subject to PaneBind's own event experiments.
- Immediate callback-to-owner-thread handoff to avoid reentrant state mutation.
- Reason-bearing window eligibility results and logging.
- Separate current topology, work-area geometry, layout data, and persistent
  identity concepts.
- Explicit cancellation before topology/configuration replacement.
- Regression tests built from historical bugs, not only idealized geometry.

### Worth adapting

- Replace registered Win32 messages as the domain envelope with a normalized,
  immutable event type. Win32 delivery remains inside the adapter.
- Split FancyZones' `WorkArea` responsibilities: core geometry/model must not
  own overlay windows, native handles, persistence, or window operations.
- Model combined-monitor geometry explicitly rather than using a null
  `HMONITOR` sentinel.
- Treat monitor identity as fallible evidence with a topology generation,
  rather than one equality operator mixing handle, serial, instance, and
  display-number fallbacks.
- Carry coordinate-space and DPI metadata through the adapter contract.
- Keep observed facts separate from product filtering policy.

### Do not adopt in R0

- Low-level keyboard/mouse control, snapping, overlays, or window movement.
- `SetWindowPlacement`, `SetWindowPos`, or any other third-party window
  operation.
- Windows types or constants in `src/core/`.
- FancyZones' telemetry stack; PaneBind's charter requires no telemetry.
- Bounded placement/topology retries as a general event source, and especially
  no high-frequency resident polling.
- Raw references into replaceable topology-owned objects.
- Volatile monitor number fallback, constant hashes, or a guessed primary
  virtual desktop when identity is uncertain.

## Unresolved research questions

These are follow-ups, not authorization to start R1:

- Which applications in PaneBind's manual matrix omit or reorder
  move/size/location events?
- Does dynamic location-hook installation ever miss the first meaningful
  geometry change, and what duplicate/child events occur?
- Which monitor identity fields remain stable across direct connection,
  dock/KVM reconnect, sleep, driver reset, and identical monitor models?
- What exact coordinate space does every planned snapshot field use under
  PerMonitorV2 and for a DPI-unaware target?
- Should a later observer subscribe to destroy/minimize/show events to close
  state-machine gaps, and what evidence justifies each addition?
- How should virtual-desktop identity be represented while Windows' observable
  state is transitional or unavailable?

## Provenance payload for `SOURCE_PROVENANCE.md`

```text
Project: Microsoft PowerToys / FancyZones
Classification: Mature production reference
Repository: https://github.com/microsoft/PowerToys
Commit / Tag / SHA reviewed: 19c4d805321db86f3634e6968e14dbf25cbba14a (detached main SHA; no exact tag)
License: MIT; https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/LICENSE
Date reviewed: 2026-08-24
Files/modules inspected: src/modules/fancyzones/FancyZones/FancyZonesApp.{h,cpp}; FancyZones/main.cpp; FancyZonesModuleInterface/dllmain.cpp; FancyZonesLib/FancyZones.{h,cpp}; FancyZonesWindowProcessing.{h,cpp}; WindowUtils.{h,cpp}; WorkArea.{h,cpp}; WorkAreaConfiguration.{h,cpp}; Layout.{h,cpp}; FancyZonesDataTypes.{h,cpp}; MonitorUtils.{h,cpp}; EditorParameters.{h,cpp}; VirtualDesktop.{h,cpp}; AppliedLayouts, AppZoneHistory, CustomLayouts and related data modules; Settings; OnThreadExecutor; ZonesOverlay; project files; native unit tests; legacy and .Next UI tests; editor unit/UI tests; fuzz tests; doc/devdocs/modules/fancyzones.md; root LICENSE.
Issues/PRs inspected: https://github.com/microsoft/PowerToys/issues/1685; https://github.com/microsoft/PowerToys/issues/43363; https://github.com/microsoft/PowerToys/issues/44058; https://github.com/microsoft/PowerToys/issues/49016; https://github.com/microsoft/PowerToys/issues/49019; https://github.com/microsoft/PowerToys/pull/44440; https://github.com/microsoft/PowerToys/pull/48473; https://github.com/microsoft/PowerToys/pull/48569; https://github.com/microsoft/PowerToys/pull/49433; path-scoped git history through the reviewed SHA.
Official documentation inspected: https://learn.microsoft.com/en-us/windows/powertoys/fancyzones; https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwineventhook
What was learned: use out-of-context event ingress and serialized owner-thread dispatch; scope noisy location events to active gestures; cancel state before topology replacement; model monitor/work-area/virtual-desktop identity explicitly; label DPI coordinate spaces; use reason-bearing filters; separate canonical layout data from caches; test teardown, missing events, and topology/DPI transitions. FancyZonesLib is Windows-specific and is not a platform-neutral core template.
Applicable PaneBind subsystem: Windows event adapter; window snapshot/filtering; monitor/work-area/DPI adapter; normalized event model; core geometry boundary; lifecycle and test strategy.
Code copied: NO
Code adapted: NO
Attribution required: NO for this reference-only review. Any future copied or substantially reused MIT material must preserve the PowerToys MIT notice and be approved/recorded separately before entering PaneBind.
```
