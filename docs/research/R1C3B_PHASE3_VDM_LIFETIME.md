# R1-C3B Phase 3 — VDM lifetime design gate

2026-09-12; after transport repair 44e76a8e1eb290924c10e23c6581836f3bda2efb.
Scope: service acquisition lifetime only, plus compact evidence-runner output.
No desktop result cache; no Shell, frame authority, placement or scheduling change.

## Evidence and prior art (before implementation)

Phase 2 user Debug run 20260910T161142993Z replays with exit 0, correctness,
profile and frame-fast gates PASS. Harness SHA256
A982CE289FC600C87FDAFF863D32540911A2A262D8B2792B82805FCBB8307F1F.
Human C+ means improved from C, not B. 60 applies / 60 suppressions, no duplicate
or missing, final/restore exact; active inventory zero. Quantum p50/p95
81.4957/145.457 ms; apply interval 100.2221/205.6511 ms; max queue 23.
VDM is 47.44401097096647% of operation-local exclusive cost, largest 52/60;
native 7/60 and location 1/60. The measured VDM stage includes creation, query
and Release; acquisition's separate time was NOT measured in Phase 2.
Source confirms one CoCreateInstance(INPROC_SERVER), one query, one Release
at each successful live validation. Splitting those costs is Phase 3 work.

Mandatory reference inspection, no external code copied/adapted:

- AltSnap (mature, GPL-3.0-or-later reference-only), local verified pin
  5c86416ad21e4b72844a998a746bd3bb0bee5f5d, hooks.c window-enumeration/filtering
  3040-3087 and License.txt. Path history: 62adaf14503417a4fdbff61e023950208436c77d
  constness; 96ea072af389362f79b66c8286289f9837c689ef SDK support. These are not
  VDM reuse evidence; PaneBind keeps its own strict desktop eligibility.
- FancyZones (mature, MIT reference-only), verified LICENSE and
  [VirtualDesktop.cpp](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/VirtualDesktop.cpp)
  / VirtualDesktop.h at 19c4d805321db86f3634e6968e14dbf25cbba14a: constructor
  acquires manager, methods query repeatedly, destructor releases. Its singleton,
  registry fallback and weaker error paths are NOT adopted.
- FancyZones [PR 29059](https://github.com/microsoft/PowerToys/pull/29059),
  inspected path history and diff of 78a94aecb965a7c10ded273a1227da3e2259b341:
  removed obsolete desktop-ID/current-ID tracking. Additional history inspected:
  890b7f4286a95ced04d7da140b474f90fd4351ed (#28556) and
  f5f8861eac976384273523be335572315a54566c (#18805).
- [Issue 49019](https://github.com/microsoft/PowerToys/issues/49019) reports
  stale registry desktop identity after switching. This is an upstream report,
  not PaneBind observation; do not replace fresh queries with cached state.

Official contracts:

- [IsWindowOnCurrentVirtualDesktop](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-ivirtualdesktopmanager-iswindowoncurrentvirtualdesktop)
  queries the supplied top-level HWND's current desktop; require S_OK and true.
- [CoGetApartmentType](https://learn.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-cogetapartmenttype)
  detects uninitialized/wrong apartment; [STA](https://learn.microsoft.com/en-us/windows/win32/com/single-threaded-apartments)
  requires apartment-affine interface use and permits reentrancy.
- [CoUninitialize](https://learn.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-couninitialize)
  must balance successful initialization, including S_FALSE, after COM objects
  are released. [Microsoft's example](https://devblogs.microsoft.com/oldnewthing/20171002-00/?p=97116)
  retains a VDM across queries and releases before apartment shutdown; its
  global pointer/permissive fallback is not a PaneBind design template.

## Chosen ownership and failure contract

One noncopyable/nonmovable manager wrapper per opt-in Phase 3 private
ExplorerGlueSession, created before test layout/arm/active work. It has no
window capability and is only a read-only service. Both role validations
borrow it on the SAME owner STA; their tokens/anchors remain independent.
This is the lower-risk fallback: C2A, legacy, C2B/C3A and Phase 1/2 default
executables retain their original ephemeral manager path.

The wrapper requires an already initialized STA/MainSTA, then takes one
balanced CoInitializeEx reference in that same apartment. It checks owner
thread/apartment for calls; no cross-thread interface export exists. Close
releases the manager first, then balances its own COM reference, and is
idempotent. Normal finish closes after restore but before Explorer target
destruction. Its member is declared after the target owners, so exception
unwinding also releases it before their CoUninitialize calls. Existing parent
wrong-thread destruction fails safe by retaining its owner-affine aggregate;
no foreign-thread COM cleanup is introduced. Wrong-thread calls do not query
or mutate the retained service; tests return to owner for cleanup.

Acquisition occurs once, without retry. Failure leaves eligibility unavailable.
Each original validation still calls IsWindowOnCurrentVirtualDesktop(current
authorized HWND), with a fresh FALSE-initialized local BOOL and exact HRESULT
check. False or query failure rejects that validation; no cached true, recreate,
or retry. Existing fail-closed native/Core paths handle that rejection.

## Evidence and tests defined before implementation

Keep virtual_desktop parent; add manager_acquire and query substages. Record
actual create/query/release counts per setup/START/active/END/restore/invalidation,
and per-validation create/query deltas. Positive active: create=0, query=1 for
EVERY successful required validation, not merely a nonzero total. Retained
service: one create and one release per Glue session. Initial one-shot
provisioning may still make ephemeral service instances; label those separately.

Deterministic fake COM tests: 60-operation-equivalent validation sequence;
one creation, every fresh query, exactly one Release before COM shutdown;
true -> false; query failure; create failure; uninitialized/MTA/foreign thread;
explicit close, repeated close, exception cleanup. A live read-only owned-HWND
probe may verify service acquisition/query/release, never switch desktops or
control user windows. Full Debug/Release and old 26/61/47/41 runner suites plus
Phase 3 fixtures are required.

Compact default output suppresses per-operation/quantum detail, not checks or
raw JSONL; VerboseOperations restores detail. Compare Phase 1/2/3 at the same
grain. VDM family exclusive share sums disjoint parent+child exclusive costs
to avoid claiming improvement merely because a new child moved time out of
the parent. No synthetic wall-clock speed claim and no FPS/latency SLA.

Research/design gate: PASS to implement this bounded change. Actual performance
and subjective A/B/C+/B-C/C/D/E remain PENDING_UAT. Do not optimize a third hotspot.
