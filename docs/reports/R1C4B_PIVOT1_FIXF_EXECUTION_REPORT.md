# R1-C4B Pivot 1 — Fix F 执行报告

日期：2026-09-27。仅 owned test probe 的 failure diagnostics、独立 native-abort
cleanup 与离线测试隔离；不是产品阶段或完整稳定门。

## 基线与历史证据

- Branch：`codex/r1c4b-live-magnet`；starting HEAD：`a20f32b667db601207f5675653bbc55520bf9314`。
- 起始 worktree clean；standard `git fetch origin` 成功，origin/main：
  `81e40facf52ffdb96f76e4d737740167d485f4a7`，origin/main...HEAD=`0/28`；branch upstream=`0/0`。
- origin 为已核实的 `ssh://git@ssh.github.com:443/zynis/panebind.git`；
  secondary 为同仓库 `https://github.com/zynis/panebind.git`，未改 Git 配置。
- 六份 brief 指定历史文件 SHA256 全匹配；旧 numeric validator 仍为
  `9FFB838F9B530257774B1E53B61102E9C99166D0926D37300350DBF7B6A4719D`。
  本轮新兼容 artifact：`uat/r1c4b-fixf/history-44154c2bdc9c479682795b2666771d2f.json`。
  它以旧契约实际复核原 D 全部正常门 PASS、原 E BLOCKED/
  `SKIPPED_NO_AUTHORITY`/历史 left=true，前后重新核对原文件 hash；不改旧 replay。
  新compat artifact SHA256：`A318F93EBE7341044C2EFE718624D7B79B23ACF06450A4B6BB9B69AEB3C3FDCD`。
- 原 D metadata 的 INVALID_EVIDENCE 仍原样；原 E formal 13 PASS 后第14次 BLOCKED、
  其他 formal NOT_RUN、旧门 FAIL 都不改写。NOT_PASSED 仅是新报告的未完成说明。

旧第14次 source=`3806562`、PID=`27332`、TID=`28532`、nonce=`118469386212298`。
以下来自本机117条原JSONL，不以旧报告替代原始证据：

| 原 sequence | 实际观察 |
|---|---|
| 64 / 66 | Raw DOWN serial7 QPC `1629130755527`；SendInput DOWN sent1，API QPC `1629130753193..1629130767075`，point `[1339,641]`。Raw真实收据早于API返回及父日志；async left=false不能否定该DOWN。 |
| 67 / 76 | native DOWN；真实 ENTER receipt QPC `1629131242623`。ENTER的thread-local GetCapture=0不代替稍后的GUI capture查询。 |
| 83 | sample1 planned `[1348,641]`，QPC `1629131686091`，left=true，pre-cancel。 |
| 84/86/92/96/100 | Raw serial9..13，QPC `1629131766881,1629131845458,1629131910719,1629132084472,1629132192318`；tag=false/device-present=true/button_flags=0；cursor `[1347,641]→[1343,641]→[1326,639]→[1313,639]→[1276,639]`。 |
| 104 / 105 | BLOCKED_BY_INPUT_INTERFERENCE；原fence具体失败子条件未记录。cleanup仍source=FG=root=capture=move_size，GUI flags2、left=true，旧条件拒绝active native。 |
| 107 / 111 | serial14 QPC `1629132298663`、cursor `[1258,639]`、无button transition；cleanup SKIPPED，left仍true。 |
| 115 / 117 | teardown后capture=0；最终资源销毁。资源销毁不是实际系统UP证明。 |

实际计数 ENTER=1、DRAG=7；cancel/EXIT/WinEvent END/source writer/shield=0。
存在轨迹偏离证据，但输入产生者为 **UNKNOWN**；device-present/tag mismatch不能
唯一识别用户、触摸板或其它自动化。失败瞬间first fence predicate亦 **UNKNOWN**，
不事后补造。旧最终left=true不是本轮当前状态。

## 研究、契约和代码边界

详细实读模块、issues/PR/full commit/license/official links见
[Fix F 研究](../research/R1C4B_PIVOT1_FIXF_ABORT_CLEANUP.md)和
[SOURCE_PROVENANCE](../research/SOURCE_PROVENANCE.md)。AltSnap pinned
`5c86416ad21e4b72844a998a746bd3bb0bee5f5d`（GPL reference-only），FancyZones pinned
`19c4d805321db86f3634e6968e14dbf25cbba14a`（MIT reference-only）；无copy/adapt。
Microsoft的SendInput不接受目标HWND，capture/root/Tag不构成原子输入交易或actor认证；
native modal-loop对owner message/Raw delivery是否完成必须由本机实际收据判定。

新CLI仅显式opt-in `--run-owned-input-reliability-test --gesture ... --mode ...`，
schema `r1c4b-takeover-owned/v5`，fence=`ordered_failure_snapshot_v1`，
cleanup=`owned_native_abort_v1`。原v4 CLI/validator/evidence不套新规则。

- Fence先写有序7项queries和14项predicate的同次诊断，含真实值、QPC、
  first/all已知失败及UNKNOWN/NOT_EVALUATED；原±1容差与first-failure顺序保留。
  原success alias只有通过后才写，并引用diagnostic_sequence；不称原子snapshot。
- ProductGestureAuthority和TestSyntheticInputIsolation成功路径原条件不变。
  新TestAbortCleanupAuthority不把native active假装clear：exact自有source/FG/root/
  capture/move_size、真实DOWN API+Raw+native DOWN+ENTER账本、有效desktop/输入/
  receiver/log、无不明button transition/foreign capture才能候选。
- Provisional intent在API前记录，允许Raw先到；sent1不能单独归属DOWN。
  已发送UP即禁止再次UP，即使receipt尚未确认。movement偏离计划本身不取消
  cleanup候选，但确定性fixture不吞任何额外真实干扰。
  `ledger_snapshot_qpc`与`ledger_receiver_sequence`记录内部mutex保护的实际
  clone边界，Raw另记录父进程`ledger_observed_qpc`；独立复算只包含该边界前
  已处理的packet，不能用早到child QPC回溯后来的parent处理。这是内部账本的
  有序证据，不是Win32/cursor/GUI的原子快照。
- owner/UI真实ACK先停收/清 pending writer，API边界再fresh检查；仅一次flags4
  专用tag `0x50424655` 的cleanup UP，不带MOVE/DOWN、不拉回cursor。
  无ACK/权限/可靠账本不发送；不cancel/ReleaseCapture/销毁source后再发送。
- 实际Raw UP、native EXIT、独立matched WinEvent END、最终有效UP/GUI clear/
  ledger settled及第二次owner ACK各自证实；scope=cleanup且acceptance_eligible=false。
  receiver/source/hook在观察后才依序退出；不能由runtime自报PASS替代独立validator。
- 只改 test-only probe、新纯diagnostic/validator/runner、测试分类和本轮docs；
  不改src/core、Observer或产品Explorer/magnet/geometry writer实现。

```text
PRODUCT_HANDOFF_SEMANTIC_DIFF = NONE
GEOMETRY_WRITER_SEMANTIC_DIFF = NONE
CLEANUP_CONTRACT_CHANGE = OWNED_NATIVE_ABORT_EXTENSION
FENCE_DIAGNOSTIC_CHANGE = FAILURE_SNAPSHOT_ADDED
```

本轮整体不是 VALIDATOR_SEMANTIC_DIFF=NONE。

代码证据：与起始a20文本仅统一行末后逐段比较，`intended_at`、`owner_proof`、
`capture_handoff_preflight`、`emit_handoff_preflight`、`write_intended`、
`process_handoff`、`process_raw_notice`、`process_final_acceptance`八段正文全部
exact相同；`magnet_input_isolation_diagnostic.h`（含product_facts/classify_product）
全文件相同。core/Explorer/Observer产品路径无tracked diff或新增文件。
不可据此说整个test probe无diff：它明确新增了v5 opt-in诊断/cleanup。

## 离线与当前环境

测试名字/真实命令/逐项源码及exit层证据完整列在
[离线审计](R1C4B_PIVOT1_FIXF_OFFLINE_AUDIT.md)。实际本机CMake/CTest为
`4.2.3-msvc3`、VS18 2026 x64、SDK10.0.26100.0，host Windows10.0.26200。
新增入口先全量show-only分类，再精确正向24 offline/7 interactive排除并再次preview；
未知、混合、空选择或fixture扩展均拒绝，不fallback默认CTest。

已执行：Debug/Release各24/24 offline PASS，selection74 checks PASS、
fake exit14 checks PASS；旧 input-isolation157、replay50、numeric-runner98
synthetic checks PASS，新runner294 checks PASS、新v5 validator138 checks PASS。
后者实际验证四个完整自制normal/abort wire正例及保留连续sequence的深层
缺RawUP/EXIT/WinEND、父处理时间反转、API boundary/nonce变化、cleanup污染、
已发送但迟parent UP等反例；不读取/转换历史日志。冻结后pure diagnostic为53 checks。
未运行默认全量CTest或7个interactive CTests。
纯模型/fixture不是GUI清理实测，所有现场门待本节后补充。

初始只读工具在managed `CodexSandboxDesktop`，foreground=0/context无效，keys全部
NOT_EVALUATED/null，不能称UP。经批准同一主机标准执行上下文再只读检查，
`host-readonly-20260927T123441924Z-2e01b772513f442882593b9db654019c.json`：
Default/WinSta0、WTSActive/unlocked、FG tuple及caller/FG IL8192前后相同，11输入
high bit全部false/UP、mapping unswapped、READY/exit0。未作预防性UP或cursor移动。
它不是后来GUI的长期授权；每次run都必须独立pre/post只读证据。
该readonly evidence SHA256：`FEC9E4F15019A52B8DC0DBECDEB6BF9FFE0784162DFBD36F009C4C389D8406F4`。

双配置完整build实际命令（只构建，不执行任何harness）：
`cmake --build out/r1c4b-live-magnet-debug --config Debug --parallel 4`，
`cmake --build out/r1c4b-live-magnet-release --config Release --parallel 4`，均exit0。
类型警告及新增账本边界收口后分别用同命令加
`--target panebind-magnet-takeover-probe`重建，两配置均exit0、无该新增warning。
工具使用上述VS bundled绝对路径；构建interactive exe不等于运行该exe。

Fake层实际观察：probeStub2→child2→aggregate2；外层implicit PowerShell Command
可映射成tool1，explicit `exit $LASTEXITCODE`实际保留2。该本机fake映射 VERIFIED，
但旧历史外层wrapper唯一原因/未单独观测的aggregate终端exit仍UNKNOWN；未确认
仓库映射bug，未为此重跑旧GUI或改旧runners。

## 限定GUI结果：第1项bootstrap阻断，立即停止

冻结顺序：Debug Move abort、Debug BottomResize abort、Debug Move normal、
Debug BottomResize normal、Release同顺序；每项一次fresh resources、无retry。
本轮不跑smoke/formal或续接第14次，不进入Explorer/产品Raw/真人UAT。
实现 checkpoint：`2f06924e88b5bef81b35e1d2414aab9b4c6d4f0f`，
`test: isolate offline runs and verify owned native abort cleanup`。clean HEAD后在
批准的主机Default桌面执行唯一一次：
`powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/run-r1c4b-input-reliability-gates.ps1; exit $LASTEXITCODE`。

实际只有第1项Debug Move controlled-abort的**程序启动尝试**，没有开始目标手势。
pre只读11项UP/READY/exit0后，普通SetForegroundWindow未取得global foreground；
fallback activation fence的GetGUIThreadInfo显示foreign capture。它在任何SendInput
之前拒绝。因此测试中断不是预注册的test fault，不得算PASS_EXPECTED_ABORT。
未重试、未推进其余七项、未修后重跑、未运行formal/smoke/Explorer。

唯一新run目录（均ignored/local）：
`uat/r1c4b-fixf/20260927T132106696Z-Debug-Move-controlled-abort-c2473a89d3db4c1da41ceb0ba51b1cb6/`。
inventory：`20260927T132105318Z-bounded-eight-c7d46eec9ec2462e95df6b3badc8fe18.json`。

| 实际新sequence | 证据 |
|---|---|
| 8/9 | global foreground=`854960`，SetForegroundWindow=false；source HWND=`13571864`、PID=`56980`、TID=`46328`、nonce=`245205829207611`，DPI192、monitor65537。 |
| 12 | activation_fence phase=move；cursor `[1310,31]`、planned activation `[1446,874]`；GUI flags0、capture HWND=`71435280`、menu/move_size0；foreign_capture_clear=false，buttons/modifiers匹配。fallback尚未注入。 |
| 14/15 | bootstrap及gesture均 `BLOCKED_BY_FOREIGN_INPUT_CAPTURE`；sendinput move/down/up全部false，temporary topmost已恢复。 |
| 16–25 | 真实initial/final owner ACK，owner TID46328、identity/nonce匹配、active operation/pending/motion/notice/nativecalls全清；无自有DOWN，使用原strict cleanup，`SKIPPED_NO_AUTHORITY`且InputSent=false、left=false。不是新native-abort PASS。 |
| 26–28 | hook实际removed；receiver registration removed/window destroyed；source/guard已销毁，source writer/shield/acceptance均0。shutdown result=BLOCKED、cleanup_result=NOT_RUN。 |

JSONL共28条全部合法、sequence连续、startup/shutdown完整。input/activation_input/
cleanup_release/abort_cleanup_release **合计0**；Raw packets、ENTER、EXIT、test-fault、
cancel、source writes全0。receiver序列仅注册1/退出2，error/overflow/drop无记录；
hook安装/移除可确认，交互event delivery没有被测试。**不能因此称接管或清理通过。**
输入产生者或foreign capture持有者原因 **UNKNOWN**；只记录获准的数值句柄/线程
事实，未查foreign title/path/UI tree，未干预该capture、foreground或既有窗口。

原metadata保留 `Result=null`/`AfterHEAD=null`：完整v5 acceptance validator因尚未
出现input_fence_diagnostic返回 `Fix F v5: diagnostics required`。这是未进入该阶段、
完整fixture proof未成立，不是JSON损坏；该工具层reason不能替代原始native blocker。
随后只读、as-is调用新Envelope和冻结的bootstrap/Raw/WinEvent/原strict cleanup
primitives，独立确认上述valid blocked prefix；不转换schema、不改metadata、不
升级为normal或cleanup PASS。额外post-audit确认同一HEAD仍clean、19项source/
binary/validator/runner hash全与BeforeIdentity一致。该复核在本报告编辑之前执行，
不是补造metadata缺失的After字段。

独立post只读Default/WinSta0、active/unlocked、stable FG/IL可靠，11输入全部UP；
当前测试没有pending DOWN，无补UP或手动复位需要。FINAL_BUTTON_STATE在该观察点
为OBSERVED_UP（LEFT及其它全部所需输入）；不是永久状态承诺。

| Artifact/implementation | SHA256 |
|---|---|
| Native source | `F540E9DD2DCCCD7311DEB006FCA8FA0073708084B12A513A8A422AF082465134` |
| Debug probe binary | `8D3E71CCCC53F9B64EE248863829C613B634350F71EA9B5692C57FB6FCAD34D7` |
| v5 validator | `4A85B6D4B75515A44CC9F7DB7B1981C957A88F248A8A61758DF71556E0106C6D` |
| Normal composition | `064B9C1EDAC92C7A7F011FDF7F8B01D15D339D0214CF21436F753E8876483D6E` |
| New JSONL | `09ECEDC632F91B4FAEB7CFA00682B7227827FE57AA8EDB79BB6701D68576B09D` |
| New metadata | `010322412D0E47E4BD4C52F230BD94325E530B3AA14E9E9F395E1076210F394F` |
| Pre readonly | `089CB6A07FCF1B6465861BE33B2DA4C8A2F1A35D52E010BCAFF02BE9231C5562` |
| Post readonly | `760621B0D8B4DBBF1940825DD9F35A4365C36F694C162C740BA9E83CE8D36458` |
| Bounded inventory | `5CBAF79AB38296A8800A7190EDF37EDCBF9D024E74C7CEEE10EA1179668F2253` |

native probe exit2、child runner exit2、aggregate process exit2、工具exit2已实际观察，
JSON inventory STOPPED/Passed=false。本次没有implicit-shell映射混淆。

| 预登记项 | 实际结果 |
|---|---|
| 1 Debug Move abort | 启动尝试BLOCKED；native ENTER/sample1/abort cleanup **NOT_RUN** |
| 2 Debug BottomResize abort | NOT_RUN；first-failure stop |
| 3 Debug Move normal | NOT_RUN |
| 4 Debug BottomResize normal | NOT_RUN |
| 5 Release Move abort | NOT_RUN |
| 6 Release BottomResize abort | NOT_RUN |
| 7 Release Move normal | NOT_RUN |
| 8 Release BottomResize normal | NOT_RUN |

## 最终门与剩余未测风险

```text
FIXF_FAILURE_DIAGNOSTICS = PASS
FIXF_OFFLINE_TEST_SELECTION = PASS
FIXF_OWNED_NATIVE_CLEANUP_CONTRACT = BLOCKED
FIXF_HISTORICAL_EVIDENCE_UNCHANGED = PASS

FIXF_DEBUG_MOVE_ABORT_CLEANUP = NOT_RUN
FIXF_DEBUG_RESIZE_ABORT_CLEANUP = NOT_RUN
FIXF_RELEASE_MOVE_ABORT_CLEANUP = NOT_RUN
FIXF_RELEASE_RESIZE_ABORT_CLEANUP = NOT_RUN
FIXF_DEBUG_MOVE_NORMAL_SINGLE = NOT_RUN
FIXF_DEBUG_RESIZE_NORMAL_SINGLE = NOT_RUN
FIXF_RELEASE_MOVE_NORMAL_SINGLE = NOT_RUN
FIXF_RELEASE_RESIZE_NORMAL_SINGLE = NOT_RUN

FIRST_UNEXPECTED_FAILURE = DEBUG_MOVE_BOOTSTRAP_BLOCKED_BY_FOREIGN_INPUT_CAPTURE
FINAL_BUTTON_STATE = OBSERVED_UP
OBSERVED_EXTERNAL_INPUT_SOURCE = UNKNOWN
TOP_LEVEL_EXIT_CLASSIFICATION = VERIFIED

FIXE_HISTORICAL_FORMAL = 13_PASS_THEN_BLOCKED_AT_14
FIXF_FORMAL = NOT_RUN
OWNED_FREE_TAKEOVER_GATE = NOT_PASSED
RAW_INPUT_TAKEOVER_ARCHITECTURE = UNRESOLVED
EXPLORER_STAGE = NOT_RUN
POST_CANCEL_LEGACY_MOUSE_DELIVERY_RISK = NOT_PRODUCT_TESTED
VISUAL_TERMINAL_RESTORE_FLICKER = NOT_HUMAN_TESTED
R1C4B_HUMAN_UAT = NOT_READY

PRODUCT_INPUT_SHIELD = NONE
PRODUCT_RAW_INPUT = NOT_IMPLEMENTED
PRODUCT_SENDINPUT_DEPENDENCY = NONE
PRODUCT_GLOBAL_MOUSE_HOOK = NONE
PRODUCT_DLL_INJECTION = NONE
PRODUCT_POLLING = NONE
PR = NO
MERGE = NO
TAG = NO
RELEASE = NO
```

Failure diagnostics PASS依据实现/53-model/138-validator反例，不宣称新input fence
失败已现场观察；本次实际bootstrap已有原activation fence完整记录。新cleanup
contract为IMPLEMENTED/AUTOMATED TESTED，**empirical BLOCKED**，没有接收/结束反例
证明其不成立，也没有现场通过证据。八项全未到目标场景，完整稳定门仍欠缺；
不拼接旧13次，不修改Recovery Index INCUBATE、不追加产品阶段或长期路线图。

本次GUI停止后只作只读审计与本报告更新；实现/validator/runner不修后重跑。
交付本报告普通docs commit并标准push当前分支；精确final SHA/remote ref与最终
worktree/divergence以该提交之后的Git核验及本轮最终交付为准。`uat/`不入Git。
