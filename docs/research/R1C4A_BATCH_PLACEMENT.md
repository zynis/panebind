# R1-C4A batch placement research

Review date: 2026-09-13. Starting main:
`81e40facf52ffdb96f76e4d737740167d485f4a7`.

## Reference gate

Reference-only inspection, not code reuse:

- AltSnap `5c86416ad21e4b72844a998a746bd3bb0bee5f5d`, GPL-3.0-or-later:
  hooks.c 827-977 (touching-window selection and deferred placement).
  Its global enumeration, resize policy, worker and sleep are not PaneBind
  authority or implementation templates. [PR 609](https://github.com/RamonUnch/AltSnap/pull/609)
  and inspected `7f4afe59076b70980f71af202f63609ca3ac5745` diff show owner work
  and consecutive-sample coalescing history. No GPL code copied or adapted.
- PowerToys/FancyZones `19c4d805321db86f3634e6968e14dbf25cbba14a`, MIT:
  DraggingState.cpp and root LICENSE; mature production reference.
  [PR 48569](https://github.com/microsoft/PowerToys/pull/48569) and inspected
  `dd26d86580168d2e368701f7b0c4d629dc9cd9ac` diff distinguish abort-on-destroy
  from successful end and clear transient dragging state. Its broader input
  hooks are not necessary for the sealed START-latched contract. No code reuse.
- PaneBind owned/companion batch operations: complete preflight, common-parent
  check, replacement HDWP, abandon failed Defer chain, independent postverify.
  Their registries do not authorize Explorer and will not be repurposed for it.

## Official HDWP contract

[BeginDeferWindowPos](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-begindeferwindowpos)
allocates a deferred-position structure; request the complete follower count.
NULL is failure. It is not a native movement commit.

[DeferWindowPos](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-deferwindowpos)
may replace the handle; always use its returned handle. Members must share a
native parent. NULL requires abandoning the chain without End. C4A flags are
NOSIZE | NOZORDER | NOACTIVATE only.

[EndDeferWindowPos](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-enddeferwindowpos)
performs positioning and sends window-position messages. Zero is failure.
These contracts do not guarantee transactionality, rollback or all-or-nothing
failure. Capture every target after native failure where identity still permits
read-only capture; stop future writes. All-followers preflight is not an atomic
native commit. One exact postverify cannot stand in for another member.

## Event and generation proof

[WinEventProc](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nc-winuser-wineventproc)
supplies source identity and generation-time milliseconds, not geometry or a
PaneBind operation identifier.
[Out-of-context delivery](https://learn.microsoft.com/en-us/windows/win32/winauto/out-of-context-hook-functions)
is asynchronous; keep callbacks short. Receipt QPC and owner geometry are not
event-time geometry.
[GetAsyncKeyState](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getasynckeystate)
high bit is current-state sampling. Retain C3A/C3B callback-delivery START
latching, not physical-input authentication.

Native receipts are role-neutral. A late receipt cannot gain the active
generation merely because it is drained late. Attribution requires member and
capability, group/gesture/batch generations, exact independently captured
geometry and registration watermarks. Ambiguity must not ACK. Exact
missing-event reconciliation is distinct from observing feedback.

Fresh START capture may find disconnected geometry after asynchronous delivery.
Reject the three-member live gate; never invent historical START geometry or
silently use prior gesture topology. This remains a human-UAT risk.

## Empirical scope

C3B sealed evidence supports pair progression and retained-VDM fresh queries,
not three-member batch behavior. C4A needs deterministic native-failure seams
and owned-window probes before review. No new third-party observations claimed.
Source/license/history and official-contract gate: PASS. Implementation and
runtime gates are separate; human UAT remains NOT TESTED until after review.
