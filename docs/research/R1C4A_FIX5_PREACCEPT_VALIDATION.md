# R1-C4A Fix 5: pre-accept validation research / design

2026-09-15; scope is pre-accept capture and structured diagnostics. No live
Magnet, active Glue Resize, lock UI, new native operation or mixed-DPI support.

## Forensic boundary

The 20-record human log `20260915T085644241Z-bce8cda6261a4bbfa66947e6f16ab32b`
has a valid first disconnected preview, five complete console waits, then a
null second capture. It contains no failed-member index, underlying eligibility,
invalidation, browser event facts or canonical-anchor result. Therefore:

```text
CURRENT_EVIDENCE_DIAGNOSTIC_RESOLUTION = INSUFFICIENT
CURRENT_HUMAN_CAPTURE_ROOT_CAUSE = NOT_PROVABLE_FROM_EXISTING_EVIDENCE
```

Code inspection: preview -> capture -> group_binding_matches ->
validate_or_retire -> receipts_healthy collapses failures to nullopt. The native
validator does NOT compare visible/positioning bounds against issuance geometry.
Its wrapper does permanently invalidate/retire on monitor/DPI failure. The
browser sink rejects every DISPID except NavigateComplete2/OnQuit as malformed.
These are proven structural behaviors, not proof of which failed in this log.

## Prior art / platform gate

- AltSnap mature GPL-3.0-or-later reference only, pinned
  `5c86416ad21e4b72844a998a746bd3bb0bee5f5d`: hooks.c 3040–3085 filtering,
  License.txt, history `397b84b2db114de979d428c3cec0cddec2a339ae` bounded-storage
  allocation fix. Dynamic geometry/monitor eligibility is not object identity.
- PowerToys/FancyZones mature MIT reference only, pinned
  `19c4d805321db86f3634e6968e14dbf25cbba14a`: WindowMouseSnap.cpp and LICENSE;
  [PR 48569](https://github.com/microsoft/PowerToys/pull/48569), merge
  `dd26d86580168d2e368701f7b0c4d629dc9cd9ac`, separates abort on destroy from
  successful movement. Preserve lifecycle invalidation, not a permissive retry.
- Microsoft [DWebBrowserEvents2](https://learn.microsoft.com/en-us/previous-versions/windows/internet-explorer/ie-developer/platform-apis/aa768283(v=vs.85))
  lists geometry separately from navigation/quit. Official
  [Left](https://learn.microsoft.com/en-us/previous-versions/windows/internet-explorer/ie-developer/platform-apis/aa768303(v=vs.85)),
  [Top](https://learn.microsoft.com/en-us/previous-versions/windows/internet-explorer/ie-developer/platform-apis/aa768305(v=vs.85)),
  [Width](https://learn.microsoft.com/en-us/previous-versions/windows/internet-explorer/ie-developer/platform-apis/aa768306(v=vs.85)),
  [Height](https://learn.microsoft.com/en-us/previous-versions/windows/internet-explorer/ie-developer/platform-apis/aa768302(v=vs.85))
  specify one Long parameter. They describe WebBrowser property/script behavior,
  not proof that Windows 11 Explorer emits them during native human dragging.
- Installed Windows SDK 10.0.26100.0 `um/ExDisp.Idl` 497–507 and ExDispid.h
  53–56 confirm one `[in] long` and IDs 264/265/266/267. Read for API contract;
  no samples or upstream implementation copied/adapted.

Research gate for explicit four-event classification and scoped capture mode:
PASS. Human root-cause proof remains unavailable. A deterministic invocation
of the actual sink will distinguish old malformed handling from the correction;
no live Explorer experiment is authorized this round.

## Original scoped design and pre-implementation test plan

GroupCaptureResult preserves snapshots, failure member/stage (Binding,
NativeValidation, ReceiptHealth, Context), eligibility enum, numeric diagnostic
domain/code and bounded browser/anchor facts. Do not serialize diagnostic free
text, APIs containing paths, URLs, user files, clipboard or input contents.
Historical logs lacking the contract remain explicitly insufficient, never
backfilled with invented facts.

PreAcceptMutableGeometry calls the complete native validator **without** the
sticky failure/retire wrapper. Identity, capability/consent generations, frame,
canonical Shell anchor, exact nonce, security and desktop stay mandatory.
Only MonitorChanged/DpiChanged can be recoverable: these are evaluated after
all identity/security/location/desktop gates, and binding plus receipt health
must be rechecked. All three members are validated even if one is recoverable;
a later fatal failure outranks a temporary monitor condition. Geometry itself
is freely recaptured. Missing/unrepresentable geometry, unknown context, state,
desktop, identity or stream failure stays fatal with an explicit diagnostic.

Returning to the **original** monitor/DPI permits another preview; no anchor
monitor/DPI is rebased. Acceptance still requires all three on the supported
same original monitor/DPI. Fixture immutable comparison continues to ignore
only visible_rect/positioning_rect. No active-source, roles, pending or native
writes before acceptance. Y performs another fresh pre-accept validation,
freezes the accepted baseline, then enables the existing strict runtime.

Recognize only the four geometry DISPIDs with IID_NULL, exact METHOD flags,
one VT_I4 argument and no named args. Count without retaining parameter values
or creating navigation receipts. Do not advance the navigation callback/latest
sequence for benign geometry, which would falsely signal unresolved receipts.
Unknown DISPID, bad signature, wrong thread, post-retirement calls, unrelated
navigation identity, quit and poisoned stream remain fail closed. A valid
geometry notification grants no authority; capture still proves the anchor.

Tests A–D: Move, Resize, repeated adjustments, disconnected no-retire. E–H:
navigation, HWND/PID/TID/rehost, quit, security fatal/retire. I/J: actual sink
valid geometry versus malformed/unknown/foreign-identity invocation. Inject
binding/native/receipt/context failures and verify structured member/stage/
reason; temporary monitor then return, later-member fatal precedence, strict
mode still retires, accepted baseline and zero pre-accept writes/generation.
Full Debug/Release, owned/companion, C2B/C3A/C3B and C4A runners, frozen Magnet.
