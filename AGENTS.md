# SnapWeave Engineering Rules

These rules apply to every contributor and every development round in this
repository. A round-specific brief may narrow the allowed scope but may not
silently broaden it.

## Scope discipline

- Implement only work explicitly authorized by the current round.
- Do not pull future features forward because they are convenient to add.
- Do not build speculative frameworks for unapproved future requirements.
- Research rounds must not drift into feature implementation.
- Stop at the round boundary and record follow-up questions instead of starting
  the next round automatically.

## Prior art first

Before designing complex window movement, resizing, snapping, grouping, hooks,
event handling, DPI, monitor, focus, z-order, maximize/minimize, filtering,
blacklist, elevation, virtual-desktop, or frame-bound behavior:

1. Inspect mature open-source prior art and its history.
2. Record known issues and historical design decisions.
3. Confirm behavior against official platform documentation.
4. Run focused experiments where documentation and prior art are insufficient.
5. Design SnapWeave's platform-neutral model.
6. Implement only after the applicable research gate passes.

AltSnap and Microsoft PowerToys/FancyZones are mandatory long-term references
for the initial Windows work. Classify additional references as mature,
experimental, proof-of-concept, or abandoned; repository popularity alone is
not evidence of maturity.

## Licensing and provenance

- Record every external project actually inspected in
  `docs/research/SOURCE_PROVENANCE.md`, including repository, exact commit/tag,
  license, review date, inspected modules/issues/PRs, lessons, applicable
  subsystem, and whether code was copied or adapted.
- Confirm license compatibility before external code enters this repository.
- If compatibility or provenance is uncertain, stop: do not copy or adapt.
- AltSnap and other GPL sources are reference-only unless an explicit future
  decision changes the repository's licensing strategy.
- Do not copy, translate, mechanically rewrite, rename-and-retain, or otherwise
  derive MIT-licensed SnapWeave code from GPL implementation code.
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

## Stop conditions

Stop the affected work, preserve evidence, and report when:

- an external source or its license cannot be verified;
- the prior-art source cannot actually be inspected;
- the core appears to require a Windows-only type without clear justification;
- research would require DLL injection, high-frequency polling, or manipulation
  of a third-party window;
- observed behavior contradicts an architectural assumption; or
- work begins to cross into the next round.

## Quality and reporting

- Distinguish `IMPLEMENTED`, `AUTOMATED TESTED`, `MANUALLY OBSERVED`,
  `NOT TESTED`, and `BLOCKED`; never infer an empirical result that was not
  observed.
- Keep machine-readable observer output stable enough for later analysis.
- Run proportionate builds and tests and record exact commands and environment.
- Preserve unrelated user changes in a dirty worktree.
- Use meaningful commits; do not split work only to inflate commit count.
