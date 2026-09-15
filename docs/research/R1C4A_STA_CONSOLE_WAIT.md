# R1-C4A Fix 2 — STA console wait research

Historical record: Fix 3 supersedes the mode-mutation/Ctrl+C-abort decisions
below, while preserving the proven STA pump and NOWAIT mechanism. Current
policy: [preserve host terminal modes](R1C4A_TERMINAL_MODE_PRESERVATION.md).

Reviewed 2026-09-14. Base: `88c2e5b9aedb85b92aa7d81b23b1e0f0ecaf29de`.

## Diagnosis and scope

The C4A harness has one synchronous `ReadConsoleW` line-reading function used
for A/B/C confirmations, group consent, readiness Enter/Q and both subjective
questions. Its caller also owns the Shell/Explorer STA objects. Waiting for a
human line therefore stops the owner message pump while those objects live.

The human reports Explorer becoming unresponsive during readiness resize.
This is consistent with the code defect and the documented STA deadlock risk.
No Explorer hang dump or blocked RPC stack was collected; the exact Explorer
call chain is not asserted. This is P1 UAT infrastructure, not Glue Resize or
dynamic-group movement failure. Manual pre-setup resize is allowed; active
Glue resize remains unsupported/fail-closed.

## Official contracts actually inspected

- [Processes, Threads, and Apartments](https://learn.microsoft.com/en-us/windows/win32/com/processes--threads--and-apartments)
  explicitly warns against non-pumping waits on STA threads, including clients
  using Shell objects.
- [Single-Threaded Apartments](https://learn.microsoft.com/en-us/windows/win32/com/single-threaded-apartments)
  requires a message loop; marshaled calls use the hidden OleMainThreadWndClass.
  Pumping may reenter callbacks, so lifecycle invalidation cannot be ignored.
- [MsgWaitForMultipleObjects](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-msgwaitformultipleobjects)
  and [MsgWaitForMultipleObjectsEx](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-msgwaitformultipleobjectsex)
  support console-input handles plus queue wakeups. Ex with QS_ALLINPUT and
  MWMO_INPUTAVAILABLE handles already-seen queued input. INFINITE is the
  production wait; no timer or sleep-based polling is needed.
- [CoWaitForMultipleHandles](https://learn.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-cowaitformultiplehandles)
  uses COM modal processing for STA callers. It is an alternative, but explicit
  unfiltered queue dispatch is simpler to audit for this console harness.
- [PeekMessageW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-peekmessagew)
  with null HWND and zero range filters retrieves thread/all-owner-window
  messages and dispatches nonqueued messages internally. Do not filter to
  PaneBind messages or HWND-null messages only. Propagate WM_QUIT cleanly.
- [ReadConsoleInput](https://learn.microsoft.com/en-us/windows/console/readconsoleinput)
  reads records rather than cooked lines, but can wait for its first record.
  [PeekConsoleInput](https://learn.microsoft.com/en-us/windows/console/peekconsoleinput)
  returns immediately; peek-then-read alone leaves a competing-reader race.
- [ReadConsoleInputEx](https://learn.microsoft.com/en-us/windows/console/readconsoleinputex)
  documents CONSOLE_READ_NOWAIT (0x0002), even for an empty buffer. This is the
  selected low-level read, dynamically resolved from system Kernel32 because
  no import library/header declaration is supplied. Windows 7+ satisfies the
  project's Windows baseline. Missing export means fail closed, no blocking
  fallback. The API signature/constant are platform facts, not copied samples.
- [Low-level input](https://learn.microsoft.com/en-us/windows/console/low-level-console-input-functions),
  [KEY_EVENT_RECORD](https://learn.microsoft.com/en-us/windows/console/key-event-record-str)
  and [console modes](https://learn.microsoft.com/en-us/windows/console/setconsolemode)
  distinguish input records, repeat counts and Unicode from cooked line/echo
  behavior. Console input is signaled while records remain. This is a local
  Windows test harness, not a new cross-platform terminal product.

## Mature references and history

AltSnap `5c86416ad21e4b72844a998a746bd3bb0bee5f5d`, GPL-3.0-or-later,
reference-only: local License.txt and hooks.c 694-737 worker message loop;
PR 609 history via immutable `7f4afe59076b70980f71af202f63609ca3ac5745` diff.
Its worker filtering is NOT a Shell STA template. No GPL code copied/adapted.

PowerToys/FancyZones `19c4d805321db86f3634e6968e14dbf25cbba14a`, MIT,
reference-only: verified root LICENSE, main.cpp apartment initialization call,
FancyZonesApp.cpp 126-189 routing; PR 48569's immutable
`dd26d86580168d2e368701f7b0c4d629dc9cd9ac` destroy subscription/dispatch diff.
Keep lifecycle delivery and abort semantics. No apartment-model equivalence
or console-reader implementation is inferred from FancyZones. No code reused.

## Independent design and test gate

Invariant: once apartment-affine objects live, no owner wait for human console
input may stop message/COM dispatch. All C4A inputs share one owner-bound
reader. The selected mechanism is Preferred A's low-level record model, using
the documented NOWAIT variant to remove the secondary blocking-read race.
There is no console worker or cross-thread COM/authority transfer.

Each turn blocks in MsgWaitForMultipleObjectsEx, pumps an unfiltered bounded
message quantum, then attempts one nonblocking input record. Partial lines,
ignored records and key-up events return to the event/message wait. A bounded
UTF-16 editor supports Unicode scalars, Enter and Backspace; Escape/Ctrl+C abort.
Input modes are temporarily scoped/restored; no code page or global settings
change. Quick Edit is disabled during the wait; raw control handling permits
clean Ctrl+C abort. The helper cannot access any Explorer/group/movement API.

Only bounded per-wait counters/status are logged, not characters, VK codes,
message contents or per-message logs. Nonqueued COM dispatch is not counted as
a returned MSG; message_dispatch_count is not a COM-call count.

Required tests: messages before completion, multiple messages, immediate input,
partial line followed by messages, WM_QUIT/abort/failure, empty reads, Unicode,
Backspace, capacity and mode restoration. An owned hidden-window STA probe
must use the real message wait/dispatch with a synthetic input completion
handle. No real Explorer reproduction is run automatically. Research/API gate
PASS; implementation/probe results and human-UAT limits are recorded separately.
