# R1-C4 架构修正与独立 Review 交接

日期：2026-09-15。范围：C4A topology-neutral closure + C4B pure Magnet。
**不是真人验收 Seal**。本轮不运行真人 UAT，不创建 PR、不 merge/tag/release。

## 起点与 Git

- 分支：`codex/r1c4a-dynamic-group-move`。
- Starting HEAD：`cce4f36bb4237fb7e4444f1da21e8f75012ba7e3`；起点 clean。
- 标准 `git fetch origin` 成功，`origin/main` 为
  `81e40facf52ffdb96f76e4d737740167d485f4a7`，`origin/main...HEAD = 0/5`。
- 不创建新分支，不改历史或 Git HTTP/TLS/proxy 配置。
- 最终 SHA、提交、标准 push、远端 SHA 和 divergence 在最终交接中记录，
  不在提交内容中伪造自引用 SHA。`uat/` 继续 ignored，仅保留本地。

## 指定真人证据取证（只读）

文件：`uat/r1c4a/20260915T032616207Z-e969f05816e2419b891201ded128ece0.jsonl`。
SHA256：`88A95D4E5FE7C0D7FB5E0D4DD04737392772DCA08E4C6990730F3DF006357DAB`。
请求检查末尾 20 条，但全文件只有 **12 条**；以下覆盖全部 12 条。
JSONL 全部合法，sequence 1–12 连续，startup/shutdown 各一条。

| sequence | type | 实际事实 |
| --- | --- | --- |
| 1 | startup | schema r1c4a/v1；pid 28024；owner STA 29596；Fix 3 mode contract；旧 live-preview contract |
| 2 | target_prompt | member A (0)，baseline/prompt generations 1/2 |
| 3 | console_wait | member_a_confirmation，complete，wait/pump/input 4/4/4，dispatch 0，error 0 |
| 4 | target_confirmed | A；confirmation/eligibility/token 3/4/5；exclusion/unique/location/token flags true |
| 5 | target_prompt | B (1)，新的 nonce 目录，generations 1/2 |
| 6 | console_wait | member_b_confirmation，complete，4/4/4，dispatch 0，error 0 |
| 7 | target_confirmed | B；generations 3/4/5；四个安全确认 flags true |
| 8 | target_prompt | C (2)，新的 nonce 目录，generations 1/2 |
| 9 | console_wait | member_c_confirmation，complete，6/6/6，dispatch 0，error 0 |
| 10 | target_confirmed | C；generations 3/4/5；四个安全确认 flags true |
| 11 | console_wait | group_consent，complete，138/138/4，dispatch 134，error 0 |
| 12 | shutdown | result BLOCKED，reason **group_declined** |

四次 wait 均观察到 input mode 503 → 503，modes_observed=true，
mode_changed=false。target_prompt=3，target_confirmed=3，group_consent **不存在**，
binding=0，readiness/accepted/gesture/batch=0。目标确认记录不含实际 HWND/PID
binding，因此不臆造三个目标的 native identity。

`CURRENT_HUMAN_EVIDENCE_ACTUAL_REASON = group_declined`

实际阶段：**BLOCKED_DURING_GROUP_CONSENT**，尚未 group bind / topology preview /
native setup / gesture。日志没有保存用户字符，不能推断键入了什么或为何未同意。
这不是“不满足固定 L”或运行时失败；旧 runner 在读取 shutdown 原因之前
强制检查三个 binding，才显示了误导性的 `exact authorized set`。
新 runner 对同一原始文件输出真实 stage/reason、四次 wait、3/3/0 计数，exit 2；
没有启动 harness 或生成替代真人日志。

## 当前实现与证据等级

| 项目 | 结果及依据 |
| --- | --- |
| 官方行为研究 | 已实际读取 Nurgo 七项页面与 EULA，AltSnap / FancyZones 固定版本代码、许可证、历史；见研究与 provenance |
| 四概念分离 | Magnet proposal / actual geometry / derived relation / gesture Glue；无 persistent Leader 或 geometric membership |
| C4A fixture | 手工 Move + Resize；只读 fresh preview；显示三对边方向/gap/overlap、三个 component；两或三关系均可；无等尺寸要求 |
| 接受/恢复 | READY 时 Y；再次 fresh validation；accepted baseline rebase；之后才启动 source；最终恢复 accepted 而非 binding-time geometry |
| 预接受写入 | 生产 setup 不再调用 apply_targets/任何 native placement；fixture 零 activity guard 与负例；源码边界审查 |
| BLOCKED 分类 | 先读 shutdown；阶段化 partial evidence，保留原 reason；进入 runtime 则要求完整授权；BLOCKED 不等于 runtime PASS |
| PASS 严格性 | 仍要求三个真实授权目标、A/B/C 手势、Ctrl/事件来源、双 Follower batch、反馈归属、timing、精确 restore；禁止 synthetic setup |
| C4B pure Core | 独立 C++20 header；只接受几何/逻辑 ID；Move 刚体，Resize 仅参与边；最多一个 combined proposal；无 native authority |
| Relation truth | 复用未修改的 R1-A graph；point-only 无关系，几何脱离自动消失；solver 不建立 relation |
| Fast motion | 已有输入 tick/frequency + edge delta 的确定性 binary policy；无 polling；阈值由 caller 明确提供 |
| Live Magnet | NOT_IMPLEMENTED；本轮不接 Explorer，不宣称 live smoothness 或性能测量 |
| C4C / C4D | shared-boundary Resize 设计问题和原创小锁 UX 文档；无实现 |

外边/内边/角点行为与 PaneBind 的独立 ranking/hysteresis/fail-closed 选择已分开标注。
吸附距离 10 只是 REFERENCE BASELINE。候选扫描 O(N)，default bound 64 / hard 256；
两次选择加 final ranking 稳定性检查，冲突不收敛则无 correction。输出 facts 为确定顺序
排序 O(N log N)，不隐瞒为全路径严格线性。尚未 profile 实际 hot path。

## 回归执行记录

环境：Windows 10.0.26200，PowerShell 5.1，VS 18 2026 x64，MSVC 19.50，SDK 10.0.26100.0。
CMake/CTest 来自 VS bundled CMake bin，未安装/修改全局工具链。

```powershell
$cmake = 'D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ctest = 'D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
& $cmake -S . -B out/r1c4-topology-debug -G 'Visual Studio 18 2026' -A x64
& $cmake --build out/r1c4-topology-debug --config Debug --parallel 2
& $ctest --test-dir out/r1c4-topology-debug -C Debug --output-on-failure
$env:MSBUILDDISABLENODEREUSE='1' # 当前构建进程；非持久配置
& $cmake -S . -B out/r1c4-topology-release -G 'Visual Studio 18 2026' -A x64
& $cmake --build out/r1c4-topology-release --config Release --parallel 1 -- /nodeReuse:false
& $ctest --test-dir out/r1c4-topology-release -C Release --output-on-failure
```

首次 Debug 使用无上限 `--parallel`，出现 MSVC C1060 heap exhaustion；限制并发后成功。
后来主机可用物理/提交空间降至约 500 MB，多次 PowerShell 在 CLR 启动阶段失败，
部分 Release/runner 命令未能开始；待资源恢复后以单并发、禁 node reuse 重试。
用户再次要求重试后，资源检查确认已恢复，最终增量构建采用两并发并禁 node reuse。
一次旧 runner 汇总进程在会话恢复后无法取回尾部结果，完整六套 runner 已重新执行，
不将缺失输出当作通过。
未关闭任何用户应用，未修改系统内存/分页文件设置。

自动测试最终结果如下；所有结果均为 AUTOMATED TESTED，不是真人 Explorer/UAT。

| 自动回归 | 结果 |
| --- | --- |
| Debug / Release 全量构建 + 最终增量构建 | PASS / PASS |
| Debug / Release CTest | 21/21 / 21/21 PASS |
| Magnet 独立检查（两配置） | 各 2,691 checks，failures=0 |
| Owned Debug / Release --self-test | PASS / PASS，failures=0 |
| Companion Debug / Release --self-test | PASS / PASS，failures=0 |
| C2B evidence runner | PASS |
| C3A evidence runner | PASS |
| C3B Phase 1 profile runner | PASS |
| C3B Phase 2 frame profile runner | PASS，41 fixtures |
| C3B Phase 3 VDM profile runner | PASS，28 fixtures |
| C4A evidence runner | PASS，118 fixtures |
| 原真人 BLOCKED 日志只读重放 | group_declined / GROUP_CONSENT，exit 2 |
| 文档本地链接 / git diff --check | PASS / PASS |

S/T 在 C4A layout/readiness 测试及 runner；A–R 和 checked arithmetic / bound /
duplicate / permutation / compatible facts 在 Magnet tests。C4A Core gesture tests
还将完整 A/B/C、成员专属 ACK、END roles-cleared 循环在不等尺寸三边图上重新执行。

其他精确命令（`$build/$config` 分别取上述 Debug、Release 目录和配置）：

```powershell
& "$build/src/platform/windows/$config/panebind-owned-window-harness.exe" --self-test
& "$build/src/platform/windows/$config/panebind-companion-harness.exe" --self-test
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c2b-evidence-runner.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c3a-evidence-runner.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c3b-profile-runner.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c3b-frame-profile-runner.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c3b-vdm-profile-runner.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c4a-evidence-runner.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/run-r1c4a-group-evidence.ps1 -ValidateEvidencePath uat/r1c4a/20260915T032616207Z-e969f05816e2419b891201ded128ece0.jsonl
```

最后一个命令仅 Validate；没有传 `-IndependentReviewPassed`，没有运行真人 harness。
production setup 代码无 native write；自动 console 测试仅操作自己的测试 console。

源码差异检查：`ExplorerGroupSession::attribute` 起直到文件末尾（包含 gesture/
quantum/restore）相对 starting SHA 字节等价（统一换行后）；Core behavior、HDWP batch、
group ingress、Explorer authority、console helper 均无差异。未修改 R0/C3B 语义。

## NOT TESTED 与停止边界

- 新拓扑中立 fixture 的真实三 Explorer Ctrl+Move、人工尺寸调整、实时手感、接受/恢复。
- Fix 3 的真实 VS Code / Windows Terminal 选取复制粘贴，仍需独立 human review。
- Live Magnet 的目标 freshness/occlusion/authority、native clamp、feedback/self-adjustment、
  resize inference 实机一致性、monitor/DPI 变化；独立 design gate 尚未打开。
- Fast-motion 阈值与迟滞调参、CPU/内存/FPS/尾延迟；没有声称实测。
- Smart Docking、AquaStretch UI、C4C shared-boundary Glue Resize、C4D lock overlay。

实现完成并标准 push 后 STOP，交独立 ChatGPT review；不越过真人 UAT 门槛。
