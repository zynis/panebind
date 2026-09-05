# R1-C2B Attempt 2 取证记录

审查日期：2026-09-05。Evidence prefix：`20260905T065805930Z`。
被审查实现：`e0bccc0e8ab8870150fa79b9f1a70cdf1c902db5`，分支
`codex/r1c2b-explorer-glue-session`。

Attempt 2 按当时的 runner 规则得到 PASS，该历史结果保持不变。本记录完整
解析原始 JSONL 并审查对应代码，确认安全、最终几何、生命周期和证据完整性
均通过；31 条 Leader LOCATION 只形成 1 次 active Follower native apply，
尚不足以接受产品目标中的实时跟随。

```text
ATTEMPT_2_LEGACY_R1C2B_EVIDENCE_GATE = PASS
ATTEMPT_2_SAFETY = PASS
ATTEMPT_2_FINAL_GEOMETRY = PASS
ATTEMPT_2_SESSION_LIFECYCLE = PASS
ATTEMPT_2_EVIDENCE_INTEGRITY = PASS
ATTEMPT_2_REALTIME_FOLLOW = INSUFFICIENT_EVIDENCE / NOT YET ACCEPTED
```

本次工作是已有证据的只读取证，没有运行新的真人 UAT。原始 evidence 保持
ignored，读取前后 SHA256 一致；报告不复制窗口标题、nonce 路径或 HWND。

## 原始文件与完整性

以下文件均位于 ignored evidence 目录 `uat/r1c2b/`，文件名由上述 prefix
加表内后缀组成。

| 文件后缀 | 字节 | SHA256 |
|---|---:|---|
| `-glue-harness.jsonl` | 42,355 | `F31862AAD2D6D73CBE747B3B5BBAB41B489736F8678CFACA6DAAA9B9AE029C54` |
| `-glue-observer.stdout.jsonl` | 2,277,907 | `A7EAC056954DA209C3AE4EBD8B66354A4934D5B30639F8EE38DC5E1AC158E1A0` |
| `-glue-observer.stderr.log` | 0 | `E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855` |

| 检查 | 结果 |
|---|---|
| Harness JSONL | 全部 69 行解析成功，0 空行；`harness_sequence=1..69` 连续、唯一 |
| Harness schema | 全部 `schema_version=1`、`schema_name=panebind.r1c2b.explorer_glue` |
| Internal trace | 36 条，`trace_sequence=1..36` |
| Observer JSONL | 全部 2,714 行解析成功，0 空行；`observer_sequence=1..2714` 连续、唯一 |
| Observer records | 5 diagnostic、496 census_snapshot、2,213 event |
| Observer schema | 全部 `schema_version=1` |
| Observer lifecycle | PMv2 verified；hook registration、initial census、hook shutdown、observer shutdown 全 complete |
| Observer field errors | 共 7 条，详见下文；目标 Leader START..END 的 33 条全部 accepted、field_errors=0 |
| Observer stderr | 0 字节 |
| Legacy summary | Harness #68：`result=PASS`、`runtime_gate=PASS` |
| Harness shutdown | #69：`disposition=complete` |
| Operations | 2 次 setup、1 次 active、2 次 restore 全 exact |
| Session cleanup | event source stopped、lifecycle clean；Leader/Follower restored exact；未关闭用户窗口 |

Observer 的 7 条 field errors 不是 JSONL 完整性错误：初始 census 序号
261、264、320、469、470 的 `OpenProcess/process_path` 返回 Win32 code 5，
这些快照均为 `not_visible`；序号 2709、2710 为其他窗口的
`identity_changed_during_snapshot` / `identity_changed_since_receipt`。
不能把整体 Observer 的 field_errors 数误报为零。

## 序列和几何逐条对照

Observer 序列与 Glue receipt 序列属于不同事件消费者。下表按各自 Leader
事件的顺序并列展示；旧 Glue trace 没有输出自身 `native_event_time`，所以
表中的 native ms 只来自 Observer，不能宣称存在已记录的跨来源 timestamp
join。Observer 几何是 `snapshot_captured_at` 时的实际采样值，也不是
WinEvent receipt 自带的历史 event-time geometry。

表中位置用 `(left,top)` 表示；所有列出的 Leader visible rect 尺寸均为
`1353 x 804`，因此完整矩形为 `(left,top,left+1353,top+804)`。

| Leader 事件 | Observer seq | Observer native ms | Observer sampled 位置 | Glue receipt seq | Behavior trace seq | 决策 | Glue interpreted Leader 位置 |
|---|---:|---:|---|---:|---:|---|---|
| START | 2461 | 262212875 | (183,510) | 1 | 2 | activated | trace 无几何 |
| LOCATION 1 | 2466 | 262212937 | (170,510) | 2 | 3 | follower_move_requested | (87,453)，由 command 和 total delta 还原 |
| LOCATION 2 | 2470 | 262212968 | (159,510) | 3 | 5 | leader_noop | (87,453) |
| LOCATION 3 | 2474 | 262212984 | (151,508) | 4 | 6 | leader_noop | (87,453) |
| LOCATION 4 | 2478 | 262213015 | (146,506) | 5 | 7 | leader_noop | (87,453) |
| LOCATION 5 | 2482 | 262213046 | (139,503) | 6 | 8 | leader_noop | (87,453) |
| LOCATION 6 | 2488 | 262213093 | (135,499) | 7 | 9 | leader_noop | (87,453) |
| LOCATION 7 | 2492 | 262213109 | (134,498) | 8 | 10 | leader_noop | (87,453) |
| LOCATION 8 | 2496 | 262213140 | (128,494) | 9 | 11 | leader_noop | (87,453) |
| LOCATION 9 | 2500 | 262213171 | (126,493) | 10 | 12 | leader_noop | (87,453) |
| LOCATION 10 | 2504 | 262213187 | (123,491) | 11 | 13 | leader_noop | (87,453) |
| LOCATION 11 | 2508 | 262213218 | (121,490) | 12 | 14 | leader_noop | (87,453) |
| LOCATION 12 | 2511 | 262213234 | (116,487) | 13 | 15 | leader_noop | (87,453) |
| LOCATION 13 | 2515 | 262213265 | (113,486) | 14 | 16 | leader_noop | (87,453) |
| LOCATION 14 | 2519 | 262213281 | (112,485) | 15 | 17 | leader_noop | (87,453) |
| LOCATION 15 | 2523 | 262213296 | (111,485) | 16 | 18 | leader_noop | (87,453) |
| LOCATION 16 | 2526 | 262213312 | (108,482) | 17 | 19 | leader_noop | (87,453) |
| LOCATION 17 | 2530 | 262213328 | (106,481) | 18 | 20 | leader_noop | (87,453) |
| LOCATION 18 | 2534 | 262213359 | (102,480) | 19 | 21 | leader_noop | (87,453) |
| LOCATION 19 | 2538 | 262213375 | (100,478) | 20 | 22 | leader_noop | (87,453) |
| LOCATION 20 | 2542 | 262213390 | (100,477) | 21 | 23 | leader_noop | (87,453) |
| LOCATION 21 | 2546 | 262213406 | (98,475) | 22 | 24 | leader_noop | (87,453) |
| LOCATION 22 | 2550 | 262213437 | (97,473) | 23 | 25 | leader_noop | (87,453) |
| LOCATION 23 | 2554 | 262213453 | (94,471) | 24 | 26 | leader_noop | (87,453) |
| LOCATION 24 | 2558 | 262213484 | (92,469) | 25 | 27 | leader_noop | (87,453) |
| LOCATION 25 | 2562 | 262213500 | (90,465) | 26 | 28 | leader_noop | (87,453) |
| LOCATION 26 | 2566 | 262213515 | (90,464) | 27 | 29 | leader_noop | (87,453) |
| LOCATION 27 | 2570 | 262213531 | (90,462) | 28 | 30 | leader_noop | (87,453) |
| LOCATION 28 | 2574 | 262213562 | (90,461) | 29 | 31 | leader_noop | (87,453) |
| LOCATION 29 | 2578 | 262213578 | (89,458) | 30 | 32 | leader_noop | (87,453) |
| LOCATION 30 | 2582 | 262213609 | (89,457) | 31 | 33 | leader_noop | (87,453) |
| LOCATION 31 | 2586 | 262213625 | (87,453) | 32 | 34 | leader_noop | (87,453) |
| END | 2588 | 262214062 | (87,453) | 33 | 35 | completing | (87,453) |

第一条 request trace 的 `visible` 实际是 Follower target
`(1440,453,2793,1257)`。旧 `record_trace()` 在 decision 含 command 时会
用 command target 覆盖该字段；它不是第二个 Leader sample。结合初始
Follower `(1536,510,2889,1314)` 可得 total delta `(-96,-57)`，再应用于
初始 Leader `(183,510,1536,1314)`，输入 Leader 恰为
`(87,453,1440,1257)`。其余 30 条 noop trace 直接记录相同的 Leader 矩形。

## Drain 证据边界与计数

| 项目 | 结果 |
|---|---|
| Raw Leader START / LOCATION / END | 1 / 31 / 1 |
| Observer distinct sampled Leader LOCATION geometries | 31 |
| Glue distinct interpreted Leader LOCATION geometries | 1 |
| FollowerMoveRequested decision | 1 |
| leader_noop / follower_noop_count | 30 |
| active Follower native operation / apply | 1 / 1 |
| 精确 drain-cycle 总数 | `NOT_RECORDED / NOT_RECOVERABLE` |
| 每次 drain 的事件序列边界 | `NOT_RECORDED / NOT_RECOVERABLE` |
| 每次 drain 的 event count | `NOT_RECORDED / NOT_RECOVERABLE` |
| 每次 drain 的 Leader LOCATION count | `NOT_RECORDED / NOT_RECOVERABLE` |
| 每次 drain 的 Follower LOCATION count | `NOT_RECORDED / NOT_RECOVERABLE` |
| 每条 Glue receipt 的 native event timestamp | 旧 trace 未输出；不可把 Observer 时间直接贴入 Glue receipt |
| 事件队列高水位 / capacity | 31 / 512 |
| 全生命周期 accepted event count | 34 |
| 内部处理并记录的 event receipts | Leader 1..33 |
| pending 高水位 / capacity | 1 / 64 |
| Follower active LOCATION | START..END 内为 0；内部已处理 feedback 为 0 |
| suppressed / duplicate / missing / reconciled | 0 / 0 / 1 / 1 |
| unexpected / recursive | 0 / 0 |

`max_event_queue_depth=31` 证明出现过深度 31 的积压，不能直接改写成
“只有一次 drain，且该 drain 恰有 31 条 LOCATION”。旧日志缺少 drain id、
边界和逐 drain 计数，无法恢复所要求的每次 drain 表。

全生命周期 accepted count 34 与处理 trace 中的 33 条不相等。清理代码
`finish_and_restore()` 在 stop 后会 drain 并丢弃尾部 receipt；但旧 evidence
没有记录那条额外 receipt 的身份，不能给它补造事件种类或时间戳。

## 唯一 active operation 与 before-END 结论

| 字段 | 值 |
|---|---|
| Harness record / operation_id | 63 / 3 |
| phase / role | active_follower / follower |
| behavior_operation_generation | 1 |
| source_leader_sequence | 2 |
| requested visible | (1440,453,2793,1257) |
| actual visible | (1440,453,2793,1257) |
| requested positioning | (1429,453,2804,1268) |
| actual positioning | (1429,453,2804,1268) |
| native apply attempted / outcome known / exact receipt | true / true / true |
| size / identity / location / monitor-DPI stable | 全 true |
| operation_recorded trace | 4 |
| END event / trace | 33 / 35 |

`source Leader 2 < END 33` 且 `operation_recorded trace 4 < END trace 35`
证明该 native operation 在 owner **处理 END 之前**已经返回并记录。
旧 operation evidence 没有原生调用时刻或 END-arrival watermark，不能据此
证明其发生在用户真实松手、原生 END 产生之前。Harness 在 terminal 返回后
才集中输出 trace 和 operation，不能拿各 JSON 行的 `recorded_at` 当作
原始执行时间。

Observer 在 END 之后实际观察到了 Follower LOCATION：

| 事件 | Observer seq | native ms | visible |
|---|---:|---:|---|
| Leader END | 2588 | 262214062 | (87,453,1440,1257) |
| Follower active target LOCATION | 2591 | 262214109 | (1440,453,2793,1257) |
| Follower restore LOCATION | 2702 | 262218875 | (189,316,1542,1120) |

Follower target LOCATION 的 native 时间晚于 END 47 ms。因此
“Follower active LOCATION=0”应限定为 START..END 区间或内部已处理反馈数，
不能说 Windows 在整个记录期间从未发出 Follower LOCATION。上述后置事件
也不能倒推出缺失的 `SetWindowPos` 精确调用时刻。

## END reconciliation 与恢复

Trace 35 处理 END receipt 33 并进入 completing；trace 36 进入 completed。
Harness #66 对 operation generation 1 / source 2 记录：

```text
command_trace_match_count = 1
exact_operation_receipt = true
disposition = reconciled_by_operation_receipt_and_final_snapshot
feedback_event_sequence = null
expected_visible = (1440,453,2793,1257)
actual_visible = (1440,453,2793,1257)
```

初始 Leader `(183,510,1536,1314)` 到最终
`(87,453,1440,1257)` 的 total delta 是 `(-96,-57)`；初始 Follower
`(1536,510,2889,1314)` 加该 delta，精确得到最终 Follower
`(1440,453,2793,1257)`。缺失内部反馈由 exact native receipt 和 exact
final snapshot 合法 reconciliation，不需要制造 feedback。

恢复后的 Leader 为 `(140,267,1493,1071)`，Follower 为
`(189,316,1542,1120)`，均与各自原始位置精确一致。

## 代码支持的根因

以下位置均指 starting HEAD
`e0bccc0e8ab8870150fa79b9f1a70cdf1c902db5`，而非 Fix 2 编辑后的行号。

| 旧代码位置 | 证据与含义 |
|---|---|
| `src/platform/windows/explorer/explorer_glue_session.cpp:1412` | 每个 drained batch 只捕获一次 `batch_leader` / `batch_follower` |
| 同文件 `1429` | batch 中每条 LOCATION/END 都读取同一 batch snapshot |
| 同文件 `1136`、`1187` | `process_event()` 又作 live captures，但优先使用传入的 `observed_geometry`，不会恢复历史几何进展 |
| `src/core/behavior/glue_move_coordinator.h:452` | 每条 LOCATION 都计数并调用 `plan_follower()` |
| 同文件 `549` | target 等于 current Follower 时返回 LeaderNoOp；对应 1 move + 30 noop |
| `src/platform/windows/explorer/explorer_glue_session.cpp:1092` | command 存在时 trace visible 被覆盖为 Follower target，不能误算成 Leader sample |
| 同文件 `1718` | owner 的 `while (PeekMessageW(...))` 没有固定处理 quantum；source notification 分支只 continue，未当即 drain |
| `src/platform/windows/explorer/explorer_glue_event_source.cpp:489` | drain 开始已清除 `notification_pending_`；没有证据证明本次是 notification 永不 re-arm |

确证根因命名为 `BATCH_SNAPSHOT_COLLAPSE`：原始 Observer 记录 31 个不同的
Leader sampled 位置，但 Glue 把全部 31 个 LOCATION 解释为唯一最终位置；
第一条产生 1 次 active move，后续 30 条产生 noop，和代码逻辑完全一致。

旧 owner message loop 允许先积压 delivery 再 drain，逐 receipt 的 full
capture 也增加 owner 工作量；二者是需要修复和验证的调度因素。但旧证据
没有逐 pump/drain instrumentation，无法精确量化各自对本次积压的贡献，
也无法断言 31 次 callback 都由某一次最外层 PeekMessage loop 交付。

## 取证方法

在 Windows PowerShell 中对三份原件使用 `Get-FileHash -Algorithm SHA256`
核验前后指纹，并用 `git check-ignore` 确认 ignored。对两份 JSONL 使用
`Get-Content -Encoding UTF8`、`ConvertFrom-Json`，设置
`$ErrorActionPreference = 'Stop'` 完整解析；验证序列连续、唯一、记录数量、
schema、diagnostics、目标事件及 operation 精确结果。代码审查限定为本地
starting HEAD 对应实现，没有修改 R0 Observer 或运行新的 Explorer UAT。
