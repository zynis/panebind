# SnapWeave Project Charter

## Mission

SnapWeave exists to make arranging and manipulating desktop windows feel
coherent, predictable, and lightweight. It will explore interactions such as
snapping, attached-window movement, coordinated resizing, stretching, docking,
and convenient pointer/keyboard operations without tying the product's core
model to one operating system.

**Windows is the first implementation platform, not the identity of the
product.**

SnapWeave may use established tools, including AquaSnap, as behavior and UX
references. It is not an AquaSnap clone: private implementation details will
not be reverse engineered, decompiled, or copied.

## Goals

- Provide useful desktop window enhancements with predictable interactions.
- Build behavior on documented platform semantics, inspected prior art, and
  reproducible observations rather than API intuition alone.
- Keep geometry, event semantics, topology, and future behavior policy
  portable wherever the domain permits.
- Make the Windows integration native, event-driven, and production-minded.
- Maintain a source tree that is understandable, testable, and suitable for
  community and commercial use under the MIT License.
- Preserve user privacy and keep idle resource use low.

## Non-goals

- Reproducing another product's private implementation.
- Shipping simultaneous Windows, macOS, and Linux implementations during the
  Windows-first phase.
- Inventing portability layers for hypothetical requirements.
- Injecting code into third-party processes.
- Depending on continuous high-frequency window polling.
- Collecting telemetry or requiring a network connection for core behavior.
- In R0 specifically: moving/resizing third-party windows, Snap, Glue, zones,
  auto-tiling, persistent groups, or global input controls.

## Product principles

1. Interactions should be understandable and reversible by the user.
2. Default behavior should cooperate with operating-system window management.
3. Edge cases involving DPI, displays, elevation, visibility, ownership, and
   native frame bounds are first-class design inputs.
4. A small, robust feature is preferable to a broad feature built on guessed
   semantics.
5. Research findings and known limitations are part of the product, not
   disposable development notes.

## Privacy principles

- No telemetry is collected or transmitted.
- Core operation has no network dependency.
- Window metadata observed for local behavior remains local unless a future,
  explicit user-controlled export feature is approved.
- Logs are an intentional research/debug surface. R0 writes them only to
  standard output, under the user's control; window titles and process paths
  may be sensitive and must be handled accordingly.

## Performance principles

- Use operating-system events as the primary signal.
- Do not use resident high-frequency polling loops.
- Keep idle CPU near zero as a long-term objective.
- Keep resident memory below 20 MB as a long-term target, subject to later
  measurement and architecture review.
- Avoid heavyweight runtimes and dependencies when native C++ and platform APIs
  are sufficient.

The CPU and memory figures are directions, not R0 acceptance thresholds. R0
must not make architectural decisions that obviously preclude them.

## Platform strategy

The source is separated into:

- a platform-neutral core for geometry, normalized events, and future behavior;
- platform adapters that observe native windows and translate their state;
- an application layer that owns lifetime, configuration, and diagnostics; and
- a future operations adapter boundary for approved window actions.

Only the Windows adapter exists during the Windows-first phase. macOS and Linux
adapters should be added only when there is an approved round with real platform
requirements and evidence.

## Open-source strategy

- SnapWeave's own code is MIT-licensed.
- Every inspected external source is recorded with an immutable revision and
  use status.
- GPL sources such as AltSnap are reference-only: architectural lessons and
  independently designed behavior may inform SnapWeave, but GPL code is not
  copied, adapted, translated, or mechanically rewritten into this codebase.
- Code reuse from permissive sources requires an explicit compatibility check,
  provenance entry, and any required attribution before the code is introduced.
- When licensing is uncertain, implementation stops until it is resolved.

## Roadmap

### R0 — Research and architecture baseline

- Inspect mandatory prior art and record provenance.
- Establish normalized geometry and event boundaries.
- Build a read-only Windows observer for enumeration and event logging.
- Test pure geometry and record honest automated/manual evidence.

### Later rounds — subject to separate approval

Potential later work includes an architecture review, deeper behavior
experiments, a window-operations adapter, snapping, and attached-window
behavior. These are roadmap directions, not approved R0 scope. R0 may recommend
research questions but must not automatically begin the next round.

