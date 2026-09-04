# PaneBind Architecture Baseline

Status: R1-A platform-neutral algorithm baseline, the unchanged R1-B
owned-window operations boundary, the implemented R1-C1 companion-process
operations boundary, the sealed R1-C2A Explorer single-translation boundary,
and the implemented R1-C2B Explorer Glue Move test-session boundary. This
document records implemented boundaries and current decisions; runtime
acceptance evidence and gate results are recorded separately. R1-C2B real
Explorer validation is still `PENDING_UAT`; implementation and automated tests
do not substitute for that human evidence. Debug Attempt 1 safely stopped at
pre-authority `UnsafeLayout`; Fix 1 adds a non-consuming readiness preview and
does not reinterpret that attempt as Glue runtime.

## System flow

```text
Native OS event
    -> R0 observation adapter (evidence only), or
       narrow R1-C2B Explorer Glue WinEvent source
    -> Validated role-bound receipt and normalized event/geometry
    -> R1-A Core: visible geometry, adjacency graph, TranslationSession,
       and MovePlan(target_visible_rect)
    -> R1-C2B Core: GlueMoveCoordinator state/generation/receipt ledger
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
- an immutable, initial-relative pure follower move plan; and
- the R1-C2B `GlueMoveCoordinator`, including normalized Leader/Follower
  receipts, session and operation generations, a bounded expected-operation
  ledger, deterministic completion/reconciliation, and explicit abort reasons.

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

### R1-C2B Glue Move coordinator

R1-C2B adds the first narrow platform-neutral behavior coordinator under
`src/core/behavior/`. It consumes only opaque `WindowId` values, normalized
roles and event kinds, visible rectangles, local event sequence, session and
operation generations, and exact operation outcomes. It has no Explorer,
WinEvent, COM, HWND, timing, monitor, DPI, or native-operation concept.

Its state machine is `Idle -> Armed -> Active -> Completing -> Completed`, with
`Aborted` as the fail-closed terminal state. `arm` requires an actual R1-A
`WindowAdjacencyGraph` whose component is exactly Leader plus Follower and has
exactly one relation, then constructs an R1-A `TranslationSession` from that
verified pre-hook baseline. Exact Leader START revalidates and activates it;
the graph, membership, roles, and initial rectangles remain frozen. Each Leader
LOCATION is classified against the session-initial Leader rectangle.
`Unchanged` is a no-op, `Translation` consumes the R1-A total-delta plan, and
`ResizeOrMixed` aborts. No incremental Follower delta is accumulated.

Every Follower command is assigned a new operation generation and entered in a
bounded pending ledger. Exact expected feedback is acknowledged and suppressed;
an exact repeat of the current acknowledged geometry is duplicate feedback and
is also suppressed. Unexpected Follower geometry, Follower START, stale or
non-monotonic sequence/generation, native or post-verification failure,
invalidation, timeout, or capacity failure aborts. Missing LOCATION is not
fabricated: Leader END may reconcile a completed exact native receipt against
the exact final pair snapshot and records it as missing/reconciled rather than
event-acknowledged.

This is a single-session Explorer test baseline, not a general product Glue
engine. It adds no Glue Resize, Snap, dynamic group membership, persistent
binding, Ctrl activation, global input, or R1-C3 behavior.

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

Attempt 3 eligibility and runtime passed after a human completed both
confirmations in Debug and Release and target-correlated evidence proved
issuance, exactly one translation, exact post-verification, and exact restore.
Implementation/build/tests remain distinct from that manual evidence. The
optional stale-token lifetime step failed closed and is not part of the Runtime
Gate. R1-C2B builds on the target-consent prefix through a separate private Glue
authority; it does not relax this one-shot move contract.

The final Explorer observations match the R1-B/R1-C1 model: programmatic
primary and restore operations emitted LOCATION feedback without natural
START/END. Each tested phase happened to emit one target LOCATION, while
unrelated events interleaved in the global stream. This is an observation, not
a fixed cardinality contract. A future suppression design must use operation
receipt, capability generation, target identity, and geometry, and handle
missing/repeated/interleaved feedback.

### R1-C2B Explorer Glue session boundary

R1-C2B authorizes exactly two role-bound Explorer test windows for one temporary
Glue Move session. Leader and Follower are separately provisioned through the
R1-C2A user-consent eligibility path. Follower provisioning begins only after
Leader issuance, so its immutable baseline permanently excludes Leader. Pair
authorization requires two distinct live targets, complete target-consent
prefixes, exact locations, stable process/window identities, the same monitor
and DPI, and a new `Y + ENTER` Glue-consent generation. A private full-
fingerprint peer binding lets each target tolerate only the other authorized
member during live inventory checks; it neither exposes HWND nor changes an
ordinary R1-C2A token's one-primary-translation limit.

After both target-consent prefixes pass but before formal pair begin, the UAT
harness may call a repeatable layout-readiness preview at most three times. The
preview borrows the two still caller-owned sessions, live-revalidates exact
identity/location/state/security plus same monitor/DPI, and computes layout
capacity. It does not bind or consume Glue authority, consume either session,
perform a native operation, or arm a hook. An invalid target may be retired
by the existing fail-closed live validation, but no successful preview grants
operation authority.

Preview evidence records Leader/Follower visible sizes, common work-area size,
horizontal and vertical required/available/excess sizes and fit booleans,
same-monitor/DPI, the selected orientation, and explicit proof that no temporary
peer exception remains. `NEEDS_MANUAL_RESIZE` tells the
human exactly how much each orientation exceeds the work area. Only the human
may resize and press ENTER for the next check; this is
`TEST FIXTURE PREPARATION ONLY`, before Glue consent and hook arm. A final
`FIT` permits the harness to call formal begin, which independently reinspects
and replans to close the preview-to-authority TOCTOU boundary.

The fixture records both original visible and positioning rectangles, then
centers a size-preserving pair inside their common work area. Horizontal
Leader/Follower adjacency is preferred; vertical adjacency is the deterministic
fallback. Both use an exact zero gap, `SWP_NOSIZE | SWP_NOZORDER |
SWP_NOACTIVATE`, and exact post-verification. If the existing window sizes fit
neither orientation, setup blocks instead of resizing. This planner is
`TEST FIXTURE LAYOUT ONLY`, not Snap.

Only after setup, exact verification, and construction of the two-node/one-edge
R1-A graph does `ExplorerGlueSession` arm its additive WinEvent source. The R0
`WindowsObserver` and its JSONL remain independent evidence and never become a
runtime IPC/control bus. The Glue source has no public arbitrary-HWND factory;
only the role-bound session creates it from private live bindings.

For each unique target PID, the source installs three out-of-context hook slots:
the START-to-END lifecycle range, exact LOCATION, and exact DESTROY. Flags are
fixed to `WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS`. PID filtering is
only noise reduction. During owner drain every receipt is still checked against
the exact role-bound HWND, PID/TID, root status, capability generation, hook
slot, `OBJID_WINDOW`, and `CHILDID_SELF`. Other windows and object/child events
do not enter behavior.

Before assigning a sequence or touching the ring, the callback applies fixed,
allocation-free envelope checks: supported event, exact Leader/Follower HWND,
`OBJID_WINDOW`, and `CHILDID_SELF`. Unrelated same-process windows and
accessibility-child bursts are counted without consuming a receipt sequence or
queue slot. For a target receipt it assigns a monotonic local sequence, copies a
fixed record into a preallocated 512-entry ring, and uses `PostThreadMessageW`
to notify the owner. Owner drain performs the remaining hook-slot/PID and live
PID/TID/root/capability checks. The callback performs no COM inventory, native
query, geometry capture, topology work, logging, blocking wait, allocation,
behavior, or window operation. Overflow never overwrites an old receipt; overflow,
notification failure, reentrancy, wrong-thread use, hook lifecycle failure,
sequence exhaustion, or identity/root mismatch poisons the source and aborts
the session.

The creating STA and its message loop own target eligibility, event drain,
behavior, translation preparation, native apply, verification, termination, and
cleanup. Waiting uses a waitable timeout plus `MsgWaitForMultipleObjectsEx`, not
resident polling. Each drained receipt batch captures one frozen live Leader/
Follower geometry pair before any event in that batch can cause a native move.
This prevents a later Follower apply from retroactively changing the geometry
assigned to an older receipt. It also permits a queued START+LOCATION pair only
when the live Leader remains an exact or same-baseline pure translation from the
armed layout; state drift or resize aborts.

Before each active Follower `SetWindowPos`, the Windows ledger records the
behavior operation ID, source Leader sequence, expected visible/positioning
targets, and the event source's latest receipt sequence as a registration
watermark. A Follower LOCATION is attributable only if its receipt sequence is
strictly after that watermark and its geometry matches the pending expected
target. If a Follower receipt already follows a Leader LOCATION in the same
drained batch, it must match a previously completed exact operation or the last
acknowledged geometry before a new apply may occur; otherwise the session
aborts. Geometry written by the new operation can therefore never launder an
older or user-produced Follower receipt into self-feedback.

Leader END stops new planning and captures an exact final pair. Successful
native results with missing feedback may be reconciled against that snapshot;
no WinEvent acknowledgement is invented. Completion, abort, timeout, destroy,
native failure, post-verification failure, and all public exception paths enter
the same terminal ordering: stop and unhook first, discard now-inactive queued
receipts, restore Follower and Leader independently to their original exact
rectangles, release the private pair authority, and leave both user-created
Explorer windows open. The aggregate is strictly owner-thread-affine; it is not
transferable between threads.

The evidence runner treats complete runtime success, an explicitly supported
pre-native safe block, and malformed/contradictory evidence as distinct
outcomes: `PASS` exits 0, `SAFE_BLOCKED / KNOWN_BLOCKED` exits 2, and
`INVALID_EVIDENCE` exits 1. On a safe block, Glue consent/authority/binding,
layout operations, hook arm, drag trace, and active Follower operations must be
absent, and all safety flags must remain false. A pre-native summary uses
`feedback_suppression_evidence = not_reached`; it never claims a missing event
was reconciled. This evidence classification changes neither the Glue behavior
coordinator nor the event source.

PASS validation excludes pre-authority human-resize lifecycle by using the
drag prompt as the external Observer phase lower bound. It reconstructs the
centered zero-gap layout, checks the shared nonzero final translation, closes
the setup/active/restore operation chain, and requires one-to-one command and
feedback reconciliation. Legacy markerless handling is limited to offline
replay of the exact first-attempt prefix and hashes; it is unavailable to a new
live run.

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
model. The R0 stream by itself still cannot support window control. R1-C2B uses
its separate exact-target source, local receipt sequence, operation watermark,
generation, and geometry ledger for its narrow session; it does not reinterpret
R0 timestamps as causal authority.

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
- The R1-C2B Explorer Glue harness may compose the Explorer capability resolver,
  the narrow Glue WinEvent source, and the platform-neutral
  `GlueMoveCoordinator`; native target bindings and operation permits remain
  private to the Windows session.
- `platform/windows` translates exact native receipts and operation results into
  core values. Core does not call back into the platform and cannot resolve a
  token or execute `SetWindowPos`.

## Threading and lifetime baseline

The R0 Windows observer installs out-of-context hooks on the thread that runs a
Win32 message loop. It owns every hook handle for the full observation lifetime
and unhooks on orderly shutdown. The callback records the native receipt and
queues bounded work; snapshot time is recorded separately because delivery is
asynchronous. It does not mutate third-party window state. The design contains
no background sampling loop.

The separate R1-C2B session is created, armed, run, stopped, and restored on one
owner STA/message-loop thread. Its WinEvent callback performs fixed receipt
ingress only; all capability checks, geometry snapshots, behavior decisions,
native operations, and cleanup are serialized on the owner. The session does
not support ownership transfer. Event waiting is message-driven with a bounded
waitable timeout; no high-frequency polling or R0 observer IPC is involved.

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

Product Snap/general-Glue eligibility is separate from R0 observation policy.
R0's filter must not be mistaken for a permanent product blacklist. R1-C2B has
its own exact Explorer pair authority, while the R1-A graph still accepts a
caller-prefiltered `WindowGeometry` collection and encodes no visibility,
cloaking, minimization, maximization, elevation, application, monitor, DPI, or
virtual-desktop policy.

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
- Baseline exclusion and user-consented Debug/Release eligibility/runtime are
  empirically passing. Attempt 2 remains blocked by `E_FAIL`; the runtime PASS
  comes from human evidence, not implementation or automated tests alone.
- Generic third-party management, other applications, global input, injection,
  polling, Glue, Snap, R1-C2B, and later product behavior remain outside R1-C2A.

## R1-C2B architecture invariants

- Exactly two separately user-consented, post-baseline Explorer test windows
  may be bound: one Leader and one Follower. Follower's immutable baseline must
  exclude the already-issued Leader; no third window may enter the pair.
- The private Glue authority is role-, pair-, consent-, capability-, and
  session-generation bound. It exposes no arbitrary-HWND operation surface and
  does not loosen R1-C2A's ordinary one-shot authority.
- Before formal begin, a repeatable non-consuming preview may live-revalidate
  and measure the pair at most three times. It never binds/consumes Glue
  authority, consumes either target session, performs native apply, or arms the
  event source. Only a `FIT` result proceeds, and formal begin must independently
  reinspect/replan rather than trusting the preview snapshot.
- The temporary horizontal-or-vertical zero-gap layout is a size-preserving UAT
  fixture, not Snap. Setup happens before hooks arm; unhook happens before
  restore. If neither orientation fits, only the human may resize during
  pre-authority fixture preparation; PaneBind reports live required/available/
  excess dimensions and never resizes the targets.
- The live graph is built with R1-A adjacency and must contain exactly the two
  targets and one relation. Arming freezes this verified pre-hook topology,
  membership, roles, and initial geometry; START revalidates and activates it.
- The core coordinator is platform-neutral and uses R1-A initial-relative
  `TranslationSession` plans. Leader resize/mixed geometry is terminal rather
  than reinterpreted as movement.
- The Glue WinEvent source is additive and role-bound. Its callback performs no
  COM, geometry, topology, logging, blocking work, or native operation; owner
  drain performs full validation and behavior.
- Hook flags are fixed to `WINEVENT_OUTOFCONTEXT |
  WINEVENT_SKIPOWNPROCESS`; process filtering never grants authority. Exact
  HWND/PID/TID/root/capability/object-child validation remains mandatory.
- Receipt and pending-operation queues are bounded. Overflow, notification or
  hook failure, reentrancy, wrong-thread use, sequence exhaustion, identity
  drift, and trace/native-operation capacity exhaustion fail closed.
- Pending Follower identity and a receipt-sequence watermark are registered
  before native apply. Exact geometry plus later sequence is required for
  acknowledgement; time proximity and fixed event cardinality are not used.
- Duplicate exact feedback is suppressed, missing feedback is reconciled only
  through exact native result plus final snapshot, and unexpected feedback
  aborts. Follower START is never a second Glue session.
- Event-batch geometry is frozen before any operation in that batch. A queued
  START+LOCATION pair may represent an already-translated Leader, but resize,
  state drift, or an unattributable later Follower receipt aborts.
- Every terminal path attempts owner-thread stop/unhook, inactive-receipt
  discard, exact independent Follower/Leader restore, and authority release.
  PaneBind never closes the user-created Explorer windows.
- Evidence runner outcomes remain disjoint: complete runtime `PASS`, strictly
  zero-authority/zero-operation `SAFE_BLOCKED`, and malformed or contradictory
  `INVALID_EVIDENCE`. Pre-native feedback state is `not_reached`, not a runtime
  reconciliation claim.
- R0 observer semantics and R1-C2A user-visible semantics remain unchanged.
  R1-C2B uses no R0 JSONL control bus, global input, injection, high-frequency
  polling, other application, Glue Resize, persistent group, Snap, or R1-C3
  behavior.
