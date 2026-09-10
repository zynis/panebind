# R1-C3B Human Root decision: frame-scoped Explorer movement authority

Date: 2026-09-10. Authority: the user's explicit Human Root Amendment, not an
inferred optimization permission. Prior blocked evidence was checkpointed and
pushed as 2cc464119f3fa4093b20b54782c093c50ffdfa4c.

The prior stop was correct under the prior contract. Human Root subsequently
refined the contract. OLD_FAST_PATH_PROOF=FAILED and
SEMANTIC_EQUIVALENCE_TO_OLD_RULE=NOT_PROVEN remain historical facts. The old
[counterexample](../research/R1C3B_PHASE2_VALIDATION_CONTRACT.md) is not deleted:
full validation rejected same-frame multiplicity while the old candidate
accepted it. This round tests NEW_FRAME_AUTHORITY_CONTRACT_PROVEN, not equality
to the removed single-entry steady-state rule.

## Decision and object levels

PaneBind Explorer native movement authority is top-level-frame scoped.

| Level | Responsibility | Does not mean |
| --- | --- | --- |
| Canonical Shell automation object A | Retained observation anchor/sentinel: identity, current HWND, nonce location, lifecycle | A is a native movement capability or the selected tab |
| Native root frame F | Exact SetWindowPos target | Every Shell object with the same location, or a future HWND after rehost |
| Opaque PaneBind capability | Authority/session/logical ID/generation plus private pair/operation permit for F | A raw HWND, registration cookie or Shell entry |

Initial selection uniqueness is REQUIRED: immutable baseline exclusion,
user-created new Explorer frame, unique nonce candidate, one unambiguous Shell
entry at binding, canonical identity and HWND, process/thread/root/class,
security and all existing eligibility. Nothing changes before token issuance.

After issuance, the dedicated Phase 2 Glue route retains authority over F,
anchored to the original A. Extra entries/tabs in F do not mint another frame
capability. An unrelated G, G != F, gets no capability, activation, permit or
write even when its location equals A's nonce. Neither same-frame nor different-
frame B may replace A. Location is a predicate on A, never a target selector.
Leader and Follower keep separate anchor, nonce, token and frame even at one PID.

USER_PREEXISTING_WINDOWS_TOUCHED=NO means no native operation was directed to
any unauthorized independent top-level native frame HWND. It does NOT mean
authorized F can never contain another tab/object. Tab/content membership is
not independent frame authority. No tab/content manipulation is implemented.

## Platform evidence and limits

[IWebBrowser2 HWND](https://learn.microsoft.com/en-us/previous-versions/windows/internet-explorer/ie-developer/platform-apis/aa752126(v=vs.85))
documents the top-level frame rather than selected tab for tabbed IE. This is
FRAME-AUTHORITY DESIGN INPUT, not FULL WINDOWS-11-EXPLORER TAB IMPLEMENTATION
PROOF. Real Explorer tab UAT is deferred by Human Root; deterministic/model
tests are the first acceptance evidence for the revised authority predicate.

[IShellWindows::Register](https://learn.microsoft.com/en-us/windows/win32/api/exdisp/nf-exdisp-ishellwindows-register)
registers a handle and assigns a unique collection cookie.
[WindowRegistered](https://learn.microsoft.com/en-us/windows/win32/shell/dshellwindowsevents-windowregistered)
and [WindowRevoked](https://learn.microsoft.com/en-us/windows/win32/shell/dshellwindowsevents-windowrevoked)
are collection-membership lifecycle notifications, not PaneBind capabilities.
No global DShellWindowsEvents inventory reconstruction is introduced.

Pinned AltSnap/FancyZones source, licensing and history are retained in the
prior proof/provenance records. Reinspection of AltSnap placement and
FancyZones native drag HWND/destroy-abort paths confirms the level of native
targeting but does not prove PaneBind authority by analogy. No external code
is copied/adapted. Canonical IUnknown and owner-STA/reentrancy contracts remain
as cited in the prior research.

## Design and validation boundaries

An explicit Phase 2 harness enables frame validation only after both strict
UserConsent token issuances. Readiness/pair/setup/arm/START and END/restore may
capture full inventory, auditing original frame presence rather than asserting
entry cardinality remains one or reselecting by location. Ordinary C2A/public
one-shot validation and all default harness modes retain the old contract.

Steady active Glue capture, prepare, immediate pre-native and postverify use
CONSENT_BOUND_FRAME_FAST; global inventory requests must be ZERO. Full boundary
mode is FULL_GLOBAL_INVENTORY. No time-based cache or epoch dedup is needed.
Only the private Glue session sets active mode following accepted Ctrl START;
diagnostic phase counters cannot issue authority.

Every live check retains token/session/consent/capability generations, original
canonical anchor, current A HWND == F, direct current nonce filesystem identity,
browser stream health/navigation epoch, native HWND/PID/TID/process instance,
image/class/root/owner/style, visibility/cloak/min/max, virtual desktop,
process security, positioning/DWM geometry, PMv2/monitor/DPI. Immediate
validation and exact postverify are not deleted. Win32 placement flags stay
NOSIZE|NOZORDER|NOACTIVATE and synchronous. Core/MovePlan, Ctrl START latch,
eight-message fairness, quantum/coalescing and feedback ledger stay unchanged.

Before active native write: refresh Leader anchor/native witness, then Follower
immediate validation; check both existing browser receipt states without COM
or pumping before pending registration/native. Postverify checks Follower and
both receipt streams again. All checks run on the same owner STA. No WinEvent
or browser callback work is added. An already-entered native call cannot be
retracted by an invalidation delivered during that call; it must fail postverify
and prevent subsequent active writes. Undelivered events are not claimed known.

## Threats and tests defined before implementation

| Input | Required result |
| --- | --- |
| Multi-entry ambiguity before token issuance | Reject; original selection unchanged |
| Binding count 1, later same-frame count 2; A/F/location/generations healthy | Frame authority remains valid under NEW contract; no extra token/permit |
| Different G, same nonce, new or preexisting | G writes/permits/activation zero; no authority transfer |
| A direct location differs, even without useful navigation callback | Abort before next active write; B cannot substitute |
| A quit/retired/canonical unavailable while B and F remain | Abort, never promote B |
| A rehosts F -> H | Abort; H inherits nothing and F gets no later active write |
| Browser malformed/overflow/wrong thread/unadvised/post-retirement/pending unresolved receipt | Fail closed |
| Observed F destroy then numeric HWND reuse | Retired token/generation/anchor cannot resurrect |
| Native identity/state/security/desktop/geometry/monitor/DPI change | Existing fail-closed predicates preserved |

Tests must use production predicate/ledger/activation seams and unchanged Core,
include profiling ON/OFF outcomes, exact native flags/final/restore and zero
unauthorized-frame writes. Count-based active inventory evidence is not a
synthetic wall-clock speed claim. Run full Debug/Release CTest, Owned/Companion
self-tests, 26/61/47 prior runner fixtures and new Phase 2 fixtures. Positive
human UAT remains one normal Debug Ctrl drag, not a tab experiment.

Research/design authorization gate: PASS to implement and test the amended
contract. Runtime/safety implementation gates are not inferred from this
decision; results and final SHA go in the amendment execution report.
