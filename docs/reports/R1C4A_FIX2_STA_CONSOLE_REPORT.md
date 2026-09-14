# R1-C4A Fix 2 — STA-safe console wait 交接

日期：2026-09-14。范围：P1 UAT infrastructure；未执行修复后真人 UAT。

## 结论与诊断边界

全部 C4A 人工输入已统一改为 owner 线程上的 event/message wait + nonblocking
console record reader。没有 input worker；没有重写 active gesture pump。
人工 readiness resize 明确允许，但 active Glue Resize 仍 unsupported/fail-closed。

基线 `read_line()` 使用同步 ReadConsoleW，而同一线程持有 Explorer/Shell STA
对象。代码缺陷与 Microsoft STA 契约明确吻合；用户报告的 resize 卡住与之
高度一致。没有采集 Explorer hang dump，未声称已确定具体被阻塞的 RPC 栈。
本问题归为 `BLOCKED_BY_STA_CONSOLE_WAIT`，不是 Glue Resize 或动态组 runtime
失败。修复后的真人响应仍 NOT TESTED，不能用自动 probe 替代人工结论。

研究、原始平台链接、两个强制成熟参考及历史见
[R1C4A_STA_CONSOLE_WAIT.md](../research/R1C4A_STA_CONSOLE_WAIT.md) 和
[SOURCE_PROVENANCE.md](../research/SOURCE_PROVENANCE.md)。未复制或适配外部代码。

## 实现

- 新增 Windows adapter `console/sta_console_line_reader.{h,cpp}`；不包含任何
  Explorer/group/core movement API，不初始化或转移 COM 对象。
- 所有 A/B/C confirmation、group Y、readiness Enter/Q、grade、rigid-body
  response 均调用同一 reader。C4A 源码中的 ReadConsoleW 调用数为零。
- 每轮使用 MsgWaitForMultipleObjectsEx（生产 INFINITE，QS_ALLINPUT +
  MWMO_INPUTAVAILABLE），然后不按 HWND/消息类型过滤地 Peek/Translate/Dispatch。
  每次最多 dispatch 64 个 queued MSG，再处理一个输入 record；持续有工作时
  公平推进，没有工作时阻塞于系统 wait，不用 Sleep、timer 或固定周期 polling。
- 使用官方 ReadConsoleInputExW 的 CONSOLE_READ_NOWAIT，而不是有竞争窗口的
  peek + blocking ReadConsoleInputW。导出从已加载的 Kernel32 动态解析；缺失
  时 fail closed，不回退到 cooked/blocking read。
- 输入最多 255 UTF-16 units，支持 Unicode scalar、Enter、Backspace、repeat；
  key-up/非文本 console records 不成为命令；Escape/Ctrl+C clean abort。
- 输入 mode 仅在 wait 内暂存/调整/恢复并回读核验。禁用 Quick Edit 和 VT input，
  临时关闭 processed input 使 Ctrl+C 可正常返回；不改 code page 或全局配置。
  错误、异常、WM_QUIT、恢复失败均不返回可接受的命令。
- 只对自己的 console 做 echo/cursor editing；不改变 Explorer 几何。若 console
  resize/scroll 导致不能安全定位 Backspace，fail closed，不擦除不确定的输出。

WM_QUIT 被保留给上层。COM/nonqueued dispatch 可发生在 PeekMessage 内部，
`message_dispatch_count` 仅统计显式返回的 MSG，不能解释为 COM call 次数。

## Evidence

startup 新增 `console_wait_contract = sta_message_pump_v1`、
`console_input_contract = readconsoleinputex_nowait_v1` 和 owner thread id。
每次输入仅输出一条 `console_wait`，包含 wait kind、wait/pump/dispatch/input
计数、结果、mode restored 和 error；不记录字符、VK/scan code 或逐 message 内容。

runner 对新 PASS 要求每个人工输入阶段有正确 owner、pump、非阻塞读取 contract
和成功模式恢复；也校验 readiness recheck 与 consent/subjective 的阶段顺序。
即时 Enter 可以有零 queued MSG，不能误判没有 COM dispatch 机会。旧 blocked
日志继续按历史契约识别，不把缺少新字段当成历史 runtime regression。

### 历史现场保留

Fix 1 后现场 `20260913T181520657Z-f512a5df14c84f5084fadb4011199e1e` 的 metadata
指向 BASE_SHA，harness exit 2；日志在 NOT FIT preview 后以 readiness_cancelled
结束，未有 gesture/native batch。其 resize unresponsive 原因来自 Human Root
报告和代码分析，不是从该 JSON 的计数中虚构的观察。机器日志的 layout-only
分类与已知 P1 STA 原因是不同层次，均不表示 Glue runtime FAIL。

全部 ignored evidence 未修改、删除或提交：

| 文件前缀 / 后缀 | SHA256 |
| --- | --- |
| 20260913T152205215Z-4fd64b71a63c4a70980351385310c2ea / jsonl | ACDAD7F0D17086ECA165539E5D0879545E4010C552519F2094A1447A88F41880 |
| 同上 / metadata.json | 75222EDF68CB66CCB957EC39315209676F7526E6C2A9956B81D17DBF446B415E |
| 20260913T181520657Z-f512a5df14c84f5084fadb4011199e1e / jsonl | 085C9B51309D3C288AF620640C24CF915C191BA4FD501CCD4C300625C6CD32A1 |
| 同上 / metadata.json | 41BB94FCD2051F15AD7623FACCCD98A8B0926260DADF64C735AE1BA4D2682770 |

## 自动验证与冻结范围

环境：Windows 10.0.26200；VS 18 2026 x64 / MSVC 19.50；SDK 10.0.26100.0；
Windows PowerShell 5.1；VS bundled CMake/bin 下 cmake.exe / ctest.exe。

| 验证 | 结果 |
| --- | --- |
| Debug / Release build | PASS / PASS |
| Debug / Release CTest | 20/20 PASS / 20/20 PASS |
| Owned Debug / Release self-test | PASS / PASS，failures=0 |
| Companion Debug / Release self-test | PASS / PASS，failures=0 |
| C2B / C3A / C3B Phase 1/2/3 runners | PASS（26 / 61 / 47 / 41 / 28） |
| C4A runner | 78 fixtures PASS（原 66 + 新 12） |
| 原两份 blocked JSONL 兼容回放 | BLOCKED_BY_LAYOUT_READINESS / NOT_STARTED，exit 2 |

新 `sta-console-pump` CTest 在实际 owner STA 创建自己的隐藏 message-only
window，使用真实 MsgWait/Peek/Dispatch 和 event-backed input seam；证明一条/
三条消息的 handler 在行完成前执行，部分字符后等待 Enter 期间也执行消息。
另覆盖即时 Enter、WM_QUIT、Ctrl+C/Escape、空读取后的阻塞 wait、Unicode/
Backspace、容量、错误、模式恢复。没有操控 Explorer 或用户已有窗口；没有
将 synthetic input seam 或 Kernel32 导出可用性冒充真实终端/Explorer UAT。

对 BASE_SHA 执行整目录比较：`src/core/`、`src/platform/windows/explorer/` 和
旧 `explorer_glue_harness_main.cpp` 均无变化。Fix 1 context comparison、accepted
baseline、run_gesture、HDWP、feedback、Ctrl、monitor/DPI 和 capability/consent
generation 规则全部保持；泵送得到的 navigation/quit 等仍由原验证链 fail closed。

复现命令（cmake/ctest 使用 VS bundled exe）：

```powershell
cmake --build out/r1c4a-debug --config Debug --parallel
ctest --test-dir out/r1c4a-debug -C Debug --output-on-failure
cmake --build out/r1c4a-release --config Release --parallel
ctest --test-dir out/r1c4a-release -C Release --output-on-failure
.\out\r1c4a-debug\src\platform\windows\Debug\panebind-owned-window-harness.exe --self-test
.\out\r1c4a-release\src\platform\windows\Release\panebind-owned-window-harness.exe --self-test
.\out\r1c4a-debug\src\platform\windows\Debug\panebind-companion-harness.exe --self-test
.\out\r1c4a-release\src\platform\windows\Release\panebind-companion-harness.exe --self-test
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c2b-evidence-runner.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c3a-evidence-runner.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c3b-profile-runner.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c3b-frame-profile-runner.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c3b-vdm-profile-runner.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c4a-evidence-runner.ps1
```

## 人工 Gate 与停止点

独立 re-review PASS 后，必须在实际 readiness 的“手工缩小、Enter 重新检查”
等待期间由 Human Root 分别 resize A/B/C，确认每个都连续响应、无 freeze/
deadlock，然后才按 Enter。未经过 readiness 等待的 immediate-FIT run 不能
证明这个复现 Gate。`console_wait_contract` 或 dispatch 计数本身也不能证明
真人无卡顿。此人工观察和完整三手势 runtime 均仍 NOT TESTED。

绑定身份、路径、安全、monitor/DPI/VDM/generation 均不放宽；输入期间只是
STA liveness，不执行 follower placement。真实 terminal/IME/echo 组合的视觉
体验尚需人工验证；不是完整 terminal editor，也没有修改 C4B/C4C 产品能力。

## Git 与状态

Branch：`codex/r1c4a-dynamic-group-move`。
BASE_SHA：`88c2e5b9aedb85b92aa7d81b23b1e0f0ecaf29de`。
开始 clean、标准 SSH-over-443 fetch 成功、upstream 0/0。最终 commit / HEAD /
remote SHA、push 和工作树状态记录在最终交接；不在提交中伪造自引用 SHA。

```text
R1C4A_FIX2_STA_CONSOLE_ANALYSIS = PASS
R1C4A_OWNER_STA_MESSAGE_PUMP = PASS
R1C4A_OWNER_BLOCKING_READCONSOLE = ZERO
R1C4A_CONSOLE_WAIT_BUSY_POLLING = NO
R1C4A_READINESS_MANUAL_RESIZE_ALLOWED = YES
R1C4A_READINESS_NATIVE_WRITES = ZERO
R1C4A_ACCEPTED_BASELINE_REBASE = PASS
R1C4A_DYNAMIC_LEADER_MODEL = PASS
R1C4A_BATCH_PLACEMENT_GATE = PASS
R1C4A_MEMBER_SPECIFIC_FEEDBACK = PASS
R1C3B_REGRESSION = PASS
R1C4A_IMPLEMENTATION_READY = YES
R1C4A_HUMAN_UAT = REQUIRED
R1C4A_RUNTIME_GATE = PENDING_UAT
PR = NO
MERGE = NO
TAG = NO
RELEASE = NO
```

完成实现/自动验证/文档/commit/push 后停止，不自行重跑真人 UAT。
