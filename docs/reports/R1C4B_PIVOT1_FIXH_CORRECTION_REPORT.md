# R1-C4B Pivot 1 Fix H — Correction Report

日期：2026-09-28（Asia/Shanghai）。本报告仅记录对既有 Fix H 原始证据的
只读纠错复核；不改写原报告、原 artifact、原始 ZIP 或任何现场输入记录，
也没有新增 GUI、输入、Explorer、产品功能或真人 UAT。

最终纠错结论：**PASS**。Fix H 的 100 个 fresh 完整 v5 normal 运行及 owned
重复稳定性证据可被接受；此前 `INVALID_EVIDENCE` 来自两个辅助 UNKNOWN
计数的类型比较错误，而不是 gesture、Raw handoff、资源收尾或输入隔离失败。

## 原报告与 SHA 边界

- 原报告保留在
  [`R1C4B_PIVOT1_FIXH_EXECUTION_REPORT.md`](R1C4B_PIVOT1_FIXH_EXECUTION_REPORT.md)，
  其绑定提交为 `e8f12d5cf25b62c0143efc9e079e95e5228712b0`。
- 原报告的接受结论继续保持 `INVALID_EVIDENCE`；本报告不追溯改写该历史结论。
- 原 Evidence Implementation SHA 为
  `01633ff31776b42be9fc2383ed301d9884e80019`，不是原报告起点 SHA。
- 纠错审计 checkpoint SHA 为
  `41db15cec941b5ca9cb016403343f74d9bf3c4c7`。
- 原始机器 summary 继续按 `UNTRUSTED` 处理；纠错 PASS 来自独立只读 replay，
  不是把原 summary 重新命名为可信结果。

## 修正 replay 证据

只读 artifact：
`uat/r1c4b-fixh-correction/20260928T032227Z-c8e0d10d38c344879d4554be4e019742/corrected-replay.json`

| 项目 | 结果 |
|---|---|
| Schema / Result | `r1c4b-fixh-corrected-replay/v1` / `PASS` |
| BatchId | `c8e0d10d38c344879d4554be4e019742` |
| Planned / attempts /完整 NormalProof | `100 / 100 / 100` |
| progress records | `201` |
| unique RunId / nonce | `100 / 100` |
| 原文件复核 | 前 `457`、后 `457`、全部不变 |
| Git blob proofs | `23` |
| 原控制文件 / 原包绑定 | `7 / 4` |
| 原控制与包 SHA 二次抽核 | `11/11` 匹配 |
| statistics groups | `8/8 COMPLETE` |
| prerequisites | `VERIFIED` |
| retry | `0` |
| 新 GUI / 新 input | `0 / 0` |

独立二次抽核结果为 **PASS**：457 个引用未发现矛盾，原控制文件、原包及
Git blob 绑定均闭合。artifact 大小 `138830` bytes，SHA-256：
`99338365EAACCF59FDDE7AD5F24D8CC9CB9F155845EDB529EE91188D0768165D`。

## UNKNOWN 计数修正

受影响字段仅为 `ProbeInvocationsUnknown` 与 `TargetGesturesUnknown`。
旧 helper 将 Boolean `true` 错误计入字符串 `UNKNOWN`；修正 replay 以严格类型
重建原始记录。八组两字段均从旧值降为 `0`，其它计数保持一致：

| group | phase / config / operation | 两字段旧值（各） | 两字段修正值（各） |
|---:|---|---:|---:|
| 1 | smoke / Debug / Move | 5 | 0 |
| 2 | smoke / Debug / BottomResize | 5 | 0 |
| 3 | smoke / Release / Move | 5 | 0 |
| 4 | smoke / Release / BottomResize | 5 | 0 |
| 5 | formal / Debug / Move | 20 | 0 |
| 6 | formal / Debug / BottomResize | 20 | 0 |
| 7 | formal / Release / Move | 20 | 0 |
| 8 | formal / Release / BottomResize | 20 | 0 |

各组 Planned、Attempts、Preflight、Probe、Target entered、PASS、blocked、
failed、invalid-native、NotRun，以及既有七维统计数值均未改变；本报告不重复
旧报告的七维全表。八组统计仍全部为 `COMPLETE`。
新结果明确记录 `HistoricalCountersMatch=false`，只接受上表两列已解释的差异。

## 按键状态口径

`OBSERVED_UP` 仅来自原 attempt 100 的 readonly post 记录，不是当前时刻的新
观察。纠错任务没有重新查询输入状态，因此当前声明必须是：

```text
FINAL_BUTTON_STATE_AT_ORIGINAL_POST = OBSERVED_UP
CURRENT_BUTTON_STATE = NOT_OBSERVED_NOW
NEW_GUI_INVOCATIONS = 0
NEW_INPUT_EVENTS = 0
```

## 增量交付包

本次仅新增纠错增量包：
`uat/r1c4b-fixh-correction/20260928T032227Z-c8e0d10d38c344879d4554be4e019742/review-package/fixh-correction-evidence.zip`

- ZIP：`9456` bytes；SHA-256
  `50264D32BA280591354A2155A7C191184D622ACC2B384C0D22F2F9D9A4E95756`。
- 同目录外部 manifest SHA-256：
  `779C0F2AFD09A666351DED80630C21195955ACAC333626230759B55C6D31D789`。
- 原三个 Fix H ZIP 保持不变；新包不替换、合并或重打原包。
- 增量包 manifest 完整引用原七个控制文件和旧三 ZIP/manifest，但不复制它们；
  `AcceptanceClaim=NONE`，包装本身不是接受判定。
- artifact 与新 ZIP 均不上传，也不进入 Git。

## 自动测试状态

本次记录的纯测试全部 **AUTOMATED TESTED / PASS**：

| 纯测试 | 结果 |
|---|---:|
| owned stability gates（原布尔误计与类型边界） | 164 checks PASS |
| Fix H corrected replay（缓存/raw矛盾） | 102 checks PASS |
| owned stability boundaries | 90 checks PASS |
| owned stability statistics | 107 checks PASS |
| input reliability runner | 363 checks PASS |
| correction package self-test | 9 checks PASS |

这些是无 GUI、无 input、无 native/environment/Git 执行的纯测试。冻结 native
与 C++ 未改变，因此本次无需重建或重跑 CTest；这不等价于新增 CTest 结果。

## 接受边界与下一目标

```text
FIXH_ORIGINAL_ACCEPTANCE = INVALID_EVIDENCE
FIXH_CORRECTED_REPLAY = PASS
OWNED_FREE_TAKEOVER_GATE = PASS
RAW_INPUT_TAKEOVER_ARCHITECTURE = VALID_AT_OWNED_STAGE
EXPLORER_EXECUTED = NO
PRODUCT_RAW_INPUT = NOT_IMPLEMENTED
R1C4B_HUMAN_UAT = NOT_READY
NEW_GUI_INVOCATIONS = 0
NEW_INPUT_EVENTS = 0
PRODUCT_INTEGRATION = NOT_TESTED
HUMAN_UAT = NOT_TESTED
PR / MERGE / TAG / RELEASE = NO
```

`VALID_AT_OWNED_STAGE` 只证明 owned 阶段架构与证据闭合；它不证明 Explorer
窗口、产品集成、长期稳定性或真人体验。下一功能目标仅记录为：让 PureMagnet
接入已通过的接管路径，先完成 owned live Magnet，再完成三个 Explorer 窗口的
实时磁吸和既有 Ctrl 联动移动可操作版本。该目标在本报告中**未实现、未执行、
未测试**，不属于本次纠错范围。
