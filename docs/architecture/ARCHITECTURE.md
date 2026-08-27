# PaneBind Architecture Baseline

Status: R1-A platform-neutral algorithm baseline, the unchanged R1-B
owned-window operations boundary, the implemented R1-C1 companion-process
operations boundary, and the narrow R1-C2A Explorer test-window design
boundary. This document records implemented boundaries and current decisions;
runtime acceptance evidence and gate results are recorded separately. The
R1-C2A runtime gate is currently blocked; no runtime acceptance pass is
claimed.

## System flow

```text
Native OS event
    -> Platform observation adapter
    -> Normalized event and geometry
    -> R1-A Core: visible geometry, adjacency graph, TranslationSession,
       and MovePlan(target_visible_rect)
    -> Behavior engine boundary (future; not implemented)
    -> Capability-neutral translation preparation
    -> One of three separate Windows capability resolvers:
         R1-B same-process OwnedWindowToken
         R1-C1 controller-launched CompanionWindowToken
         R1-C2A allowlisted and isolated ExplorerWindowToken
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

An operation receipt is diagnostic evidence, not feedback suppression. R1-C1
researches companion-process receipts and feedback inputs, but it must not
infer suppression solely from time or event contiguity. R1-B remains unchanged
and defines no Glue event loop or product behavior.

### R1-C1 Windows companion-process operations boundary

R1-C1 adds a second, independent authority for a PaneBind test fixture launched
by a PaneBind controller. It does not widen `OwnedWindowToken`,
`OwnedWindowRegistry`, or `OwnedWindowOperations`: those names and their
same-process owned-only issuance rules remain unchanged. A companion process is
external to the controller process but is still PaneBind-created test
infrastructure, not evidence that an arbitrary third-party application is safe
or eligible to control.

The controller creates a `CompanionSession` by launching the exact companion
target executable with `CreateProcessW`. A restricted inherited anonymous-pipe
handshake is the only source of candidate registrations. The controller does
not use `EnumWindows`, `FindWindow`, `FindWindowEx`, `GetForegroundWindow`,
`WindowFromPoint`, shell enumeration, a title, or another global discovery path
to issue authority. A raw `HWND` transmitted in the handshake is a native fact
to validate, not a public or durable capability.

The controller keeps the process handle returned by `CreateProcessW` for the
entire session. `CompanionWindowToken` is opaque and contains the controller
registry authority, the per-launch companion session authority, a logical
window ID, and a registration generation. It cannot be constructed from or
converted to `HWND`, and a token from one session cannot alias a later session
even when both sessions use logical IDs A/B/C/D or Windows eventually reuses a
numeric PID or handle.

Handshake acceptance and every later token resolution validate all of the
following before exposing a native handle internally:

- the held process handle is still live and still identifies the launch PID;
- token registry and session authorities, logical ID, and generation match the
  active registration;
- `GetWindowThreadProcessId` matches the companion PID and registered target UI
  thread;
- the window still has the fixed private companion class;
- it is a root, non-child, unowned top-level window; and
- its target-created per-session and per-generation marker still matches.

These repeated predicates reduce accidental stale identity but do not turn
`IsWindow`, PID, class, property, or raw handle values into independent
capabilities. The companion protocol serializes fixture lifetime commands with
operations. A target-side window destroy retires that registration and makes
its token stale; a signaled child process handle retires the entire session and
makes every token issued by it stale. A new child launch always receives a new
session authority.

The IPC byte stream is single-request and framed. A response timeout, truncated
or oversized frame, read/write failure while the child remains alive, envelope
or request mismatch, or malformed evidence permanently marks the session
`SessionPoisoned`: both pipe endpoints close, all tokens retire, and no later
capture or native operation may reuse that stream. Teardown may then wait for
EOF-driven child exit and, if necessary, use only the exact retained fixture
process handle for bounded fallback cleanup.

The visible-to-positioning pure-translation calculation is capability-neutral
and may be shared internally between R1-B and R1-C1. It accepts captured
geometry and a requested `target_visible_rect`, verifies equal size and one
checked delta, and translates the current positioning rectangle by that delta.
The authority layers remain separate: the shared preparation code accepts no
arbitrary `HWND`, performs no discovery, and cannot resolve either token type.
Owned and companion registries independently perform issuance, lifetime
validation, and native-handle resolution.

After all companion members pass preflight, single-window placement may call
`SetWindowPos`, while a follower batch uses the complete
`BeginDeferWindowPos`/`DeferWindowPos`/`EndDeferWindowPos` chain. Results must retain
operation or batch ID, companion token/session/generation, PID/TID, requested
target, before geometry, actual visible and positioning geometry, monitor/DPI,
and the native stage and outcome. Every native attempt must be followed by
recapturing all still-valid members. A successful native return with actual
geometry different from requested is a post-verification failure, not success.
Preflight is PaneBind all-or-nothing; Win32 placement is not described as a
transaction and R1-C1 provides no rollback guarantee.

The companion target must record `WM_WINDOWPOSCHANGING`,
`WM_WINDOWPOSCHANGED`, `WM_MOVE`, `WM_SIZE`, and `WM_NCDESTROY` with logical
window and operation context. Its test-only uncooperative mode may modify a
requested `WINDOWPOS` deterministically so the controller must detect an
actual-versus-requested mismatch. This is a required evidence design, not a
claim that a particular message count, ordering, or runtime result has already
been observed.

Controller and companion executables must declare Per-Monitor DPI Awareness V2.
The R1-C1 baseline is limited to the same interactive session and same
integrity level; elevated, UIAccess, AppContainer, cross-session, mixed-DPI,
and multi-monitor targets require separate evidence. The controller does not
elevate itself, alter UIPI message filters, or attach input queues.

Operation receipts, target-side messages, and out-of-context WinEvents are
three feedback inputs. R1-C1 records them for attribution research but does not
implement a Glue feedback-suppression state machine. Attribution may compare
session/token/generation, expected target, and verified actual geometry; it
must not require `MOVESIZESTART`/`MOVESIZEEND`, one WinEvent per request,
time-only matching, or event adjacency and contiguity.

Companion shutdown must be requested through the session IPC and awaited for a
bounded interval. A fixture cleanup fallback may terminate only the exact
process object held by that `CompanionSession`, never an arbitrary PID, process
name, wildcard, or user application. Closing the session invalidates all its
tokens regardless of whether shutdown was graceful.

R1-C1 itself creates no third-party eligibility or product-interaction policy.
Its companion authority cannot be reused or generalized for R1-C2A Explorer
test windows.

### R1-C2A Windows Explorer test-window boundary

R1-C2A defines a third, separate Windows capability for one explicitly
user-consented File Explorer test window. It does not widen either the R1-B
owned capability or the R1-C1 companion capability, and it is not a generic
third-party-window registry. No user-preexisting Explorer window, other
application, second target, leader/follower group, Glue session, Snap behavior,
global input, or production selector enters this boundary.

The two automatic provisioning attempts remain immutable blocked history.
Attempt 1 used Shell-inventory delta and exposed opaque-location and ambiguous
frame problems. Attempt 2 added `DShellWindowsEvents`, registration-cookie
resolution, canonical COM identity, and a retained provisioning lease, but its
single documented `CoCreateInstance(CLSID_ShellBrowserWindow)` call returned
`E_FAIL` before a lease, registration, target, or native operation existed.
`AUTO_PROVISIONING_ON_CURRENT_WINDOWS11 = BLOCKED`. The retained design and
tests remain diagnostic history; the CLSID path is not active/default and is
never fallback for the current UAT.

Attempt 3 is a controlled interactive UAT. The harness creates only a unique,
empty, non-reparse local directory under ignored
`uat/r1c2a/consent-target-<nonce>` and records its exact `FILE_ID_INFO`. It
captures every baseline Shell entry's reliable `HWND` in a permanent forbidden
set. Empty and inaccessible locations remain `OPAQUE_PREEXISTING`; a missing
reliable HWND blocks before the user prompt. Baseline membership is immutable
for the session, so navigating a preexisting window to the nonce directory can
never upgrade it.

The harness prints the absolute nonce path and asks the human to create one new
Explorer top-level window and navigate that window to the path. `Ctrl+N` is a
suggested human action only. PaneBind does not call ShellExecute, start
Explorer, navigate a window, synthesize input, force foreground, or infer
authority from the gesture. ENTER records explicit user confirmation, not a
password, credential, integrity boundary, or native identity fact.

After confirmation, a fresh read-only `IShellWindows` inventory selects HWND
values absent from the permanent baseline, filters them by exactly one live
Shell entry whose filesystem identity equals the nonce directory, and requires
exactly one remaining candidate. Zero candidates, multiple candidates, a tab
in a baseline frame, or any reused/preexisting HWND blocks without native
apply. The candidate must additionally pass the complete Explorer allowlist:
root/top-level/style/owner, visible/uncloaked/normal state, current desktop,
class, PID/TID and retained process instance, canonical system
`explorer.exe` identity, compatible session/integrity/UIAccess/AppContainer,
monitor, DPI, and exact live location.

`ExplorerWindowToken` remains Explorer- and test-session-specific, opaque, and
unconstructable from a raw `HWND`. Its authority combines explicit consent,
the immutable baseline exclusion, unique exact candidate identity, live
eligibility, and matching session/capability/consent generations. The session
records baseline, prompt, target-confirmation, eligibility, token, and
move-consent generations. Consent alone cannot issue or refresh a token.

Target confirmation only permits token issuance. The harness then presents a
sanitized target summary and requires a second explicit affirmative consent
before movement. Candidate change, navigation, destruction, process exit,
minimize/maximize, monitor/DPI change, or any generation mismatch before apply
blocks. Without the second consent, `SetWindowPos` is never called.

The sole authorized test behavior is one same-monitor pure translation through
the shared visible-to-positioning bridge, using
`SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE`. Native success is not PaneBind
success: the same HWND/process/PID/TID/class/location/monitor/DPI and exact
visible/positioning rectangles must pass post-verification with size preserved.
Restore is a separate immediate live-revalidated translation to the initial
position and is required for the runtime Gate; it is not rollback.

Because the human creates the Explorer target, Attempt 3 never calls
`IWebBrowser2::Quit`, sends `WM_CLOSE`, invokes a Shell close command, or
terminates/restarts Explorer. After restore the human may close the test window.
An optional manual stale-token check may verify no native apply after closure;
skipping it leaves `WINDOW_DESTROY_LIFETIME = NOT TESTED` without blocking the
single-translation baseline.

Observer startup and the interactive harness may be composed only by an
evidence script that saves ignored logs; the script cannot create Explorer,
send keys, close windows, or control a baseline window. Creation/navigation
events precede operation attribution and cannot masquerade as translation or
restore feedback. The interactive mode is not a CTest and cannot be completed
by Codex impersonating user consent.

Attempt 3 eligibility and runtime are `PENDING_UAT` until a human completes the
two confirmations and the target-correlated evidence proves issuance, exactly
one translation, exact post-verification, and restore. Implementation, builds,
and deterministic tests are reported separately and cannot make those runtime
Gates pass. R1-C2B is not started.

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
- The R1-C1 controller harness may depend on R1-A core, capability-neutral
  translation preparation, and a separate Windows companion resolver; process
  handles, IPC, and native tokens must not enter `core`.
- The R1-C2A Explorer harness may depend on Shell inventory, a separate
  Explorer eligibility/capability resolver, and capability-neutral translation
  preparation; Shell COM, process handles, and Explorer policy must not enter
  `core`.
- No current layer depends on the future behavior engine or implements R1-C
  feedback suppression.

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

## R1-C1 architecture invariants

- The R1-B owned-window token, registry, adapter, and issuance boundary remain
  owned-only and are not generalized.
- Companion authority originates only from a controller-created process and
  its restricted inherited-pipe handshake; no global window discovery issues a
  token.
- The held process handle and per-launch session authority anchor process
  lifetime; PID, TID, class, root/owner/style, marker, and generation are
  revalidated predicates, not standalone capabilities.
- A destroyed window invalidates its registration, child exit invalidates the
  whole session, and a later session cannot accept an older token.
- Geometry preparation may be shared, but owned and companion capability
  issuance and resolution remain separate.
- Native cross-process placement always produces structured post-verification
  evidence and makes no transaction or rollback claim.
- PMv2 and same integrity are explicit R1-C1 constraints; broader integrity,
  desktop, monitor, and DPI behavior is not inferred.
- WndProc and WinEvent records are feedback inputs, not a completed
  suppression algorithm or a guaranteed acknowledgement stream.
- R1-C2, real third-party control, global input, injection, Glue runtime, Snap,
  and product interaction remain outside this round.

## R1-C2A architecture invariants

- Read-only Shell inventory is a candidate source, never operation authority.
- Every preexisting Shell entry must supply a reliable HWND for permanent
  exclusion; empty or inaccessible locations remain opaque diagnostics and
  never grant target authority.
- Attempts 1 and 2 automatic provisioning remain blocked history; the
  `CLSID_ShellBrowserWindow` path is not active/default and is never a fallback.
- Attempt 3 creates only the ignored nonce directory; the human creates and
  navigates the Explorer frame without synthesized/global input or foreground
  forcing.
- Only exactly one new post-baseline candidate `HWND` at the unique test
  location, represented by exactly one Shell entry with exact filesystem file
  identity, can receive an Explorer-specific token. A baseline HWND is forever
  forbidden even if it later reaches that location.
- Target confirmation and move consent are distinct generation-bound facts;
  ENTER is not a credential and consent never substitutes for live eligibility.
- Explorer issuance and resolution remain distinct from owned and companion
  capabilities, and no generic third-party or public raw-`HWND` API exists.
- Process image, class/root/style/owner, visibility/cloak/state, Shell location,
  integrity, lifetime, monitor, and DPI facts are live-revalidated before use.
- The only authorized operation is one explicit, same-monitor pure translation
  followed by exact post-verification and a separately verified restore.
- Attempt 3 never automatically closes the user-created Explorer window;
  optional closure and stale-token evidence are human-controlled.
- Baseline exclusion is empirically passing. Attempt 2 remains blocked by
  `E_FAIL`; Attempt 3 eligibility/runtime remain `PENDING_UAT`, with no runtime
  PASS inferred from implementation or automated tests.
- Generic third-party management, other applications, global input, injection,
  polling, Glue, Snap, R1-C2B, and later product behavior remain outside R1-C2A.
