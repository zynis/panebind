# R0 Consolidated Prior-Art Review

Review date: 2026-08-24.

```text
PRIOR_ART_GATE = PASS
```

The gate applies only to the R0 read-only Windows event/geometry observer and
platform-neutral geometry baseline. It does not authorize Snap, Glue, zones,
global input control, persistent groups, window operations, or R1 work.

## Gate evidence

| Requirement | Result | Evidence |
| --- | --- | --- |
| AltSnap actually inspected | PASS | Source, history, issues, PRs, license, and upstream AltDrag comparison are recorded in [ALTSNAP_REVIEW.md](ALTSNAP_REVIEW.md). |
| AltSnap immutable revision | PASS | `5c86416ad21e4b72844a998a746bd3bb0bee5f5d`; describe `1.68-48-g5c86416`; nearest release `1.68`. |
| AltSnap license boundary | PASS | GPLv3-or-later source headers/GPLv3 license text; reference-only; copied/adapted `NO`/`NO`. |
| FancyZones actually inspected | PASS | Source, tests, history, issues, PRs, license, and current literal paths are recorded in [FANCYZONES_REVIEW.md](FANCYZONES_REVIEW.md). |
| FancyZones immutable revision | PASS | PowerToys `19c4d805321db86f3634e6968e14dbf25cbba14a`; no exact tag points at this commit. |
| FancyZones license boundary | PASS | MIT; reference-only during R0; copied/adapted `NO`/`NO`. |
| Official event/geometry semantics | PASS | 33 claim-level Microsoft Learn citations in [R0_WINDOWS_EVENT_MODEL.md](R0_WINDOWS_EVENT_MODEL.md). |
| Consolidated provenance | PASS | Every external project actually inspected is recorded in [SOURCE_PROVENANCE.md](SOURCE_PROVENANCE.md). |
| Clear design lessons | PASS | Consolidated below and detailed in the project reviews. |

There was no source, network, permission, or license blocker. An unavailable
AltSnap issue (`#618`, HTTP 404) and the missing pre-public fork history are
recorded as narrow evidence limitations; no conclusion depends on the missing
issue or an invented removal commit.

## Reference classification

| Reference | Classification | Why it is useful | Why it is not a template |
| --- | --- | --- | --- |
| AltSnap | Mature, active behavioral prior art | Deep real-world move/resize/snap/filter/DPI/input history and corrective issue record. | GPL boundary; Win32/global-state coupling; no automated test target; it controls windows and input outside R0. |
| PowerToys / FancyZones | Mature production reference | Event/lifetime separation, work-area/topology abstractions, reason-bearing filters, extensive tests, and production hardening history. | `FancyZonesLib` is intentionally Windows-specific and combines native behavior, UI, storage, telemetry, and operations. |
| AltDrag | Mature historical comparison | Verifies the former injected `WH_CALLWNDPROC`/subclassing architecture that AltSnap removed. | GPL reference-only; injection and dual-bitness helper violate PaneBind principles. |
| Microsoft Learn Win32 documentation | Authoritative platform contract | Defines the APIs, delivery semantics, race limits, coordinate/DPI meanings, and manifest behavior. | Documentation cannot establish actual application event sequences or solve product policy. |

Repository popularity was not used as maturity proof. Activity, releases,
source organization, test surfaces, corrective history, issues, and licenses
were examined.

## Consolidated architectural lessons

### Adopt as independent design principles

- Use narrow `WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS` registrations;
  do not inject code into event-generating processes.
- Keep callback work bounded, queue immutable receipts, and serialize mutable
  processing on the hook-owning message-loop thread.
- Keep native observation separate from future native operations. Receiving an
  `HWND` is not authority to reposition it.
- Distinguish `GetWindowRect` positioning bounds from DWM extended visible-frame
  bounds and retain missing/error state rather than fabricating equality.
- Declare Per-Monitor V2 in the executable manifest and treat coordinate space,
  target-window awareness, raw window DPI, monitor, and work area as separate
  evidence.
- Represent filter decisions with named reasons; do not hide policy inside
  geometry or reduce visibility/ownership/style/path facts to one unexplained
  boolean.
- Treat native window and monitor handles as ephemeral adapter identifiers.
- Derive regression questions from historical failures: event cancellation,
  topology replacement, destroyed targets, DPI context mismatch, callback
  backlog, optional process metadata, and identity/hash consistency.

### Adapt rather than copy

- FancyZones' registered-message handoff becomes an explicit PaneBind native
  receipt queue and normalized event; no Win32 message or handle enters core.
- AltSnap's mutable per-axis scan informs later candidate research, but any
  future geometry policy should use explicit distances, overlap, provenance,
  and deterministic tie-breaking designed independently.
- Work area, current topology, persistent display identity, virtual desktop,
  layout, and behavior are separate concepts. R0 records only current monitor
  and work-area facts.
- Upstream blacklists are evidence of compatibility complexity, not a list to
  copy. R0 uses a minimal documented observation policy and logs its reasons.

### Avoid

- Injected `WH_CALLWNDPROC`, third-party subclassing, dual-bitness injection
  helpers, or any other DLL injection.
- High-frequency or resident polling as the core event source.
- Global low-level mouse/keyboard window control in R0.
- A Windows-heavy module mislabeled as a portable core.
- Long or reentrant cross-process work in a hook callback.
- Undocumented native-snap heuristics promoted to normalized truth.
- Treating a PMv2 manifest, merged PR, issue report, or upstream test as proof
  of behavior in PaneBind's environment.
- Copying, translating, or mechanically restructuring GPL implementation code.

## R0 observer decisions unlocked by the gate

The approved R0 implementation is deliberately smaller than both references:

1. `EnumWindows` performs a best-effort initial census.
2. One hook covers the contiguous system move/size start/end constants; a
   second exact hook covers location changes.
3. Both use out-of-context delivery and skip the observer process.
4. The callback records bounded raw metadata into a finite queue; a posted
   thread message triggers snapshot/log processing on the owner thread.
5. Location events require `OBJID_WINDOW`, `CHILDID_SELF`, and a root HWND.
   Start/end events require a non-null, currently queryable root HWND but do
   not invent undocumented object-ID requirements.
6. The R0 location hook remains registered throughout observation. FancyZones'
   dynamic subscription is valuable production evidence, but R0 is measuring
   unpaired/application-originated geometry events and must not assume every
   useful location change is enclosed by a start/end pair. Queue overflow is
   reported explicitly; polling is not introduced.
7. Default candidates reject invisible, non-root, cloaked, and tool-only
   windows, while minimized/maximized state remains observable. This is an R0
   observation policy, not a future Snap/Glue eligibility definition.
8. Snapshots retain raw native facts and convert them into an optional-field,
   platform-neutral model. `GetDpiForWindow` is labeled target-window DPI, not
   physical monitor DPI.
9. Only the three normalized event types are implemented. FancyZones lifecycle
   hooks are recorded as future research questions, not pulled into R0.

## Required empirical tests and honest limits

The reviews establish design inputs, not local runtime results. The observer
must still test or explicitly mark untested:

- actual start/location/end order, duplication, missing pairs, and object IDs;
- Explorer, Notepad, Terminal, Chromium, VS Code, Excel, and Power BI where the
  environment provides them;
- normal/minimized/maximized/custom-frame positioning versus visible bounds;
- negative-origin and mixed-DPI monitor topology;
- DPI-unaware/system-aware/per-monitor-aware targets;
- process-path access denial, short-lived handles, destruction/reuse, and
  elevation boundaries; and
- queue reentrancy/backpressure behavior.

Upstream issue reports and tests remain cited prior art. They are never
relabeled as PaneBind `MANUALLY OBSERVED` or `AUTOMATED TESTED` results.

## License and source-use conclusion

All R0 implementation is independently designed from the brief, official API
contracts, and the consolidated lessons above. No external implementation code
entered the repository.

```text
ALTSNAP_CODE_COPIED = NO
ALTSNAP_CODE_ADAPTED = NO
POWERTOYS_CODE_COPIED = NO
POWERTOYS_CODE_ADAPTED = NO
ATTRIBUTION_REQUIRED_FOR_R0_CODE = NO
```

The PowerToys MIT license could permit reuse with notice preservation, but no
reuse occurred. That known compatibility does not weaken the provenance rule:
future reuse must be explicitly approved and recorded before code enters.
