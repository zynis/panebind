# PaneBind V1 集中交付进展

当前用户功能接线：正式 Medium GUI／托盘入口已接通普通窗口资格、Move／Resize 磁吸、Ctrl 邻接组移动／共享边缩放、停靠／拉伸、窗口命令、中文六页设置与应用规则；离散布局恢复、最大化起手恢复、独立 native writer、视觉属性恢复及实际吸附／共享边反馈均为生产调用链，不只是 helper 或测试模型。

实际能证明的操作目前仍主要来自隔离 guest 的普通权限 owned 窗口。已有 Release owned 回环、窗口命令、设置真实操作、视觉流程，以及双窗共享边 Resize／约束交集的局部证据；最新 aae 批在新 T 布局 START 领取失败，整批为 FAIL。真实 Explorer、完整跨应用组、显示矩阵和最终便携包尚未通过整版验收。不能将已接线等同可在普通宿主试用，也不能把局部自动 DEV 写成人工 UAT。

## 源码与包身份

本稿是持续开发快照，不固定最终交付 SHA，不预填下一批结果。

| 项目 | 当前准确状态 |
| --- | --- |
| 开发分支 | `codex/r1c4b-live-magnet` |
| 方案入库 | `c1e8f82f8156db7c4778473f6b4689327f771013`；变更限 `AGENTS.md`、`README.md`、`docs/PANEBIND_V1_DEVELOPMENT_GUIDE.md`，主执行者已完成标准推送 |
| 核对时当前已提交／已受测 runtime | `a2eb05f7e76d05ec9378e0d12631dfd22c326fbb`；工作树另有待提交新接线，不将其冒充本包实现 |
| 构建边界 | aae 包内 Debug／Release 二进制均绑定 a2eb；本次实际运行 Release。新 click、真实 ZIP 与 I10 接线尚未形成固定新构建身份，不预填其构建或现场结果 |
| 当前已收证批次 | `aae67076ccda4bdb9300c9015108231a`；collection、精确 guest／临时任务清理回执已取得，整体 FAIL |
| aae ZIP／manifest SHA-256 | `31132A5BCBB4EAB8F488BCE647B80A5A36A6E57540B5DA83FF41FC5BE2D8B8EF`／`DF95AEEA2336F7217A051A5341B8BE85F5E379147B48CC377B369ABCEF4BC793` |
| aae Release／Debug EXE SHA-256 | `A1F92E7773607A6DB8967E0AE10B47257474C3947B3F3B96B643A639574C9E56`／`6C1B5EB112CC8169E166A13A627AE16B2A354BAB63882224FCC65301E0C2C596` |
| 已实际运行历史批 88e | 源码 `fcca29306ded7f483f8381901bbc7fb706cdf1c6`；RunId `88e4574bac3c4eba801431a559d58a3a` |
| 已实际运行历史批 8cc | 源码 `be011280b0d8dc7715163655642964cb112f5656`；RunId `8cc920c8b1584531b248e602b672d186` |
| 8cc ZIP SHA-256 | `902FBFFED8028C3B0DD2C1ED69A9C3099BB53F7E6B87B5DCB449C9EDBADA0479` |
| 8cc manifest SHA-256 | `F907191C43BC13423890EC5EEC4B417F7546BA7FDA5C29C015F5284D74BCC5AD` |
| 8cc Release／Debug EXE SHA-256 | `E64087A761163A9A1BA4293291997098B822F177C9BC369DF35BEF4993FB4A39`／`CE5B232741824E5BA68F47F43867AB3EB400658F572C3FE2760A00A3EA22475B` |

每批实际身份保存在 `uat/v1-sandbox/<RunId>/package-receipt.json` 与 `input/manifest.json`。8cc、88e、4232／3e5 等历史包、22a9 旧包及原始失败继续保留，不覆盖、不冒充 aae 或最终用户便携包。打包时 receipt 的部署字段不是当前执行结果，现场状态以实际 launch／collection／cleanup 回执为准。

## 已运行的功能与明确失败

### aae：双窗共享 Resize 局部通过，T 布局 START 失败

原始证据目录：

`uat/v1-control/aae67076ccda4bdb9300c9015108231a/collection-20261015T140152142Z-ddf706cc15c248109b5ed8df484de5e5/`

- Release／Medium 的双窗竖向共享边、相反侧 source、横向共享边实际缩放及 min 交集统一钳制通过（fixture seq548–582）；参与窗口实际矩形与产品 native exact 回执对应，例如 `process_id=6704` 的 call35–40。固定尺寸邻居场景在真实 DOWN／Move／UP 后保持几何且没有 placement（seq583–591）。这些是 M07／约束处理子场景，不是完整三窗／所有 source 或整条 I10 通过。
- 随后新建 T 布局已到实际前台／命中准备；产品 seq89 是 `prepared_actual_source_and_hit`，seq90 读到 source 自身 capture／move-size=`1835770`，fixture seq598 为 `layout_real_DOWN_production_start=FAIL`。该次未领取产品 START，不能写作原生观测到 `clicks=2`，也不能以准备快照推定产品已接管。
- 当前工作树已补“拖动过的 press 不再保留为双击首击候选”，支持返回原锚点后仍失效；这是针对上述失败的实现修复，尚待新 SHA／Run 验证，不把假设写成已证实根因。
- 原产品正常退出，shutdown 的 hooks／overlay／native worker／visual recovery／restore 标记均为 true，issued completion unknown=false；但 fixture seq601 的实际 post-stop client DOWN／UP 投递为 false。因此只报告原生退出回执和失败的输入恢复观察，不判整体 cleanup PASS。
- collection 已保存，本批精确 guest 已退出、临时任务已移除，证据前后哈希不变；这些编排清理不代替产品 cleanup。T 之后的 grid17、设置生命周期、完整连续链、应用矩阵和真 ZIP 普通入口未由本批执行，保持 NOT_RUN。

### 88e：Release／Medium 局部功能

原始证据目录：

`uat/v1-control/88e4574bac3c4eba801431a559d58a3a/collection-20261015T130241266Z-5cad0286f4094d2a98d5965a144fdeb2/`

- owned A/B/C 普通 Move 五阶段、Ctrl A、实际 Resize 后 A 回环、Ctrl C source／follower，以及 Center／DockLeft 实际矩形通过。它证明 owned 操作子链，不是完整 Explorer 或混合应用 I02。
- 真实生产设置窗口、六页截图及鼠标／Tab／Enter／Esc 操作、取消不保存、吸引距离改为 0 后下一手势效果、正常重启后设置及实际效果保持，通过对应局部检查。六页截图存在不等于所有 DPI、可访问性或设置项均已验收。
- Explorer 止于 `new_directory_file_identity_unavailable`，未执行其功能流程。
- Edge 绑定后，真实 DOWN 的产品手势检查失败；VS Code／Terminal 绑定受阻，不能记作应用功能通过。
- 视觉测试已观察原始属性、正常退出、临时透明 UP 恢复及两次摇动的相应结果，但最后 `visual_GUI_real_checkbox` 即时读取失败（fixture seq539），整批仍为 FAIL。后续 be 版本修正了该读取等待，不能回写旧批通过。

### 8cc：视觉流程返回，布局阶段中断

原始证据目录：

`uat/v1-control/8cc920c8b1584531b248e602b672d186/collection-20261015T132057658Z-ce753c3739044698b6a459e57d538228/`

- 视觉流程执行到原配置、原始 tuple 和布局恢复，并写出 `visual_mapping_sequence`（fixture seq600）。这是该批视觉子流程证据，不是整版清理或所有异常退出通过。
- 随后布局 fixture 创建、设置及前台核验完成，原始记录停在 seq605 的光标观察。产品日志需按 `process_id=7184` 区分重启后的实例：seq61／63 曾准备成功，seq62 出现 `application_track_limits_unavailable`，seq64 实际 capture 和 move-size 均为 HWND `1508758`。这些事实支持定位自建 STA／同步查询／原生操作衔接，不能仅凭行序断言全部跨线程因果。
- 本批没有正常完成的 guest summary／整批退出回执。已按精确本轮实例身份销毁 guest、移除临时任务并保留证据；这是测试影响被隔离，**不是产品 cleanup PASS**，也不证明原生调用已经自行退出。
- Explorer 新目录的本地 file ID 已真实取得；但目标根窗 token 记录为 `integrity_rid=8192, elevated=true, medium=false`（fixture seq252），产品随即拒绝 `target_not_medium_non_elevated`，没有发送该目标手势。不能把 bootstrap 的 High 或旧 High guest 基线写成这个根窗的 `12288`，也不启用旧例外绕过正式 Medium 判定。
- Edge 的 DOWN 前身份复核失败仍保留。Terminal 原观察表明实际根映像为 cmd，不是此前假定的 conhost。Code 的隐藏根／子根资格拒绝（主执行者诊断中含 `0x80070005`）尚未确定根因；不能把准备 ZIP、启动命令返回或移除参数当作绑定成功。
- guest 的 System32 Notepad 与 WindowsNotepad Appx 确实缺失；这是当前矩阵的具体软件缺项，不是已经测试失败的 Notepad 业务行为。

此前 269b 的首设置 checkbox 命中失败、df93 的外层 1800／内层 1200 超时范围不一致及无 fixture 输入拒绝，也继续保留原始记录。后续修复／新 Run 不将历史改写成通过。

## 当前生产实现与新修复

后继批 `92b2b41ea7f946bebca7a519a3c79831`（源码 `caf4f7934258105d9eed743aaa438174d67a20f8`）已生成并部署真实七文件便携 ZIP，但未到达其普通入口验收。Release 视觉流程在第二次摇动起手后实际记录 `FocusLost`（产品 seq45），独立系统光标回读仍为旧位置（fixture seq470）；这不是已确定的 hook 根因，也不放宽失焦撤权。随后正常 shutdown 的资源字段及实际客户区 DOWN/UP 投递均通过（fixture seq473–475），整批依然 FAIL。完整原始证据保存在 `uat/v1-control/92b2b41ea7f946bebca7a519a3c79831/collection-20261015T142746862Z-2777c1a48e6244e7839ee601f67830af/`；精确 guest／启动任务已清理，文件保留。收集中文目录时的首份部分副本也保留，后续仅设置编排进程与本次 SSH 命令的 UTF-8，不修改机器／用户配置，取得稳定前后清单。

该局部失败不阻止未运行的布局、约束、设置与中断流程；下一批把它们前移，仍保留视觉失败与必要的一次失败时只读事实，不重复操作未知前台窗口。

主通路为 `panebind.exe` → 轻量真实输入适配 → `Catalog` 身份／桌面／几何 → 锁定原始 DOWN 与成员／配置 → 纯几何计划 → 唯一 `WindowWriter` → 实际回读 → 关系和非激活反馈。正式入口是通用 Medium 产品，不依赖固定三窗、Explorer nonce、旧 shield 交接或自动 driver 才能使用。

已提交 runtime 包含此前 4232 以来的具体修正：

- hover 预备只取资格／命中所需快照，避免重复同步 bounds 查询；真实 operation 仍保留完整 bounds 预检，不把未知约束设为允许。
- 测试 STA 的有界等待处理必要 sent messages，针对 8cc 自建窗约束查询与等待相互阻断；不是给产品增设轮询。
- Catalog 使用确定的实际矩形遮挡判定；吸附边线与 Ctrl 共享边从实际回执／几何绘制。尚无本批现场可视验收。
- Edge driver 消除 DOWN 前重复 bounds 查询；Terminal 精确根映像适配为实际 cmd。都等待匹配新构建复验，不凭修代码关闭。
- 已接原生 grid17 拒绝与后续 Move／Ctrl 恢复入口；aae 的固定 min=max 邻居拒绝已实际执行，grid17 尚未到达。grid 由自有应用在 `WM_WINDOWPOSCHANGING` 实施，不伪造产品已知 grid。
- Code 去掉可能覆盖显式命令行的 bootstrap data 设置，仅作为可区分假设；尚不能据此宣布根因或修复成功。

原始 DOWN、单 writer、撤权后无新调用、在途调用与完成未知分开、actual postverify 及精确视觉所有权合同继续保留。构建、L1/core 测试与局部 owned 结果都不能替代当前 Release 的整功能／模块验收。

当前待提交工作树另外接入三项：拖动后 click 候选退休；真实 Release 七文件便携 ZIP 的同 HEAD／哈希打包、guest 中文空格目录解压和无参数普通入口验证；I10 第二成员在实际 sizing 调用中消失及存活成员 Move／Ctrl 恢复用例。它们均不是本次 aae 二进制的已测能力；新构建身份和现场回执尚未固定。ZIP 解包生产函数的合成输入检查、语法／编译检查只属离线证据。

## 集中缺口与待核验项

### 已修或已有接线，等待对应现场闭环

| 项目 | 证据与现状 | 下一项明确验证 |
| --- | --- | --- |
| 设置命中／checkbox 时序 | 269b 首命中失败的定向修复后已有真实六页操作；88e 视觉 checkbox 失败后，aae 视觉配置／tuple／布局恢复流程返回 | 旧失败保留，不再把已复验的同一 checkbox 当未修缺陷；登录启动、排除、导入导出及损坏配置 I07 仍待到达 |
| 布局 START／click 候选 | aae 双窗共享 Resize、min 交集和固定邻居拒绝已过；新 T 布局 START 未领取，实际进入 native size | 新 click 修复后真实重验 T 与后续操作；不能预写 clicks=2 或根因已证实 |
| Edge／Terminal adapter | 重复 bounds 查询及实际 cmd 根映像识别已有具体修正 | 精确新窗口重新绑定、真实 DOWN、完整几何及客户区输入 |
| grid 与应用钳制 | 生产 min/max、共享交集与 actual postverify／拒绝已接线，aae 固定邻居拒绝和交集钳制真实通过；未知 app grid 不伪称可读取 | 跑原生 grid17 非 exact 后拒绝及恢复；尚未取得的分类和回读不冒充 exact |
| 视觉／恢复与边线 | 临时／永久原值、独立 helper、completion barrier、迟到完成与实际边线已接线，已有视觉局部证据 | 当前构建补异常／未知完成和反馈实际观察，不能把正常 stop 推成硬崩溃通过 |
| post-stop 实际输入恢复 | aae shutdown 各资源 flags 通过，但实际 client click 投递失败 | 查明并复验真实 DOWN／UP 恢复；退出码或 guest 销毁不能关闭该失败 |

### 仍缺的实际环境或尚未闭合能力

| 项目 | 准确边界 |
| --- | --- |
| 正式 Medium Explorer | 本批根窗 elevated=true，不满足正式资格；本地目录身份已恢复，不再误报为 file ID 未实现。不得套用旧 High guest 例外。 |
| Code | 精确可见合格根绑定仍未成立，隐藏根／子根拒绝尚未定因；参数变化只是待检验假设。 |
| Notepad | 当前 guest 的指定系统 EXE／Appx 缺失；未运行的应用路径不能记 PASS，不在宿主安装或修改账户／UAC来补分。 |
| E03／E04 | 当前没有真实双屏／负坐标与混合 DPI 环境证据，属于具体环境缺项；纯模拟坐标不代替。 |
| 200% UI、虚拟桌面、普通用户真机 | 尚未执行，分别保留 `NOT_RUN`；当前截图或 guest 局部结果不外推。 |
| I08／I10／I11 | driver 断开收尾、第二成员真实 native failure、完整两轮与 15 分钟混合运行仍未闭环。I10 在途调用测试接线待新构建／运行；grid 非 exact 或窗口消失不冒充 API 返回 FALSE。 |
| 最终便携交付 | 已接真七文件 ZIP 同源校验、中文空格目录解压和普通无参入口测试，待固定新构建并实际运行；guest 包、identity-only 或合成解包不替代 I12。 |

## 持续验证与交付边界

验收全集见 [V1_ACCEPTANCE.json](V1_ACCEPTANCE.json)，产品操作见[中文快速入门](../V1_QUICK_START.md)。当前仍为 **0/40** 对标整体通过；B01–B32 全部必过，B33–B40 固定延后但保留在分母中。整版 DEV 门槛尚未满足，真人 UAT 为 `PENDING`。文档、机器表与后续证据更新应继续区分源码接通、局部实际结果和完整门槛，不能用测试数或旧 High 局部证据凑分。

主执行继续匹配当前 SHA／新 RunId 的隔离验证和普通缺陷修复；本稿不预填这些功能结果，也不构成阶段终止点。后续源码／包哈希、功能回执及最终标准 Git 远端核验继续由实际证据集中补齐。原失败与历史包保留，`uat/`、EXE、ZIP 不入 Git；不创建 PR、merge、tag 或公开 release。

现场自动输入仍仅在本任务专属可丢弃 guest；远端文件仅在 `I:\PaneBindMVP1Runs\` 本任务目录，不触碰 CrossRec、其他用户数据、账户、token、UAC 或宿主设置。普通宿主试用和真人手感不能由自动 DEV 自签通过。
