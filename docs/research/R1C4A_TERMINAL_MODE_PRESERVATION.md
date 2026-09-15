# R1-C4A Fix 3 — terminal mode preservation

Date: 2026-09-15. Base: `625d72436f41da21f06089d0fbf45a728319997e`.

## Decision and evidence

Human Root reports that VS Code Integrated PowerShell cannot normally select/
copy the printed nonce while Fix 2 waits for input. Source inspection confirms
InputModeScope actively clears Quick Edit, processed input and VT input at
every wait. Restoring later does not preserve the UX DURING the wait.
This is BLOCKED_BY_TERMINAL_MODE_REGRESSION, not Glue/Resize failure.

Invariant: STA-safe input waiting must not take ownership of unrelated
terminal interaction semantics. Remove mode mutation, not the STA pump.

Official sources inspected:

- [SetConsoleMode](https://learn.microsoft.com/en-us/windows/console/setconsolemode):
  line/echo flags govern ReadFile/ReadConsole, whereas processed input also
  decides Ctrl+C delivery. Quick Edit and VT input are host interaction policy.
  There is no requirement here to clear these bits for record-based NOWAIT input.
- [ReadConsoleInputEx](https://learn.microsoft.com/en-us/windows/console/readconsoleinputex):
  CONSOLE_READ_NOWAIT is the nonblocking read contract, independent of asking
  the cooked line API to return early. Keep the existing dynamically resolved
  API, never add a blocking secondary read.
- [VS Code terminal basics](https://code.visualstudio.com/docs/terminal/basics#_copy-paste):
  Windows copy/paste bindings and mouse selection belong to the frontend;
  application mouse mode can change selection routing. Do not reconfigure
  frontend keybindings or intercept Ctrl+C to compensate for the harness.
- [Console VT input sequences](https://learn.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences#input-sequences)
  specifies Backspace as DEL (0x7f) and Escape as ESC (0x1b). The initial native
  mode-A experiment preserved all mode bits but returned YQ for Y/Backspace/Q:
  the old editor ignored DEL. Add only the required control-character mappings,
  not a mode override or a terminal framework. Arbitrary CSI editing is not
  supported; an ESC-prefixed unsupported control sequence fails closed.
- [Process creation flags](https://learn.microsoft.com/en-us/windows/win32/procthread/process-creation-flags)
  and [STARTUPINFO](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/ns-processthreadsapi-startupinfow):
  a test-only child can own a separate console and request SW_HIDE. This will
  isolate automated mode fixtures from the user's active terminal. No new
  process group or control handler is used by the product reader.

Fix 2's AltSnap/FancyZones source/history review and official STA/message-pump
proof remain applicable; the mechanism is not redesigned. No new source code
from those projects was inspected or reused in Fix 3. No external code copied.

## Implementation boundary

Remove InputModeScope and all runtime SetConsoleMode calls. Observe modes with
GetConsoleMode before/after only; never restore or overwrite the host's state.
If a host itself changes mode, record that observation without claiming the
reader caused it or trying to undo it. A mode query failure is reported, not
silently replaced with a guessed mode. Normal controlled acceptance evidence
requires equal before/after modes and mode_changed=false.

Escape remains cancellation. Q remains prompt-defined. An in-band 0x03 or
Ctrl+C key record is ignored, not echoed, injected, accepted as a command or
treated as abort. With processed input enabled, Ctrl+C may never be an input
record; the host's normal behavior is left alone. No SetConsoleCtrlHandler,
keyboard hook, clipboard API, worker thread or terminal setting change.

The MsgWait/Peek/Translate/Dispatch loop, NOWAIT reading, partial-line behavior,
Unicode/Backspace, WM_QUIT and all group/readiness/placement paths remain.

## Automated versus human evidence

Automated native probe: a hidden, test-created private-console child exercises
the production reader under three input modes (protected bits both on/off).
An owned message handler measures mode DURING read and injects short test input
into that child's console only. Before/during/after must match; source audit
requires zero mode setters in production. The parent has no COM objects when
waiting for this test child; no runtime input worker is introduced.

This is not a VS Code/Windows Terminal clipboard emulation or a manual probe.
Actual selection -> Ctrl+C -> paste elsewhere -> Enter, plus readiness resize
liveness, remain PENDING_HUMAN after independent review. The final C4A UAT's
first nonce confirmation provides that manual gate; no extra manual probe is
run by Codex. Preserve existing ignored evidence and any live old harness.
