# R1-C3B Phase 2 safety change register

2026-09-09/10; BLOCKED, experimental implementation withdrawn. See
[validation contract](../research/R1C3B_PHASE2_VALIDATION_CONTRACT.md).

CHECK: global candidate uniqueness.

OLD: re-proven from global Shell inventory at every full validation, including
each active capture, prepare, immediate pre-native and postverify.

NEW (REJECTED PROPOSAL, NOT THE EFFECTIVE CONTRACT): full selection uniqueness through setup/arm/START; after accepted Ctrl
START in the dedicated Phase 2 session, exact retained COM object and private
generation-scoped pair authority are the identity invariant. Active validation
has no global inventory. END/restore full inventory audits the original target,
never selects or grants authority to another matching window.

INITIAL SAFETY ARGUMENT (INSUFFICIENT): object/location are distinct facts. No operation accepts a caller
HWND or searches by location. A duplicate unrelated/preexisting window cannot
replace the retained object or resolve its token/permit. Leader and Follower
remain independently bound. Fresh direct location and all native/security/
desktop/geometry checks remain. No implementation code is derived from GPL.

FAIL-CLOSED SIGNALS: browser navigation/quit/stream failure or queued unresolved
receipt, canonical/HWND change, token/consent/pair/activation generation change,
native identity/process failure, location mismatch, state/monitor/DPI changes,
WinEvent stream poison. Both-target receipt checkpoint after COM and before
native adds an owner-side guard, not callback work. Invalidation never rebinds.

REMAINING RISK: non-atomic OS samples; events not yet delivered cannot be known.
An already-in-flight native call is not retractable. Real Shell/reentrancy and
profile overhead/smoothness remain human UAT risks, not synthetic observations.
Only active writes after observed invalidation must be zero; valid cleanup is
separate and cannot bypass token retirement. Same-user malicious process/OS
compromise is outside this harness's threat model.

UNCHANGED: native flags, exact postverify, feedback generations/watermarks,
Ctrl START latch, scheduling/coalescing, input hooks, virtual desktop/security
checks, two-window/same-monitor/same-DPI scope, all default legacy entry points.

ACTUAL CHANGE SHIPPED: NONE. Full inventory and its original uniqueness/shared-
frame rejection remain at every existing point. No weakened check is retained.

WHY BLOCKED: distinguishing unrelated B on another HWND from a sibling Shell
object on A's HWND is essential. Fresh canonical A/HWND/location and healthy
A callbacks do not prove current frame-entry multiplicity. Saved binding counts
are not live facts. A countermodel with H's current entry count 2 is rejected by
the old full validator but accepted by the candidate object-only proof. No
current-host Explorer tab behavior is claimed to have been manually observed.

EPOCH FALLBACK: also unproved. Current exact-object browser epoch is not a
global location/membership epoch; COM/reentrancy and native boundaries prevent
blind sharing between current validation points. Do not deduplicate by time.

DISPOSITION: experimental tracked patch/new-file drafts and model outputs kept
only under ignored out/r1c3b-phase2-rejected-draft/. All source/test/runner/CMake
edits removed from the working diff. No UAT, PR, merge or new branch. Safety gate
BLOCKED means the proposed optimization is not approved, not that sealed R0,
C2A, C2B or C3A evidence failed.

## Human Root approved refinement — 2026-09-10

Authority: [formal decision](../architecture/R1C3B_FRAME_AUTHORITY_DECISION.md).
Prior sections describe the old contract and are permanently retained.

OLD: ongoing global inventory re-proves single entry and candidate uniqueness.
NEW: strict initial selection freezes a frame; post-issuance movement authority
is frame-scoped and anchored to original canonical A. Same-frame B does not
create a second native frame. Different-frame B can never receive A's permit.
WHY: SetWindowPos operates F, not a Shell tab; Human Root intentionally changed
this contract. No old-rule semantic-equivalence claim is made.

REMOVED FROM ACTIVE HOT PATH: global entry cardinality / candidate uniqueness.
NOT REMOVED: initial uniqueness, original canonical anchor/location, frame HWND,
token/session/consent/capability generations, process/thread/image/class/root,
security/state/desktop, geometry/monitor/DPI, event health and exact postverify.
Anchor navigation, quit (even with B/F surviving), rehost, retirement and reuse
still abort. No replacement anchor, raw-HWND capability, global membership
ledger, time cache, callback expansion, or native flag/scheduling change.

REMAINING RISK: ordinary non-atomic OS observations and asynchronous delivery;
real Windows 11 Explorer tab implementation and smoothness remain NOT TESTED.
The first same-frame acceptance is explicitly deterministic/model evidence.

IMPLEMENTED / AUTOMATED TESTED: d8bdc0ba3ec7ef625081eacf49113c8293e51268.
Both-target browser receipt checks plus an owner-only non-consuming pending
destroy scan guard native registration. Observed destroy/identity-stream
failure retires frame tokens before cleanup, preventing numeric HWND reuse
from reviving authority. No callback body expansion; no ordinary C2A/default
mode contract change. Complete results and remaining empirical risks are in
[the amendment report](R1C3B_FRAME_AUTHORITY_EXECUTION_REPORT.md).
