# PaneBind Engineering Rules

These rules apply to every contributor and every development round in this
repository. A round-specific brief may narrow the allowed scope but may not
silently broaden it.

## Scope discipline

- Implement only work explicitly authorized by the current round.
- Do not pull future features forward because they are convenient to add.
- Do not build speculative frameworks for unapproved future requirements.
- Avoid over-engineering and large abstractions that are not supported by
  evidence and current-round requirements.
- Research rounds must not drift into feature implementation.
- Stop at the round boundary and record follow-up questions instead of starting
  the next round automatically.

## Prior art first

Before designing complex window movement, resizing, snapping, glue/grouping,
hooks, event handling, input behavior, DPI, monitor topology, focus, z-order,
maximize/minimize, filtering, blacklist, elevation, virtual-desktop, or
frame-bound behavior:

1. Inspect mature open-source prior art.
2. Inspect its issues, pull requests, and commit history, and record historical
   design decisions and fixes.
3. Confirm behavior against official platform documentation.
4. Run focused empirical observations where documentation and prior art are
   insufficient.
5. Design PaneBind's platform-neutral model from that evidence.
6. Define and run the applicable tests.
7. Implement only after the applicable research gate passes.

The mandated research flow is:

```text
Prior Art
-> Issues / PR / Commit History
-> Official Platform Documentation
-> Empirical Observation
-> PaneBind Design
-> Tests
-> Implementation
```

AltSnap and Microsoft PowerToys/FancyZones are mandatory long-term references
for the initial Windows work. Classify additional references as mature,
experimental, proof-of-concept, or abandoned; repository popularity alone is
not evidence of maturity.

For AltSnap and its AltDrag history, research movement, resizing, snapping,
mouse interaction, hooks, zones, filtering and blacklists, monitor and DPI
behavior, architecture changes, and bug fixes. These GPL sources are
reference-only.

For PowerToys/FancyZones, research production architecture, event processing,
work areas, monitor topology, DPI, filtering, layouts, tests, and reliability
hardening. Any future code reuse requires a separate license, provenance, and
attribution decision before code enters PaneBind.

## Licensing and provenance

- Record every external project actually inspected in
  `docs/research/SOURCE_PROVENANCE.md`, including repository, exact commit/tag,
  license, review date, inspected modules/issues/PRs, lessons, applicable
  subsystem, whether code was copied or adapted, and required attribution.
- Confirm license compatibility before external code enters this repository.
- If compatibility or provenance is uncertain, stop: do not copy or adapt.
- AltSnap and other GPL sources are reference-only unless an explicit future
  decision changes the repository's licensing strategy.
- Do not copy, translate, mechanically rewrite, rename-and-retain, or otherwise
  derive MIT-licensed PaneBind code from GPL implementation code.
- Preserve required attribution for any explicitly approved reused code.

## Product and platform boundaries

- Windows is the first implementation platform, not the identity of the
  product.
- Keep domain geometry, normalized events, topology, adjacency, and behavior
  free of unnecessary operating-system dependencies.
- Native handles, Win32 structures, Win32 headers, and Windows event constants
  belong under the Windows platform adapter, never under `src/core/`.
- Platform adapters translate native windows and events into normalized models.
- Do not create placeholder macOS or Linux implementations merely to appear
  portable.
- Favor C++20, CMake, native platform APIs, small dependency surfaces, no
  telemetry, no network requirement, and low idle resource use.

## Event-driven design

- Use platform events as the primary signal; fixed high-frequency polling must
  not be the core mechanism.
- In particular, do not introduce resident 1 ms, 5 ms, 8 ms, 16 ms, or 100 ms
  polling loops as a substitute for platform events.
- A bounded retry justified for a specific operation is not authorization for
  a polling event source.

## Windows baseline and resource goals

- Use C++20, CMake, native Win32 APIs, and Per-Monitor DPI Awareness V2 for the
  current Windows implementation baseline.
- Keep the product portable-first, without DLL injection, telemetry, or a
  network dependency for core operation.
- Treat idle CPU approximately equal to zero and resident memory below 20 MB as
  long-term goals that must be measured rather than assumed.

## R0 limits

R0 may observe, log, normalize, and calculate. It must not control third-party
windows. In R0, do not add:

- `SetWindowPos`/`DeferWindowPos` control of third-party windows;
- Snap, Glue, zones, auto-tiling, persistent groups, or window binding;
- global input behavior that controls windows;
- DLL injection; or
- high-frequency polling as an event source.

Prefer out-of-context WinEvent hooks. Any event beyond the approved minimal set
must be justified in the research record. Pure geometry logic should be
deterministic and platform-independent.

## Evidence before behavior

- A demo that appears to work is not acceptance evidence by itself.
- Behavior claims must be supported by at least one reliable form of evidence:
  automated tests, manual observation, inspected prior art, or official
  platform documentation.
- Do not relabel upstream reports or tests as PaneBind observations.

## User data safety

- Do not force-close applications that may contain unsaved work.
- Do not discard user data, reset another application's state, or close
  third-party applications merely to complete UAT.
- Treat unsaved Notepad tabs and equivalent application state as user data.

## Git discipline

- Do not develop directly on `main`.
- Do not force-push, rewrite shared history, reset unknown changes, or mix
  unrelated work into a round.
- At each round, record the starting and final SHA, branch, commits, working
  tree status, and local/remote divergence.
- Preserve unrelated user changes in a dirty working tree.

## Stop conditions

Stop the affected work, preserve evidence, and report when:

- an external source or its license cannot be verified;
- the prior-art source cannot actually be inspected;
- unknown local changes overlap the authorized work;
- the core appears to require a Windows-only type without clear justification;
- research would require DLL injection, high-frequency polling, or manipulation
  of a third-party window;
- observed behavior contradicts an architectural assumption; or
- work begins to cross into the next round;
- proceeding would risk user data loss;
- a prohibited implementation would be required; or
- a major architecture assumption lacks evidence.

## Quality and reporting

- Distinguish `IMPLEMENTED`, `AUTOMATED TESTED`, `MANUALLY OBSERVED`,
  `NOT TESTED`, and `BLOCKED`; never infer an empirical result that was not
  observed.
- Keep machine-readable observer output stable enough for later analysis.
- Run proportionate builds and tests and record exact commands and environment.
- Use meaningful commits; do not split work only to inflate commit count.
