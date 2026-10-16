# V1 关键运行证据摘录

本目录是现有 [V1_ACCEPTANCE.json](../V1_ACCEPTANCE.json) 的脱敏复核材料，不是新任务书，也不自行授予功能或整版 PASS。

先读 [index.json](index.json)：每批绑定原产品源码 SHA／EXE SHA-256、独立 driver 源码 SHA／EXE SHA-256、RunId、manifest 哈希、原始日志的本地相对路径与整文件哈希。四份 JSONL 共152条原始观察，约149 KiB；大体积原件继续只在 ignored `uat/` 保留。

| 摘录 | 实际范围与限制 |
| --- | --- |
| `714c-release-scoped.jsonl` | dd60 Release／Medium，真实设置诊断和普通七文件 ZIP 入口、保存改变动作、两次真实 NormalUp、正常退出与客户区投递；不是 I01/I12 或整版通过 |
| `c01-release-components.jsonl` | b60 Release／Medium，单屏停靠／拉伸、实际 Deadline／恢复、真实部分 native 失败与后续操作；保留缺 UP、单屏跨屏阻断、应用命中／清理失败 |
| `bdab-release-owned.jsonl` | 50df Release／Medium，实际 owned 历史回环、T/L/四格／共享边及 UP 子项；保留八窗实际失焦／光标未到目标，不能冒充混合应用 I02/I03 |
| `97-release-activation-failure.jsonl` | dd60 同 guest Debug 成功后的 Release 实际客户激活失败；测试输入返回1不等于投递，后继未发送的检查不记为投递失败 |

摘录保留 `raw_sequence`，及原始行 **UTF-8、不含行尾换行符** 的 SHA-256。嵌套的 JSON 字符串转换成对象，绝对路径／SID／账户／标题字符串按索引说明脱敏；因此摘录不是原始行逐字节副本。原始整文件哈希与摘录哈希分别记录，不相互替代。数字 HWND/PID/代际只是该次运行事实，不授予后续控制权限。

`record.result=PASS` 只保留原子步骤当时的结果；是否完整满足当前功能合同，由验收表逐项判定。JSONL 行排列不构成 Raw、hook、WinEvent 或 GUI 的共同原生时序。没有归属的 Raw UP、driver 发送成功、退出码0和销毁 guest 均不能补成正常抬键或产品 cleanup PASS。

在仓库根可核验摘录：

```powershell
$index = Get-Content docs/reports/v1-evidence/index.json -Raw -Encoding UTF8 | ConvertFrom-Json
foreach ($entry in $index.records) {
  if ((Get-FileHash -LiteralPath $entry.excerpt_path -Algorithm SHA256).Hash -ne $entry.excerpt_sha256) {
    throw "Excerpt hash mismatch: $($entry.evidence_id)"
  }
}
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/v1-validate-acceptance.ps1
```

另有 [input-up-dispatch-directed.txt](input-up-dispatch-directed.txt) 是新确定GUI消费时序缺陷的脱敏工具输出转录：旧226/1、修复239/0，绑定产品修复源码`408ac527a6bfda7b01a5e83249bd5ed0eca2714d`。它不是程序原stdout，不包含真实输入或原生placement验证，也不证明旧LL-UPmissing根因。`index.json.offline_transcripts`记录原转录与脱敏文件的独立哈希。[408ac52-build-offline-transcript.txt](408ac52-build-offline-transcript.txt)另记新R产品构建、各10项D/R离线回归及独立EXE哈希检查；D driver分开构建，尚无新包或现场结果。

本目录`.gitattributes`仅将`*.jsonl`／`*.txt`／`*.json`固定为`text eol=lf`，避免Windows的autocrlf使复核文件在另一checkout变换字节。未改根attributes或用户Git配置，既有摘录原字节哈希保持不变。

增量8189由R产品408ac52与D driver83bf分离：`8189-fixture-selected.jsonl`／`8189-product-selected.jsonl`保留真实快速UP点terminal exact／NormalUp及后继Move子结果、GUI组件、诊断／portable两个错pin失败与正常退出／实际客户投递。新2632 ZIP普通入口因driver错pin未运行，不能用dd60旧入口通过替代。精确guest／任务清理回执与产品stop分别在索引记录；D15只按定向范围关闭，D08仍OPEN。索引中的`records`现含旧批和新批，不修改旧行或哈希。

历史实测经 `partial_observation_ids` 回到原产品身份，不因当前 `candidate_identity_ref`、更新 driver 或更新报告而换绑新 EXE。本轮新增产品修复与现场结果另作增量记录，不覆写这些反例。
