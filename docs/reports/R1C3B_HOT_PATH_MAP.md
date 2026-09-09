# R1-C3B Phase 1 — Baseline Hot-Path Map

Recorded before source changes, 2026-09-09. Baseline:
`d901094404f04772e1d641842df76d13c551a734`.
Branch: `codex/r1c3b-smoothness-profile`.

## Actual active path

```text
Explorer / Windows generates WinEvent
  -- asynchronous OUTOFCONTEXT delivery --> installing owner STA
  callback: fixed filter, Ctrl at START, existing receipt QPC, bounded queue
  -> first pending edge: PostThreadMessage (later receipts inherit pending edge)
  -> owner MsgWaitForMultipleObjectsEx / bounded eight-message PeekMessage loop
  -> private notification retrieval OR direct drain of already-delivered backlog
  -> drain_owner_queue: hook/target identity validation, normalized receipts
  -> one processing quantum, lifecycle-preserving coalescing
  -> Leader full capture
  -> Follower full capture
  -> exact-role/generation/activation and feedback lookahead
  -> Core on_event: classify, initial-relative MovePlan, no-op/feedback policy
  -> execute_translation: activation, permit, bounded operation allowance
  -> prepare_translation: Follower full validation AGAIN, expected snapshot/rect
  -> apply_prepared: permit and Follower full immediate validation AGAIN
  -> pending-before-native registration
  -> SetWindowPos (NOSIZE | NOZORDER | NOACTIVATE)
  -> full postverify validation, visible/positioning/identity comparisons
  -> operation receipt and Core result; deferred ACK if applicable
  -> Follower WinEvent receipt / next quantum capture / exact match / suppression
  -> END: fresh Leader/Follower full capture, reconcile, unhook, fixture restore
```

All behavior, native calls, captures and profile state remain on one owner STA.
The callback runs on that installing thread too, potentially while COM/native
calls pump/reenter it. The external R0 Observer has a separate process/queue;
its workload can perturb the system, but that is a hypothesis, not a proven
cause. Callback delivery does not mean physical input generation or presentation.

## Baseline locations and timing visibility

Line anchors refer to the baseline above, not changing implementation line numbers.

| Step / source | Native API and COM/Shell involvement | Potential wait / current visibility |
| --- | --- | --- |
| `explorer_glue_event_source.cpp::receive_raw_event` | fixed envelope filtering; START GetAsyncKeyState; QPC; PostThreadMessage | queue posting, no COM/snapshot/serialization; callback QPC only, no notification/dispatch relation recorded |
| `explorer_glue_session.cpp:1960 run_until_terminal_impl` | timeout handle; MsgWaitForMultipleObjectsEx; PeekMessage; TranslateMessage/DispatchMessage | event wait, internal delivery and reentrancy; no fine wake/dispatch trace |
| `event_source::drain_owner_queue` | GetWindowThreadProcessId / GetAncestor | current identity validation; no duration split; no new sampling of historical geometry |
| `session.cpp:1550 drain_event_source` | Bridge capture for Leader then Follower | combined sample start/end only; all capture costs conflated |
| `session.cpp:1233 process_event` | current samples; frozen topology; Core classify/MovePlan/feedback | pure Core plus snapshot copies/checks; decision end stamps but no start/duration |
| `session.cpp:864 execute_translation` | activation/permit and Bridge preparation | CPU work + full Follower revalidation; combined owner-to-native |
| `explorer_session.cpp:4860 prepare_translation` | validate_or_retire + expected snapshot + translation conversion | live validation can cross COM/disk/native boundaries |
| `explorer_session.cpp:4942 apply_prepared` | another immediate validation, pending callback, SetWindowPos, complete validation | native duration and broad native-to-postverify only |
| `process_event` operation result / feedback | Core on_operation_result / on_event; pending exact match | callback and ACK points, no policy-stage cost |
| END / cleanup | two fresh captures; stop/unhook; independent restore | same safeguards, fixture-only; do not mix with active operation timings |

### Full capture internals, in actual order

`explorer_session.cpp:2082 validate_native_target`, via `validate_or_retire:2716`:

1. owner/token/ledger resolution, authority kind and consent generation;
2. Shell observation lifecycle and delivered browser-receipt processing;
3. bound COM HWND witness, **global Shell inventory**, peer exact fingerprint,
   candidate/baseline uniqueness, exact nonce location;
4. IsWindow/process-handle liveness, PID/TID/GetProcessId;
5. `validate_explorer_image:1364`: system path and process image path,
   **two CreateFileW / GetFileInformationByHandleEx / GetFinalPathNameByHandleW
   checks** for installed/actual image identity;
6. class, GA_ROOT, style, owner, visibility;
7. DWM cloak, minimized/maximized;
8. CoCreateInstance(IVirtualDesktopManager), current-desktop query;
9. `query_process_security:1418` for controller and target: OpenProcessToken,
   integrity/user SID/session/elevation/UIAccess/AppContainer and anchor equality;
10. GetWindowRect and DWM extended visible-frame bounds;
11. DPI contexts, MonitorFromWindow/GetMonitorInfo/GetDpiForWindow, frozen anchors;
12. eligibility predicate and snapshot completion.

Successful active apply typically includes five full validations: quantum
Leader, quantum Follower, prepare Follower, immediate Follower, postverify
Follower. END adds two more. This is a **static call-path count**, not a measured
duration distribution. COM can deliver new receipts between these semantic
boundaries. Repeated calls within a quantum are not automatically duplicates.

## Existing evidence and missing visibility

Frozen C3A raw Debug `20260909T101910373Z` and Release `20260909T103941915Z`
were reread with the existing offline runner: both PASS. Accepted behavior and
hashes are in [C3A human validation](R1C3A_HUMAN_VALIDATION_REPORT.md).
Debug/Release active applies: 15/39, Leader quanta 15/40, distinct samples
15/39, subjective C/C. Release p50: receipt-to-owner 112.974 ms (589 raw
receipts), owner-to-native 149.5724 ms (39 operations), native 7.6767 ms,
native-start-to-postverify 40.0458 ms, source-receipt-to-postverify 209.8503 ms.
Different-grain medians cannot be added. Fine Shell/security/image/geometry
stages, notification relationships and owner dispatch times are
**NOT RETROACTIVELY RECOVERABLE**. No fine-stage bottleneck is inferred.

## Risk / candidate matrix

Cost evidence below means stage-specific current evidence, not broad timing.
Every removal or caching permission remains NO / UNPROVEN before human profile.

| Fact | Why it exists | Can change during drag? | Current detection | Current cost evidence | Cheaper reliable signal? | Stale-authority risk | Safe to remove? |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Token/activation/permit generation | exact limited authority | yes, retired/mismatch | private ledgers + exact generations | not isolated | immutable lookup may be cheap; still mandatory | critical | NO |
| HWND/PID/TID/process instance | prevent reuse/wrong target | yes, destruction/replacement | native IDs, retained process handle/liveness | not isolated | events alone do not prove freshness | critical | NO |
| Image file identity/path | allow installed Explorer only | file/path/identity race possible | process path + both file identities | not isolated | future bound identity proof required | critical | NO / UNPROVEN |
| Root/owner/style/class | only eligible top-level target | yes | native queries each validation | not isolated | no complete event-only invalidation proof | high | NO |
| Security/user/session/integrity | no elevation/foreign-user expansion | cannot assume immutable | both token queries and anchor comparison | not isolated | future predicate-by-predicate proof | critical | NO |
| State/cloak/desktop | reject hidden, max/min, wrong desktop | yes | native state, DWM, COM desktop query | not isolated | events are asynchronous and incomplete for all facts | high | NO |
| Monitor/DPI/work area | preserve coordinate meaning | yes | PMv2/native facts + anchors | not isolated | future topology/transition contract required | high | NO |
| Positioning/visible geometry | true target and exact feedback | yes, user/manager/native adjustment | GetWindowRect + DWM | not isolated | mouse delta is not geometry authority | critical | NO |
| Shell/location/uniqueness | nonce target and baseline exclusion | yes, navigation/peer/third candidate | observation + global inventory + exact location | not isolated | bound witness lifetime/apartment/failure model unproven | critical | NO |
| Prepare versus immediate check | close pre-native race window | yes, intervening COM can reenter | fresh full validation + expected equality | broad owner-to-native only | not the same semantic instant | critical | NO |
| Postverify | prove what actually happened | yes, application may adjust | full validation + exact two-rect comparison | broad native-to-postverify only | geometry/full split is Phase 2 candidate only | critical | NO |
| Last exact Follower snapshot | previous known result, not permanent truth | yes, user/other manager can move it | fresh capture + receipt watermarks | not isolated | generation+watermark alone not yet proven sufficient | critical | NO / UNPROVEN |

There are repeated full validations and CPU snapshot copies, but no live-read
dedup with same semantic instant/no wider stale window has been proved. The
current decision is **SAFE_DEDUP = NONE**, not a speculative cache. Profile
first. Pure-CPU candidates may be revisited only with an explicit equivalence
proof; no predicate is removed in Phase 1.

```text
R1C3B_HOT_PATH_MAP_GATE = PASS
SAFETY_CHANGE_REGISTER = EMPTY
R1C3B_ROOT_CAUSE = PENDING_HUMAN_PROFILE
```
