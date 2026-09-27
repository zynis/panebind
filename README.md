# PaneBind

PaneBind is an early-stage, open-source desktop window-enhancement project.
Windows is the first implementation platform, while the geometry and event
model are designed to remain platform-neutral.

The current R1-C3B baseline includes a narrowly authorized two-Explorer
Ctrl+Move test session, with human Debug smoothness accepted between A and B.
The [human validation report](docs/reports/R1C3B_HUMAN_VALIDATION_REPORT.md)
records its evidence, authority boundaries and limitations. This is not a
general-purpose or production-ready window manager. R0 remains a separate
read-only observer; Glue Resize is not implemented.

This stacked development branch adds the bounded three-Explorer
[C4B live Magnet harness](docs/architecture/R1C4B_LIVE_MAGNET.md): ordinary
Move/Resize magnetic correction and existing Ctrl Glue Move, with separate
explicit consent. C4A implementation/review is complete; its human final seal
is deferred to C4B integrated UAT. C4B pure Core and ordinary owned integration
are automatically tested, but the first human run failed exact placement.
The [Fix 3 authority gate](docs/reports/R1C4B_FIX3_EXECUTION_REPORT.md) is
REJECTED: the first real owned Move and Bottom Resize both reasserted the raw
native trajectory after an immediately exact correction. Implementation is
NOT READY; further automated interaction and human UAT are stopped. The
[alternatives research](docs/research/R1C4B_FIX3_ALTERNATIVES.md) describes the
decision boundary. The explicitly authorized
[Pivot 1 cancel/Raw Input research](docs/reports/R1C4B_PIVOT1_EXECUTION_REPORT.md)
adds an independent test-only probe; its three development observations stopped
at input-correlation or owned activation prerequisites. Cancellation remains
UNKNOWN at that checkpoint, takeover/Explorer NOT RUN, and the candidate
UNRESOLVED—not rejected by an unexecuted cancellation test. Human UAT remains
NOT READY. C4C Glue Resize
and C4D relation affordance remain pending. Screen-edge live Magnet, other apps,
mixed-DPI/monitor operation and product UI are outside this branch's scope.

[Pivot 1 Fix A](docs/reports/R1C4B_PIVOT1_FIXA_EXECUTION_REPORT.md) removes the
unsupported local-active/focus and fresh-callback prerequisites. Its 142
synthetic checks pass; after the session unlocked, real background Raw Input
and native Move cancellation were observed. Capture released and EXIT occurred,
but the rect restored to its initial state after API return and before EXIT.
The fixed return-rect-retention gate fails; further interaction stops. This is
not post-EXIT reassertion or proof that every wait-EXIT/takeover variant fails.

[Pivot 1 Fix B](docs/reports/R1C4B_PIVOT1_FIXB_EXECUTION_REPORT.md) formally
supersedes return-rect retention with a real END-barrier handoff. One Debug
owned Move passes full-original-anchor reconciliation and 18 Raw-driven
continuations. The next Bottom Resize stops before any write at an unresolved
fresh authority/stability check; its missing proof details do not establish
native reassertion. Overall architecture remains UNRESOLVED, repetitions and
Explorer NOT RUN, Human UAT NOT READY. Old Fix A evidence/verdict is unchanged.

[Pivot 1 Fix C](docs/reports/R1C4B_PIVOT1_FIXC_EXECUTION_REPORT.md) adds structured
test-only preflight and exact-source out-of-context WinEvent END matching.
Both Debug Bottom Resize attempts stop before any write: the cursor's actual
hit-test root is not the owned source/guard, despite the one authorized guard
geometry repair covering both planned trajectories with a 50px margin.
Matching END, fresh GUI clearance and exact terminal P/V are observed; this is
an input-authority blocker, not a geometry counterexample. Architecture remains
UNRESOLVED. Old Fix B Move PASS is preserved; Fix C Move, repetitions, Explorer
and Human UAT are not run. No further interaction or authority changes proceed.

[Pivot 1 Fix D](docs/reports/R1C4B_PIVOT1_FIXD_EXECUTION_REPORT.md) separates
product gesture authority from synthetic-input isolation and adds a test-only
post-END, nonactivating owned shield. The first Debug Bottom Resize records a
full original-anchor trajectory, 18 Raw continuations, actual UP and teardown,
but the independent validator overflows an Int32 QPC accumulator. Its preserved
verdict is INVALID_EVIDENCE, not an accepted Resize PASS or a geometry
counterexample. Further Move, smoke/formal batches, Explorer and human UAT are
stopped. Old Fix B Move PASS and old Fix C verdicts remain unchanged;
architecture remains UNRESOLVED. No product shield or Raw Input is implemented.

[Pivot 1 Fix E](docs/reports/R1C4B_PIVOT1_FIXE_EXECUTION_REPORT.md) repairs only
64-bit evidence arithmetic and accepts the immutable Fix D Resize run through
a new corrected replay artifact; the original INVALID_EVIDENCE metadata is
unchanged. A current Debug Move and all four operation-specific 5/5 smoke
groups pass. Debug Move formal stops at repetition 14 on input interference
after 13 passes, before cancellation or any takeover write. Other formal groups
are NOT RUN; no retries occur. Overall owned stability remains unaccepted and
architecture UNRESOLVED, not rejected by a geometry counterexample. Explorer
and human UAT remain NOT READY. No native/product runtime changes are made.

## Build

Requirements:

- CMake 3.25 or newer
- a C++20 compiler
- Windows SDK for the observer target

```text
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

On Windows, the resulting `panebind-observer.exe` emits JSON Lines.
Run it with `--enumerate-only` for a one-time snapshot or without arguments to
observe the three R0 WinEvent types until interrupted.

See [the project charter](docs/charter/PROJECT_CHARTER.md),
[architecture](docs/architecture/ARCHITECTURE.md), and
[R0 research](docs/research/PRIOR_ART_REVIEW.md) for scope and evidence.

## License

PaneBind is licensed under the MIT License. External research sources and
their code-use status are tracked separately in
`docs/research/SOURCE_PROVENANCE.md`.
