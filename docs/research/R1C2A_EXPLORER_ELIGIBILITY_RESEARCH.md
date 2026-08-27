# PaneBind R1-C2A Explorer Eligibility Research

Status: **R1-C2A PRIOR-ART AND BASELINE-EXCLUSION GATES PASS;
AUTOMATIC PROVISIONING BLOCKED; USER-CONSENTED ELIGIBILITY AND RUNTIME
PENDING INTERACTIVE UAT**

Review date: 2026-08-26; provisioning-recovery supplement reviewed
2026-08-27; user-consented recovery supplement reviewed 2026-08-27.

## Scope and evidence labels

This gate authorizes one explicit pure translation of one normal File Explorer
window only when a human explicitly creates and consents to the test target,
and the current test workflow independently proves that the top-level HWND was
created after its baseline and is displaying a unique empty directory under
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
| Microsoft Windows SDK headers | 10.0.26100.0 `ExDisp.idl`, `ExDisp.h`, `ExDispid.h`, and `ocidl.h`, reviewed 2026-08-27 | interface/type/DISPID contracts inspected locally; no code copied or adapted |

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

**FACT.** The sample documents the `CoCreateInstance` -> `Navigate2` ->
`put_Visible` calls, but it does not state that the new automation object's
frame `HWND` is immediately readable when `CoCreateInstance` returns.

**HISTORICAL PANEBIND DECISION (ATTEMPTS 1 AND 2).** The automatic path made
exactly one `CoCreateInstance(CLSID_ShellBrowserWindow)` attempt and retained
the resulting object as a provisioning lease. Its correlation and cleanup
model is preserved below as research and deterministic-test evidence. On this
Windows 11 workstation the final recovery attempt returned `E_FAIL` before an
`IWebBrowser2` lease existed. Automatic provisioning is therefore recorded as
`BLOCKED`; it is no longer the active/default R1-C2A UAT path and must not be
retried, weakened, or used as fallback after user-consented discovery.

By contrast, the documented
[`ShellExecute`](https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shellexecutew)
behavior says that when **Launch folder windows in a separate process** is
disabled (the documented default), opening a folder uses an existing Explorer
window if one is open. The
[`SHELLEXECUTEINFO`](https://learn.microsoft.com/en-us/windows/win32/api/shellapi/ns-shellapi-shellexecuteinfow)
contract also says that `hProcess` can be null when no process was launched and
is not returned in every launch case. Consequently neither `ShellExecute` nor
`ShellExecuteEx` proves a new process, much less a unique new top-level HWND.

The current Microsoft
[`start`](https://learn.microsoft.com/en-us/windows-server/administration/windows-commands/start)
documentation defines that command's `/separate` switch for 16-bit program
memory space and says it is unsupported on 64-bit platforms; it is not an
Explorer-frame isolation contract. The reviewed current Microsoft Win32/Shell
documentation provides no Windows 11 contract that `explorer.exe /n`,
`explorer.exe /separate`, `explorer.exe <path>`, or `start <path>` creates one
new top-level Explorer HWND. Historical descriptions and observed command-line
behavior therefore cannot issue capability authority.

**PANEBIND DECISION.** `ShellExecute`, `ShellExecuteEx`, direct
`explorer.exe` invocation (with or without `/n` or `/separate`), and
`start <path>` are rejected as automatic target-authority paths. They may be
studied diagnostically but cannot be active provisioning, fallback, or proof
that an Explorer frame is new.

[`IShellWindows`](https://learn.microsoft.com/en-us/windows/win32/api/exdisp/nn-exdisp-ishellwindows)
is a collection of registered Shell windows, not a File-Explorer-only security
boundary. Each item must be queried for `IWebBrowser2`, HWND, current location,
and then pass the Explorer-specific allowlist. Microsoft describes a Shell
registration cookie as unique *within the collection*; it is not documented as
a global, permanent, or security identity.

### Shell registration connection point

The recovery design uses the official Shell registration model rather than a
sleep followed only by collection enumeration:

- [`DShellWindowsEvents`](https://learn.microsoft.com/en-us/windows/win32/shell/dshellwindowsevents)
  receives `IShellWindows` registration and revocation notifications.
- [`WindowRegistered`](https://learn.microsoft.com/en-us/windows/win32/shell/dshellwindowsevents-windowregistered)
  supplies the `long` cookie granted to a registered Shell window.
- [`WindowRevoked`](https://learn.microsoft.com/en-us/windows/win32/shell/dshellwindowsevents-windowrevoked)
  supplies the cookie whose registration was revoked.
- The Windows SDK declares `DShellWindowsEvents` as an `IDispatch`
  dispinterface with outgoing interface identifier
  `DIID_DShellWindowsEvents = FE4106E0-399A-11D0-A48C-00A0C90A8F39`,
  `DISPID_WINDOWREGISTERED = 200`, and `DISPID_WINDOWREVOKED = 201`.

The `DShellWindowsEvents` Learn requirements table names
`IID_IShellWindows`; that is not the outgoing-interface identifier passed to
`IConnectionPointContainer::FindConnectionPoint`. The installed SDK's
`DIID_DShellWindowsEvents` declaration and `ShellWindows` coclass source
interface define the connection-point contract.

**FACT.** A client queries the retained `IShellWindows` object for
`IConnectionPointContainer`, calls
[`FindConnectionPoint`](https://learn.microsoft.com/en-us/windows/win32/api/ocidl/nf-ocidl-iconnectionpointcontainer-findconnectionpoint)
with `DIID_DShellWindowsEvents`, and calls
[`Advise`](https://learn.microsoft.com/en-us/windows/win32/api/ocidl/nf-ocidl-iconnectionpoint-advise)
with a sink whose `QueryInterface` supports that outgoing DIID as well as its
`IUnknown`/`IDispatch` bases. `Advise` returns a `DWORD` connection token. The
client later supplies that same token to
[`Unadvise`](https://learn.microsoft.com/en-us/windows/win32/api/ocidl/nf-ocidl-iconnectionpoint-unadvise),
which releases the connection point's retained sink interface.

The two cookies are distinct namespaces and must never be conflated:

```text
Advise DWORD cookie             -> owns one sink subscription
WindowRegistered long lCookie   -> identifies one Shell registration
WindowRevoked long lCookie      -> revokes that Shell registration
```

The reviewed contract does not promise that a registration cookie value is
never reused after revocation or across collection instances. PaneBind retains
the same `IShellWindows` object and qualifies each registration receipt by test
session, subscription generation, local sequence, and revoked/live state.

The sink receives the two Shell methods through `IDispatch::Invoke`; they are
not C++ vtable methods on `DShellWindowsEvents`. It validates the expected
DISPID, one Automation `long` argument, and event generation, then appends a
local monotonic receipt sequence. It does not perform provisioning state
transitions reentrantly inside `Invoke`. The official
[`DISPPARAMS`](https://learn.microsoft.com/en-us/windows/win32/api/oaidl/ns-oaidl-dispparams)
contract stores positional arguments in reverse order. Thus each one-argument
Shell event uses `rgvarg[0]`, while `NavigateComplete2(pDisp, URL)` presents
the URL argument before the `pDisp` argument in `rgvarg`; the sink validates
the actual VARIANT shape rather than relying on unchecked casts.

**PANEBIND DECISION.** Subscribe successfully before the single Explorer
creation attempt. Keep the connection point, sink, and `IShellWindows` alive
until bounded provisioning/cleanup ends; call `Unadvise` before destroying the
sink and before COM apartment shutdown. A failed `Advise`, an invalid event
shape, callback-after-retirement detection, or inability to `Unadvise` cleanly
blocks positive attribution. The STA pumps messages, as required by
[`Single-Threaded Apartments`](https://learn.microsoft.com/en-us/windows/win32/com/single-threaded-apartments);
fixed high-frequency polling is not introduced.

### Cookie resolution and canonical COM identity

[`IShellWindows::FindWindowSW`](https://learn.microsoft.com/en-us/windows/win32/api/exdisp/nf-exdisp-ishellwindows-findwindowsw)
has this relevant ABI shape: `pvarLoc` and `pvarLocRoot` are `VARIANT*`,
`swClass` and options are `int`, the returned window value is `long`, and the
returned automation interface is `IDispatch**`. With
[`SWFO_COOKIEPASSED`](https://learn.microsoft.com/en-us/windows/win32/api/exdisp/ne-exdisp-shellwindowfindwindowoptions)
(`0x4`), `pvarLoc` is interpreted as a registration cookie rather than a
PIDL. `SWFO_NEEDDISPATCH` (`0x1`) requires an `IDispatch` result. The event and
SDK IDL define the cookie as Automation `long`, so the cookie input is a
`VT_I4` `VARIANT`; `pvarLocRoot` remains null or `VT_EMPTY`.

The documented outcomes are `S_OK` for a match, `S_FALSE` for no match,
`E_NOINTERFACE` when `SWFO_NEEDDISPATCH` finds a window but cannot obtain its
dispatch interface, and `E_PENDING` only when `SWFO_INCLUDEPENDING` is used.
PaneBind does not use `SWFO_INCLUDEPENDING` to weaken readiness.

Every post-subscription `WindowRegistered` cookie is evaluated. An unrelated
registration is recorded and ignored after it fails object identity; its
existence alone is not a blocker. A candidate registration must resolve with
`SWFO_COOKIEPASSED | SWFO_NEEDDISPATCH` and then match the retained creation
object by COM identity.

Microsoft's
[`IUnknown` identity rule](https://learn.microsoft.com/en-us/windows/win32/com/rules-for-implementing-queryinterface)
states that querying any interface of one object for `IID_IUnknown` returns
the same physical pointer. Pointers to interfaces other than `IUnknown` are
not required to be equal. PaneBind therefore queries both the retained
`IWebBrowser2` and cookie-resolved `IDispatch` for `IID_IUnknown` and compares
only those canonical pointers within the owning STA. A raw
`IWebBrowser2*`/`IDispatch*` address comparison is invalid evidence, and
interface pointers are not moved across apartments without COM marshaling.

Canonical identity is necessary but not sufficient. Exactly one non-revoked
registration must match, and all three window facts must agree:

```text
retained IWebBrowser2::get_HWND
== FindWindowSW(cookie) returned window
== live eligible Explorer top-level HWND
```

The common HWND must not occur in the session's permanent preexisting-HWND
forbidden set. No match, two matching cookies, a matching object with a
preexisting HWND, a revoked matching cookie, or any three-way mismatch blocks
token issuance.

### Navigation and cleanup event limits

The SDK signature for `IWebBrowser2::Navigate2` takes one required URL/file/
PIDL `VARIANT*` and four optional `VARIANT*` arguments. Microsoft's
[`Navigate2` documentation](https://learn.microsoft.com/en-us/previous-versions/aa752134%28v%3Dvs.85%29)
explicitly supports a PIDL representing a Shell namespace folder. PaneBind
retains the single object returned by the official
`CoCreateInstance(CLSID_ShellBrowserWindow, ..., CLSCTX_LOCAL_SERVER)` path,
subscribes that object's `DIID_DWebBrowserEvents2` connection point, then calls
`Navigate2` with the nonce directory PIDL and makes the object visible. It
does not use a second creation fallback.

The SDK defines `DWebBrowserEvents2::NavigateComplete2` as
`(IDispatch* pDisp, VARIANT* URL)` with DISPID 252. Microsoft's
[`NavigateComplete2` documentation](https://learn.microsoft.com/en-us/previous-versions/windows/internet-explorer/ie-developer/platform-apis/aa768285%28v%3Dvs.85%29)
says it is asynchronous, can represent a top-level window or frame, and may
occur while content is still downloading. Its URL may be canonicalized,
redirected, or represented as a PIDL. Therefore it is only a readiness hint:
its `pDisp` must first canonical-`IUnknown` match the provisioning lease, and
the target still must pass the independent live Shell `FILE_ID_INFO` location
check. The event URL cannot be location authority.

Because the sink deliberately does not retain or trust event URLs, navigation
history is also fail-closed: more than one same-object `NavigateComplete2`
before issuance is ambiguous; an accepted 0-or-1 count is frozen at issuance,
and any later same-object completion permanently invalidates the token and
cleanup authority. This permits false negatives rather than accepting an
unobservable away-then-back sequence.

[`IWebBrowser2::get_HWND`](https://learn.microsoft.com/en-us/previous-versions/mt725310%28v%3Dvs.85%29)
returns `SHANDLE_PTR` and, under the documented tabbed-browser ambiguity,
identifies the top-level frame rather than a tab. `FindWindowSW`'s separate
Automation `long` output does not remove that ambiguity; the three-way check
first normalizes that `long` with the SDK handle-conversion contract and then
compares it with the `SHANDLE_PTR` result. Baseline exclusion remains required.

[`IWebBrowser2::Quit`](https://learn.microsoft.com/en-us/previous-versions/aa752140%28v%3Dvs.85%29)
closes the automation object and returns `S_OK` for method success. The
documentation does not state that HWND destruction or `WindowRevoked` has
completed when `Quit` returns.

**OFFICIAL LIMITATION.** The reviewed Microsoft pages and SDK declarations do
not specify a total ordering among `WindowRegistered`, `get_HWND` readiness,
`Navigate2` return, `NavigateComplete2`, visibility, `Quit` return,
`WindowRevoked`, and HWND invalidation. Receipt sequence is therefore local
evidence only, never a platform-wide ordering claim. PaneBind uses one bounded
event/message-driven deadline, permits deferred cookie resolution within that
deadline, and treats `WindowRevoked` before issuance as stale. Cleanup waits
boundedly for the matching revocation and/or exact-object HWND invalidation;
it never escalates to arbitrary `WM_CLOSE` or process termination.

The registration event APIs are documented for Shdocvw.dll
5.00.2014.0216/Internet Explorer 5-era Shell integration; `IWebBrowser2` is
documented for Windows XP desktop and later, while the official Explorer
creation sample targets `_WIN32_WINNT 0x0600`. Microsoft does not publish a
separate modern-Explorer event-ordering or one-tab/one-registration support
guarantee. R1-C2A therefore records current-workstation behavior as empirical
evidence and keeps every undocumented invariant fail-closed.

### Modern tab ambiguity

`IWebBrowser2::HWND` historically returns a top-level frame rather than an
individual tab. Microsoft does not document a modern File Explorer one-entry /
one-tab / one-HWND invariant. PaneBind therefore:

- deduplicates inventory by top-level HWND;
- treats multiple Shell entries or inconsistent locations for one HWND as
  ambiguous;
- never upgrades a baseline HWND merely because a new tab/location appeared;
  and
- never treats a new tab as a new top-level target. The active user-consented
  path does not call `Quit` at all.

## User-consented target authority (Attempt 3)

Attempt 3 is a controlled, interactive UAT rather than automatic Explorer
provisioning. PaneBind creates only the empty nonce directory and captures the
read-only Shell baseline. It then asks the human to create a **new** File
Explorer top-level window and navigate that new window to the displayed path.
PaneBind does not open, navigate, activate, or select a window on the user's
behalf.

`Ctrl+N` in Explorer is one suggested human action, not an authority signal or
platform contract. The user may create the new frame another way. Regardless
of the action, a tab, a reused frame, zero new HWND values, or more than one
eligible new HWND blocks the run. PaneBind never infers consent or provenance
from the keyboard gesture itself.

The active issuance contract is:

```text
explicit user target-creation confirmation
+ complete permanent baseline forbidden-HWND set
+ exactly one post-baseline top-level HWND candidate at the exact nonce FILE_ID_INFO
+ full live Explorer eligibility
+ matching session/consent/capability generations
= one ExplorerWindowToken for this interactive test
```

The ENTER used to confirm that the human has completed a step is an explicit
user-authorization fact. It is not a password, proof of identity, security
credential, integrity boundary, or substitute for any native eligibility
check. Consent composes with platform eligibility and live identity; it cannot
upgrade a forbidden preexisting HWND.

The UAT uses two separate consent facts. The first permits PaneBind to inspect
the post-baseline inventory and, if safe, issue a token. After a sanitized
summary confirms new HWND, canonical Explorer image, exact test directory,
normal state, monitor, and DPI, the second permits exactly one translation.
Without the second consent no `SetWindowPos` call is permitted. A change in
candidate identity or required eligibility between those generations blocks
the native call.

At minimum, the session records monotonic baseline, prompt, user-confirmation,
eligibility, move-consent, and token generations. Token issuance and every use
must agree with the current session and consent generations. These are local
capability freshness facts, not claims of operating-system authentication.

The active interactive sequence is:

```text
initialize STA COM
create unique empty non-reparse local uat/r1c2a/consent-target-<nonce>
record the nonce directory FILE_ID_INFO
capture every baseline Shell entry's reliable HWND into a permanent forbidden set
prompt the human to create one new Explorer frame and navigate it to the nonce path
wait for explicit user confirmation without synthesizing input
capture a new read-only Shell inventory
select HWND values absent from the permanent forbidden set
filter them by exactly one live Shell entry at the nonce FILE_ID_INFO
require exactly one remaining candidate HWND
require the complete Explorer process/class/state/security/desktop/monitor/DPI allowlist
issue one generation-bound ExplorerWindowToken
show a sanitized target summary and request separate move consent
live-revalidate the same target and generations
translate once, postverify exactly, and restore independently
leave the user-created Explorer window open for the human to close
```

An existing baseline Explorer that navigates to the nonce directory while the
harness is waiting remains forbidden. If that is the only exact-location
entry, the run reports a preexisting-target rejection and stops; it does not
move the window or navigate it back. Empty or inaccessible baseline locations
remain `OPAQUE_PREEXISTING` and cannot later be promoted.

The nonce directory is printed as an absolute path because the human must
navigate to it. It must be empty, local, non-reparse, identified by exact file
identity, and ignored by Git. PaneBind does not automatically navigate any
Explorer window to that path.

No `SendInput`, simulated `PostMessage`, keyboard/mouse hook, UI Automation
click, foreground forcing, product selector UI, drag picker, or global input
behavior is introduced. Attempt 3 establishes only the minimum interactive UAT
authority fact; it does not design production selection UX.

## Historical automatic test-target isolation (Attempts 1 and 2)

The following sequence records the superseded automatic provisioning design.
It remains relevant to the immutable blocked observations and deterministic
tests, but it is not the active/default Attempt 3 path:

```text
initialize STA COM
create unique empty uat/r1c2a/target-<nonce> directory
create and retain one IShellWindows collection object
subscribe DShellWindowsEvents and establish a subscription generation
capture every baseline entry's reliable HWND into a permanent forbidden set
record available PID/TID/class/location only as diagnostic baseline facts
establish one absolute provisioning deadline
CoCreateInstance(CLSID_ShellBrowserWindow) exactly once
retain IWebBrowser2 plus its canonical IUnknown as a provisioning lease
subscribe the retained object's DWebBrowserEvents2 connection point
Navigate2(test-directory PIDL)
put_Visible(TRUE)
receive Shell registration/navigation events through a bounded STA message pump
resolve each registration cookie with FindWindowSW
require exactly one non-revoked canonical-IUnknown match to the lease
require lease HWND == cookie HWND == live eligibility HWND
require that HWND to be nonzero and absent from the permanent forbidden set
require exact nonce-directory FILE_ID_INFO and the full Explorer allowlist
```

## Explorer test-target isolation shared by all attempts

Baseline exclusion and positive target attribution are separate contracts.
For each preexisting Shell entry, a reliable HWND is sufficient to put that
numeric value permanently into `forbidden_preexisting_hwnds` for this session.
An empty, inaccessible, or temporarily unavailable location makes that entry
`OPAQUE_PREEXISTING`; it does not make the exclusion set incomplete and can
never become the target even if it later displays the nonce directory. If any
baseline Shell entry cannot provide a reliable HWND, exclusion completeness is
unknown and the run blocks before prompting the user. Reuse of a forbidden
numeric HWND during the session is conservatively rejected as a safe false
negative.

Available baseline location status, location source, URL digest, and optional
`FILE_ID_INFO` remain path-free diagnostic facts. They can detect or explain a
preexisting navigation but cannot issue, retain, move, or close a capability.
Attempt 3 positive authority instead requires explicit consent, exactly one
post-baseline candidate at the exact location identity, full live eligibility,
and matching generations. The historical automatic attempts additionally
required a post-subscription registration and same-object lease identity.

Candidate issuance, live revalidation, and restore use the stricter rule: the
new frame must have exactly one Shell entry, and that entry's live filesystem
`FILE_ID_INFO` must equal the test directory's file identity. A URL digest,
status, source, title, or path string cannot substitute for that identity.

In Attempt 3, zero new exact-location candidates, more than one such candidate,
or an exact-location candidate that is in the forbidden baseline set blocks
isolation. In the historical automatic attempts, no matching registration, a
registration resolving to a baseline HWND, or more than one registration
matching the lease also blocked isolation. A baseline frame remains permanently
forbidden regardless of later navigation, closure, or conservative numeric
HWND reuse. No existing-window fallback exists.

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
immediately before native apply, post-verification, and restore. The historical
automatic path also checked before its optional exact-lease close. Navigation
away invalidates the token and prevents further native apply.

The opaque baseline digest is deliberately asymmetric with this authority
check: it can preserve a change witness when a preexisting location cannot be
opened for `FILE_ID_INFO`, but it can never make an inaccessible location an
eligible target.

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
session authority, one logical target ID, capability generation, and the
consent generation against which it was issued. It cannot be default, HWND, or
integer constructed and has no HWND conversion. Raw HWND and COM objects
remain internal to the STA-bound Explorer session.

Reason-bearing rejection includes at least preexisting/reused window, wrong
application/image/class, not top-level, owner/style/state/cloak/desktop,
location/integrity mismatch, ambiguous candidate, window/process exit,
monitor/DPI change, and post-verification mismatch.

Preexisting HWND values are permanently excluded for the session. A human
confirmation, navigation to the exact nonce directory, or later consent cannot
upgrade one. Failure to identify a unique new target cannot widen authority.

## Single translation and safe delta

R1-C2A performs one explicit single-window translation and no batch/Glue.
The token must first have been issued after the target-creation confirmation;
the harness then requires a distinct affirmative move consent and live
revalidates every authority/identity/eligibility fact before native apply. It
reuses `window_translation`:

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

## Restore, user-owned close, and stale lifetime

Restore is a separate live-validated translation from current actual geometry
to the original visible position. It is cleanup, not rollback, and a successful
restore is required for the Attempt 3 runtime Gate.

Attempt 3 does not own the user-created Explorer object and therefore does not
call `IWebBrowser2::Quit`, send `WM_CLOSE`, invoke a Shell close command, or
terminate/restart Explorer. After restoration it tells the user that the test
window may be closed manually. Leaving `WINDOW_DESTROY_LIFETIME = NOT TESTED`
does not block the single-translation baseline.

The historical automatic attempts allowed `IWebBrowser2::Quit` only through an
exact retained provisioning lease; that research and test evidence is
preserved but grants no close authority to Attempt 3. A hidden
`CabinetWClass` frame, new-HWND set delta, PID, class, consent, registration
event, or token alone never grants cleanup authority.

An optional post-restore human step may ask the user to close the exact test
window and then confirm completion. If performed, the old token must fail
preflight with `native_apply_attempted = false`. Skipping that step is permitted
and is reported as `WINDOW_DESTROY_LIFETIME = NOT TESTED`; the harness must not
coerce closure merely to increase test coverage.

The empty directory is removed only after no live Shell entry still references
it. No user file is created or modified.

## Feedback evidence

The independent observer records the exact issued target HWND/PID and single
translation plus restore. R1-C2A measures START/LOCATION/END, ordering,
duplicates, missing events, and actual geometry without assuming R1-C1 counts.
Events from the human creation/navigation phase are outside the operation
interval and cannot be relabelled as translation or restore feedback.

Attribution inputs are Explorer token/session/generation, operation ID,
expected/actual geometry, HWND/PID/TID, and matching WinEvent geometry. Time,
raw HWND, START/END, contiguity, or one-request-one-event are insufficient.
No suppression or Glue state machine is implemented.

## Required tests

Existing deterministic automatic-recovery tests cover:

- baseline entries with valid, empty, and inaccessible locations but reliable
  HWND values remain complete, opaque entries remain forbidden, and one entry
  without a reliable HWND blocks exclusion completeness;
- no registration, unrelated registrations only, one matching cookie, several
  unrelated plus one match, and two matching cookies;
- matching COM identity with a preexisting HWND, a new HWND with wrong COM
  identity, same object with location mismatch, and the sole eligible
  same-object/new-HWND/exact-location case;
- `WindowRevoked` before issuance, wrong subscription generation, malformed or
  post-retirement callback, Advise/Unadvise ordering, and callback reentrancy
  isolation;
- cleanup lease/session/identity mismatch causes no unsafe close, while exact
  lease cleanup invalidates the token and a stale apply makes no native call;
- the existing location/image/class/topology/state/cloak, authority/generation,
  safe-delta/work-area, translation/overflow, and post-verification checks.

The recovery model, Shell-event connection-point boundary, provisioning lease,
cookie correlation, and deterministic cases above were **IMPLEMENTED** and
**AUTOMATED TESTED** on 2026-08-27. That result validates the fail-closed model
and its test seams; it is not positive evidence that this workstation can
create and attribute the required Explorer object at runtime.

Attempt 3 deterministic consent-model tests must cover:

- one new-HWND exact-location candidate becomes eligible only after target
  confirmation, while zero or multiple new candidates block;
- a preexisting exact-location HWND and an opaque preexisting HWND can never be
  upgraded;
- absent move consent and consent/session/generation mismatch prevent native
  apply;
- candidate identity change, navigation away, minimize/maximize, or
  monitor/DPI change between the two consents prevents native apply;
- loss of identity/eligibility before restore prevents use of stale HWND state;
  and
- the optional human-close stale-token check makes no native call, while a
  skipped check remains `NOT TESTED` rather than inferred.

These deterministic tests cannot replace the required human UAT. Codex must
not synthesize either confirmation or represent itself as the human granting
third-party-window authority.

The three-run `--provision-only` stability Gate below applied to Attempt 2. It
is preserved as blocked history and is not a prerequisite that Attempt 3 may
retry or reinterpret:

```text
Attempt 2 required three consecutive Debug provision-only passes
Attempt 2 stopped after run 1 returned CoCreate E_FAIL
Provision-only runs 2 and 3 were not run, by the fixed stop-on-first-failure rule
PROVISIONING_STABILITY_GATE = BLOCKED
```

Attempt 3 instead requires the deliberately interactive
`--interactive-consent-test` UAT. It is not registered in CTest. Until a human
completes both consent steps and the target passes issuance, one translation,
exact post-verification, and restore, eligibility and runtime remain
`PENDING_UAT`, not PASS.

No path may substitute a baseline Explorer window.

## Local desktop isolation observations

The following results are **EMPIRICAL OBSERVATIONS** from this workstation on
2026-08-26. They describe only these runs and do not establish universal File
Explorer behavior.

1. The first harness run blocked during pre-create inventory because one
   preexisting Shell location could not be opened. No target was issued and no
   native placement occurred. **INFERENCE:** requiring `FILE_ID_INFO` for every
   preexisting entry was stricter than needed to detect navigation of user
   windows. The then-current baseline model was refined to retain a compound
   opaque per-entry witness; candidate and live authority were not weakened.
2. A second run of an older build took its single-CoCreate path and reported
   `E_FAIL`, but that build lost the diagnostic stage, so the evidence cannot
   attribute the HRESULT to a particular COM call. During the run, the
   independent observer recorded two previously absent hidden
   `CabinetWClass` identifiers and six `LOCATION_CHANGE` events. A final
   read-only Shell inventory grew from 11 to 13 entries and the two added
   entries had empty `LocationURL` values. The harness established no retained
   target correlation and made no native placement call. **INFERENCE:** these
   concurrent facts demonstrate ambiguity that must fail closed; they do not
   prove that one CoCreate universally creates two frames or that either frame
   represented the nonce directory.
3. The latest executed build blocked during pre-create inventory because an empty
   `LocationURL` could not supply the mandatory exact baseline digest. It made
   no target correlation and no native placement call. This is the intended
   fail-closed result under the contract implemented for that run. Later
   final-worktree changes only hardened deadline checks after that unreachable
   baseline Gate; they were built and automated-tested but were not used to
   repeat the already decisive desktop experiment.

The 2026-08-27 recovery design does not relabel or erase any of those results.
It corrects the over-strong coupling exposed by run 3: a baseline entry with a
reliable HWND but unavailable location can be excluded as
`OPAQUE_PREEXISTING`, while positive authority is established independently by
post-subscription registration, canonical COM identity, three-way HWND
equality, and exact target location.

### 2026-08-27 provisioning-recovery observation

The revised design and its deterministic tests were built and automated-tested
before the fixed desktop Gate. Provision-only attempt 1 then produced the
following **EMPIRICAL OBSERVATION**:

- read-only baseline inventory contained 14 Shell entries and all 14 supplied
  reliable HWND values; 10 locations were valid, two were empty, and two were
  inaccessible;
- the two empty and two inaccessible entries remained
  `OPAQUE_PREEXISTING`, all 14 HWND values entered the permanent forbidden set,
  and baseline exclusion therefore passed without promoting any opaque entry;
- `DShellWindowsEvents` connection-point `Advise` succeeded before creation,
  and the subscription retired cleanly;
- the one authorized official
  `CoCreateInstance(CLSID_ShellBrowserWindow)` call returned `E_FAIL` at the
  `create_browser_window` stage, before an `IWebBrowser2` provisioning lease or
  Shell registration could be established;
- zero `WindowRegistered` cookies, zero matching cookies, zero target tokens,
  and zero native placement attempts were observed;
- a post-attempt read-only inventory contained 15 entries. Relative aggregate
  counts changed by `+1` total, `+1` accessible, `+1` empty, and `-1`
  inaccessible, but no per-HWND category mapping proves which category belongs
  to the new entry. The unqualified `+1` is time-correlated with the attempt,
  but without a lease or registration cookie it cannot be attributed to the
  run or safely closed. Its category and attributable-orphan count are
  therefore unknown, not zero;
- an earlier raw summary field over-stated this fact as zero attributable
  orphans. The immutable raw evidence remains unchanged, while the
  implementation now emits unknown/null when creation produces no lease;
- a read-only safety audit found all four retained nonce directories empty and
  non-reparse; and
- no fallback creation or preexisting-window path was attempted.

Because attempt 1 failed, fixed provision-only attempts 2 and 3 were **NOT
RUN**. This is the prescribed stop-on-first-failure behavior, not a
retry-until-pass strategy. `PROVISIONING_STABILITY_GATE`, provisioning,
eligibility, and runtime remain blocked. The event-driven subscription and
canonical-identity design now have implementation and automated-test evidence,
but still have no positive real-Explorer provisioning or attribution evidence.

Consequently, the desktop isolation and single-translation acceptance path has
not passed. Observer output from the second run is not target-correlated
translation feedback and must not be relabelled as a successful Explorer
operation.

### Recovery-attempt status

The recovery history is additive and must not be rewritten as success:

1. **Attempt 1 — automatic Shell-inventory provisioning: BLOCKED.** The
   experiments exposed incomplete baseline-location assumptions and ambiguous
   hidden frames; no target was issued and no native placement occurred.
2. **Attempt 2 — Shell registration / `CLSID_ShellBrowserWindow` recovery:
   BLOCKED.** Baseline exclusion passed, but the one authorized
   `CoCreateInstance` returned `E_FAIL` before a provisioning lease or matching
   registration existed. `AUTO_PROVISIONING_ON_CURRENT_WINDOWS11 = BLOCKED`.
3. **Attempt 3 — user-consented target authority: CURRENT APPROACH,
   PENDING INTERACTIVE UAT.** It neither repairs nor falls back from Attempt 2;
   the user creates the frame and grants two explicit consents, while PaneBind
   independently proves new-HWND exclusion, exact file identity, full live
   eligibility, and generation freshness.

Passing deterministic consent-model tests or building the interactive harness
will establish implementation readiness only. `R1C2A_ELIGIBILITY_GATE` and
`R1C2A_RUNTIME_GATE` cannot pass until a human completes the UAT and the raw
target-correlated evidence validates translation and restore.

## Adopted and rejected designs

Adopted:

- user-consented creation of one new Explorer top-level frame, followed by
  independent post-baseline/exact-location/live-eligibility proof;
- separate target-confirmation and move-consent generations, neither treated
  as a credential or native identity substitute;
- a complete pre-create forbidden HWND set in which unavailable locations are
  retained as opaque preexisting entries rather than promoted or discarded;
- exactly one new exact-location candidate HWND and exactly one matching Shell
  entry; zero, multiple candidates, tabs, and reused frames block;
- exact target filesystem identity for issuance and every live use;
- retained process handle, file identity, security/state/desktop allowlist;
- Explorer-specific opaque capability and reason-bearing eligibility;
- live revalidation, one shared-bridge translation, post-verification, restore;
- user-owned close only, with an optional manual stale-token check; and
- independent WinEvent evidence.

The official `CLSID_ShellBrowserWindow` creation request, retained-object
identity, Shell registration connection point, cookie correlation, and
exact-object cleanup remain preserved as Attempt 2 research/test history. They
are not active/default Attempt 3 authority.

Rejected:

- ordinary ShellExecute reuse as an isolation mechanism;
- `ShellExecuteEx`, `explorer.exe <path>`, `/n`, `/separate`, or `start <path>`
  as a modern new-HWND guarantee;
- repeated CoCreate attempts, a deadline reset after any readiness signal,
  creation before the Shell event subscription, or Explorer geometry setters
  during provisioning;
- sleep plus full enumeration as the sole attribution mechanism;
- requiring every baseline window to expose a location, upgrading an opaque
  baseline entry, or accepting a reused baseline HWND;
- a registration cookie, event order, `NavigateComplete2` URL, raw interface
  pointer, or raw HWND as standalone authority;
- titles, basename, class, PID, HWND, location string, or IShellWindows
  membership alone as authority;
- any preexisting-window fallback or baseline HWND upgrade;
- treating `Ctrl+N`, ENTER, consent alone, or a new tab as target identity;
- synthesized/global input, UI Automation clicks, foreground forcing, or a
  production selector/picker in this UAT round;
- automatic close of the user-created Explorer target;
- generic third-party token/manager/operations;
- multiple real-window batch, Glue, input hooks, Snap, resize, injection,
  process kill, shell restart, polling, auto-elevation, or cross-monitor move.

## Research gate

```text
R1C2A_PRIOR_ART_GATE = PASS
R1C2A_BASELINE_EXCLUSION_GATE = PASS
ATTEMPT_1_AUTO_INVENTORY_PROVISIONING = BLOCKED
ATTEMPT_2_SHELL_REGISTRATION_PROVISIONING = BLOCKED
AUTO_PROVISIONING_ON_CURRENT_WINDOWS11 = BLOCKED
ATTEMPT_2_RUNTIME_RESULT = CREATE_BROWSER_WINDOW E_FAIL; NO LEASE; NO REGISTRATION; NO TARGET; NO NATIVE APPLY
PROVISIONING_STABILITY_GATE = BLOCKED
ATTEMPT_3_USER_CONSENTED_AUTHORITY = CURRENT_APPROACH
R1C2A_CONSENT_AUTHORITY_DESIGN = PASS
R1C2A_INTERACTIVE_UAT = PENDING
R1C2A_ELIGIBILITY_GATE = PENDING_UAT
R1C2A_RUNTIME_GATE = PENDING_UAT
THIRD_PARTY_AUTHORITY = EXPLORER TEST FIXTURE ONLY
EXTERNAL_CODE_COPIED = NO
EXTERNAL_CODE_ADAPTED = NO
R1C2B = NOT STARTED
```
