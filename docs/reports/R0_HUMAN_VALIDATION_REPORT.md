# PaneBind R0 Human Validation Report

Initial validation date: 2026-08-24 (Asia/Shanghai).
UAT 07 revalidation and seal date: 2026-08-25 (Asia/Shanghai).
Initial evaluated source HEAD: `b50a1c9b2d94996f10b4d700e0c9d71f057e140f`.
Final revalidation source HEAD: `06ac97248d5849970d01e235037a1aab6778ff15`.
Branch: `codex/r0-prior-art-event-geometry`.
Decision: **R0_BASELINE = SEALED**.
Minimum rerun: **NONE**.

## 1. Scope and evidence handling

This report analyzes ten local JSONL observer logs under `uat/`, reviews the R0
observer against those records, and records the R0 seal decision. It does not
start R1 and does not authorize window control, Snap, Glue, input behavior, or
any other product feature.

The raw logs remain ignored and untracked. No raw JSON line, full window title,
or user file path is reproduced here. Window titles are treated as
`[redacted]`. Process names, window classes, native session identifiers, event
counts, geometry, monitor identity, and DPI are retained only where needed to
support the findings.

The logs do not embed a Git SHA. The evaluated SHAs above identify the clean
source trees reviewed across the initial and final validation passes; Observer
implementation files did not change between them. The available
`panebind-observer.exe` predates the logs, but that is source/timestamp
correlation rather than a cryptographic build-identity assertion. A future
diagnostic build identifier is a research question, not a reason to relabel the
observed records.

## 2. Method

Each file was divided into initial diagnostics/census, background receipts, the
target interaction cluster, and shutdown. Target attribution used the expected
process, stable root HWND/PID, normalized START/LOCATION/END records, geometry,
state transitions, sequence numbers, and temporal clustering. Sensitive titles
were not used to choose among same-process windows.

In the tables:

- `Target raw A/R` is the target HWND's raw WinEvent count split into accepted
  and structurally rejected records during the identified interaction.
- `File A/R` is the full file's accepted/rejected WinEvent count; census and
  diagnostic rows are excluded.
- Rectangles use `[left, top, right, bottom)` and preserve the distinction
  between positioning bounds and DWM visible-frame bounds.
- Results use only `MANUALLY OBSERVED`, `NOT OBSERVED`, `AMBIGUOUS`,
  `RETEST_REQUIRED`, or `BLOCKED`.

## 3. JSONL integrity and observer lifecycle

| ID | Bytes | JSON lines | Sequence | Startup/census | Shutdown | Queue/post diagnostic | Parse result |
| --- | ---: | ---: | --- | --- | --- | --- | --- |
| 01 | 1,278,797 | 1,172 | `1..1172`, continuous | complete | complete | none | PASS |
| 02 | 1,482,060 | 1,478 | `1..1478`, continuous | complete | complete | none | PASS |
| 03 | 1,851,107 | 1,566 | `1..1566`, continuous | complete | complete | none | PASS |
| 04 | 1,229,792 | 1,043 | `1..1043`, continuous | complete | complete | none | PASS |
| 05 | 1,297,075 | 1,130 | `1..1130`, continuous | complete | complete | none | PASS |
| 06 | 1,142,509 | 1,054 | `1..1054`, continuous | complete | complete | none | PASS |
| 07 | 1,306,247 | 1,314 | `1..1314`, continuous | complete | complete | none | PASS |
| 08 | 1,069,835 | 943 | `1..943`, continuous | complete | complete | none | PASS |
| 09 | 1,465,281 | 1,307 | `1..1307`, continuous | complete | complete | none | PASS |
| 10 | 1,685,369 | 1,766 | `1..1766`, continuous | complete | complete | none | PASS |

Every line in all ten files parsed as schema version 1 JSON. Every file starts
with `thread_dpi_context=verified_per_monitor_v2`, contains
`hook_registration=complete` and `initial_census=complete`, ends with
`hook_shutdown=complete` followed by `observer_shutdown=complete`, has a final
newline, and has no duplicate, decreasing, or missing observer sequence.

No file contains `event_queue_overflow`, `events_dropped`,
`queue_notification_failure`, an incomplete observer diagnostic, or an obvious
truncation. Files other than 02 and the revalidated 07 contain five nonfatal
initial-census `OpenProcess` access-denied errors for non-target windows. File
02 additionally contains four explicitly rejected identity-race errors for
transient non-target HWNDs. Revalidated file 07 contains six non-target
`OpenProcess/process_path` errors and one background identity-race error. Target
interaction snapshots have no field errors.

## 4. Test matrix: identity, counts, ordering, and result

| ID | Application / operation | Target process; HWND; PID | START / LOCATION / END | Target raw A/R | File A/R | First to last accepted UTC | Observed ordering | Result |
| --- | --- | --- | ---: | ---: | ---: | --- | --- | --- |
| 01 | Explorer native move | `explorer.exe`; `0x00000000002210d0`; `29120` | `1 / 46 / 1` | `48 / 0` | `48 / 449` | `12:16:38.809378Z` to `12:16:39.416837Z` | START → LOCATION ×46 → END | `MANUALLY OBSERVED` |
| 02 | Explorer native resize | `explorer.exe`; `0x00000000002210d0`; `29120` | `1 / 7 / 1` | `9 / 0` | `49 / 753` | `12:20:18.013949Z` to `12:20:19.134976Z` | START → LOCATION ×7 → END | `MANUALLY OBSERVED` |
| 03 | VS Code native move | `Code.exe`; `0x00000000007616c4`; `3020` | `1 / 271 / 1` | `273 / 244` | `273 / 617` | `12:18:59.393169Z` to `12:19:02.349211Z` | START → LOCATION ×271 → END | `MANUALLY OBSERVED` |
| 04 | VS Code native resize | `Code.exe`; `0x00000000007616c4`; `3020` | `1 / 74 / 1` | `76 / 5` | `76 / 291` | `12:21:38.361934Z` to `12:21:40.272512Z` | START → LOCATION ×74 → END | `MANUALLY OBSERVED` |
| 05 | Excel native move | `EXCEL.EXE`; `0x0000000000101392`; `10496` | `1 / 50 / 1` | `52 / 0` | `52 / 402` | `12:22:52.363776Z` to `12:22:53.139810Z` | START → LOCATION ×50 → END | `MANUALLY OBSERVED` |
| 06 | Excel native resize | `EXCEL.EXE`; `0x0000000000101392`; `10496` | `1 / 5 / 1` | `7 / 0` | `7 / 369` | `12:23:34.232740Z` to `12:23:34.995561Z` | START → LOCATION ×5 → END | `MANUALLY OBSERVED` |
| 07 | Explorer maximize/restore | `explorer.exe`; `0x00000000002210d0`; `29120` | `0 / 2 / 0` | `2 / 0` | `5 / 629` | `23:55:24.127466Z` to `23:55:27.546446Z` | LOCATION(max) → LOCATION(restore) | `MANUALLY OBSERVED` |
| 08 | VS Code maximize/restore | `Code.exe`; `0x00000000007616c4`; `3020` | `0 / 3 / 0` | `3 / 4` | `6 / 261` | `12:28:51.284095Z` to `12:28:54.701003Z` | LOCATION(max) → LOCATION(restore) ×2 | `MANUALLY OBSERVED` |
| 09 | Explorer AltSnap move | `explorer.exe`; `0x00000000002210d0`; `29120` | `1 / 24 / 1` | `26 / 0` | `26 / 603` | `12:31:04.412521Z` to `12:31:05.316762Z` | START → LOCATION ×24 → END | `MANUALLY OBSERVED` |
| 10 | Explorer AltSnap resize | `explorer.exe`; `0x00000000002210d0`; `29120` | `1 / 22 / 1` | `24 / 0` | `24 / 1,064` | `12:31:40.384725Z` to `12:31:42.481496Z` | START → LOCATION ×22 → END | `MANUALLY OBSERVED` |

The 244 rejected target-HWND records in 03, five in 04, and four in 08 are
non-window object/child `LOCATIONCHANGE` receipts. They were explicitly logged
and correctly rejected before normalization. All observed target sessions keep
the same HWND, PID, monitor, and DPI from first through last accepted record.

## 5. Test matrix: geometry, monitor, and stability

| ID | Initial → final positioning bounds | Initial → final visible-frame bounds | Monitor / DPI | HWND stable | PID stable |
| --- | --- | --- | --- | --- | --- |
| 01 | `[330,265,1888,1188)` → `[1240,216,2798,1139)` | `[341,265,1877,1177)` → `[1251,216,2787,1128)` | `DISPLAY1 / 192` | YES | YES |
| 02 | `[664,559,2222,1482)` → `[664,559,1812,1232)` | `[675,559,2211,1471)` → `[675,559,1801,1221)` | `DISPLAY1 / 192` | YES | YES |
| 03 | `[-13,-13,3085,1837)` → `[327,112,3231,1924)` | `[0,0,3072,1824)` → `[338,112,3220,1913)` | `DISPLAY1 / 192` | YES | YES |
| 04 | `[24,14,2928,1826)` → `[24,14,1939,1285)` | `[35,14,2917,1815)` → `[35,14,1928,1274)` | `DISPLAY1 / 192` | YES | YES |
| 05 | `[512,246,2816,1577)` → `[212,288,2516,1619)` | `[523,246,2805,1566)` → `[223,288,2505,1608)` | `DISPLAY1 / 192` | YES | YES |
| 06 | `[212,288,2516,1619)` → `[212,288,2777,1798)` | `[223,288,2505,1608)` → `[223,288,2766,1787)` | `DISPLAY1 / 192` | YES | YES |
| 07 | `[955,660,2816,1745)` → same after restore | `[966,660,2805,1734)` → same after restore | `DISPLAY1 / 192` | YES | YES |
| 08 | `[24,14,1939,1285)` → same after restore | `[35,14,1928,1274)` → same after restore | `DISPLAY1 / 192` | YES | YES |
| 09 | `[664,559,1812,1232)` → `[1668,660,2816,1333)` | `[675,559,1801,1221)` → `[1679,660,2805,1322)` | `DISPLAY1 / 192` | YES | YES |
| 10 | `[1668,660,2816,1333)` → `[955,660,2816,1745)` | `[1679,660,2805,1322)` → `[966,660,2805,1734)` | `DISPLAY1 / 192` | YES | YES |

All rows report **Queue overflow: NO** and **Dropped event: NO**.

## 6. Per-test interpretation

### 01 — Explorer native move

The single START is first, the single END is last, and all 46 accepted location
records are between them. Positioning size remains `1558×923`; visible-frame
size remains `1536×912`. The change is a translation of `(+910,-49)`.

### 02 — Explorer native resize

The file also contains an earlier complete Explorer move session at sequences
`930..1050`: `1 START + 38 LOCATION + 1 END`, with constant `1558×923`
positioning size. The intended resize is the later, unambiguous session at
`1206..1401`. Its left/top edges stay fixed while positioning size changes from
`1558×923` to `1148×673`. The two sessions are not combined in the counts above.

### 03 — VS Code native move

START captures the window maximized. The first location change restores it to
normal, after which the 271 location records retain constant `2904×1812`
positioning size. This is drag-to-restore followed by move. Comparing only the
START and END sizes would incorrectly call it resize.

### 04 — VS Code native resize

Left/top remain fixed; right/bottom contract. All 74 accepted location records
have different sizes. The session is an unambiguous resize characteristic.

### 05 — Excel native move

Positioning size remains `2304×1331` while the window translates `(-300,+42)`.
The 50 accepted location records contain 47 unique rectangles and three
consecutive duplicates; duplication does not break the lifecycle.

### 06 — Excel native resize

Left/top remain fixed while right/bottom expand by `(+261,+179)`. All five
accepted location rectangles are unique.

### 07 — Explorer maximize/restore

The revalidated target is an `explorer.exe` `CabinetWClass` root window with
stable HWND `0x00000000002210d0` and PID `29120`. The initial census records
`maximized=false`, positioning bounds `[955,660,2816,1745)`, and visible-frame
bounds `[966,660,2805,1734)`.

Maximize produces one accepted `GeometryChanged` record with
`maximized=true`, positioning bounds `[-13,-13,3085,1837)`, and visible bounds
`[0,0,3072,1824)`. Restore produces one accepted `GeometryChanged` record with
`maximized=false` and returns exactly to the initial positioning and visible
bounds. The two target records contain no field errors; target HWND, PID,
monitor, and DPI remain stable. No START or END occurs. The file also contains
three accepted background-window geometry records and 629 reasoned rejections,
none of which changes the target lifecycle.

### 08 — VS Code maximize/restore

The census records the normal initial bounds. Maximize produces one accepted
location record with `maximized=true`, positioning bounds
`[-13,-13,3085,1837)`, and visible bounds `[0,0,3072,1824)`. Restore produces
two identical accepted location records with `maximized=false` and the original
normal bounds. No START or END occurs.

### 09 — Explorer AltSnap move

The lifecycle is complete and balanced, the target identity is stable, and the
window translates `(+1004,+101)` with constant size. Relative to test 01, the
location count differs (`24` versus `46`) but the normalized lifecycle and
geometry characteristic are equivalent.

```text
ALTSNAP_MOVE_COMPATIBILITY = EQUIVALENT
```

### 10 — Explorer AltSnap resize

The lifecycle is complete and balanced. Right/top remain fixed; left expands by
`-713` and bottom by `+412`, changing positioning size from `1148×673` to
`1861×1085`. Relative to the resize-characteristic second session in test 02,
the normalized lifecycle and geometry characteristic are equivalent.

```text
ALTSNAP_RESIZE_COMPATIBILITY = EQUIVALENT
```

## 7. Cross-test event-model conclusions

### Native move lifecycle

Explorer, VS Code/Electron, and Excel each produce:

```text
MoveResizeStarted
-> GeometryChanged × N
-> MoveResizeEnded
```

Counts vary materially by application and gesture: Explorer 46, VS Code 271,
and Excel 50 accepted location records. The lifecycle is consistent; event rate
is not.

### Native resize lifecycle

Explorer, VS Code, and Excel produce the same lifecycle, with respectively 7,
74, and 5 accepted location records in the identified resize sessions.

### Move versus resize

Both native operations use the same three native and normalized event types.
Geometry in this controlled matrix describes whether the observed result is
move- or resize-characteristic, but R0 does not add a classification heuristic.
The VS Code drag-to-restore case demonstrates why first/last size deltas alone
are insufficient.

```text
CURRENT EVENT MODEL CANNOT INTRINSICALLY DISTINGUISH MOVE FROM RESIZE
NOT DISTINGUISHABLE BY EVENT TYPE
```

### Maximize and restore

The verified Explorer and VS Code sequences use location/state changes without
START or END. Maximize/restore is therefore not modeled as an interactive
move/resize session.

## 8. Filtering and noise

Across all ten files, the observer received 6,004 raw WinEvents:

- 5,986 raw `EVENT_OBJECT_LOCATIONCHANGE` receipts;
- 548 accepted root-window `GeometryChanged` records;
- 9 accepted `MoveResizeStarted` and 9 accepted `MoveResizeEnded` records;
- 4,039 rejected object/child-ID mismatches;
- 1,034 rejected child/non-root or destroyed-at-receipt HWNDs;
- 299 rejected invisible windows;
- 58 rejected tool windows without `WS_EX_APPWINDOW`;
- 3 rejected cloaked windows; and
- 5 explicitly diagnosed transient identity races.

Overall, 566 of 6,004 raw WinEvents (`9.43%`) were accepted and 5,438
(`90.57%`) were rejected with an explicit reason. For location changes alone,
548 of 5,986 (`9.15%`) were accepted as normalized root-window geometry.

`WINEVENT_SKIPOWNPROCESS` prevents own-process callbacks at registration; no
`own_process` event record was emitted. Target root windows for all ten tests
were accepted with reasoned noise rejection. No known manageable top-level
target was rejected, so the R0 filter is assessed as **reasonable for this
matrix**, not as a permanent product eligibility rule.

## 9. Queue and backpressure

The Observer queue capacity is 4,096 receipts. Full queues record dropped count
and first/last dropped sequence; notification failures have a separate
diagnostic. An orderly shutdown unhooks first and performs a final drain. START
or END is not specially prioritized under overflow, but a complete log cannot
silently claim an intact trace after such loss.

This batch has continuous observer sequences, no overflow/drop/post diagnostic,
and complete final shutdown in every file. Maximum observed concentration was
13 callbacks sharing one native millisecond in file 10. Snapshot delay for
records with a snapshot averaged approximately `0.433–1.250 ms` by file; the
maximum was `6.958 ms`. No burst backlog or lost critical event was observed.

## 10. Hook lifecycle and reentrancy

The owner thread creates its message queue, installs both out-of-context hooks,
runs the message loop, unhooks both handles on the owner thread, drains queued
receipts, and then emits `observer_shutdown`. All ten logs show the corresponding
complete diagnostics.

Callback state, sequence allocation, queue counters, and notification state are
atomic/mutex protected. The callback does not call accessibility/COM,
`SendMessage`, or a nested message loop. Controlled callback-reentrancy stress
is still **NOT TESTED**. PID/thread/root queries currently precede assignment of
the receive sequence and timestamp; if a future controlled test demonstrates a
reentrant path during those calls, causal entry ordering would need a focused
R0 correction and revalidation. The present logs are strictly monotonic but are
not generalized into a proof for every reentrant path.

## 11. Geometry findings

`GetWindowRect` positioning bounds and
`DWMWA_EXTENDED_FRAME_BOUNDS` visible-frame bounds are captured, serialized,
and normalized independently. Target records contain both sources with no
geometry field errors. Normal windows commonly show an 11-pixel left/right/
bottom positioning overhang at the observed DPI. Maximized records show
positioning bounds `[-13,-13,3085,1837)` while visible bounds match the
`[0,0,3072,1824)` work area. Neither source is substituted for the other.

All observed targets stayed on one 3072×1920 display with work area
3072×1824 and window DPI 192. These records support same-display consistency;
they do not establish mixed-DPI or cross-monitor congruence.

## 12. Architecture and prohibited-behavior audit

- `src/core/` remains free of Win32 types, headers, and event constants.
- Third-party window control: **NO**.
- DLL injection or remote-process memory/thread behavior: **NO**.
- Product input hooks or input synthesis: **NO**.
- Resident high-frequency polling: **NO**. The observation-duration timer is a
  one-shot stop mechanism; the owner loop blocks on messages.
- Snap: **NO**.
- Glue: **NO**.

## 13. Automated regression

The clean evaluated source tree was configured and rebuilt with Visual Studio
18 2026, MSVC 19.50, Windows SDK 10.0.26100.0, and bundled CMake 4.2.3:

```powershell
cmake -S . -B out/r0-seal-debug -G "Visual Studio 18 2026" -A x64 -DBUILD_TESTING=ON
cmake --build out/r0-seal-debug --config Debug --parallel
ctest --test-dir out/r0-seal-debug -C Debug --output-on-failure

cmake -S . -B out/r0-seal-release -G "Visual Studio 18 2026" -A x64 -DBUILD_TESTING=OFF
cmake --build out/r0-seal-release --config Release --parallel
```

Results:

```text
DEBUG_BUILD = PASS
RELEASE_BUILD = PASS
AUTOMATED_TESTS = PASS (3/3)
```

No implementation code changed in either validation pass, so all ten tests
remain evidence for the evaluated implementation. The final UAT 07 revalidation
also reran the existing Debug `ctest` suite successfully (`3/3`).

## 14. Remaining limitations and deferred risks

- Multi-monitor, negative native origins, monitor crossing, straddling, dock/
  reconnect, and topology changes remain **NOT TESTED — environment
  unavailable**. This is a deferred R1+ risk, not the current seal blocker.
- Mixed-DPI and DPI-unaware/system-aware cross-context behavior remains
  **NOT TESTED — environment unavailable**. This is a deferred R1+ risk.
- Keyboard move/resize, shell snap, minimize, close/crash/hung-target missing
  pairs, elevation, packaged-host coverage, remote desktop, sleep/lock, and
  virtual desktops remain **NOT TESTED**.
- Queue-overflow and controlled callback-reentrancy stress remain **NOT
  TESTED**; their diagnostic/code paths were reviewed but not forced in UAT.
- HWND remains ephemeral, snapshots remain best-effort, and output titles/paths
  remain sensitive local diagnostic data.

## 15. R1 architecture questions — research only

R1 is not started. Evidence from this matrix leaves these questions:

1. What geometry/topology evidence, if any, can distinguish move from resize
   without making event type claim more than Windows provides?
2. How should non-session state changes such as maximize/restore coexist with
   interactive move/resize sessions?
3. Should future session handling rely on START/END when AltSnap and native
   gestures are equivalent in this matrix but missing-pair cases remain unrun?
4. Should callback receive identity be assigned before all native queries, and
   what controlled test can prove reentrant causal ordering?
5. Should a future queue reserve capacity or priority for lifecycle events while
   retaining explicit overflow diagnostics?
6. What normalized coordinate contract survives mixed-DPI, negative-origin,
   cross-monitor, and topology-generation changes?
7. Should diagnostic records carry a non-sensitive build identity so private
   UAT evidence can be tied cryptographically to a source revision?

## 16. Seal decision

The prior-art gate, Explorer native move, Explorer native resize, VS Code and
Excel cross-application evidence, START/LOCATION/END observation, queue-loss
observability, filtering review, JSONL integrity, geometry-source separation,
platform-neutral core, prohibited-behavior audit, Debug build, Release build,
and automated tests all pass.

The revalidated test 07 contains an identifiable Explorer root window and
confirms normal → maximized → restored geometry/state changes without a
move/resize START or END. The prior test-identity blocker is resolved. No code
change was required, the other nine logs remain valid, and the final `ctest`
rerun passed 3/3.

```text
R0_BASELINE = SEALED
BLOCKERS = NONE
RETEST_REQUIRED = NONE
REVALIDATION_REQUIRED = NONE
R1 = NOT STARTED
```
