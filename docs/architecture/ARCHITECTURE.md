# SnapWeave Architecture Baseline

Status: R0 architecture baseline. This document records boundaries and current
decisions; it is not a promise that unimplemented future components exist.

## System flow

```text
Native OS event
    -> Platform observation adapter
    -> Normalized event and geometry
    -> Core state/topology (future beyond the R0 geometry baseline)
    -> Behavior engine (future)
    -> Platform operations adapter (boundary only in R0)
```

The observation and operations directions are deliberately different
interfaces. Observing a window does not grant authority to manipulate it.

## Layers

### Application layer — `src/app/`

Owns process startup, command-line handling, component lifetime, and diagnostic
output. The R0 console observer composes the Windows adapter and does not hold
window-management policy.

### Core — `src/core/`

Owns platform-neutral domain concepts. R0 includes:

- normalized `Point`, `Size`, and `Rect` geometry;
- pure intersection, overlap, edge-distance, normalization, and tolerance
  helpers;
- opaque `WindowId` values;
- the three normalized event kinds justified for the observer; and
- a minimal normalized window snapshot.

The core must not include `HWND`, `HMONITOR`, `RECT`, `POINT`, `DWORD`, Windows
headers, native WinEvent constants, or native display identifiers whose meaning
the core must interpret. `WindowId` and `display_id` are opaque strings; only a
platform adapter constructs or interprets their platform-specific spelling.

Core coordinates are signed 64-bit integral units. An adapter is responsible
for supplying a single, internally consistent coordinate space. `Rect`
normalizes its two corners at construction. Intersection means shared positive
area; touching edges are represented by zero overlap and can be evaluated with
the separate edge/tolerance helpers. Those definitions are geometry semantics,
not Snap behavior.

### Windows platform adapter — `src/platform/windows/`

Owns all Win32 types and calls. Its R0 responsibilities are limited to:

- enumerate top-level candidate windows;
- apply the documented R0 observation filter;
- collect a native `WindowsWindowSnapshot`;
- distinguish OS positioning bounds from DWM visible frame bounds;
- resolve monitor work area and per-window DPI;
- map the three selected WinEvents to normalized event kinds; and
- serialize observational evidence without controlling target windows.

The native snapshot retains facts that are Windows-specific, such as `HWND`,
`HMONITOR`, class name, raw DPI, and DWM cloaking. A conversion step produces
the smaller `NormalizedWindowSnapshot`. That distinction prevents an accidental
expansion of the cross-platform domain whenever Windows exposes another flag.

### Future sync/behavior engine — not implemented in R0

A future engine may own topology, adjacency, grouping, and behavior policy only
after those models are researched and approved. R0 provides geometry
primitives, not a Glue graph or Snap algorithm.

### Future platform operations adapter — boundary only

A later adapter could accept reviewed, explicit commands from a behavior layer
and translate them into native operations. It must define error handling,
privilege boundaries, DPI semantics, feedback-loop suppression, and atomic
multi-window behavior before implementation. R0 contains no operations
interface and makes no native call that moves or resizes a third-party window.

## Normalized event model

R0 keeps only events directly supported by the research observer:

```text
MoveResizeStarted
GeometryChanged
MoveResizeEnded
```

The Windows adapter maps the corresponding native events after validating the
native window and object/child identifiers. Potential states such as minimized,
restored, activated, and closed remain research candidates rather than
speculative enum members. Event payload growth should follow demonstrated core
needs.

Native event timestamps are retained as source metadata. Wall-clock timestamps
used in logs are diagnostic metadata and are not treated as a causal ordering
model. Later work must research coalescing, re-entrancy, late delivery, destroyed
handles, and feedback suppression before relying on event sequences for window
control.

## Snapshot boundary

```text
HWND
  -> WindowsWindowSnapshot
       native handle/monitor, PID, path, title, class, styles, visibility,
       cloaking, min/max state, DPI, positioning rect, visible-frame rect,
       monitor/work area
  -> NormalizedWindowSnapshot
       opaque IDs, portable state, application/title, consistent geometry,
       display work area and scale
```

Failure to retrieve optional metadata is represented as missing data, not as a
fabricated default. A missing visible-frame bound is not silently replaced in
the native snapshot. The serializer can expose both facts so later analysis can
distinguish them.

## Dependency direction

- `app` may depend on `platform` and `core`.
- `platform/windows` may depend on `core` and the Windows SDK.
- `core` depends only on the C++ standard library.
- `core` never depends on `platform` or `app`.
- No current layer depends on a future behavior or operations implementation.

## Threading and lifetime baseline

The R0 Windows observer installs out-of-context hooks on the thread that runs a
Win32 message loop. It owns every hook handle for the full observation lifetime
and unhooks on orderly shutdown. The callback validates and snapshots at event
time; it does not mutate third-party window state. The design contains no
background sampling loop.

## DPI and display baseline

The Windows executable declares Per-Monitor DPI Awareness V2. Native coordinate
semantics are recorded in the event research document and verified through
observation; the core does not assume 96 DPI. The normalized display scale is
derived at the adapter boundary, while the native snapshot retains raw DPI.

Monitor identity and work area are snapshot facts, not permanent properties of
a window. Moving a window can change both. R0 records these changes but does not
attempt cross-DPI synchronization or correction.

## Filtering boundary

The observer's filter is a named, testable platform policy, not behavior logic.
It must reject invalid/non-root/child-object event targets, invisible windows,
DWM-cloaked windows, and tool windows that are not explicitly presented as app
windows. It also skips the observer's own process. Exact evidence and caveats
are recorded in `docs/research/R0_WINDOWS_EVENT_MODEL.md`.

Filtering for future Snap/Glue eligibility is a separate problem. R0's
observation filter must not be mistaken for a permanent product blacklist.

## R0 invariants

- No Windows type or header under `src/core/`.
- No DLL injection.
- No high-frequency polling.
- No global mouse/keyboard window-control behavior.
- No call that moves/resizes a third-party window.
- No Snap, Glue, zone, tiling, or persistent group engine.
- Unobserved behavior is marked untested rather than inferred.

