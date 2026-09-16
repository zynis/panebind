# R1-C4B Live Magnet 实现与自动验证交接

日期：2026-09-15 至 2026-09-16（跨日收尾）。**真人 Explorer UAT 未运行；没有 Human Seal。**

## Git / 阶段

| 项目 | 值 |
| --- | --- |
| BASE_SHA | `0aa13cbcc61a272fdc3ab0089b39d69a66381fe7` |
| ARCHITECTURE_SHA | `efa9d8523da21f63a06ccdbc13d96c63c8c75c53` |
| RUNTIME_SHA | `70670906f1da2ede4c1f4c3ca5e867d9ec6bb911` |
| TEST_SHA | `70670906f1da2ede4c1f4c3ca5e867d9ec6bb911`（与 runtime 同一闭环） |
| 分支 | `codex/r1c4b-live-magnet`，从上述 BASE 新建的 stacked branch |
| origin/main 起点 | `81e40facf52ffdb96f76e4d737740167d485f4a7` |

开始前标准 fetch origin PASS，旧 C4A 分支精确位于 BASE，worktree clean，
local/upstream 0/0。未 merge C4A、未修改 main、未改 Git 配置或历史。
REPORT_SHA / FINAL_HEAD / REMOTE_HEAD 在最终交接给出，避免在提交内伪造自引用 SHA。
最终报告提交后重建嵌入 SHA，标准 push，并以标准 ls-remote 核验。

C4A implementation/review 已完成；其 human final seal 正式延后到 C4B integrated UAT，
不是 C4A FAIL。旧 C4A evidence 不会被提升为 C4B PASS。

## 实现闭环

- [架构](../architecture/R1C4B_LIVE_MAGNET.md)与
  [focused research](../research/R1C4B_LIVE_MAGNET_INTEGRATION_RESEARCH.md)
  引用既有 AquaSnap 行为、Magnet/Geometry/Relation/Glue 分层和 frame authority。
  AltSnap GPL 仅参考，没有复制/改写上游代码。
- 纯 `MagnetGestureCoordinator`：START Ctrl latch、首个 meaningful geometry 分类、
  四边/四角 Resize mask、类型冲突 abort、两个 frozen targets、代次与 pending ledger。
  无 Ctrl → Magnet；Ctrl pure Move → 既有 C4A Glue；Ctrl Resize → unsupported/zero writes。
- 单一 `ExplorerGroupEventSource`。没有新 hook、timer、cursor polling、DLL injection、
  Raw Input 或 keyboard content collection。一次 owner quantum 最多一次 solver 和
  一次 Magnet native correction；旧 Glue 仍只用 HDWP follower batch。
- 原 solver 最小扩展：selected_x/y、gesture-local preferences；enter=10/release=16
  是 INITIAL UAT BASELINE，不能宣称最优。已分类 Resize 的参与边可以回到初始坐标，
  但所有非参与边仍冻结；默认未分类的 resize inference 保持严格。
- 速度策略复用 MotionSample/classify_motion，使用 raw geometry 和 receipt QPC；
  初始实验阈值 2000 physical geometry units/s，不是性能 SLA。高速/无效样本清 latch，
  不做 correction。屏幕 live Magnet OFF，窗口 Magnet ON。
- C4B 专属 consent generation/permit 与旧 C4A 授权分开。私有 source-only bridge 每次
  验证 exact member/token/consent/窗口身份、原 anchor/nonce、安全、desktop、monitor/DPI。
  全 preflight 后登记 pending，再一次 SetWindowPos，随后 actual 双矩形 exact postverify。
  Move flags=21，Resize flags=20；不改变 focus/activation/z-order/owner/style。
- 新 checked signed-inset bridge；overflow、crossed/nonpositive 和 native range 拒绝。
  Source raw 被更新抢先时，无 native write 地丢弃 proposal，仅新 receipt 才可再试。
  超出原工作区的 proposal 不应用，不自动缩放/摆放窗口。
- Self-feedback 以身份、capability/consent/group/gesture/operation、watermark、provider tick
  和 exact geometry 归因；duplicate 不再喂 solver。END 只能 reconciliation，不能冒充
  self-LOCATION ACK。没有 ignore-next-event 或固定时间窗口屏蔽。
- 已校正位置不会成为下一步 raw 累加基线。观察到相同 raw 在 correction 后被重申时保守
  abort，防止与系统形成反复写入；这种边界在真实 Explorer modal loop 中的频率尚未验证。
- 每个 END 从 fresh actual geometry 重建 graph；没有 persistent relation registry、
  persistent Leader 或 geometric membership。两/三关系合法、point-only 无关系，Resize
  到 overlap=0 自然 detach。Candidate 不作为 Relation authority。

## Harness / evidence

新 `panebind-explorer-live-magnet-harness` / `r1c4b/v1`，与旧 C4A 分离。
三扇新 Explorer 分别确认后，再提示明确 live correction/Glue/restore 授权。
目录名包含 A/B/C 标签。形成阶段提示粗略拖近、必要时手工缩小；不要求测坐标或凑 0px。
M1/M2/M3、Bottom/Top Resize、detach/reconnect 每项最多 5 次检查，不重新 provisioning。
接受 fresh 三成员连通 topology 后，G1/G2/G3 分别 A/B/C leading，最后 exact restore。

STA console reader 只增加可选 owner work：C4B 等待 Enter/Y 时也按八消息 quantum
处理同一个源；旧读者默认仍为原逻辑，不改变 console modes/复制快捷键。
已通过等待中处理消息、owner affinity、工作失败不得接受 consent 的测试。

记录真实 callbacks、owner quanta、raw samples、corrections/selected constraints、native
flags/单调用/pending/actual/时序、feedback、END graphs、Glue batch/member feedback、
accepted/final snapshots、queue 与 active inventory/manager-create counters。只收集固定
主观选项；无 console text、clipboard、用户文件名或自由 URL。

新 runner 要求 IndependentReviewPassed、clean frozen HEAD、匹配该 HEAD 的 Debug
build identity、运行前后 binary hash 一致、ignored evidence。合成夹具有显式
`synthetic_fixture` 标签，普通 human reader 拒绝它；它们不是真人主观评价。
技术记录完整只返回 REVIEW_REQUIRED，不自动执行 Human Seal。

## 自动验证结果

环境：Windows 10.0.26200 / PowerShell 5.1 / VS 18 2026 x64 / MSVC 19.50 /
Windows SDK 10.0.26100.0。构建限制 parallel=2，关闭本次进程的 MSBuild node reuse。

| 检查 | 结果 |
| --- | --- |
| Debug configure/build/CTest | PASS，26/26 |
| Release configure/build/CTest | PASS，26/26 |
| Owned Debug / Release | PASS / PASS，failures=0 |
| Companion Debug / Release | PASS / PASS，failures=0 |
| C2B runner | PASS |
| C3A runner | PASS |
| C3B Phase 1 / 2 / 3 | PASS / PASS（41）/ PASS（28） |
| C4A evidence runner | PASS，140 fixtures |
| C4A mutable capture / browser / console | Debug/Release CTest PASS |
| C4A capture serializer | 两配置各四种 synthetic stage PASS |
| 既有 Magnet Core | 两配置各 2,691 checks PASS |
| 新 live gesture unit | 两配置各 168 checks PASS |
| Rect bridge unit | 两配置各 8 checks PASS |
| Native owned geometry probe | 两配置各 14 calls，failures=0 |
| C4B evidence fixtures | PASS，29（明确 synthetic only） |

Native probe 的前 11 个普通 X/Y/XY Move、四边/四角 Resize 全 exact；第 12 个有意
clamp 的测试按预期 nonexact，不重试；最后两项把 pure solver 与真实 owned source
SetWindowPos 组合，+7 gap / -4 overlap 都一次 exact。这两项 target geometry 为
synthetic，不冒充实际第二窗口或 Explorer 观察。Probe 未控制第三方窗口。

主要命令：

```powershell
$cmake = 'D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ctest = 'D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
$env:MSBUILDDISABLENODEREUSE='1'
& $cmake -S . -B out/r1c4b-live-magnet-debug -G 'Visual Studio 18 2026' -A x64
& $cmake --build out/r1c4b-live-magnet-debug --config Debug --parallel 2 -- /nodeReuse:false
& $ctest --test-dir out/r1c4b-live-magnet-debug -C Debug --output-on-failure
& $cmake -S . -B out/r1c4b-live-magnet-release -G 'Visual Studio 18 2026' -A x64
& $cmake --build out/r1c4b-live-magnet-release --config Release --parallel 2 -- /nodeReuse:false
& $ctest --test-dir out/r1c4b-live-magnet-release -C Release --output-on-failure
# 两配置分别运行：
& "$build/src/platform/windows/$config/panebind-owned-window-harness.exe" --self-test
& "$build/src/platform/windows/$config/panebind-companion-harness.exe" --self-test
& "$build/src/platform/windows/$config/panebind-magnet-native-probe.exe"
```

各 PowerShell runner 使用 `powershell.exe -NoProfile -ExecutionPolicy Bypass -File`：
test-r1c2b-evidence-runner、test-r1c3a-evidence-runner、test-r1c3b-profile-runner、
test-r1c3b-frame-profile-runner、test-r1c3b-vdm-profile-runner、test-r1c4a-evidence-runner、
test-r1c4b-evidence-runner；另运行 test-r1c4a-capture-json 的 Debug/Release 参数。
新 runner 只测试了无 review flag 的拒绝路径；没有运行真人路径。

## 证据等级与风险

IMPLEMENTED / AUTOMATED TESTED：上述 Core、owned native bridge、routing/feedback
协议、schema 和 baseline regression。Active inventory/VDM manager creates 的 **0**
是已实现并由 counter guard/自动 fixture 检验的 contract；没有伪称测得真人 Explorer
的 active 计数或平滑度。真实 p50/p95 只能从下一次人类日志计算，目前不填写假数字。

NOT TESTED：真实 Explorer modal-loop 反馈时序、START capture 延迟、preflight 被新 raw
抢先的频率、reassertion abort 的实际体验、10/16 与速度阈值的手感、真实一至多轴吸附、
三窗口 integrated Glue 顺滑度、CPU/常驻内存目标与尾延迟。应用 clamp 或身份变化仍会
abort，不会反复 native 补写。真实场景若出现 storm/jitter/queue runaway 必须 STOP。

## Human handoff（2026-09-16 Fix 1 / Amendment 001 暂停）

上文是 `f53de795` 之前的历史交接，不代表当前 readiness。
首次真人日志已失败；见 [Fix 1 取证报告](R1C4B_FIX1_FORENSICS.md)。
当前 `CURRENT_LIVE_MAGNET_NATIVE_ARCHITECTURE=UNRESOLVED`，不得再次请求真人重测。
今后必须先通过同 SHA 的真实 Explorer 自动交互门禁，再通过独立 review。
runner 当前硬阻止启动；自动门禁实现尚未获 architecture gate 放行。

R1C4B_HUMAN_UAT=NOT_READY。
R1C4A_HUMAN_FINAL_SEAL=PENDING_INTEGRATED_UAT。
C4C / C4D NOT_STARTED；PR / merge / tag / release NO。全部原 UAT 文件保留本地，不提交。
