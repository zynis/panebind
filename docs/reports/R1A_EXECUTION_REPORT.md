# PaneBind R1-A Execution Report

Report date: 2026-08-25 (Asia/Shanghai).

## 1. Round and scope

```text
Round = R1-A — Adjacency, Topology & Translation Classification Baseline
Starting main = ebf620ab04c38be8f4544f436424b807e30b951a
Required R0 tag = r0-baseline
R0 tag target = ebf620ab04c38be8f4544f436424b807e30b951a
Branch = codex/r1a-adjacency-topology
Evaluated implementation HEAD = 54f827c2e2599c427e3854aaf41ddf35ebcfd9a2
```

R1-A implements only platform-neutral visible-geometry adjacency, edge
relations, an undirected graph/component solver, geometry-change
classification, checked translation, and a pure initial-relative move plan. It
does not implement Glue behavior, Snap, input handling, a platform operations
adapter, or control of any third-party window.

## 2. Prior-art gate

The targeted review is recorded in
[`R1_ADJACENCY_TOPOLOGY_RESEARCH.md`](../research/R1_ADJACENCY_TOPOLOGY_RESEARCH.md)
and the updated
[`SOURCE_PROVENANCE.md`](../research/SOURCE_PROVENANCE.md).

| Source | Pinned revision | License/use | Result |
| --- | --- | --- | --- |
| AltSnap | `5c86416ad21e4b72844a998a746bd3bb0bee5f5d` | GPL-3.0-or-later, reference-only | PASS |
| AltDrag | `e2740d605b0336a3b391fec26794718864b19521` | GPL-3.0-or-later, reference-only | PASS |
| PowerToys/FancyZones | `19c4d805321db86f3634e6968e14dbf25cbba14a` | MIT, reference-only for R1-A | PASS |
| Microsoft Learn geometry documentation | live pages reviewed 2026-08-25 | facts cited/paraphrased | PASS |

No external code was copied, adapted, translated, or mechanically rewritten.
AltSnap and AltDrag remain GPL reference-only. FancyZones provided production
topology/DPI/lifetime lessons but no window adjacency graph implementation.

```text
R1A_PRIOR_ART_GATE = PASS
EXTERNAL_CODE_COPIED = NO
EXTERNAL_CODE_ADAPTED = NO
```

## 3. Adjacency mathematical definition

`WindowGeometry` contains an opaque `WindowId` and one visible rectangle. The
caller is responsible for selecting windows and normalizing all rectangles into
one semantic coordinate space.

Only opposing edge pairs are considered. After the pair is canonicalized by
`WindowId`, signed gap means:

```text
first.Right  <-> second.Left   second.left  - first.right
first.Left   <-> second.Right  first.left   - second.right
first.Bottom <-> second.Top    second.top   - first.bottom
first.Top    <-> second.Bottom first.top    - second.bottom
```

```text
signed_gap > 0 = visible gap
signed_gap = 0 = exact touch
signed_gap < 0 = shallow intrusion
```

A candidate is valid only when its signed-gap magnitude is within the explicit,
nonnegative `edge_tolerance` and its orthogonal overlap has strictly positive
length. Tolerance never expands the orthogonal intervals.

Consequences:

- exact side touch: adjacent;
- small positive gap within tolerance: adjacent;
- small edge intrusion within tolerance: adjacent with negative gap;
- gap/intrusion beyond tolerance: not adjacent;
- corner-only or perpendicular near-touch: not adjacent;
- empty rectangle: isolated node, no relation;
- deep overlap/containment with no near opposing edge: not adjacent; and
- more than one qualifying contact for a pair: ambiguous, not adjacent.

No product tolerance or `minimum_overlap` default is embedded. The exact
orthogonal overlap is retained in `AdjacencyRelation` for later evidence-backed
policy.

## 4. Graph and topology model

`WindowAdjacencyGraph`:

- sorts nodes by `WindowId::value()`;
- rejects duplicate IDs and negative tolerance;
- scans each unordered pair exactly once (`O(n²)`);
- stores at most one canonical undirected relation per pair;
- produces no self or duplicate edge;
- returns connected components in canonical ID order;
- includes transitive neighbors and isolated nodes; and
- rejects an unknown component start ID.

Input permutation does not alter nodes, relations, components, or move plans.
The graph stores no monitor, DPI, native handle, virtual desktop, global cache,
or Windows eligibility state.

## 5. Geometry-change classification

```text
Unchanged     all four bounds equal
Translation   size unchanged and position changed; checked dx/dy present
ResizeOrMixed width or height changed
```

All four edges must share the same translation delta. Size-changing
maximize-like or drag-restore geometry is `ResizeOrMixed`, not `Translation`.
R1-A does not infer resize direction or window state.

Checked arithmetic avoids signed overflow. When classification or translation
requires an extent or delta, an unrepresentable value throws
`std::overflow_error`; an unrepresentable adjacency orthogonal overlap does the
same. Rectangle translation checks all four coordinates.

## 6. Pure move plan and drift prevention

`TranslationSession` copies the leader's initial connected component from the
graph. It retains value snapshots, not graph references or native windows.

For each planning call:

```text
total_delta = current_leader_visible - initial_leader_visible
follower_target = initial_follower_visible + total_delta
```

The output contains followers only, sorted by ID, with a
`target_visible_rect`. An unchanged leader returns unchanged follower targets.
`ResizeOrMixed` returns no plan. Any overflow fails the whole call; no partial
result is exposed.

No follower-previous rectangle or incremental event delta is retained, so
repeated and non-monotonic leader positions cannot accumulate drift.

## 7. Implementation inventory

New core headers:

```text
src/core/geometry/checked_arithmetic.h
src/core/model/window_id.h
src/core/topology/window_adjacency.h
src/core/movement/translation.h
src/core/movement/move_plan.h
```

`WindowId` moved to a common model header; `events::WindowId` remains a
compatibility alias. `NormalizedWindowSnapshot` now depends directly on the
common ID. The Windows observer added one explicit event-header include after
that transitive dependency was removed. Observer logic and event mapping were
unchanged; builds and automated tests passed, and no new manual runtime
validation was performed.

The implementation is header-only through the existing `panebind_core`
interface target. No spatial index or external library was introduced.

## 8. Automated tests

Two CTest targets were added:

```text
topology
translation
```

The two new test sources contain 51 direct `expect` call sites, plus typed
relation/exception/change/plan checks and fixed deterministic generated loops:

- every permutation of five topology inputs (`120`);
- every permutation of four move-plan inputs (`24`);
- generated signed gap and whole-topology translation cases; and
- `187` repeated total-delta plans for drift checks.

Coverage includes all four opposing edges, exact/tolerance/outside cases,
corner-only, one-unit overlap, small intrusion, near-edge containment, deep and
multiple-contact overlap, zero-width/height rectangles, duplicate IDs,
negative/large/near-limit coordinates, chains, 2×2, L shapes, independent
components, isolation, classification cases, repeated plans, session snapshot
ownership, and all-or-nothing overflow.

Final Debug CTest result:

```text
1/5 geometry                 PASS
2/5 core-model               PASS
3/5 topology                 PASS
4/5 translation              PASS
5/5 windows-text-encoding    PASS
TOTAL = 5/5 PASS
```

## 9. Build verification

Environment:

- Visual Studio 18 2026;
- MSVC 19.50;
- Windows SDK 10.0.26100.0; and
- bundled CMake 4.2.3.

Commands:

```powershell
cmake -S . -B out/r1a-debug -G "Visual Studio 18 2026" -A x64 -DBUILD_TESTING=ON
cmake --build out/r1a-debug --config Debug --parallel
ctest --test-dir out/r1a-debug -C Debug --output-on-failure

cmake -S . -B out/r1a-release -G "Visual Studio 18 2026" -A x64 -DBUILD_TESTING=ON
cmake --build out/r1a-release --config Release --parallel
ctest --test-dir out/r1a-release -C Release --output-on-failure
```

An initial Debug build exposed one missing explicit event-header include after
the `WindowId` dependency cleanup. The consuming Windows Observer source now
includes that header directly. The final results are:

```text
DEBUG_BUILD = PASS
RELEASE_BUILD = PASS
DEBUG_AUTOMATED_TESTS = PASS (5/5)
RELEASE_AUTOMATED_TESTS = PASS (5/5)
```

## 10. Boundary and forbidden-API audit

- `src/core/` contains no `HWND`, `HMONITOR`, Win32 `RECT`/`POINT`,
  `windows.h`, WinEvent constant, monitor API, or DPI API.
- No `SetWindowPos`, `MoveWindow`, `BeginDeferWindowPos`, `DeferWindowPos`,
  `SendInput`, `mouse_event`, `keybd_event`, input hook, or product polling was
  added.
- R1-A did not manipulate Explorer, Excel, VS Code, or any third-party window.
- The existing observer remains read-only.
- No Snap, Glue behavior, Glue Resize, feedback suppression, platform
  operation, tray, settings, or UI was added.

```text
THIRD_PARTY_WINDOW_CONTROL = NO
DLL_INJECTION = NO
RESIDENT_HIGH_FREQUENCY_POLLING = NO
R1B = NOT STARTED
```

## 11. Remaining questions and risks

- Product tolerance and any future minimum-overlap policy lack validated
  defaults.
- Visible targets must eventually be converted to positioning operation
  rectangles by a separately researched adapter.
- Eligibility, occlusion, maximized/minimized state, elevation, and virtual
  desktop remain behavior-layer policy.
- Topology generation and session invalidation need a live behavior contract.
- Multi-monitor and mixed-DPI runtime behavior remain `NOT TESTED`; signed-core
  tests do not make those platform claims.
- Window destruction, application constraints, operation failure, feedback
  suppression, and atomic multi-window behavior remain unimplemented.
- A future contact-set model may be needed if product evidence requires
  preserving multiple simultaneous edge contacts.

## 12. R1-B boundary — research suggestion only

The suggested next research gate is a platform operations adapter plus a
PaneBind-owned window harness. It should first move only PaneBind-created test
windows and define operation result, DPI conversion, topology generation,
feedback suppression, and failure semantics. R1-A does not implement or start
that work.

## 13. Gate result

```text
R1A_PRIOR_ART_GATE = PASS
R1A_ALGORITHM_BASELINE = PASS
THIRD_PARTY_WINDOW_CONTROL = NO
R1B = NOT STARTED
```
