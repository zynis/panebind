# R1-C4 Magnet, Geometry, Relation Graph and Glue

Original design, 2026-09-15. Supersedes fixed-L C4A fixture requirements.

```text
raw Move/Resize -> actual raw geometry -> Magnet proposal -> native apply
 -> exact actual postverify -> Relation Graph rebuild -> Glue behavior
```

Magnet is a constraint, geometry is observed truth, Relation Graph is derived
adjacency, Glue is gesture-scoped behavior. MAGNET_CONSTRAINT != GLUE_RELATION.
Same-side alignment is not adjacency. Reuse R1-A: opposing-edge gap within
tolerance AND positive orthogonal overlap. No equal-size requirement; point
contact has no relation. Geometry detachment rebuilds the graph, without
Ungroup/DeleteRelation/persistent member operations.

PERSISTENT_LEADER = NONE; PERSISTENT_GROUP_MEMBERSHIP = NONE. Group means the
current connected component. C4A's stable three-frame authorized set bounds
capabilities, not persistent geometric membership. Roles exist only within a
gesture. Existing GroupReady/coordinator, HDWP and feedback stay unchanged.

## C4A acceptance

Bind without moving windows. Human MOVEs/RESIZEs A/B/C into any connected
three-member topology. Enter captures a fresh preview of all pair relations,
orientation/gap/overlap and components. Two or three relations are legal,
with unequal dimensions. Y accepts via fresh complete validation, rebases
restore geometry and only then starts the event source. Pre-accept native
writes, HDWP, roles, pending and gesture generation are zero. No synthetic
layout. Monitor/DPI/desktop/frame/anchor/nonce/security remain validated.
Gestures A, B, C retain authority and restore the accepted, not binding, baseline.

## Pure Magnet — PANE_BIND_DESIGN_INFERENCE

One solve returns at most ONE combined rectangle and Move delta or Resize-edge
adjustment. No capability, native operation, relation persistence or Glue action.
Move preserves extents. Resize initial/raw inference permits at most one
changed edge per axis. No changes, opposing-edge changes, unrepresentable
geometry or a mismatched supplied edge set fail closed. Opposite edges stay
fixed, including B.top alignment while B.bottom touches C.

Kinds: ScreenOuterEdge, WindowOuterEdge, WindowInnerEdge,
AdjacentCornerAlignment, ParallelEdgeAlignment. Enumerate each participating
edge against same-axis target edges. Filter attraction, relative geometry and
predicted overlap; recheck selected predicates against the single combined
result. Same-axis ranking: smaller absolute delta, larger predicted overlap,
kind priority, stable logical WindowId, then edge identity. No native handle
or arrival/container order. Retain all satisfied compatible constraints.

Bounded eligible snapshot (default 64, hard cap 256), O(N) candidate evaluation;
two selection passes plus a final ranking-stability check use final predicted
overlap. An oscillating/incompatible pair abstains rather than searching
iteratively. Diagnostic satisfied-constraint output is sorted O(N log N) for
permutation-independent evidence; no spatial index/global
inventory. Future START caches identity/visible bounds/monitor/DPI eligibility;
events invalidate stale targets. Core cannot establish target authority.
Incompatible/invalid combined proposals fail closed, never sequential native
X/Y writes. Only exact actual postverify may feed the Relation Graph.

Default attraction 10 normalized geometry units is REFERENCE BASELINE, not
optimal. Current Windows inputs use physical desktop pixels under PMv2;
all coordinates/thresholds share one authority. Binary fast suppression uses
supplied event ticks/frequency and maximum edge delta, with caller-supplied
threshold. Invalid elapsed time fails closed. No timer/cursor polling.
Hysteresis deferred; if added release > enter, numerical values require tests
and UAT, and are not an upstream contract.

LOW_RESIDENT_OVERHEAD: events, bounded queues, meaningful coalescing, latest
geometry; not every raw event or hot-path inventory. No measured idle/runtime
smoothness claim. Live Magnet NOT_IMPLEMENTED; independent live gate required.

## Tests defined before implementation

A–E: unequal A/B and A/C, two/three-edge connected graphs, point-only corner.
F–H: B move toward A only, C only, both. I–K: B.bottom to C.top; B.top to A.top
with bottom fixed; C.right shrink to zero overlap drops B/C. L–P: inner/outer/
corner, same-axis deterministic conflict, one combined XY result. Q–R: ambiguous
resize, fast suppression. S–T: pre-accept writes zero, three-relation READY.
Also overflow/bounds/duplicates, input permutation, all satisfied constraints,
fresh accepted baseline, stage-classified BLOCKED and strict PASS negatives.
Full Debug/Release + owned/companion + legacy/C4A runners required.

## Deferred work

Smart Docking and AquaStretch UI are separate features. C4C Glue Resize moves
shared boundaries (A.right/B.left together) according to graph shared edges,
not whole-component scaling; NOT STARTED. Live self-feedback, recursion,
eligibility and target invalidation require an independent integration gate.
[Relation affordance](R1C4D_RELATION_AFFORDANCE.md) is original UX only.
