# PaneBind R1-A Adjacency, Topology, and Translation Research

Status: **R1-A PRIOR-ART GATE PASS**

Review date: 2026-08-25.

## Scope and evidence labels

This review authorizes only platform-neutral geometry, adjacency, topology,
classification, and pure translation planning. It does not authorize Snap,
Glue behavior, input hooks, a platform operations adapter, or control of any
third-party window.

- **FACT** means directly inspected in the pinned source, repository history,
  an issue/PR, or official Microsoft documentation.
- **INFERENCE** means a conclusion drawn from those facts.
- **PANEBIND DECISION** is an independently designed R1-A contract. No external
  implementation code was copied, adapted, translated, or mechanically
  rewritten.

## Sources and license boundaries

| Source | Classification | Immutable revision | License / use |
| --- | --- | --- | --- |
| [AltSnap](https://github.com/RamonUnch/AltSnap) | Mature active behavioral prior art | [`5c86416ad21e4b72844a998a746bd3bb0bee5f5d`](https://github.com/RamonUnch/AltSnap/commit/5c86416ad21e4b72844a998a746bd3bb0bee5f5d), `1.68-48-g5c86416` | GPL-3.0-or-later; **REFERENCE ONLY** |
| [AltDrag](https://github.com/stefansundin/altdrag) | Mature historical comparison | [`e2740d605b0336a3b391fec26794718864b19521`](https://github.com/stefansundin/altdrag/commit/e2740d605b0336a3b391fec26794718864b19521), `v1.1-8-ge2740d6` | GPL-3.0-or-later; **REFERENCE ONLY** |
| [PowerToys / FancyZones](https://github.com/microsoft/PowerToys) | Mature production reference | [`19c4d805321db86f3634e6968e14dbf25cbba14a`](https://github.com/microsoft/PowerToys/commit/19c4d805321db86f3634e6968e14dbf25cbba14a) | MIT; reference-only in R1-A; any future reuse requires separate approval and attribution |
| Microsoft Learn Win32 documentation | Authoritative platform contract | Live documentation reviewed 2026-08-25 | Facts paraphrased and cited |

## AltSnap and AltDrag targeted findings

### Candidate filtering and geometry source

**FACT.** AltSnap's `ShouldSnapTo` and candidate enumeration reject the active
window, invisible/cloaked/zero-size windows, minimized windows,
`WS_EX_NOACTIVATE`, and candidates without one of several window-style or list
qualifications. Maximized candidates are cropped to monitor work area. Edge
visibility is tracked after z-order occlusion cropping. See
[`ShouldSnapTo` and enumeration](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L678-L789),
[issue #681](https://github.com/RamonUnch/AltSnap/issues/681), and merged
[PR #682](https://github.com/RamonUnch/AltSnap/pull/682).

**FACT.** AltSnap distinguishes the `GetWindowRect` positioning rectangle from
`DWMWA_EXTENDED_FRAME_BOUNDS`. Snap candidates prefer the visible DWM frame and
placement later applies per-edge frame corrections. See
[`GetWindowRectLL` / `FixDWMRectLL`](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/unfuck.h#L1091-L1153).
AltDrag [issue #38](https://github.com/stefansundin/altdrag/issues/38) and
unmerged [PR #136](https://github.com/stefansundin/altdrag/pull/136), plus
AltSnap [issues #347](https://github.com/RamonUnch/AltSnap/issues/347) and
[#566](https://github.com/RamonUnch/AltSnap/issues/566), show why invisible
borders and gap policy cannot be treated as one rectangle.

**PANEBIND DECISION.** R1-A `WindowGeometry` contains a visible rectangle only.
The caller selects eligible windows. Positioning/operation geometry, styles,
cloaking, z-order, DPI, monitor identity, and application lists remain outside
the graph.

### Edge scan, tolerance, and ordering

**FACT.** AltDrag and AltSnap scan monitor and window rectangles, with monitors
first. X and Y thresholds shrink independently. AltSnap's
[`MoveSnap`](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L1052-L1157)
and
[`ResizeSnap`](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L1160-L1278)
compare both opposing and same-side edges for interactive snapping. The
perpendicular test expands ranges by the current threshold, so it admits
near-corner relationships without true overlap. After a match, the mutable
threshold receives a signed delta rather than an absolute distance. There is no
test oracle for ties, opposite-side candidates, or insertion-order invariance.

**INFERENCE.** This is useful failure-mode evidence, not a deterministic graph
contract. Monitor-first and enumeration-order effects, same-side alignment, and
mutable per-axis thresholds must not enter PaneBind topology.

### Sticky resize history

**FACT.** AltSnap sticky resize evolved through commits
[`1dd26c9`](https://github.com/RamonUnch/AltSnap/commit/1dd26c9058e8a9bc58cac2e4547421af425d1682),
[`5fdb078`](https://github.com/RamonUnch/AltSnap/commit/5fdb078e80ba0c2afaca38d38f3463eae6a7959e),
[`5fb8307`](https://github.com/RamonUnch/AltSnap/commit/5fb83076e44af319cee09071c8e2a15120808d85),
[`fa5c70a`](https://github.com/RamonUnch/AltSnap/commit/fa5c70a5029f418cac56007053fe67024cbeef86),
[`3555048`](https://github.com/RamonUnch/AltSnap/commit/3555048a8b06e0cc5d2a13f7d07f90bfd568a656),
and [`8b774a1`](https://github.com/RamonUnch/AltSnap/commit/8b774a1e70072df83d8a45fe5e5cbd59b06c016c).
The current
[`AreRectsTouchingT`](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/unfuck.h#L1692-L1716)
requires opposing edges within tolerance and an inclusive perpendicular segment
intersection. A diagonal corner-only configuration therefore produces multiple
direction bits.
`ResizeTouchingWindows` later accepts only a pure single direction. See
[`EnumTouchingWindows` and resize handling](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L839-L978).

**FACT.** The implementation only handles immediate neighbors. It is not a
recursive component solver or persistent topology graph. Open
[issue #620](https://github.com/RamonUnch/AltSnap/issues/620) discusses recursive
sticky resize, [issue #507](https://github.com/RamonUnch/AltSnap/issues/507)
requests moving adjacent groups, and AltDrag
[issue #32](https://github.com/stefansundin/altdrag/issues/32) remains a feature
request. None of the pinned sources contains the graph required by PaneBind.

**PANEBIND DECISION.** Corner-only contact is not adjacency. PaneBind uses a
strictly positive perpendicular overlap, not inclusive endpoint intersection.
R1-A independently implements an undirected graph and connected components; it
does not derive them from GPL code.

### DPI and monitor history

**FACT.** AltSnap [issue #413](https://github.com/RamonUnch/AltSnap/issues/413)
remains open despite merged [PR #415](https://github.com/RamonUnch/AltSnap/pull/415).
AltDrag also retains unresolved visible-frame and DPI proposals. This history
does not establish a cross-monitor/mixed-DPI unit contract.

**PANEBIND DECISION.** Core accepts signed coordinates already normalized into
one opaque semantic space by its caller. Negative/large-coordinate tests prove
arithmetic portability only; they do not claim multi-monitor or mixed-DPI
validation.

## FancyZones targeted findings

### Work areas, identity, and topology lifetime

**FACT.** FancyZones `WorkAreaConfiguration` owns Windows monitor-keyed work
areas. Each `WorkArea` combines monitor/virtual-desktop identity, an immutable
work-area rectangle, layout, assigned windows, overlay UI, and operations. This
is a production Windows boundary, not a platform-neutral core template. See
[`WorkAreaConfiguration.cpp`](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/WorkAreaConfiguration.cpp#L6-L66)
and
[`WorkArea.h`](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/WorkArea.h#L11-L84).

**FACT.** `WorkArea::Snap` obtains a visual target from
`Layout::GetCombinedZonesRect`, then `AdjustRectForSizeWindowToRect` uses
`GetWindowRect` and `DWMWA_EXTENDED_FRAME_BOUNDS` margins to derive a positioning
rectangle before any placement call. See
[`WorkArea.cpp`](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/WorkArea.cpp#L127-L153),
[`Layout.cpp`](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/Layout.cpp#L317-L344),
and
[`WindowUtils.cpp`](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/WindowUtils.cpp#L364-L400).
This is further production evidence that a visual target and an operation
rectangle are distinct contracts.

**FACT.** Display/work-area changes rebuild topology after comparing native
handles, device identity, serial, virtual desktop, and work-area geometry.
Merged [PR #48473](https://github.com/microsoft/PowerToys/pull/48473) fixed a
dangling `WorkArea*` during replacement. Merged
[PR #49985](https://github.com/microsoft/PowerToys/pull/49985) later changed
replacement from gesture finalization to abort. Open
[issue #49016](https://github.com/microsoft/PowerToys/issues/49016) documents
remaining identity/hash/stale-entry risks.

**PANEBIND DECISION.** R1-A graph construction consumes one immutable value
snapshot. It has no native monitor token, virtual-desktop ID, global cache, or
Windows eligibility rule. Future live sessions must be invalidated as a whole
when a behavior-layer topology generation changes.

The targeted production-history chain inspected was:

| Change | Exact commit | R1-A lesson |
| --- | --- | --- |
| [PR #48473](https://github.com/microsoft/PowerToys/pull/48473) | [`ae9f241ef13737dab6f861767bbfdfca72b78475`](https://github.com/microsoft/PowerToys/commit/ae9f241ef13737dab6f861767bbfdfca72b78475) | End/cancel consumers before topology-owned objects are recycled. |
| [PR #49985](https://github.com/microsoft/PowerToys/pull/49985) | [`d68980a81bb8de144bdec998a114e948bf68c563`](https://github.com/microsoft/PowerToys/commit/d68980a81bb8de144bdec998a114e948bf68c563) | Topology replacement must abort rather than finalize a stale gesture. |
| [PR #49433](https://github.com/microsoft/PowerToys/pull/49433) | [`37d8729ac3eec734f4d000079145d6fcb40db3a5`](https://github.com/microsoft/PowerToys/commit/37d8729ac3eec734f4d000079145d6fcb40db3a5) | Reload scalar policy from one canonical source; do not mix generations. |
| [PR #28556](https://github.com/microsoft/PowerToys/pull/28556) | [`890b7f4286a95ced04d7da140b474f90fd4351ed`](https://github.com/microsoft/PowerToys/commit/890b7f4286a95ced04d7da140b474f90fd4351ed) | Reuse the active work-area identity/rectangle snapshot instead of re-enumerating a competing view. |
| [PR #44440](https://github.com/microsoft/PowerToys/pull/44440) | [`6c2a99dfd6a12ad98feeda0acbc663aa84865676`](https://github.com/microsoft/PowerToys/commit/6c2a99dfd6a12ad98feeda0acbc663aa84865676) | Coordinate-space metadata and interpretation must be consistent. |

Monitor identity was also reviewed across commits
[`0ab0fb5`](https://github.com/microsoft/PowerToys/commit/0ab0fb5dd46acfd4f80aadd696197cd1998e675a),
[`35bb428`](https://github.com/microsoft/PowerToys/commit/35bb4280d0bbaf91fd649329a911240ed0522117),
[`73c2593`](https://github.com/microsoft/PowerToys/commit/73c259342b340d78d4a702213ddbfebd1208af60),
and [`8dcdcba`](https://github.com/microsoft/PowerToys/commit/8dcdcbaa37a16cdb6c4b96f757a26f3a37f72962).
The repeated corrections reinforce that native monitor identity is adapter
state, not a field for the R1-A adjacency graph.

### Zone overlap and sensitivity are not adjacency

**FACT.** `Layout::ZonesFromPoint` expands zone rectangles by
`sensitivityRadius` for cursor hit-testing, separately tracks strict half-open
containment, and selects among overlapping zones using a user-configured
algorithm. See
[`Layout.cpp`](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/Layout.cpp#L186-L270).
This radius is cursor-to-zone UX policy, not a window-edge gap. FancyZones does
not implement an edge-pair relation or window adjacency graph. Near-boundary
multi-zone capture and positive-area overlapping-zone selection are separate
branches; neither produces signed edge gaps or orthogonal-overlap relations,
and neither defines corner-only window adjacency.

**FACT.** Merged [PR #49433](https://github.com/microsoft/PowerToys/pull/49433)
fixed existing work areas that refreshed custom layout shape but retained stale
spacing, sensitivity, and count from another snapshot. The regression test
explicitly compares adjacent zero-spacing zones before applying edited spacing.

**PANEBIND DECISION.** Do not reuse `sensitivityRadius`, fixed center
sensitivity, or zone overlap selection. `edge_tolerance` is an explicit R1-A
input with no product default. Graph/session calculations use one canonical
geometry snapshot and no mutable global policy.

### DPI and coordinate-space production failures

**FACT.** Merged [PR #44440](https://github.com/microsoft/PowerToys/pull/44440)
fixed an editor overlay failure caused by mixing DPI-virtualized and PMv2
coordinate interpretations. The
[FancyZones product documentation](https://learn.microsoft.com/en-us/windows/powertoys/fancyzones)
still records mixed-DPI limitations and possible gaps for DPI-unaware targets.

**PANEBIND DECISION.** R1-A never infers DPI, physical pixels, work area, or
monitor identity. These remain adapter/behavior responsibilities and
`NOT TESTED` product risks.

### Eligibility remains caller policy

**FACT.** `FancyZonesWindowProcessing::DefineWindowType` produces a
reason-bearing Windows classification for minimized, invisible, tool, non-root,
popup, owned/child, excluded-application, and virtual-desktop cases. Automatic
placement has additional policy beyond manual placement. See
[`FancyZonesWindowProcessing.cpp`](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/FancyZonesWindowProcessing.cpp#L9-L92)
and the pinned
[`WindowProcessingTests.Spec.cpp`](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesTests/UnitTests/WindowProcessingTests.Spec.cpp).

**PANEBIND DECISION.** R1-A receives a caller-prefiltered `WindowGeometry`
collection. No Windows eligibility state or policy is encoded in core.

## Official Windows geometry constraints

Microsoft documents that
[`GetWindowRect`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowrect)
uses exclusive right/bottom coordinates, is DPI-virtualized, and may include
invisible resize borders. It directs callers to `DWMWA_EXTENDED_FRAME_BOUNDS`
for visible bounds, which are not DPI-adjusted. `MONITORINFO` documents that
monitor and work-area rectangles use virtual-screen coordinates and that
non-primary coordinates may be negative.

**PANEBIND DECISION.** R1-A uses normalized half-open visible rectangles from
the caller. Positioning rectangles and platform conversion never enter the
adjacency relation or pure move plan.

## PaneBind adjacency mathematical contract

For two distinct, non-empty visible rectangles, only opposing edge pairs are
candidates:

```text
first.Right <-> second.Left
first.Left  <-> second.Right
first.Bottom <-> second.Top
first.Top    <-> second.Bottom
```

For a candidate, `signed_gap` is oriented so that:

```text
signed_gap > 0  visible space between edges
signed_gap = 0  exact touch
signed_gap < 0  shallow edge intrusion
```

After `first` and `second` are canonicalized by `WindowId`, the four formulas
are:

```text
first.Right  <-> second.Left   second.left  - first.right
first.Left   <-> second.Right  first.left   - second.right
first.Bottom <-> second.Top    second.top   - first.bottom
first.Top    <-> second.Bottom first.top    - second.bottom
```

The candidate is valid only when:

```text
abs(signed_gap) <= explicit edge_tolerance
orthogonal_overlap > 0
```

Tolerance applies only along the candidate edge normal. It does not inflate the
perpendicular intervals. `edge_tolerance` must be nonnegative; a negative value
is an invalid argument rather than a hidden “disabled” mode. Consequences:

| Geometry | R1-A result |
| --- | --- |
| Exact side touch and positive overlap | adjacent |
| Small positive gap within tolerance and positive overlap | adjacent; positive gap retained |
| Small intrusion within tolerance and positive overlap | adjacent; negative gap retained |
| Near-edge containment with exactly one qualifying opposing contact | adjacent as shallow intrusion |
| Gap or intrusion beyond tolerance | not adjacent |
| Corner-only contact | not adjacent |
| Perpendicular near-touch without real overlap | not adjacent |
| Empty rectangle | retained as an isolated node; no relation |
| Deep containment/significant overlap with no near opposing edge | not adjacent |

If more than one opposing contact is valid for the same pair, whether on two
axes or caused by an unusually large tolerance around tiny/contained geometry,
the pair is ambiguous and R1-A rejects it rather than silently choosing an axis.
This preserves one relation per unordered window pair and avoids hiding
information behind enumeration order. A future evidence-backed model may
represent a contact set, but R1-A does not need that complexity.

No `minimum_overlap` option is added. Strict positive overlap is the mathematical
baseline, and the relation reports its exact length. A product-level minimum has
no validated default and remains a later policy question.

## Graph and deterministic identity contract

- `WindowId` is unique within one input snapshot; duplicates are rejected.
- Nodes and relations are canonicalized by `WindowId::value()`.
- The graph is undirected, has no self edge, and has at most one edge per
  unordered pair.
- Pair scanning is `O(n^2)`, appropriate for desktop-scale inputs.
- Connected components include transitive neighbors and return IDs in canonical
  order; an isolated node returns itself; an unknown start ID is an error.
- Input permutation cannot change nodes, relations, or component results.
- Caller-side Windows eligibility does not enter the graph.

## Geometry change and translation contract

```text
Unchanged     all four bounds equal
Translation   width and height unchanged; position changed; dx/dy available
ResizeOrMixed width or height changed, including maximize/drag-restore cases
```

R1-A does not infer resize direction. A translation must reproduce all four
target edges from the original rectangle plus one `dx/dy`. If a mathematically
valid translation delta is not representable as `Distance`, classification
fails with an arithmetic error; it never reports `Translation` without a delta.

## Pure move-plan contract

A translation session snapshots the leader and its initial connected component.
Planning emits follower `target_visible_rect` values only; it never calls a
platform API. Every plan is computed from immutable session initial rectangles
and the leader's total translation:

```text
target_follower(t) = initial_follower +
                     (current_leader(t) - initial_leader)
```

Follower previous positions and incremental event deltas are not inputs. This
prevents accumulated rounding, event-loss, and feedback drift. Unchanged leader
geometry yields unchanged follower targets. `ResizeOrMixed` yields no move plan.

## Arithmetic boundary

Coordinates and distances use signed 64-bit core types. Existing R0 contracts
already exclude spans that cannot be represented as `Distance`. R1-A adds
checked subtraction/addition for gaps and translation: far-apart unrepresentable
gaps are outside tolerance, while an unrepresentable translation fails the
whole plan rather than returning partial targets. Tests cover large representable
positive/negative coordinates and near-limit overflow cases. Implementation
must not directly subtract unrepresentable coordinates or apply `abs` to
`INT64_MIN`; magnitude/tolerance checks are overflow-safe.

## Rejected designs

- AltSnap/AltDrag mutable scan thresholds, enumeration order, or same-side snap
  alignments as graph truth.
- GPL sticky-resize implementation structure or bit flags.
- FancyZones cursor sensitivity as adjacency tolerance.
- `rectangles intersect` or `distance is small` as adjacency.
- Corner-only contact or tolerance-expanded perpendicular intervals.
- A hidden/default tolerance or product pixel/DPI conversion policy.
- Hard-coded Windows eligibility in core.
- Persistent/global topology caches or partial refresh.
- Incremental follower updates, platform operations, spatial indexes, or R1-B
  harness work.

## Research gate

```text
R1A_PRIOR_ART_GATE = PASS
ALTSNAP_ALTDRAG_LICENSE = GPL REFERENCE ONLY
POWERTOYS_LICENSE = MIT REFERENCE ONLY FOR R1-A
EXTERNAL_CODE_COPIED = NO
EXTERNAL_CODE_ADAPTED = NO
THIRD_PARTY_WINDOW_CONTROL_AUTHORIZED = NO
R1B = NOT STARTED
```
