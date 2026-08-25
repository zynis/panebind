# PaneBind Architecture Baseline

Status: R1-A platform-neutral algorithm baseline plus the R1-B owned-window
operations boundary. This document records implemented boundaries and current
decisions; runtime acceptance evidence is recorded separately, and this is not
a promise that unimplemented product behavior exists.

## System flow

```text
Native OS event
    -> Platform observation adapter
    -> Normalized event and geometry
    -> R1-A Core: visible geometry, adjacency graph, TranslationSession,
       and MovePlan(target_visible_rect)
    -> Behavior engine boundary (future; not implemented)
    -> R1-B Windows owned-window operations adapter
    -> Native placement and post-operation snapshot receipt
```

The observation and operations directions are deliberately different
interfaces. Observing a window does not grant authority to manipulate it.

## Layers

### Application layer — `src/app/`

Owns process startup, command-line handling, component lifetime, and diagnostic
output. The R0 console observer composes the Windows adapter and does not hold
window-management policy.

### Core — `src/core/`

Owns platform-neutral domain concepts. The implemented R0 and R1-A baseline
includes:

- normalized `Point`, `Size`, and `Rect` geometry;
- pure intersection, overlap, edge-distance, normalization, and tolerance
  helpers;
- opaque `WindowId` values;
- the three normalized event kinds justified for the observer; and
- a minimal normalized window snapshot;
- visible-geometry `WindowGeometry` inputs and explicit adjacency tolerance;
- signed edge relations, a deterministic undirected adjacency graph, and
  connected-component solving;
- geometry-change classification with checked translation deltas; and
- an immutable, initial-relative pure follower move plan.

The core must not include `HWND`, `HMONITOR`, `RECT`, `POINT`, `DWORD`, Windows
headers, native WinEvent constants, or native display identifiers whose meaning
the core must interpret. `WindowId` and `display_id` are opaque strings; only a
platform adapter constructs or interprets their platform-specific spelling.
An R0 `WindowId` is observer-session-scoped and ephemeral, not persistent
identity; native handle reuse is correlated with the required process ID and
receipt sequence in the Windows log envelope.

Core coordinates are signed 64-bit integral units. An adapter is responsible
for supplying a single, internally consistent coordinate space. `Rect`
normalizes its two corners at construction. Intersection means shared positive
area; touching edges are represented by zero overlap and can be evaluated with
the separate edge/tolerance helpers. Those definitions are geometry semantics,
not Snap behavior.

R1-A adjacency consumes visible rectangles only. Positioning/operation
rectangles remain a separate future adapter concern. Only opposing edges can be
adjacent; the edge tolerance is an explicit caller value on the edge-normal
axis, the perpendicular overlap must have positive length, and corner-only
contact is rejected. A positive signed gap means visible space, zero means exact
touch, and a negative gap means shallow intrusion. Multiple qualifying contacts
for one pair are rejected as ambiguous in this baseline.

The full `INT64_MIN..INT64_MAX` rectangle span remains unsupported, but R1-A
uses checked subtraction and addition for edge gaps, classification deltas, and
planned targets. Unrepresentable distant gaps are outside tolerance;
unrepresentable translation fails the whole calculation rather than producing
a partial plan.

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
Normalized text and log strings use UTF-8; the Windows adapter explicitly
transcodes UTF-16 and JSON-escapes it. `process_name` means the executable
basename when available, not a stronger semantic application identity.

### Future sync/behavior engine — not implemented

R1-A implements the pure adjacency graph, connected-component query, geometry
classification, and initial-relative translation plan. It does not subscribe to
events, retain native windows, choose product eligibility, or execute a plan.
A future behavior engine may consume these reviewed values only after its own
research gate. No Glue or Snap behavior engine exists.

### R1-B Windows owned-window operations adapter

R1-B introduces a Windows-only operation boundary between the R1-A pure
`MovePlan` and native placement. Its authority is deliberately limited to
independent top-level windows created and registered by the PaneBind harness.
It is not a general third-party window operations API, and no Explorer, Office,
editor, browser, terminal, system, or other user window can be supplied through
this interface.

`OwnedWindowToken`, `OwnedWindowRegistry`, and the owned adapter must not be
renamed or generalized into arbitrary-window capabilities. Any future support
for an external or third-party HWND requires a separate research gate and a new
eligibility, process-identity, lifetime, and capability contract; it cannot be
obtained by widening this R1-B issuance boundary.

Public capture and operation entry points accept an opaque `OwnedWindowToken`,
not an `HWND`, integer handle, or caller-provided native-handle collection. The
token has no public conversion to a native handle. The one native issuance
boundary is private to the owned-window registry: it may receive the `HWND`
returned by the harness's `CreateWindowEx` call and issue a token only after
checking that the handle belongs to the current process and expected harness UI
thread, uses the fixed private harness window class and marker property, and is
an independent unowned top-level window associated with the current
registration generation. A private process-monotonic registry authority is
part of token equality, so a token issued by another registry cannot alias a
local logical ID/generation. Before every capture or operation, the registry
repeats the live registration, native-handle, PID/thread, root/owner/style,
class, marker, authority, and generation checks. `WM_NCDESTROY` removes the
registration and invalidates its token before a later window can reuse the
numeric handle value.

The R1-B bridge supports translation only. For each member it captures the
current visible-frame and positioning rectangles and requires the requested
`target_visible_rect` to have the same size and the same checked delta on all
four edges. It then translates the current positioning rectangle by that delta;
it never treats a visible-frame rectangle as a positioning rectangle. A
different target size is rejected rather than invoking an unresearched visible
resize conversion.

A multi-window request preflights every member before beginning any native
operation. Token authority, ownership, current geometry, pure-translation
shape, checked arithmetic, representable native coordinates, and the complete
target set must all validate before `BeginDeferWindowPos`. Only then may the
adapter build the `DeferWindowPos` chain and call `EndDeferWindowPos`. This is a
PaneBind prevalidation all-or-nothing rule; the native sequence is not described
as transactional and provides no PaneBind rollback guarantee.

After any attempted native sequence, including a reported native failure, the
adapter captures actual visible and positioning geometry for every still-owned
member. The operation receipt distinguishes preflight, begin, defer-chain, end,
and post-verification stages and records requested versus actual geometry rather
than converting an application- or Windows-adjusted result into success. The
receipt also retains the owned token generation and the before/after monitor
and DPI facts needed to diagnose a topology or coordinate-context change.

The harness declares Per-Monitor DPI Awareness V2 explicitly. Monitor and DPI
are operation-time snapshot facts, not permanent token properties. R1-B records
them before and after placement; this boundary does not by itself claim that
mixed-DPI or cross-monitor behavior has been validated.

An operation receipt is diagnostic evidence, not feedback suppression. A
future R1-C behavior layer may consume receipts, token generations, expected
targets, and acknowledged actual geometry when designing suppression, but it
must not infer suppression solely from time or event contiguity. R1-C is not
started, and R1-B defines no Glue event loop or product behavior.

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
- R1-A core movement/topology depends only on other core value types and the C++
  standard library.
- The R1-B harness may depend on R1-A core and the Windows owned-window
  operations adapter; the adapter must not become a dependency of `core`.
- No current layer depends on the future behavior engine or R1-C feedback
  suppression.

## Threading and lifetime baseline

The R0 Windows observer installs out-of-context hooks on the thread that runs a
Win32 message loop. It owns every hook handle for the full observation lifetime
and unhooks on orderly shutdown. The callback records the native receipt and
queues bounded work; snapshot time is recorded separately because delivery is
asynchronous. It does not mutate third-party window state. The design contains
no background sampling loop.

## DPI and display baseline

The Windows executable declares Per-Monitor DPI Awareness V2. Native coordinate
semantics are recorded in the event research document and verified through
observation; the core does not assume 96 DPI. `GetDpiForWindow` is retained as
raw target-window DPI in the native snapshot. If normalized, it becomes an
optional effective-window scale, not a physical monitor-scale claim: Windows
can report 96 for a DPI-unaware target or system DPI for a system-aware target.

Monitor identity and work area are snapshot facts, not permanent properties of
a window. Moving a window can change both. R0 records these changes but does not
attempt cross-DPI synchronization or correction.

## Filtering boundary

The observer's filter is a named, testable platform policy, not behavior logic.
It excludes invalid/non-root/child-object event targets, invisible windows,
DWM-cloaked windows, and tool windows that are not explicitly presented as app
windows from the `default_candidate`/normalized event stream. Structurally
observable policy rejections remain diagnostic records with a named reason and
raw facts. The observer also skips its own process. Exact evidence and caveats
are recorded in `docs/research/R0_WINDOWS_EVENT_MODEL.md`.

Filtering for future Snap/Glue eligibility is a separate problem. R0's
observation filter must not be mistaken for a permanent product blacklist.
The R1-A graph accepts a caller-prefiltered `WindowGeometry` collection and
encodes no visibility, cloaking, minimization, maximization, elevation,
application, monitor, DPI, or virtual-desktop policy.

## R0 invariants

- No Windows type or header under `src/core/`.
- No DLL injection.
- No high-frequency polling.
- No global mouse/keyboard window-control behavior.
- No call that moves/resizes a third-party window.
- No Snap, Glue, zone, tiling, or persistent group engine.
- Unobserved behavior is marked untested rather than inferred.

## R1-A invariants

- Adjacency and move planning use visible, platform-neutral geometry only.
- Tolerance is explicit; no product pixel or DPI default is embedded in core.
- Graph nodes/relations/components and plan outputs are deterministic under
  input permutation.
- Duplicate `WindowId` values and invalid tolerance are rejected explicitly.
- Move plans contain follower target visible rectangles only and are recomputed
  from immutable session-initial geometry plus the leader's total delta.
- Resize-or-mixed leader geometry produces no translation plan.
- No platform operation, event hook, input state, feedback suppression, Snap,
  Glue Resize, or third-party window control is part of R1-A.

## R1-B architecture invariants

- Native operation authority originates only from the owned-window registry;
  public capture and operation APIs never accept a raw `HWND`.
- A token is registry-authority plus registration-and-generation identity, not
  a cast native handle, and `WM_NCDESTROY` invalidates it.
- Only PaneBind harness-owned, independent top-level windows are in scope.
- A visible target is converted to an equal-delta positioning translation;
  visible resize conversion is outside R1-B.
- All members pass ownership, geometry, and arithmetic preflight before a
  deferred native sequence begins.
- Native deferred positioning is not described as transactional, atomic, or
  rollback-capable; actual state is captured after an attempt.
- PMv2 is explicit, while monitor and DPI remain before/after snapshot facts.
- Receipts expose inputs for later feedback research but implement no R1-C
  suppression, Glue loop, Snap behavior, or third-party window control.
