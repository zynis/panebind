# R1-C4A dynamic bounded Glue group

Status: IMPLEMENTED / AUTOMATED TESTED. Human UAT REQUIRED **after independent
review PASS**; no three-Explorer runtime acceptance is claimed.

> Leader/Follower are gesture-scoped roles, not persistent member identities.

> A Glue Group has members. A gesture has one transient Leader and zero or more transient Followers.

These are Human Root long-term product rules, not fixture-only terminology.

## Independent concepts and lifetimes

Authorized Member Set is immutable consent/capability authority, ordered by
logical WindowId. Glue Component is a fresh visible-geometry connected
component at START, built only from authorized snapshots. Gesture Roles are
the actual source and other component members for one gesture. Adjacency
never creates authority.

The opaque group seal's monotonic generation is also its authority identity:
there is no renewal/rebinding mechanism this round. Member logical ids are
session-qualified ledger identities (each underlying session has its own token
id namespace). Presentation slots A/B/C preserve provisioning order; a frozen
logical-id permutation defines canonical/native batch order. Neither ordering
uses numeric HWND comparison. Historical gesture records retain the old source
for auditing only and never authorize a later gesture.

GroupReady -> GestureActive -> GestureCompleting -> GroupReady is repeatable.
Successful END clears coordinator, pending, frozen topology and roles. Group
authority, native bindings, tokens and event source survive. Identity, stream
or placement failure poisons the group. Keep the sealed C3B pair path intact;
reuse TranslationSession and the checked visible-to-positioning bridge.

## Windows and batch boundary

Exactly three independently consented new Explorer frames, distinct nonces,
fixed role-neutral callback bindings, one owner STA and retained VDM interface
with fresh queries. Original canonical anchor, navigation/quit, native identity,
image/security, state, geometry and monitor/DPI checks remain mandatory.

Ctrl high-bit latch at START is unchanged. Plain drag and late Ctrl cause no
follower writes; early release remains latched. Another member START while
active aborts. Only current Leader END completes normally.

Each START fully validates all members and rebuilds live topology. Live gate
requires a component of three. Freeze membership and initial geometry. One
Leader sample calls TranslationSession once and emits one two-follower batch
in logical-id order. Targets use initial geometry plus total displacement.

Validate every follower and Leader witness before any native write. Register
all expectations, check delivered lifecycle facts for all members without
COM/pumping, then HDWP. Keys bind group/gesture/batch/member/capability and
watermarks. Feedback and exact postverify are member-specific and order-neutral.
Followers never drive movement. Old-gesture receipts cannot ACK a new batch.

At END, reconcile exact final geometry, verify offsets/component, empty pending
and clear roles. Missing events remain explicitly missing. No restore between
the A/B/C gestures. After all three, guarded exact restore to pre-setup positions;
never close Explorer. Native failure stops writes, without rollback claims.

## Fixture, tests and boundary

Fix 3 invariant: STA-safe input waiting must not take ownership of unrelated
terminal interaction semantics. The reader never calls SetConsoleMode: Quick
Edit, processed input and VT input stay under the host's control. Modes are
observed before/after, not restored. Escape and prompt-defined Q cancel;
in-band Ctrl+C is ignored, and host copy/interrupt shortcuts are not intercepted.
The existing pump/NOWAIT path stays; VT DEL/ESC are recognized for the bounded
editor without disabling VT input. Full terminal editing is not a product goal.
The contract is `sta_message_pump_v2` + `preserve_host_mode_v1`; actual frontend
copy/paste is PENDING_HUMAN. See [Fix 3 report](../reports/R1C4A_FIX3_TERMINAL_REPORT.md).

Fix 2 standing harness invariant: once the owner has apartment-affine
COM/Shell objects, it must never wait for human console input without pumping
Windows/COM messages. All C4A confirmations, group consent, readiness Enter/Q
and subjective inputs use `StaConsoleLineReader`: MsgWaitForMultipleObjectsEx,
unfiltered owner dispatch, and documented ReadConsoleInputExW NOWAIT records.
There is no console worker or movement processing in this helper. Per-wait
aggregate evidence contains counters/status only, never input/message contents.
The active `run_gesture` pump, frame guards and accepted baseline stay unchanged.
See [STA wait research](../research/R1C4A_STA_CONSOLE_WAIT.md) and
[Fix 2 handoff](../reports/R1C4A_FIX2_STA_CONSOLE_REPORT.md).

Fix 1 separates the immutable binding snapshot from the accepted restore
baseline. `preview_readiness()` makes a fresh, owner-only complete capture of
all three existing capabilities on every request. It has no native movement,
HDWP, hook activation, roles or gesture-generation side effects. A human may
resize the same consented frames and press Enter to preview again, or Q to
cancel. Nongeometry identity/anchor/security/monitor/DPI changes fail closed.

`setup()` independently captures and recomputes readiness again. A fresh NOT
FIT returns to the same readiness loop with zero native writes. Only a fresh
FIT freezes the accepted baseline and assigns `original_` / `current_` before
the existing pure-translation setup. The unchanged restore path now consumes
that accepted baseline, never the pre-resize binding geometry. Authority and
member capability generations are not reissued. The original final native
preflight remains fail-closed if a window changes after baseline acceptance.

Each capture is logged as `readiness_preview`, including the setup recheck;
the successful setup capture is also identified once as `readiness_accepted`.
The activity counters and QPC belong to the capture/acceptance boundary, not
JSON serialization time (the setup result is serialized after `setup()`
returns). The runner requires setup-before == accepted snapshots, native
start after that capture, and final visible/positioning restore == accepted
snapshots. A historical layout-only block remains valid blocked evidence,
not a runtime regression; old PASS evidence without this contract cannot
be promoted to Fix 1 PASS.

See [Fix 1 report](../reports/R1C4A_FIX1_READINESS_REPORT.md).

L-shape A-B/A-C, no B-C corner edge. Translation-only setup, checked work-area
fit, extents, arithmetic, monitor/DPI, overlap and edges; print fit deficits.
Tests cover consecutive A/B/C leadership and generations, unauthorized and
disconnected nodes, preflight, member feedback orders/duplicates/missing/stale,
Begin/Defer1/Defer2/End failures, per-member mismatch and invalidation.

PRODUCT_MAX_COMPONENT_SIZE = UNDECIDED. Three is the live fixture bound.
No polling, injection, arbitrary-HWND admission, resize, mixed-DPI or z-order.
Full legacy regression; implement/test/push, then STOP for independent review
before human UAT. C4B scaling and C4C z-order are not started.

## Evidence and conservative boundaries

Native feedback contains no PaneBind batch id or geometry. The owner matches
only independently captured current visible geometry against that member's
exact pending result and its generation/sequence/native-time watermarks. An
ambiguous same-millisecond or pre-native receipt does not ACK. A repeated
outstanding target is rejected rather than choosing a batch. Missing feedback
is reconciled only at exact END; no fabricated WinEvent is logged. Buffered
JSON serialization time is not used for any latency measurement.

The private bridge uses the original complete Explorer validator for every
member, including fresh VDM queries, and checks all delivered browser facts and
native PID/TID/root/owner before admission. The owner additionally guards raw
queued destroy/concurrent-START/follower-END receipts. Leader END may be queued
while a final operation is finishing; it is a completion barrier, not a token
revocation. A final END-sourced exact batch is explicitly labeled; evidence
still requires actual LOCATION-sourced native batches before END.

Real asynchronous START can arrive after geometry has moved enough to lose
adjacency. Such a fixture is rejected, not repaired from a fabricated historical
snapshot. The new path does not enlarge the visible work area or secretly
resize a too-large L-shape. Three-member performance, subjective rigid-body feel
and real Explorer failure recovery remain NOT TESTED before human validation.
