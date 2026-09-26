# R1-C4B Fix C — test-only WinEvent END 与分项诊断

Base `10d429706b8afb3be11f308370af89964a5a054a`；本轮仅自建空白窗口
free Move / free Bottom Resize。既有 [Fix B 单次 Move 正例](../reports/R1C4B_PIVOT1_FIXB_EXECUTION_REPORT.md)
保持有效；旧 Resize 的复合失败仍不能事后猜测根因。研究与 actual inspection
见 [Fix C research](../research/R1C4B_PIVOT1_FIXC_END_DIAGNOSTICS.md)。

## 两条 END witness，不同时间含义

独立 test EXE 新 opt-in CLI：
`--run-owned-end-diagnostics-test --gesture move|bottom-resize --evidence-log NEW_FILE`。
v3 schema `r1c4b-takeover-owned/v3`；旧 v1/v2 CLI、validator、raw evidence
与报告保持历史合同，不追认旧结果。R0 observer / 产品 runtime 不修改。

一个 fresh run 只有一个实际手势、一个 source、一个 hidden receiver、一个
fresh WinEvent hook；重复 gate 的每个 Move/Resize pair 使用两个独立 fresh
run，不复用句柄/receiver/hook，避免旧 gesture 混入当前 source。
hook 按 exact PID/UI thread 观察 MOVESIZESTART→MOVESIZEEND，只有
WINEVENT_OUTOFCONTEXT，没有 SKIPOWNPROCESS/SKIPOWNTHREAD、DLL 或 injection。
安装线程持续 pump，所有退出路径在同线程检查 Unhook 返回并记录。

callback 只记录实际 envelope、独立 callback QPC/sequence，经有界 ingress
通知 owner；不 capture、泵消息、读取 COM 或写窗口。owner matcher 检查
hook/run/source/thread、gesture arm 与真正 START→END 配对；无 START、错
HWND/thread、旧 END 不解锁。callback 时间是异步 receipt，不是 OS 内部 exit
instant。真实 native EXIT 和匹配 WinEvent END 都必须存在；先到的 witness
不授予写入权限。首个 native write 必须严格晚于两者 QPC。

## 分阶段 preflight，诊断先于决定

native EXIT 后只记录 `handoff_preflight phase=native_end`，不写；matching
WinEvent END 后 owner 下一工作重新 fresh capture/proof 并记录
`phase=winevent_end`，才决定是否 handoff。不是按时间重试或轮询。
诊断包含全部 identity/desktop/foreground/GUI/capture/cursor/buttons/Raw health/
foreign capture/DPI/monitor/terminal actual P/V availability 与 equality、native
DRAG/无归属 geometry facts，保留查询成功/失败，而不是一个 compound bool。
同时记录优先级 failure class 和全部 failed checks，validator 独立重算。

缺失 native/WinEvent END 不写；环境或 authority 不可信保持 BLOCKED，不冒充
geometry counterexample。可信 matching END 后 actual P/V 仍偏离 native EXIT
terminal，或 native DRAG/无归属 geometry 再现，FAIL/STOP。native EXIT 到
WinEvent END 期间的 visible lag 可作为 candidate 记录，匹配 END 后再次实际
读取；不增加 DWM 宽限、Sleep、timer retry、重复 SWP 或容差。callback→owner
处理之间的真实 DRAG/POS 也按实际 QPC 回看，不用迟到的 match bool 隐藏。

## 意图与写入数学保持不变

terminal actual 只证明稳定；原始 DOWN pointer 与 native START P/V 冻结为
intent anchor。Move 使用完整 dx/dy；Bottom Resize 只变 bottom，left/top/right
不变，以现有 checked visible→positioning bridge 交叉验证。
handoff actual 与 intended 不同才最多一写；随后实际 Raw movement 通知 owner
当前 cursor 一次、原始 anchor 全量计算、每 quantum 最多一写。immediate/full
P/V 严格 exact，Raw UP terminal 无末端补写；cleanup 不属于 acceptance。

## 失败清理与执行 gate

先退休 writer/acceptance scope，记录独立 CleanupInputDiagnostic。只有 fresh
exact source、desktop/foreground、无 foreign capture/menu/move-size、own cursor
root、held left、其余输入清才允许一次 test-only LEFTUP。保留 receiver armed
以观察实际 cleanup Raw UP，独立 final high bit/capture/cursor；之后注销。
authority 不足就 NO INPUT + `cleanup_skipped_no_authority`。无论清理是否成功，
都不能补足失败 gesture 的 Raw UP/final exact 或令失败 run PASS。

单次 runner `scripts/run-r1c4b-end-diagnostics.ps1` 不自动进入下一项。
执行严格顺序：Debug BottomResize×1 → 完整 PASS 才 Debug Move×1 → 两者 PASS
才 `run-r1c4b-end-diagnostics-gates.ps1`，独立 Debug5/Release5 smoke 全 PASS 后
才 Debug20/Release20 formal。自动脚本需要两个当前 clean implementation/
binary 的独立完整 PASS metadata，每 pair Move 与 BottomResize 都通过才计数；
任一失败保留第一条证据并停止，无 retry-until-pass。只有唯一 guard failure
才可另行按任务书修正 test-only guard geometry，不能自动扩大 authority。

完整 owned gate 后亦 STOP，Explorer 最多 READY_FOR_NEXT_STAGE。本轮没有
Magnet integration、Explorer 操作、Human UAT、product Raw Input/SendInput、
global mouse hook、DLL injection、resident polling、PR/merge/tag/release。
实际结果见 [Fix C 执行报告](../reports/R1C4B_PIVOT1_FIXC_EXECUTION_REPORT.md)：
两次 Debug BottomResize 均在 matching END 后因唯一 cursor root authority
阻断、零写入；一次获准 guard geometry 修正后覆盖 +50px 已通过，但 actual root
仍非 source/guard。Structured diagnostic PASS，复合 WinEvent handoff gate FAIL，
architecture UNRESOLVED。旧 B Move PASS 保持；C Move/repetitions/Explorer NOT_RUN，
cleanup SKIPPED_NO_AUTHORITY，停止后续交互与权限修改。
