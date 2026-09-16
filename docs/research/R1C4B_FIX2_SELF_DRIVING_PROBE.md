# R1-C4B Fix 2 self-driving probe research and test contract

2026-09-16; base `aa037f0017bdc93d30227d959289d2059eff969c`.
Human Root authorizes a dedicated test EXE with SendInput, explicitly independent
of Codex computer-use. No product or consent runtime changes are authorized.
The old human-driven diagnostic remains reproducible and unchanged.

## Prior art / official evidence

Re-inspected mature AltSnap (GPL-3.0-or-later, reference only), pin
`5c86416ad21e4b72844a998a746bd3bb0bee5f5d`: License.txt,
LetWindowKickBack/MoveResizeWindowNow_ and history
`df25d36c6369bb13aa02ec83974e625fc7922c35` / PR 739. A synthetic sizing
notification is not a real system modal loop; none of its implementation,
input hooks, sleeps or message-synthesis logic is reused.
Re-inspected mature PowerToys/FancyZones (MIT, reference only), pin
`19c4d805321db86f3634e6968e14dbf25cbba14a`: LICENSE, WindowMouseSnap.cpp,
PR 48569 / `dd26d86580168d2e368701f7b0c4d629dc9cd9ac`. Explicit lifecycle
completion/abort matters; ordinary placement or highlight updates do not prove
live drag-RECT authority. Source references are in the Fix 1 research record.

Official live documents read 2026-09-16:

- [SendInput](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput):
  input stream injection, return count, UIPI, existing key-state interference.
- [MOUSEINPUT](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-mouseinput):
  absolute normalized coordinates, virtual desktop mapping, no-coalescing flag.
- [OpenInputDesktop](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-openinputdesktop):
  alone does not exclude disconnected sessions.
- [WTSINFOEX_LEVEL1](https://learn.microsoft.com/en-us/windows/win32/api/wtsapi32/ns-wtsapi32-wtsinfoex_level1_w):
  active/unlocked session facts (Windows 10+ baseline, not the Windows 7 reversed flags).
- [WM_NCHITTEST](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-nchittest):
  signed screen points; HTCAPTION/HTBOTTOM identify real nonclient interaction.
- [SetForegroundWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setforegroundwindow):
  may be denied; no AttachThreadInput or focus-lock bypass.
- [ShowWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-showwindow):
  initial STARTUPINFO may override the first show request. This hypothesis was
  checked, not assumed: the instrumented run observed startup_flags=0 and an
  already visible window; the actual blocker was foreground denial.
- [SetWaitableTimer](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-setwaitabletimer):
  one-shot test-input pacing, not a product event source or geometry retry.

## Independent implementation / empirical gate

One new owned HWND per process, one UI thread and one bounded input-driver
thread. No foreign HWND input parameter, Explorer, hooks or computer-use IPC.
Full PMv2 manifest. Primary work-area-contained setup leaves at least 100 px
after the 180 px horizontal Move and 120 px bottom Resize plus a 7 px pulse.
Three-second console countdown/Esc cancel before input. WTS active/unlocked,
matching current/input desktop and foreground checks fail closed.

Find HTCAPTION and HTBOTTOM with a bounded hit-test scan; verify foreground,
window PID/TID, point ownership, modifier/button and expected cursor fences
before injected input. Driver sends 20 absolute mouse moves at one-shot 30 ms
intervals and waits on actual UI lifecycle events, never fabricating WM_MOVING
or WM_SIZING. Each first real drag callback posts exactly one +7 correction;
the next real callback and END establish T2/T3. Record generated input path,
native messages, proposed RECT, positioning and visible geometry independently.
Normal completion restores the saved cursor; interference stops the run and
does not force cursor restoration. Cleanup may only release this driver's own
held button and destroy its own empty HWND, never another app's window.

Offline validator must recalculate raw trajectory from the initial positioning
and actual generated cursor path, compare T0/T1 targets and T2/T3 pulse retention,
and reject missing lifecycle, wrong input/modal type, missing correction,
sequence/order/capture errors, or false harness PASS. Classifications are
STABLE, REASSERTED, IMMEDIATE_REJECTED, DWM_ASYNC_ONLY, UNKNOWN; evidence capture
success is distinct from architecture acceptance. Initial classification/false-
flag tests ran before the first launch; full-envelope negative fixtures were
added during development. Final synthetic suite: 41 checks. No real input path
has passed its foreground prerequisite.

Run Debug 20 and Release 20, new process/window per repetition; no retry-until-
PASS. Input failures stop further injection; retain and report all attempts,
incomplete counts and classification disagreement. Real interactions stay out
of CTest. Explicit rejection stops Explorer bootstrap and product redesign;
only alternatives research and full regression/documentation/Git closure follow.

## Actual execution gate

See [Fix 2 execution report](../reports/R1C4B_FIX2_SELF_DRIVING_REPORT.md).
The self-driving EXE was launched from the shell in both configurations, with
no computer-use invocation. Desktop/session checks succeeded. Debug's
instrumented attempt and Release's first formal attempt both observed visible
owned windows and `SetForegroundWindow=FALSE`, foreground != target. No input
was sent. This is the brief's explicit `BLOCKED_BY_FOREGROUND`, not proof of
native modal rejection and not a computer-use blocker. Do not bypass this OS
gate with focus-stealing input, AttachThreadInput, settings or retries.

The 20/20 requirement remains **unmet** in each configuration. The driver is
IMPLEMENTED / COMPILED; its positive native interaction, actual hit-test scan,
SendInput, T0–T3 and cursor restoration paths remain NOT TESTED. Foreground
fail-closed / owned-window cleanup are AUTOMATED OBSERVED. Pure classifier and
full synthetic evidence acceptance/rejection are AUTOMATED TESTED, not real
modal evidence. No Explorer bootstrap/gate or product architecture change.
