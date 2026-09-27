# Fix F：offline CTest 与 exit 分层审计

审计日期：2026-09-27。范围仅测试分类、正向选择、无 GUI 的假进程 exit 回归；不改旧 D/E runner、validator、metadata 或 replay artifact。不包含本轮 GUI/cleanup 实测。

## 本机注册与工具

从 `out/r1c4b-live-magnet-debug/CMakeCache.txt` 实读工具路径：
`D:/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/`。
本机 CMake/CTest 均为 `4.2.3-msvc3`；初始 `ctest --test-dir out/r1c4b-live-magnet-debug -C Debug --show-only=json-v1` 返回 30 项，全部没有阶段标签。新增纯 abort diagnostic 后为 31 项：24 offline、7 interactive。

已检查本机 `ctest --help` 的 JSON、正向 regex/label、`--no-tests=error` 能力。官方 [CTest 4.2 手册](https://cmake.org/cmake/help/v4.2/manual/ctest.1.html)说明 show-only 不执行测试、regex/label 正向过滤与 fixture 自动扩展；网页目前是 4.2.8，实际可用性依据本机 4.2.3-msvc3，不把网页 patch 版本当作本机版本。

## 全部分类

实际注册命令均为单个无参数 exe。core 在 `<build>/<Configuration>/`；Windows 在 `<build>/src/platform/windows/<Configuration>/`。精确 target/路径关系同时固定在 `Get-OfflineTestAudit`；名字含 `unit` 不构成安全依据。

| CTest 名称 | 实读源码（相对仓库）/实际调用依据 |
|---|---|
| magnet-gesture | `tests/core/magnet_gesture_tests.cpp`；pure gesture facts |
| magnet-constraint-solver | `tests/core/magnet_tests.cpp`；pure solver |
| glue-group-move | `tests/core/glue_group_move_tests.cpp`；pure normalized coordinator |
| geometry | `tests/core/geometry_tests.cpp`；pure geometry |
| core-model | `tests/core/model_tests.cpp`；pure identity/snapshot/events |
| topology | `tests/core/topology_tests.cpp`；pure topology |
| translation | `tests/core/translation_tests.cpp`；pure translation plans |
| glue-move-coordinator | `tests/core/glue_move_coordinator_tests.cpp`；pure receipts/commands；检查 core 依赖无 Win32/API |
| test-foreground-bootstrap-model | `operations/test_foreground_bootstrap_model_tests.cpp`；布尔事实/计数，不调用 SendInput |
| windows-magnet-postverify | `operations/magnet_postverify_diagnostic_tests.cpp`；构造 P/V 和 JSON，不捕获 HWND |
| test-handoff-preflight-diagnostic | `operations/magnet_handoff_preflight_diagnostic_tests.cpp`；纯 classifier |
| test-product-input-isolation | `operations/magnet_input_isolation_diagnostic_tests.cpp`；纯 identity/rect/root 数值模型 |
| test-owned-native-abort-diagnostic | 新 `operations/magnet_owned_abort_diagnostic.h/_tests.cpp`；完整实读，仅标准库，无 Win32 API |
| windows-rect-adjustment | `operations/window_rect_adjustment_tests.cpp`；纯 checked rect 算术 |
| explorer-browser-sink | `explorer/explorer_browser_sink_tests.cpp` 和实际 `explorer_shell_inventory.cpp` testing factory/Invoke/drain/destructor；本进程 FakeBrowser/IDispatch、COM apartment、锁/线程；不 Advise、不创建 HWND/订阅真实交互源 |
| explorer-group-mutable-capture | `explorer/explorer_group_capture_tests.cpp`；Boundary lambdas/内存 snapshots/JSON；含 synthetic serializer 分支，无实际 native capture |
| windows-explorer-glue-profile-unit | `explorer/explorer_glue_profile_tests.cpp`；本地 error/QPC/atomic CPU fixture，没有窗口操作 |
| windows-text-encoding | `text_encoding_tests.cpp`/`text_encoding.cpp`；JSON 与 WideCharToMultiByte，无 HWND |
| windows-owned-operations-unit | `operations/owned_window_operations_tests.cpp`/实际 registry/apply；ledger/bridge、空请求 preflight；registry 无 active HWND，构造仅分配 mutex/ledger，empty apply 在 native resolve 前返回 |
| windows-companion-unit | `companion/companion_session_tests.cpp`；独立 token ledger/协议/geometry 模型，不启动 companion |
| windows-explorer-unit | `explorer/explorer_session_tests.cpp`；ledger/eligibility/consent/lease/geometry 事实模型，不构造实际 Explorer session |
| windows-explorer-glue-session-unit | `explorer/explorer_glue_session_tests.cpp`；纯 snapshots/layout 与内存 QuantumFixture/coordinator，不启动事件源 |
| explorer-group-layout | `explorer/explorer_group_layout_tests.cpp`；snapshot topology |
| explorer-group-readiness | `explorer/explorer_group_readiness_tests.cpp`；fake capture lambda/preview/accepted snapshot |

上表除 core/text 文件外，源码前缀为 `src/platform/windows/`；全部 label=`offline`。

| interactive CTest | 实际副作用依据 |
|---|---|
| windows-magnet-owned-probe | `operations/magnet_native_probe.cpp:24` 创建/显示 owned HWND；`:52` SetWindowPos |
| windows-explorer-glue-activation-unit | `explorer/explorer_glue_activation_tests.cpp:109` 创建 HWND，`:166`/`:197` 实际定位 |
| windows-explorer-vdm-unit | `explorer/explorer_virtual_desktop_manager_tests.cpp:108` 创建 frame，实际 VDM 查询 |
| windows-explorer-glue-event-source-unit | `explorer/explorer_glue_event_source_tests.cpp:93` 创建 HWND，actual HWND event-source seams |
| explorer-group-batch | `explorer/explorer_group_batch_tests.cpp:46` 创建两 HWND，实际 HDWP |
| explorer-group-event | `explorer/explorer_group_event_tests.cpp` 创建 HWND并查询实际 HWND identity |
| sta-console-pump | `console/sta_console_line_reader_tests.cpp:55`/`:90` message-only HWND；`:143` private-console GUI child |

这些即使窗口 hidden/message-only、没有 SendInput，也不能进入 no-GUI 阶段；label=`interactive`，本轮未执行。

## 正向入口与已执行检查

入口：`scripts/run-r1c4b-offline-tests.ps1`。只接受配置/build 与只读 `-ListOnly`，没有用户 regex/repeat/rerun-failed/default-all 入口。

1. 从既定 build cache 绑定 repo/CTest，查询本机 version/help 和完整 JSON。
2. 每一注册项必须命中实读 manifest、唯一正确分类、精确 exe/无新参数；空、未知、缺失/mixed 标签或 fixture/dependency 扩展直接拒绝。
3. 构造 anchored/escaped 名称正向 regex，同时 `-L '^offline$' --no-tests=error`；再次 show-only 确认所选命令精确等于审计 offline 集合。
4. 唯一执行分支复用同一过滤，加 `--stop-on-failure --output-on-failure`；不存在未过滤 fallback。

实际命令均在仓库 cwd：

- `powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c4b-offline-selection.ps1`：74 synthetic checks，exit 0；包括 interactive relabel、缺分类/漏注册、空集合、额外参数/exe、fixture 隐式加入、preview contamination 与 AST execution fence。
- 同入口 `-Configuration Debug -ListOnly`：原 23 项正向名单，exit 0。
- `powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/run-r1c4b-offline-tests.ps1 -Configuration Debug`：原 23/23；新增 pure 后 24/24，exit 0。
- 同上 `-Configuration Release`：原 23/23；新增 pure 后 24/24，exit 0。
- 两配置仅构建 `panebind-owned-abort-diagnostic-tests`、`panebind-test-input-environment`：exit 0。环境 CLI 无 add_test，未由本审计 agent 执行；Source/receiver/window/输入 probe 从未启动。
- 完整默认 CTest：**NOT_RUN**。

新增 pure 的第一次 Debug log 为 50 checks，随后 52 checks；最终增补“已发 UP、尚无 receipt 不可重复 UP”反例后冻结为 53 checks。主 agent 已完成两配置 fullbuild（exit 0），并重新运行上述正向离线入口：Debug/Release 各 24/24、exit 0，实际 LastTest.log 均确认 pure 53 checks PASS。本子任务未并发重复构建。完整默认 CTest 仍为 NOT_RUN；这些均是纯模型，不能充当 cleanup empirical PASS。

## 新 v5 runner：仅编写与离线验证

新增 `run-r1c4b-input-reliability.ps1` 与 `run-r1c4b-input-reliability-gates.ps1`。本子任务没有执行其程序，没有执行环境 CLI、native probe 或 GUI；未来真实运行仅由主 agent 在 clean implementation checkpoint 和另行 host 授权下执行。

- single 在任何只读/native CLI 前检查 clean HEAD；aggregate 固定初始 HEAD、每步 fresh HEAD/status/hash inventory，并把 `ExpectedHEAD` 传给 single 在首次环境检查前核对。source、完整 7 文件 validator/import 链、两个 runner 和同配置两 binary 共 19 个哈希按 pre/post 绑定。
- 只读环境 schema、布尔值、HWND/PID/TID/IL/session/QPC/频率使用实际类型验证；WTS level 1、Default/WinSta0、可靠前后 identity/IL 与固定 11 项 UP 全部满足才 READY。字符串数值/布尔、浮点、invalid desktop 的零、UNKNOWN 不得转换成 UP。
- native controlled-abort exit 2 原样记录；只有独立 validator 的完整 `PASS_EXPECTED_ABORT` / gesture test-fault / cleanup PASS / takeover NOT_RUN 证据才映射 runner 0。普通 BLOCKED/native2 仍 STOP2；normal 必须 native0/full PASS。
- aggregate 只有固定 Debug/Release × Move/BottomResize × controlled-abort/normal 的八步顺序，无 repeat/formal/smoke 权限，第一次意外失败停止。失败项在判断 child exit 前保留 metadata/log SHA、native exit、四项 verdict 和具体理由。
- single 保存完整当次 pre/post 只读数据及各自 SHA、退出码。preblocked 不执行 native，不补一次 post 检查；其可靠 LEFT 观察可记 OBSERVED_UP/DOWN。native 之后没有可靠 post 时为 UNKNOWN，不继承 pre 或上一项的 UP。aggregate 独立重读 SHA 绑定原始只读证据得到当次 `FinalButtonState`；失败 fixture 的可靠 OBSERVED_UP 不等于 fixture PASS，也不授权下一项。
- 新目录、新日志和 metadata 均不覆盖已有 evidence；metadata 使用 CREATE_NEW。失败不注入额外 UP、不 retry、不切换 desktop、不改持久配置。

实际执行 `powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c4b-input-reliability-runner.ps1`：294 checks、exit 0。它只提取实际 runner 的审核函数/AST，以严格内存 content/hash/validator 接缝验证上述契约、真实量级 Int64 QPC、完整 inventory、nonce 防复用、期间 hash 变更与失败状态分类；没有 child/native/environment CLI、Git、GUI、原始 UAT 读取。两 live runner 与新测试 parser 均 0 errors。此结果为 AUTOMATED TESTED（synthetic），不是 GUI/cleanup/takeover empirical PASS。

## Exit code：记录与可证明边界

旧失败 metadata 原 SHA `3F733C50…D53D16` 实际复算匹配：`ProbeExitCode=2`、`Result=BLOCKED`、`CurrentContractsVerified=true`。旧 inventory SHA `C97A4C6D…D6257C` 匹配：attempt 14 的 child `ExitCode=2`、`STOPPED`、`Passed=false`，具体 reason 保留。旧 source 的两 runner 都显式 failure `exit 2`；未修改。

`powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c4b-exit-propagation.ps1`：14 checks，exit 0。它只启动此脚本自己的 console fake layers，不执行真实 native probe：

- ProbeStub process 2 → ChildStub 2 → AggregateStub 2，JSON `STOPPED` 且无 retry。
- 外层 PowerShell implicit `-Command`/EncodedCommand 返回 1；显式 `exit $LASTEXITCODE` 返回 2。
- 实际工具调用 `powershell.exe ... -Layer AggregateStub` 得 `exec_command.exit_code=1`，stdout 仍 child2/STOPPED。
- 相同调用最后加 `exit $LASTEXITCODE`，工具实际返回 2。

这是本机 synthetic host mapping 的直接观察，符合 [Windows PowerShell 5.1 官方 CLI 合同](https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.core/about/about_powershell_exe?view=powershell-5.1)：implicit Command 可把非 0/1 转为 1，显式 exit 保留具体值。不能反推旧调用 wrapper 的全部实现或唯一原因；`HistoricalToolWrapperCause=UNKNOWN`。旧 aggregate 终端 process exit 缺独立原始观测时仍不能靠代码推为 observed 2。未确证仓库映射错误，未修旧 runner。

结论：`FIXF_OFFLINE_TEST_SELECTION=PASS`（以上正向 subset）；新模型仅 `AUTOMATED TESTED`。当前输入是否 released、GUI cleanup/takeover 是否通过，均由主 agent 的另行明确入口与证据决定，本审计不宣称。
