# R1-C4B MVP1 — Overlay authorization continuation

用户现在可以做什么：审阅新的交接顺序模型和测试专用诊断代码；**还不能启动三窗 Explorer 实时磁吸原型或执行真人试用**。2026-09-28 用户已明确授权单次、已确认普通 Move 手势期间使用覆盖整个虚拟屏幕的 PaneBind 自有非激活隔离窗，因此上一报告的 *scope decision* blocker 已解除；本报告记录的是随后发现的测试执行安全门，不再请求同一范围的授权。

## 固定起点与完成的离线工作

- 分支 `codex/r1c4b-live-magnet`；起点 `a5ba3c5a6356a42c95a90614d73141173267f22f`；起始工作树 clean。标准 `git fetch origin` 成功，`origin/main=81e40facf52ffdb96f76e4d737740167d485f4a7`，起点相对 `origin/main` 为 `0 behind / 38 ahead`。
- 新增平台无关 `MoveHandoffGate`：exact 普通 Move、实际隔离 ready、唯一 cancel、matching native END 与每 quantum fresh authority 的顺序门；Raw UP 立即撤 writer，legacy UP 与隔离正常销毁分离；deadline/stop 走失败逃生。它不是 Win32 权限证明。其失败原因使用 enum，不保留悬垂字符串引用。对应离线测试通过。
- 新增 `BUILD_TESTING` 下的 owned 全虚拟屏幕 layered-overlay 候选实验程序，设计使用非零 alpha、非穿透、`WS_EX_NOACTIVATE`，并为每显示器中心命中、Raw/legacy UP 与 teardown 预留独立事实字段；这些事实**尚未采集**。静态检查后，其全部合成输入 CLI 场景在建窗/建证据文件/发送输入前返回 `NOT_READY`，以防不安全现场运行。它不链接进产品。
- 新增 `BUILD_TESTING` 下只读 Explorer DOWN/START 归因诊断：复用三次 nonce 目录、baseline exclusion 与 exact consent binding，记录 Raw `MSG.pt/time`、当前 hit-test、WinEvent START/END、foreign GUI/几何事实；**未运行**，不取消、不建遮罩、不写窗。历史 R1-C2A 研究拒绝把直接启动 Explorer 或 `/n`、`/separate` 当成全新 HWND 的保证，故诊断保留人工创建 nonce 窗口，不导航或绑定旧窗。
- `scripts/test-r1c4b-offline-selection.ps1` 的合成白名单审计 `76 checks PASS`；`scripts/run-r1c4b-offline-tests.ps1` 的 Debug、Release 正向离线清单各 `26/26 PASS`。两个新诊断目标均在 VS18/MSVC、Windows SDK 10.0.26100.0 的 Debug 与 Release 构建 PASS；未启动旧 GUI 测试清单。

## 现场安全门及实际结果

测试程序若在自有窗口的模拟 LEFTDOWN **之后、全屏遮罩 ready 之前**遭遇 source/foreground/desktop/capture 失效，既有 Fix H test fence 与本次新程序都不能证明随后模拟 LEFTUP 必定命中自有窗口。把 UP 发向未知 root 可能影响用户已有应用；不发又可能遗留全局按下状态。旧 Fix H `100/100 PASS` 只属于原版受控正常轨迹，不是这些新异常分支的自动恢复保证。若遮罩已建立但捕获/前台在核验到发送之间变化，也存在非原子残余风险。没有证据可把该缺口填成隔离 PASS。

因此本轮**未运行** owned overlay 的 SendInput、native cancel、实际遮罩显示或 Move writer；也未进入 Explorer 归因或三窗集成。显式 `--run-owned-overlay-probe --scenario normal` 仅实测得到 `NOT_READY`，未创建证据文件/窗口或发送输入。这是安全拒绝结果，**不是** overlay 机制失败的经验反例。现有已验收 Fix H 原始证据、纠错包和 v5 validator 均未改写；`uat/`、ZIP、EXE 未进入 Git。

要解除现场阻断，需要先取得一个能承受模拟按下后异常的隔离测试环境（例如无用户数据的可丢弃 Windows 会话/VM），或设计并独立证明在 pre-overlay、cancel↔END 等阶段均有效的自有安全释放路线。不能仅把旧 Fix H fence 接入新程序、放开 `NOT_READY` 开关，或要求用户在当前桌面替代执行未解决的架构实验。全屏隔离的产品机制本身仍是候选，不能称“唯一可行方案”。

```text
FIXH_CORRECTED_REPLAY = PASS (historical accepted evidence)
MVP1_SHARED_MOVE_TAKEOVER = PARTIAL_CORE_ONLY
MVP1_OWNED_LIVE_MAGNET = NOT_RUN
MVP1_EXPLORER_MOVE_MAGNET = NOT_RUN
MVP1_CTRL_GLUE_SAME_ENTRY = NOT_RUN
MVP1_INPUT_HANDOFF_AND_CLEANUP = BLOCKED_BY_SAFE_TEST_INPUT_RECOVERY
MVP1_NATIVE_RESIZE_PRESERVED = NOT_REVALIDATED (product path unchanged)
MVP1_RELEASE_FUNCTIONAL_RUN = NOT_RUN
MVP1_TRYOUT_CANDIDATE = BLOCKED
PRODUCT_RAW_INPUT = NOT_IMPLEMENTED
PRODUCT_INPUT_ISOLATION = NOT_IMPLEMENTED (test-only candidate disabled)
PRODUCT_SENDINPUT_DEPENDENCY = NONE
PRODUCT_GLOBAL_INPUT_HOOK = NONE
PRODUCT_DLL_INJECTION = NONE
PRODUCT_POLLING = NONE
PHYSICAL_INPUT_HUMAN_UAT = NOT_RUN
VISUAL_TERMINAL_RESTORE_FLICKER = NOT_HUMAN_TESTED
FULL_R1C4B_ACCEPTANCE = NOT_CLAIMED
PR / MERGE / TAG / RELEASE = NO
```
