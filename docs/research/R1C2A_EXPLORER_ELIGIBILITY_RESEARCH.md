# PaneBind R1-C2A Explorer Eligibility Research

Status: **R1-C2A PRIOR-ART GATE PASS**

Review date: 2026-08-26.

## Scope and evidence labels

This gate authorizes one explicit pure translation of one normal File Explorer
window only when the current test workflow can prove that the top-level HWND
was created after its baseline and is displaying a unique empty directory under
ignored `uat/r1c2a/`. It does not authorize an existing Explorer window, a
second real window, any other application, continuous behavior, or Glue.

- **FACT** is directly supported by pinned source/history or official
  Microsoft documentation.
- **INFERENCE** is a conclusion drawn from those facts.
- **PANEBIND DECISION** is the fail-closed R1-C2A contract.

No external code was copied, adapted, translated, or mechanically rewritten.

## Sources and license boundary

| Source | Immutable revision | License / use |
| --- | --- | --- |
| [AltSnap](https://github.com/RamonUnch/AltSnap) | [`5c86416ad21e4b72844a998a746bd3bb0bee5f5d`](https://github.com/RamonUnch/AltSnap/commit/5c86416ad21e4b72844a998a746bd3bb0bee5f5d) | GPL-3.0-or-later; **REFERENCE ONLY** |
| [AltDrag](https://github.com/stefansundin/altdrag) | [`e2740d605b0336a3b391fec26794718864b19521`](https://github.com/stefansundin/altdrag/commit/e2740d605b0336a3b391fec26794718864b19521) | GPL-3.0-or-later; **REFERENCE ONLY** |
| [PowerToys / FancyZones](https://github.com/microsoft/PowerToys) | [`19c4d805321db86f3634e6968e14dbf25cbba14a`](https://github.com/microsoft/PowerToys/commit/19c4d805321db86f3634e6968e14dbf25cbba14a) | MIT; reference-only in R1-C2A |
| Microsoft Win32 and Shell documentation | live pages reviewed 2026-08-26 | facts paraphrased and cited |

## Prior-art findings

### AltSnap and AltDrag

**FACT.** AltSnap has generic Explorer exclusions through executable, title,
and class patterns and otherwise discovers/moves raw desktop HWND values. Its
candidate model does not prove launch provenance, an exact filesystem
location, a new-versus-preexisting frame, or a test-session authority.

**INFERENCE.** Mature generic movement evidence is useful for Explorer geometry
and application-adjustment risks but cannot issue the first real-application
capability. AltDrag's injection/subclass history remains rejected.

### FancyZones Explorer UI-test helper

**FACT.** The pinned FancyZones UI helper records a baseline set of
`CabinetWClass` HWND values, starts Explorer for a folder, and selects the first
HWND absent from the baseline. See the pinned
[`FancyZonesTestHelper`](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZones.UITests.Next/Utils/FancyZonesTestHelper.cs#L479-L540).

This supports HWND set-delta as one necessary candidate signal. It does not
validate Shell location, canonical process image, integrity, cloaking,
minimize/maximize, multiple-new-window ambiguity, or live eligibility. Its
cleanup closes every `CabinetWClass` window; R1-C2A explicitly rejects that
behavior.

**PANEBIND DECISION.** Inventory is not capability. A candidate must be the
only new top-level HWND and independently pass location, process, class/state,
security, geometry, and lifetime checks. Zero or multiple new candidates are a
hard stop.

## Official Explorer creation and inventory

Microsoft's
[`Developing with Windows Explorer`](https://learn.microsoft.com/en-us/windows/win32/shell/developing-with-windows-explorer)
documents both sides needed by the fixture:

- open Shell windows can be discovered through `IShellWindows`
  (`CLSID_ShellWindows`); and
- a new Explorer instance can be requested through `IWebBrowser2`
  (`CLSID_ShellBrowserWindow`), followed by `Navigate2` and `put_Visible`.

The official sample builds a PIDL-backed VARIANT with
`SHCreateItemFromParsingName`/`SHGetIDListFromObject` and uses a single-threaded
COM apartment. It also demonstrates `IWebBrowser2::Quit`.

By contrast, the documented
[`ShellExecute`](https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shellexecutew)
behavior may reuse an already open Explorer window under the default folder
option. Ordinary ShellExecute is therefore unsuitable for this isolation Gate.
Older `/n` command-line descriptions are not used as a modern tabs/new-HWND
guarantee.

[`IShellWindows`](https://learn.microsoft.com/en-us/windows/win32/api/exdisp/nn-exdisp-ishellwindows)
is a collection of registered Shell windows, not a File-Explorer-only security
boundary. Each item must be queried for `IWebBrowser2`, HWND, current location,
and then pass the Explorer-specific allowlist.

### Modern tab ambiguity

`IWebBrowser2::HWND` historically returns a top-level frame rather than an
individual tab. Microsoft does not document a modern File Explorer one-entry /
one-tab / one-HWND invariant. PaneBind therefore:

- deduplicates inventory by top-level HWND;
- treats multiple Shell entries or inconsistent locations for one HWND as
  ambiguous;
- never upgrades a baseline HWND merely because a new tab/location appeared;
  and
- does not call Quit unless the new frame remains uniquely correlated with one
  test-directory entry.

## Explorer test-target isolation

The required provisioning sequence is:

```text
initialize STA COM
capture read-only IShellWindows baseline and baseline HWND set
create unique empty uat/r1c2a/target-<nonce> directory
CoCreateInstance(CLSID_ShellBrowserWindow)
Navigate2(test-directory PIDL)
put_Visible(TRUE)
bounded read-only inventory refresh
set difference = post HWNDs - baseline HWNDs
require exactly one new HWND
require exactly one matching Shell entry for that HWND and test directory
require every baseline HWND/location fingerprint unchanged
```

If no new HWND appears, the Shell reused a baseline frame/tab and isolation is
blocked. If more than one appears, selection is ambiguous. If a baseline frame
navigated, user-existing-window safety is blocked. No existing-window fallback
exists.

Window titles are diagnostic only and are never an authority input or included
unsanitized in committed evidence.

## Filesystem location identity

The fixture directory is unique, empty, and located only under ignored
`uat/r1c2a/`. Shell `LocationURL` is accepted only as a candidate location fact.
File URLs are converted through
[`PathCreateFromUrlW`](https://learn.microsoft.com/en-us/windows/win32/api/shlwapi/nf-shlwapi-pathcreatefromurlw).

The expected directory and reported directory are opened for attribute access.
`FILE_ID_INFO` volume/file IDs from
[`GetFileInformationByHandleEx`](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getfileinformationbyhandleex)
must match. String equality, title, localized display name, or URL text alone is
insufficient.

Location is re-inventoried and file-identity-checked at issuance, preflight,
immediately before native apply, post-verification, restore, and optional close.
Navigation away invalidates the token and prevents further native apply.

## Explorer process and application identity

The candidate HWND must provide nonzero PID/TID through
`GetWindowThreadProcessId`. The controller opens the process with
`PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE` and retains that handle.

The expected image is `GetSystemWindowsDirectoryW() / explorer.exe`.
`QueryFullProcessImageNameW` supplies the current process image path. Both
expected and observed image files are opened and their volume plus 128-bit
`FILE_ID_INFO` are compared. A basename-only `explorer.exe` match is rejected.

The process handle must remain nonsignaled and continue to report the same PID.
Because Explorer can be a shared Shell process, the process handle proves only
the process instance; capability identity also requires the isolated HWND,
TID, Shell location, eligibility fingerprint, authority, and generation.

## Security, class, state, and desktop allowlist

Every issuance and live preflight requires:

```text
IsWindow
GetAncestor(GA_ROOT) == HWND
WS_CHILD absent
GW_OWNER == null
IsWindowVisible
DWMWA_CLOAKED == 0
not minimized
not maximized
current virtual desktop
valid PID/TID
canonical explorer.exe file identity
same integrity RID and Windows session
UIAccess == false
AppContainer == false
observed File Explorer class allowlist
exact Shell filesystem location identity
created_after_test_baseline == true
```

`CabinetWClass`/`ExploreWClass` are observed fail-closed class allowlist facts,
not a stable Microsoft identity contract. They can only reject; class alone can
never authorize.

[`IVirtualDesktopManager::IsWindowOnCurrentVirtualDesktop`](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-ivirtualdesktopmanager-iswindowoncurrentvirtualdesktop)
must report true. Integrity/UIAccess/AppContainer/session mismatch is
`IntegrityMismatch`/ineligible; PaneBind does not elevate or alter UIPI filters.

## Eligibility and capability model

R1-C2A defines Explorer-specific types, never a generic third-party registry:

```text
ExplorerEligibilityReason
ExplorerTestSession
ExplorerWindowToken
ExplorerWindowOperations
```

`ExplorerWindowToken` contains private controller authority, private test
session authority, one logical target ID, and generation. It cannot be default,
HWND, or integer constructed and has no HWND conversion. Raw HWND and COM
objects remain internal to the STA-bound Explorer session.

Reason-bearing rejection includes at least preexisting/reused window, wrong
application/image/class, not top-level, owner/style/state/cloak/desktop,
location/integrity mismatch, ambiguous candidate, window/process exit,
monitor/DPI change, and post-verification mismatch.

Preexisting HWND values are permanently excluded for the session. Failure to
provision a new target cannot widen authority.

## Single translation and safe delta

R1-C2A performs one explicit single-window translation and no batch/Glue.
It reuses `window_translation`:

```text
target visible - current visible = checked (dx,dy)
target positioning = current positioning + checked (dx,dy)
```

The deterministic test-only delta is the first of
`(+80,+50)`, `(-80,-50)`, `(+50,-50)`, `(-50,+50)` whose entire target visible
rectangle remains inside the same monitor work area. Failure to find one is a
hard stop. This is fixture configuration, not product policy.

Native flags remain:

```text
SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
```

No foreground, topmost, show/hide, async, input attachment, or resize behavior
is authorized. Native TRUE is not PaneBind success; requested and actual
visible/positioning rectangles, unchanged size, location, process/window
identity, monitor, and DPI must all match exactly.

## Restore, close, and stale lifetime

Restore is a separate live-validated translation from current actual geometry
to the original visible position. It is cleanup, not rollback.

The session may call `IWebBrowser2::Quit` only when the target is still the
unique new HWND, has exactly one unambiguous Shell entry at the exact test
directory, and all process/class/security facts remain valid. Otherwise it
leaves the window open and reports `SAFE_CLEANUP_NOT_PERFORMED`.

No Explorer process is terminated, restarted, or killed. No broadcast close is
sent. If safe close is performed, the old token must fail preflight with no
native call. If close safety cannot be proven,
`WINDOW_DESTROY_LIFETIME = NOT TESTED` is acceptable.

The empty directory is removed only after no live Shell entry still references
it. No user file is created or modified.

## Feedback evidence

The independent observer records the exact target HWND/PID and single
translation plus restore. R1-C2A measures START/LOCATION/END, ordering,
duplicates, missing events, and actual geometry without assuming R1-C1 counts.

Attribution inputs are Explorer token/session/generation, operation ID,
expected/actual geometry, HWND/PID/TID, and matching WinEvent geometry. Time,
raw HWND, START/END, contiguity, or one-request-one-event are insufficient.
No suppression or Glue state machine is implemented.

## Required tests

Deterministic tests cover reason classification, baseline/new-set delta,
preexisting and ambiguous rejection, location/image/class/topology/state/cloak
rejection, authority/generation/stale/cross-session isolation, safe-delta/work
area, shared translation/resize/overflow, and post-verification identity change.

The explicit desktop harness must either:

- prove isolation, translate once, verify, restore, optionally close safely,
  and pass; or
- fail closed before native apply with an exact isolation blocker.

It must never substitute a baseline Explorer window.

## Adopted and rejected designs

Adopted:

- official `CLSID_ShellBrowserWindow` creation request;
- read-only `IShellWindows` before/after inventory;
- exactly-one new HWND set-delta plus exact filesystem identity;
- retained process handle, file identity, security/state/desktop allowlist;
- Explorer-specific opaque capability and reason-bearing eligibility;
- live revalidation, one shared-bridge translation, post-verification, restore;
- exact COM-object close only under proven safety; and
- independent WinEvent evidence.

Rejected:

- ordinary ShellExecute reuse as an isolation mechanism;
- `/n` as a modern HWND guarantee;
- titles, basename, class, PID, HWND, location string, or IShellWindows
  membership alone as authority;
- any preexisting-window fallback or baseline HWND upgrade;
- generic third-party token/manager/operations;
- multiple real-window batch, Glue, input hooks, Snap, resize, injection,
  process kill, shell restart, polling, auto-elevation, or cross-monitor move.

## Research gate

```text
R1C2A_PRIOR_ART_GATE = PASS
R1C2A_RUNTIME_RESULT = REQUIRES ISOLATION EXPERIMENT
THIRD_PARTY_AUTHORITY = EXPLORER TEST FIXTURE ONLY
EXTERNAL_CODE_COPIED = NO
EXTERNAL_CODE_ADAPTED = NO
R1C2B = NOT STARTED
```
