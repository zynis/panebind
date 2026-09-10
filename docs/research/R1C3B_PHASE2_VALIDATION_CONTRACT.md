# R1-C3B Phase 2 validation contract

Review started 2026-09-09, stopped 2026-09-10 (Asia/Shanghai).
Starting implementation 14e0c6818894c6912d3b3cb4d5a922f427b4e6ce.

STATUS: BLOCKED. The initial design below was written before the experiment,
then rejected during adversarial review. The experimental source/runner changes
have been withdrawn; they are not a Phase 2 implementation or UAT candidate.
The effective runtime contract is the unchanged Phase 1 full validation.

## Blocking distinction: another frame versus another object in this frame

An unrelated B with a different HWND cannot inherit A's token merely by reaching
the same location. That useful conclusion does NOT prove the target frame still
has one Shell entry. Existing C2A research explicitly rejects tabs/shared frames;
the live inventory enforces shell_entry_count == 1 and one location witness.

The exact-object observation's bound_window_entry_count and
exact_location_match_count are saved at binding. Its facts() copies those
members; it does not refresh them. Canonical IUnknown proves the retained COM
object, not the absence of other objects sharing its top-level frame.

The official [HWND property contract](https://learn.microsoft.com/en-us/previous-versions/windows/internet-explorer/ie-developer/platform-apis/aa752126(v=vs.85))
explicitly distinguishes top-level frame from selected tab in tabbed IE. This
is a documented counterexample to a general COM-object-to-HWND bijection, NOT
an empirical claim about this host's Windows 11 Explorer tab behavior. No
reviewed official contract guarantees that adding/switching a sibling Shell
object delivers navigation/quit to the retained A before a frame operation.

Deterministic countermodel executed in the experimental Debug unit executable:

| Fact | Initially | After sibling object appears |
| --- | --- | --- |
| Canonical A / retained IWebBrowser2 | A | A |
| A's current HWND / authorized HWND | H / H | H / H |
| A's direct filesystem identity | nonce A | nonce A |
| A's generation and delivered browser stream | valid | unchanged/valid |
| Observation's saved bound-entry count | 1 | 1 (snapshot) |
| Current global inventory entry count for H | 1 | 2 |
| Existing full selection/frozen-fingerprint validation | eligible | rejects |
| Proposed object-only proof | eligible | still eligible |

Output: PHASE2_SHARED_FRAME_COUNTEREXAMPLE = REPRODUCED_IN_MODEL.
This establishes an observability/proof gap, not an observed real Explorer
exploit. It is insufficient to declare this state impossible or silently relax
the no-tabs/shared-frame rule. Passing the other negative tests does not fix it.

### Inventory epoch fallback assessment

Current UserConsent retains an exact browser observation, not a complete
global Shell inventory mutation epoch. Another existing Shell object's location
can change without registering a new top-level window. An A-only browser epoch
cannot certify the global inventory, nor can a registration-only epoch certify
all locations. Between current capture/prepare/immediate/postverify inventory
uses are outgoing Shell location/HWND calls and virtual-desktop COM calls;
STA reentrancy is permitted. Native placement is also an explicit epoch boundary.

There is no proved immutable interval spanning two existing inventory requests.
Sharing would require a new observed global mutation/dispatch contract or a
separate, evidenced target-frame witness. Time-only reuse, assuming COM cannot
dispatch, treating an unchanged A counter as a global epoch, or moving inventory
under a differently named wrapper is not an acceptable fallback. No useful
inventory epoch deduplication is approved in this round.

Stop under AGENTS.md: a major architectural assumption lacks evidence. Do not
start human optimized UAT; return the frame-identity/epoch question for review.

## Evidence and prior art

The local Phase 1 human run 20260909T143539511Z passes the frozen offline
validator. Subjective C is supplied by the user. Global Shell inventory is
82.9303798959% of matched operation-local exclusive time, largest in 16/16
operations. Its 92 calls have p50 40.22775 ms / p95 106.9213 ms. This is the
primary cause; long quanta (p50 218.5243 ms) produce downstream backlog (max 51).
Notification dispatch p50 about 1030 ms is not causal processing wait: 20
drains precede dispatch, 362 receipts inherit a pending notification, and no
observed dispatch precedes drain. Do not redesign scheduling.

Reinspected reference-only source/license/history:

- AltSnap 5c86416ad21e4b72844a998a746bd3bb0bee5f5d, GPL-3.0-or-later,
  hooks.c worker 695-735, License.txt, [PR 609](https://github.com/RamonUnch/AltSnap/pull/609),
  commits 7f4afe59076b70980f71af202f63609ca3ac5745 and
  7d4c7deb17437a7d5d350fd8074f6285444f4c51. Mature movement reference;
  coalescing/worker history is not authority to move COM to a worker here.
- FancyZones 19c4d805321db86f3634e6968e14dbf25cbba14a, MIT, root LICENSE,
  FancyZones.cpp destroy/AbortMoveSize paths; mature production reference.
  [PR 48569](https://github.com/microsoft/PowerToys/pull/48569), commit
  dd26d86580168d2e368701f7b0c4d629dc9cd9ac preserves abort on destroyed target.
  [PR 18106](https://github.com/microsoft/PowerToys/pull/18106), commit
  22786a6bdcbbc6eaeb417a6c6f1f15bb6fb0a550 removes repeated default-layout
  work. Neither proves PaneBind's object-bound contract. No code copied/adapted.

Official contracts:

- [QueryInterface identity](https://learn.microsoft.com/en-us/windows/win32/com/rules-for-implementing-queryinterface)
  guarantees canonical IUnknown identity for an object, not HWND lifetime.
- [STA and reentrancy](https://learn.microsoft.com/en-us/windows/win32/com/single-threaded-apartments)
  require owner-apartment access; an outgoing COM call may admit reentrancy.
- [NavigateComplete2](https://learn.microsoft.com/en-us/previous-versions/aa768334(v=vs.85))
  can identify a top-level or frame dispatch. URL text is not capability proof.
- [Unadvise](https://learn.microsoft.com/en-us/windows/win32/api/ocidl/nf-ocidl-iconnectionpoint-unadvise)
  ends the connection; retaining a pointer alone does not prove subscription health.

PaneBind source inspection confirms the observation owns exact IWebBrowser2
and canonical IUnknown references, queries current HWND and current filesystem
identity, and processes bounded browser receipts only on its STA. Callback
sequence versus processed sequence permits a no-COM final receipt checkpoint.
Callbacks themselves remain unchanged. Global inventory traverses all Shell
entries, reads COM HWND/location and resolves filesystem identity; this is
global work, not simply a local HWND lookup.

## Selection versus runtime authority

Selection proves which newly confirmed, baseline-excluded, exact unique
candidate the user authorized. Setup/arm/START retain this full proof.
After accepted Ctrl START, authority is the retained exact object plus native
identity, ledger, consent/capability generation and private pair seal/permit.
Location remains a required condition on that object, never an object selector.

Once a user-consented Explorer target is bound to a canonical Shell object
and a generation-scoped capability is issued, a later unrelated Explorer
navigating to the same filesystem location does not transfer, duplicate,
or broaden that capability.

Full boundary inventories after activation audit the originally bound target;
they do not select another candidate or require global nonce uniqueness again.
This explicit refinement also applies to END/restore in the opt-in session,
so unrelated B cannot silently block legitimate cleanup of unchanged A.
All other entry points keep their original selection validation.

## Threat model

| Case | Decision and evidence required |
| --- | --- |
| A: unrelated or preexisting B reaches nonce | No abort solely for B. No search/rebind exists in active path; B cannot mint token, seal, permit, activation or generation. Same PID/path is insufficient. Full bound audit still requires original A entry. |
| B: Leader navigates away | Abort on navigation epoch or fresh direct location mismatch; no next active Follower write. |
| C: Follower navigates away | Same, before next write. Direct location check detects delayed/missing useful navigation events. |
| D: rehost/HWND or canonical object changes | Abort, no replacement and no HWND-only fallback. |
| E: quit/destroy/process exit | Abort via browser receipts/native lifetime/identity checks. |
| F: malformed/overflow/wrong-thread/retired/unsubscribed or pending unprocessed browser evidence | Fail closed. Existing WinEvent poison also aborts. Unknown browser lifecycle callbacks already poison existing sink; no callback expansion. |
| G: token/consent/capability/pair/activation generation mismatch | Private binding/ledger checks reject before native. Separate Leader and Follower witnesses even for one PID. |

Threat scope is cooperative Windows/Shell correctness, not protection from a
compromised same-user OS/Shell process. Every OS observation is a sample, not
an atomic transaction with SetWindowPos. An invalidation delivered during an
already-entered native call cannot retract that call. It must fail postverify
and prevent any subsequent active write. Never claim to detect undelivered OS
events instantaneously. Cleanup is separately classified, fully validated,
and cannot resurrect an invalid token.

## Rejected candidate design and TOCTOU boundaries (not implemented)

Dedicated opt-in Phase 2 profile entry; legacy, Owned, Companion, C2A, console
C2B, default C3A and Phase 1 profile behavior remain unchanged.

Full inventory: provisioning, confirmation, setup/arm, START, END,
restore/cleanup and explicit invalidation branch. Active steady quanta and
active prepare/immediate/postverify retain their separate validation points,
but replace global candidate selection with fresh bound-object proof. No
time cache and no inventory shared across COM dispatch/native/event boundaries.
Inventory epoch sharing is NOT NEEDED if this exact contract passes tests.

Every active validation retains: token ledger/generation, healthy subscribed
browser stream/epoch, fresh canonical identity, current bound HWND, direct
exact filesystem location, native process instance/PID/TID/image, class/root/
owner/style, visibility/cloak/min/max, virtual desktop, user/session/integrity/
elevation/UIAccess/AppContainer, positioning and visible geometry, PMv2,
monitor/work area/DPI anchors. Exact native postverify and pending-before-native
feedback correlation are unchanged.

Before active native apply, refresh both independent witnesses, then perform
Follower immediate full native eligibility/geometry validation. After all COM
reads and immediately before pending registration/native, inspect existing
receipt counters for BOTH targets without COM or message pumping. New pending
or invalid receipts reject. Native flags and owner loop are unchanged.

## Tests defined before implementation

Deterministic tests: each threat above including location mismatch with no
event; both roles; duplicate B/new and B/preexisting cannot receive operation,
token or activation; no valid generation transfer; sticky invalidation; pending
receipt during COM invalidates final guard. Count fixture old repeated N
inventory requests versus steady fast zero, not synthetic timing speedup.
Normal-path equivalence must cover MovePlan, requested geometry, flags,
feedback correlation, final geometry and restore. Run all prior Debug/Release
CTest, Owned/Companion native tests and 26/61/47 runner fixtures plus Phase 2.
Runner must validate phases/modes/legal reasons, actual inventory counts,
invalidation and automatically compare frozen Phase 1 Debug metrics with new
Debug metrics. No raw event count speed claim. Human smoothness remains
PENDING_UAT; C fails the subjective objective and returns to review.

The initial design gate was withdrawn after the shared-frame countermodel.
Validation-contract/threat-model/fast-path/epoch/safety/equivalence gates are
BLOCKED. Active inventory zero is NOT ACHIEVED. No new runtime claim is made.

## Subsequent Human Root amendment — 2026-09-10

The history above remains unchanged: OLD_FAST_PATH_PROOF=FAILED and
SEMANTIC_EQUIVALENCE_TO_OLD_RULE=NOT_PROVEN. Human Root now explicitly defines
native authority as the exact selected top-level frame, not entry cardinality
or selected tab. The [new decision](../architecture/R1C3B_FRAME_AUTHORITY_DECISION.md)
supersedes the proposed steady-state predicate, not the historical result.
Its new-contract tests intentionally accept healthy post-issuance same-frame
multiplicity while preserving initial rejection and anchor invalidation.
