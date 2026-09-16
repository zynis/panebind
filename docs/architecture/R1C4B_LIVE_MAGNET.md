# R1-C4B live Magnet architecture

Stacked branch from `0aa13cbcc61a272fdc3ab0089b39d69a66381fe7`.
C4A implementation/review complete; human final seal DEFERRED to integrated
C4B UAT, not FAIL. Old C4A harness/schema remains reproducible.

## Scope and authority

Exactly three newly user-consented Explorer root frames, same original monitor,
DPI, current desktop and security class. A new explicit live consent authorizes
bounded source correction during real Move/Resize, existing Ctrl Glue Move, and
final accepted-baseline restore. It does not grant authority over preexisting
frames, other apps, new HWNDs after rehost, navigation, closure, focus, z-order,
owner or style. The C4B-only private seal/permit is distinct from old C4A consent.

One existing ExplorerGroupEventSource, no second hook, input hook or poller.
The new harness starts it after live consent/bind. A new live session owns the
existing group session; it reuses the old Glue coordinator, batch executor,
member feedback and accepted-baseline restore. Private snapshot injection shares
the already validated owner sample with old Glue processing, not another solver.
Old C4A calls retain their default source-start-after-accept path.

## Gesture router and raw truth

START captures the exact authorized set and latches Ctrl. Without Ctrl, the first
meaningful geometry sample classifies Move or one resize edge per axis. With
Ctrl, the existing Glue coordinator may initialize at START (no native write),
but only pure Move can produce follower commands. Ctrl Resize records
ctrl_resize_not_implemented / C4C_UNSUPPORTED and zero native writes. Late Ctrl
never changes route. Once classified, size/edge conflicts abort further writes.
There is exactly one writer per gesture: Magnet source OR Glue followers.

Magnet freezes the two non-source logical IDs, generations and geometries at
START. Unexpected target events/geometry, identity, stream, monitor/DPI or desktop
failure aborts; no active target-set refresh. Source raw input is the latest
meaningful native geometry, never accumulated from corrected geometry. Known
exact self-feedback is suppressed before motion/classification/solver input.
Unchanged samples do not run the solver. At most one solver and one correction
per owner quantum; individual receipts are only filtered/counted/coalesced.

The coordinator is portable: no HWND, WinEvent, COM, DWM or native API. It owns
generation, source, initial/raw geometry, latched type/edges, frozen targets,
axis preferences, pending expectations and bounded counters. Latches die with
the gesture. PERSISTENT_LEADER=NONE; PERSISTENT_GROUP_MEMBERSHIP=NONE.

## Single solver / hysteresis

Extend the existing MagnetConstraintSolver minimally with preferred X/Y
constraints and explicit selected_x/selected_y. Prefer the same eligible
constraint through release distance 16; otherwise clear it and rank candidates
within enter distance 10 using existing deterministic rules. These are INITIAL
UAT BASELINE physical desktop pixels under PMv2, not mathematically optimal.
Signed gaps and shallow overlap both solve to exact opposing-edge alignment.

A latched Resize can return to its initial rectangle or change only one edge
of an already classified corner. The solver's optional latched-edge contract
still forbids changes to every nonparticipating edge; default unlatched inference
stays strict. This is not a Windows-side duplicate geometry solver.

MotionSample uses previous meaningful RAW geometry and receipt QPC/frequency.
Initial live speed threshold is an explicit experimental 2000 geometry units/s;
not an SLA or upstream claim. Suppressed/invalid motion clears preferences and
performs no correction. No continuous attenuation curve or timer.

## Native transaction and feedback

The private bridge admits only the current active source member with the exact
live consent/group/capability/gesture/operation identity. Full boundary capture
precedes active mode. Active captures reuse consent-bound canonical frame
authority and retained VDM: global inventory=0, manager creates=0, every needed
desktop query fresh. Audit counters must prove those zeroes, not timing guesses.

Checked signed insets map requested visible bounds to positioning bounds.
Reject overflow, crossed/nonpositive results and native coordinate limits.
One SetWindowPos: Move NOSIZE|NOZORDER|NOACTIVATE, Resize NOZORDER|NOACTIVATE.
Never use HDWP for Magnet; it remains the Glue follower path.

All preflight -> pending registered -> one native call -> fresh actual capture.
Both actual rectangles must exactly match; clamp/reject aborts, no corrective
retry loop. The ledger records source identity, capability/consent/group,
gesture/operation, source receipt, registration watermark/native tick and
expected/actual geometry. Wrong generations/member/watermark/geometry cannot
ACK. Duplicate exact receipts do not rerun the solver. Uniquely attributable
self receipts may ACK; ambiguous receipts abstain/fail closed, never time-window
or ignore-next-event suppression. At END, all missing receipts require exact
postverified operations, no unexpected changes and exact latest final truth.

## Console and integrated UAT

Existing STA console input gains an optional owner-quantum callback, invoked
in bounded eight-message chunks for C4B. Default old readers remain unchanged.
This is necessary so real dragging remains live while the harness waits for
Enter/Y; it is not another event source. No input modes or clipboard access.

New harness/schema r1c4b/v1 and runner require IndependentReviewPassed, clean
frozen implementation, Debug binary and stable hash, ignored evidence. Prompt
rough placement, never pixel measurement or synthetic fixed-L placement.
Allow up to five attempts per action without reprovisioning: M1 C below A,
M2 B right of A, M3 B away/back for XY; B shorter then bottom-to-C and top-to-A;
C.right shrink to detach; return to a connected topology. Preparatory manual
adjustments remain live and never imply a fixed size/layout requirement.

Y accepts a fresh connected component of three (two OR three relations), then
G1/G2/G3 Ctrl Move A/B/C use old Glue semantics and exact restore. Unsupported
or out-of-sequence gestures cannot be turned into a successful UAT. Subjective
fixed choices: MAGNET_FEEL A–E, TOO_STICKY/MISSES/JITTER YES/NO,
GROUP_RIGID_BODY_FEEL YES/MOSTLY/NO. Never log free-text commentary.

Each END rebuilds WindowAdjacencyGraph from actual fresh geometry. Alignment
does not automatically imply relation; point-only contact is not adjacency;
shrinking overlap to zero removes the relation without CRUD or a registry.

## Evidence and verification

Record startup contracts/SHAs/options, bound identities, raw source receipts,
classification/route, frozen targets, meaningful samples/solver calls/motion
suppression/latch counters, each correction's raw/proposed/positioning/actual
rects, axis selection and single-call/pending/postverify proof; ACK/duplicate/
missing/error counts; actual END graphs; quantum/timing/queue and zero-inventory/
manager-create audits. No clipboard, user filenames or arbitrary URLs/text.

Tests: legacy pure 2691 checks plus preference enter/hold/release/reacquire and
fast clear; all classification edges/corners/conflicts and Ctrl routes; pending
ordering, exact/wrong/duplicate/missing feedback; target changes; signed geometry
bridge/overflow; owned native probe; relation formation/detach/reapproach;
synthetic positive/negative evidence. Full Debug/Release, all old runners and
C4A capture/console regression. Stop on recursive writes, oscillation, runaway
queues, authority bypass or Ctrl Resize writes. No fabricated performance SLA.

C4B live experience and both human seals remain PENDING/NOT_RUN at handoff.
Screen live Magnet OFF; other apps, mixed monitor/DPI, elevated, C4C, C4D,
Smart Docking, AquaStretch, z-order grouping and product UI remain out of scope.

## Implemented safety details

An END exact snapshot is reconciliation, **not** a self-LOCATION ACK. The Core
feedback entry requires LOCATION explicitly. Equal/unordered provider ticks
cannot ACK; known corrected geometry is never recycled as raw solver input.
An identical raw rectangle reasserted after a correction aborts rather than
producing a recursive operation storm. Real Explorer modal-loop behavior and
the practical frequency of that conservative abort still need human UAT.

A proposal superseded by a newer source snapshot before native entry is
discarded without pending registration or a write. Only a new native receipt
can retry it, with a new operation generation; there is no internal retry loop.
Requested visible bounds outside the original supported work area are also
not applied. The harness tells users to manually reduce window sizes and leave
movement room, without prescribing equal dimensions or a fixed layout.

Console confirmation performs one final drain of already-delivered receipts;
it does not synthesize gestures. Q/Escape cancellation does not trigger that
extra drain. The C4B test stream is explicitly synthetic_fixture and cannot be
accepted by the ordinary human evidence path. The real harness uses
human_interactive, an embedded Git SHA and a Debug identity query; the runner
checks HEAD, clean worktree and binary hashes before/after. Neither successful
technical evidence nor fixed-choice subjective data automatically seals UAT.
