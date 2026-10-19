# PaneBind V1 集中交付进展

当前用户功能接线：正式 Medium GUI／托盘入口已接通普通窗口资格、Move／Resize 磁吸、Ctrl 邻接组移动／共享边缩放、停靠／拉伸、窗口命令、中文六页设置与应用规则；离散布局恢复、最大化起手恢复、独立 native writer、视觉属性恢复及实际吸附／共享边反馈均为生产调用链，不只是 helper 或测试模型。

本轮新增的实际用户行为证明：同一212 Release产品在6dba隔离guest中，实际通过GUI切换智能拉伸，完成忽略邻窗、无正交交叠、最近阻挡、隐藏目标过滤、尺寸超限明确拒绝及恢复后继续操作，并验证一次左边框真实双击入口及匹配UP。此前cea的四窗两轴共享Resize、后继普通Move和八窗任意末成员Ctrl Move证据保留。这些是当前产品新增的owned分支证明，不等于完整M08-F05／B19／I05、真实应用混合组或Full通过。

当前整版开发仍为 **PARTIAL**。产品212与7635便携字节未变；9724有限owned证据、dd02十个设置／生命周期与普通包入口子范围继续复用，托盘仍BLOCKED。be94 Full的B Resize因未知前台切换而FocusLost中断，原FAIL与原因UNKNOWN保留。7c6c→744已复验客户helper的source-only接线，但744应用前置仍失败。后继fe0在PaneBind产品从未启动的条件下完成有界原生客户对照：Terminal、Notepad、Code、Edge均未取得完整客户交互通过，Edge正常关闭未确认；它限制了当前guest可验证范围，不把所有问题归于环境或宣布唯一系统根因。历史**7/60功能、B29／B32共2/40=5%**不增、不换绑；生产接线60/60不等于验收完成。不能宿主试用，真人UAT PENDING，暂不进入REA分析。

## 当前结果：组布局与智能拉伸分支已实测，真实应用前置仍未齐

2026-10-19本轮首次使用原配置连接 `crossrec-desktop`／`192.168.1.6:22` 实际exit0，目标核实为 `ZS-WORKSTATION`；没有修改SSH、安全设置或重启宿主。标准 `origin` fetch成功，本轮核验HEAD为 `829682b9935563e1d9d6aaaf0664fb40ad9e6d8d`，与 `origin/codex/r1c4b-live-magnet` 差异0/0。恢复连接仅解除该传输阻断，不代表普通桌面、多屏或整版验收已具备。

新RunId `d76be229715b4d5cb777cb0cc9e76088` 按冻结product212／driverd2、包装829682生成后保持未部署／未运行，原包保留。运行前确认其他应用缺少与Edge相同的原生客户交互前置门；最小driver-only修复已提交为 `eec93e22d66d148f2611dfd59291208c66114aa1`，Debug／Release构建及受影响guest-path各1/1、内部208项通过，51项静态身份guard通过。产品EXE、权限条件及7635便携包均未改；这些检查不是应用实际可交互的证据。

后继 `be94eba2a078403ca5db1b961c0f2aae` 使用product212／drivereec93e／包装eec93e，Full／Release已实际部署、启动、收集和精确清理。fixture134行；g1–g4的Move／Ctrl子链和正常UP有实测，但123行 `B_real_frame_resize` g5目标 `[414,80,674,288]`、实际 `[414,80,674,260]`，未完成Resize。产品156行实际WinEvent foreground为HWND1180380、不同于source327754，随后157行为FocusLost／normal_up=false／pending_retired=true；165行拒绝该root非Medium／non-elevated，166行记录该root已不live。新前台PID、类名及起因UNKNOWN，不认定系统、Console或具体产品根因，不放宽前台／capture检查，也不补成正常UP。

失败后的测试清理UP不计产品正常收尾。fixture127／产品170实际正常退出0并记录自身资源回收；fixture132自建客户实际DOWN／UP计数 `[0,0]→[1,1]`，133真实Raw注销，134自建窗口销毁。wrapper退出1、descendants0、forced=false、product_cleanup_pass=false仍保留各自语义；guest22052与本轮任务于 `2026-10-19T10:07:48.9534764Z` 精确清理，证据未变、其他资源未改。原件保留在 `uat/v1-control/be94eba2a078403ca5db1b961c0f2aae/collection-20261019T100537352Z-6d636cb1e7e2435db170032082c97a5f/`，未以guest销毁替代产品清理。

后继 `dd021c0b0fd64f6a98126945dc8da48f` 为product212／drivereec93e／包装 `92117d05328a28ff934e61c42f14bdd9f1d8e363` 的Release `EntryAndSettings`，fixture1267行。十个独立阶段分别PASS：settings_GUI、GUI_diagnostic、rapid_terminal_motion、startup、five_button_mapping、import_export、save_failure、diagnostic_export_failure、corrupt_restart、exclusion；其中rapid_terminal_motion仅native-only，不授Glue或增强Resize。1029行精确当前产品UID1托盘图标不可达，`menu_input_sent=false`，tray为BLOCKED，不能用命令入口补托盘PASS。

1250行原 `portable_normal_entry_result` 写有 `I12=PASS`；实际证据支持同7635普通无参GUI、配置效果／Move、正常退出及交付字节的入口子范围。按主方案整I12还要求全部入口等门槛，当前托盘未过，故不将该driver标签提升为整I12。产品实际停止、退出后客户投递恢复（1264计数 `[10,10]→[11,11]`）、Raw注销及自建窗销毁分别有回执；EntryAndSettings整体FAIL／driver退出1，descendants0、forced=false。guest9612与专属任务于 `2026-10-19T10:18:45.6190737Z` 精确清理，证据未变、无其他资源改动。原件在 `uat/v1-control/dd021c0b0fd64f6a98126945dc8da48f/collection-20261019T101721305Z-159479434b854ab6b5f422ec1aa81b50/`。

`ApplicationsOnly` 批 `7c6c3d233155473ea2a3b08c57d58d3e` 已实际运行并收集139行：Terminal实际BOUND，但真实root命中为HTCAPTION，不满足原生客户区前置，未发客户输入／未运行产品手势；Notepad本包／guest无明确官方EXE，Code官方启动后无合格新root，均为startup BLOCKED且未发应用输入。Edge实际BOUND、布局成功，但暂停产品后的客户区helper额外查询整组三窗 `matrix_sources` 时被拒绝，后续Catalog代际不同，客户区／产品输入均未发送；具体native查询拒绝原因UNKNOWN，不推断为WM_GETMINMAXINFO、应用挂起或产品失败。

7c6c的fresh／same-identity关闭前提未满足，Edge appclose未确认，cleanup FAIL；wrapper descendants4／forced=true，不能由产品自身exit0／资源回收及自建客户输入恢复抵消。guest21528与专属任务于 `2026-10-19T10:24:01.0382700Z` 精确清理、证据未变，不补产品cleanup PASS；原件在 `uat/v1-control/7c6c3d233155473ea2a3b08c57d58d3e/collection-20261019T102043892Z-cddf0986e50742ebb7f60147901de602/`。

测试driver客户区helper修复提交为 `01ab9ec48f10ff338b550d9ae8c41c6b050a17d7`：复用已有fresh／Catalog同一source快照，不在此重读全组或尺寸限制，仍保留fresh命中、GUI上下文及客户语义门。Debug／Release构建均exit0、受影响offline与53项静态检查通过；随后 `ApplicationsOnly` RunId `744fff40a030404ebafe9e2879190179` 已实际运行，包装身份为 `0e05846bc5fed22020be2ca39d202c63337478be`，不是待跑批次。

744的154行原JSONL合法连续。[关键原行](v1-evidence/744-r11-source-snapshot-preflight-revalidation-selected.jsonl)56行Terminal真实root131804命中HTCAPTION2，59行实际pause／resume成立但原生客户未就绪，70行增强NOT_RUN；Notepad82行、Code113行均startup BLOCKED且没有产品手势。Edge122行已实际对root131936执行WM_NCHITTEST，返回失败／error1460；123行pause／resume成立、客户未就绪，135行BLOCKED且客户输入未发送。本次已触达修复后的source-only调用链，不证明旧查询拒绝的根因；暂停增强不等于产品进程或hooks不存在，也不能据此判唯一环境原因。

Edge138／142行仍为 `exact_root_close_not_observed_in_bound`，139行依赖清理门拒绝继续，不强制控制身份失效窗口。产品自身144行quit／exit0及hooks、overlay、writer、capture、关联和视觉资源正常清理；149行自建客户实际计数 `[0,0]→[1,1]`，153行Raw实际注销，154行自建窗口销毁／产品关闭。这不抵消Edge正常关闭未确认：wrapper wait0／exit1／descendants4／forced=true。guest21076与专属任务于 `2026-10-19T10:45:01.5458315Z` 精确清理、证据未变、其他资源未改，guest销毁不补应用cleanup PASS。原件在 `uat/v1-control/744fff40a030404ebafe9e2879190179/collection-20261019T103905106Z-fda709d9331344d49903db84929b1347/`。

### cea／fe0：两个入口已实际运行，结果不再待填

实现修订为 `f1f2508b6bd25e13297d8b465ff67bb36ddba53f`；其后继 `f9fc72b1692250531525103a4b5d5aa352116aa0` 仅含文档增量，构建按实际HEAD重编后的driver自报身份、manifest与打包身份统一为f9fc72b。产品仍是原212 EXE，不将旧驱动或运行记录换绑新EXE。Debug／Release构建及必要offline结果见[验收表](V1_ACCEPTANCE.json)；这两批现场只执行Release。

| 配置 | 原212产品SHA-256 | 实际f9fc72b驱动SHA-256 |
| --- | --- | --- |
| Debug | `8AF5BB9BDF0555EB070E40E902D8DC951D122C8010AB0783DF9B28A878C4CBB6` | `D8E46A2623D31284DAAB1343201AE4A920338BA022F6EB8B1BC94F1A89810528` |
| Release | `E2F93AEC6714551E5F49A40CE07D8CC512F847AF77067145B46A4C57DBAC69BE` | `11E72B72847B7AA5866DA8C30F29A8D92C5884A2DB448EF2AC395F5E9A809F3E` |

`GroupBoundsOnly` RunId `cea534853e6049c8b88cefc3b35c188e`：154行fixture合法连续，D58四窗双轴共享Resize、D75随后的普通Move、D124八窗末成员Ctrl Move通过；g1／g2／g3均有真实匹配UP、NormalUp、精确几何与capture／关联／遮罩退出，合计13次真实placement。D91／145分别为两个独立组件PASS，场景之间实际确认资源安全，没有补造前代手势；D146产品正常退出0，D151自建客户实际DOWN／UP `[0,0]→[1,1]`，D153真实Raw注销、D154自建窗销毁。wrapper退出0、descendants0、forced=false，但原常量 `product_cleanup_pass=false` 不改写，以这些实际收尾回执分别评价；不是仅由退出码或guest销毁授PASS。G06／T-L历史、I11、Explorer、应用矩阵和Full本批均NOT_RUN。

原件在 `uat/v1-control/cea534853e6049c8b88cefc3b35c188e/collection-20261019T110531451Z-97560be4f23240dc98fe510325759108/`；ZIP SHA `B22A5C9893E69AB1AECA3D7550E9DEA8A614F489907EC6C029AFAFAFA1C4C475`。专属guest3280／任务于 `2026-10-19T11:07:03.2054958Z` 精确清理，证据未变、其他资源未改；完整manifest、WSB、原件哈希与关键摘录见[既有证据索引](v1-evidence/index.json)。

`NativeClientsOnly` RunId `fe0e2b7191134dc7a3dad634b8b0c983`：91行fixture合法连续。该入口在首次产品launch及几何设置之前分流；各实际边界核对产品handle／PID为空、候选EXE名进程／产品根窗口／产品日志不存在，未调用产品状态、writer或cleanup；不宣称所有改名实例、fixture Raw或其他输入观察都不存在。Terminal D30真实root命中HTCAPTION2，未发客户输入；执行者查看本批BMP，实际显示cmd启动错误`3489660962 (0xd0000022)`，不是沿用744截图。D34精确根关闭确认，原生客户仍FAIL。Notepad D40指定官方EXE缺失，Code D65无唯一合格新root，均BLOCKED而非产品功能失败。

Edge D75实际hit子窗66294／root66286、HTCLIENT1，单次客户点击和D79焦点检查通过；D80页面DOM效果仍未观察，D81实际标题为“无标题 - 用户配置1 - Microsoft Edge”，正常关闭未确认。D84按既有安全门停止依赖输入，未争抢未知窗口或伪造收尾；D91自建窗销毁，driver FAIL。全批产品功能为NOT_RUN、产品cleanup为`NOT_APPLICABLE_NOT_STARTED`，不能把空进程句柄或guest销毁写成产品cleanup PASS。新root、默认应用几何及无产品等条件与744不完全相同，此对照不作严格单变量或唯一系统根因证明。

原件在 `uat/v1-control/fe0e2b7191134dc7a3dad634b8b0c983/collection-20261019T110858257Z-37da6f908cb34721b4c982b5edc32652/`；ZIP SHA `EF666E2195869C52BC3308D41A962553905D7B2BAD1A10496B6657101096D244`，BMP和日志原件继续本地保留、哈希入索引。专属guest23344／任务于 `2026-10-19T11:10:36.3036490Z` 精确清理，证据未变、其他资源未改。未增加observer、同路线重试矩阵或额外900秒批次。

当前原7635便携ZIP SHA仍为 `576E730C7CD1C132662EF83BE82B97588473510B8B7507C861C55448ADEEA2F6`，不重建、不覆盖历史包。当前212的e7／477等视觉、在途、grid及第二成员故障子范围与9724有限连续运行继续按原身份复用，不为补组件而再跑一套OwnedComponents。7c6c、744、be94、dd02及所有旧失败保留。

### 6dba：智能拉伸有限分支与一次真实鼠标入口通过

`StretchModesOnly` RunId `6dbae80a02b74cd882df53293a439a02` 已实际执行Release，driver／打包身份为 `31f94b64f346de0c5542e5f378006702d6c1be7d`，产品仍为原212／`E2F93AEC…AC69BE`。没有重建产品、改写旧结果或重跑完整智能开启链／900秒。Debug和Release驱动均构建成功，仅保留既有C4456警告；受影响guest-path各1/1通过（D 0.26秒／wall 0.31秒，R 0.15秒／wall 0.27秒，内部208项，symlink子项SKIP1314）。Debug未执行本批现场功能，不能由Release结果外推。

| 构建／制品 | SHA-256 |
| --- | --- |
| Debug driver | `CCAC084524A0941063EEB3A68858C8C4FD64A84E13484D732464F9029BF51B5B` |
| Release driver | `096DCCA22672343C24BCB914863F11E94778CCCA3CD73BF888D81F721446F699` |
| 本批测试ZIP | `6520106B5A460B2B7470CA9B91C34FF8CDF04B1367EF6C9FFC2B45E195CB22A9` |
| manifest／WSB | `A1CC1A69B1FB5040141896D00246DC9694AE01E520B8A9FCFE9A120D66668072`／`0EC0214692B6DC09313C519CE0046F4D5033831FD9DA2DF80B9F9463A636AA42` |

fixture原件272行合法连续，driver源码逐行一致；产品日志81行。产品PID6344，实际source HWND197420，D153从本次真实Undo回执绑定source key5；其原可见矩形为`[1300,180,1700,480]`，工作区`[0,0,3050,1596]`。预期取自独立fixture几何，而非产品计划反算：

| 实际分支 | 原始证据与结果 |
| --- | --- |
| GUI关闭智能拉伸 | D86确认保存后的false，D88–89实际StretchLeft到`[0,180,1700,480]`，全高度邻窗`[0,0,1220,1596]`不动；D92实际Undo。 |
| GUI开启、无正交交叠 | D93只移动同一邻窗到`[0,520,1220,620]`，source保留实际Undo结果；D130确认true，D133仍到工作区左边、邻窗不动，D136 Undo。 |
| 最近阻挡／隐藏过滤 | D142在近窗`[800,180,1000,480]`与远窗`[600,180,800,480]`间选近边1000；D146只隐藏精确近窗，D149目标改为远边800，其他几何不变。各次真实Undo通过；这是hidden资格过滤，不是应用rule排除。 |
| 尺寸上限优先 | D157源最大可见宽600，请求宽700；D159／P43明确`batch_preflight_target_violates_size_constraints`、同key5／PID／源码与请求，单条回执、零native claim／attempt，source与两目标未变。原`identity_valid=false`是提前拒绝回执的原值，不伪改为true，也不称为已钳制600。拒绝后没有Undo；恢复原限制后D165新合法命令、D168真实Undo通过。 |
| 一次左边框鼠标入口 | D214实际HTLEFT10、真实DOWN／UP／DOWN／UP与mapped StretchLeft，P63 exact到`[0,180,1700,480]`，D215邻窗不动、D218 Undo。不是四边／四角鼠标矩阵。 |

鼠标释放不只凭SendInput返回或退出码判定：D211／P60观察第一下g1实际LL-UP，QPC359722962落在该次UP调用区间，明确为native passthrough，`normalized_NormalUp=false`。第二下P62为g2 mapped DOWN／key5，P68为同代mapped UP／key5，QPC360998396落在D213唯一第二UP调用区间360994487–361003166内。该映射双击闭环成立，但不冒充普通Move的NormalUp或所有手势抬键通过。

D255完整原配置恢复、D256测试布局资源关闭、D263阶段PASS；D264／P81实际产品正常退出，hooks／overlay／worker／capture／关联／视觉资源逐项回收，`issued_native_completion_unknown=false`。D269退出后真实客户DOWN／UP计数`[0,0]→[1,1]`，D271自有Raw注销，D272自建窗销毁通过。wrapper wait0／exit0／descendants0／forced=false；原`product_cleanup_pass=false`常量不改写，按上述真实回执评价所测清理，绝不以销毁guest补PASS。

原件在 `uat/v1-control/6dbae80a02b74cd882df53293a439a02/collection-20261019T114216606Z-a24f748d0b3140a9bd4ec523f9668204/`；fixture SHA `9F45662EEC693435CF9B671543F76D867F6323424951F64D57E220F4BA65BF15`，产品日志SHA `82FF89394DC143973A564220CB9F1EB8A5E31FC40F84638ACB339C4486DD9BD1`。专属guest7880／任务于 `2026-10-19T11:43:29.1823061Z` 精确清理，证据未变、其他资源未改，部署和原始材料仍保留；索引按本批身份追加。此结果不授完整F05／B19／I05，不覆盖应用rule排除、真实应用兼容、普通完整桌面、显示矩阵或真人UAT。

### 5b0：原生首入口对照止于WDAG基线，标准用户步骤未执行

`5b0b482b4f7544f5bdf2463963242f87`的实际模式为`MediumNativeIdentityOnly`，外层`NativeClientsOnly`只是兼容信封。产品仍为212、driver仍为31f原EXE，脚本／打包源码70eacb3。先使用WDAG原token（实际High12288／elevated=true）与同一原生fixture `--build-identity`核对启动及stdout通路；实际`CreateProcessWithTokenW`返回false／Win32 error5，子进程未创建，精确身份输出未取得。该事实只说明本次启动调用被拒绝，原因仍UNKNOWN，不证明PowerShell loader是根因、标准用户原生程序不可运行或Sandbox只能High。

基线未成立，因此没有创建标准账号、没有执行标准用户子进程，也未启动产品／GUI／Explorer或发送合成输入；这些均为NOT_RUN。前后桌面DACL哈希一致，账户回执NOT_CREATED，子进程清理无遗留；不能把无子进程叫作正常native exit。该路线上不追加无依据重试。原件在`uat/v1-control/5b0b482b4f7544f5bdf2463963242f87/collection-20261019T120544161Z-9d0447617e0e48f0a7f98df29ea324ee/`，专属guest23440／任务于`2026-10-19T12:06:48.0854185Z`精确清理，证据和部署保留、其他资源未改；guest销毁不提供产品cleanup PASS。

### 三项最重要的剩余缺口与下一动作

| 缺口 | 当前事实与最小下一步 |
| --- | --- |
| 真实应用与产品入口 | 当前已测Sandbox会话没有完整可交互的五类应用矩阵；无产品对照也未完成原生客户语义，托盘UID1仍不可达。5b0原生入口对照已运行，WDAG基线创建返回error5，未放行标准账号步骤；已有Medium凭据／子进程事实仍不证明普通完整桌面，也不能反向写成Sandbox只能High。现有资源内存不足，不要求新建VM、购买设备、降低应用安全设置或用宿主自动输入代替；尚未满足的普通桌面／应用条件保持具体UNKNOWN／BLOCKED，不重复这条无新变量的对照。 |
| 当前Release完整连续链 | be94的真实未知前台变化使B Resize撤权，触发者及起因仍UNKNOWN；cea局部PASS不关闭该Full失败。应在具备条件的上述会话复验同一候选完整操作链；若形成具体产品反例则按影响修复，不靠增加检查或放宽foreground／capture门凑通过。 |
| 显示与会话矩阵 | 当前单屏144DPI guest不满足普通完整桌面、双屏负坐标、混合100%／150%、200% UI及真实会话变化门。需要可用的相应隔离显示／会话条件；纯几何模拟和本轮截图不能代替。 |

恢复SSH、入口子范围、组组件或本次拉伸分支通过不等于整版即将通过。现无用户搬包、再次恢复SSH或重复跑900秒的待办；忽略邻窗及本次有限分支已实测，不再列为NOT_RUN或重复专项。5b0环境对照不放行后，继续未覆盖的活动Ctrl暂停→原生操作→恢复，以及Ctrl+Shift原生绕过后按新关系联动；这两项不依赖完整真实应用环境，当前尚未实测，不预填PASS。剩余工程缺口不全归环境，不进入REA，不自签整版DEV或真人UAT。

### 7944：保留原部署失败，不再作为当前SSH阻断

已生成 `7944a5a704f94112870483b88c62a0c9` 的 Full／Release 包（product212／driverd2／包装c910），包含现有官方 Terminal、Code 资产和原7635便携包。部署及随后两次有界只读连接均在原可信 `crossrec-desktop`／`192.168.1.6:22` 超时，exit255；原因UNKNOWN，不推断关机、休眠或系统故障。[首次失败原始回执](v1-evidence/7944-r10-full-release-deploy-blocked.json)及包哈希见索引，后两次为主执行者实际工具输出，未伪称原生功能日志。未调用Launch、未取得remote-created回执，本批Full功能为NOT_RUN；不以包生成替代完整Release验收。

7944保持原失败状态，不重试或覆盖本批目录；本轮连接已恢复，原先“恢复SSH”的外部动作不再待办。后续由执行者核实任务目录归属，复用冻结产品EXE／资产并自动部署新的候选driver，无需用户搬文件或新建VM。不单独重跑已通过的9724时长；Full内部实际执行另记。此前guest／任务清理及原证据、包保留各自身份。

## 212产品不重建：dedc／e7／e21／048当前实际范围

四批均使用原212产品，driver分别63／63／89／d2，完整源码、EXE／包／manifest与精确guest清理身份见索引。现有具体功能case及B行已追加对应PO／摘录引用，保留旧source／RunId和主状态，不仅在global history留结论。

- [dedc原行](v1-evidence/dedc-r10-debug-core-and-horizontal-follower-rejection-selected.jsonl)：533行合法连续，178 I02／commands、四快速native-only与485／489／505竖共享边子项通过。521水平g4失败；实际PID5272 source call8 exact后，follower因foreground变化拒绝且未attempt，P135记录FG1049458。根因UNKNOWN，不放宽权限门。后继未运行。
- [e7原行](v1-evidence/e7e0-r10-release-two-rounds-strict-duration-and-modern-terminal-selected.jsonl)：4374行合法连续，同PID7156两轮和20slot完成，原资源QPC间隔899.9940725秒，end比最终deadline早7.4593ms，严格900秒门FAIL；不是旧file_size错误。idle、视觉／在途／grid／batch组件通过。现代WindowsTerminal.exe官方1.25.2733.0资产实际绑定，五阶段free／XYsnap／hold无新调用／detach／resnap、实际legacy NormalUp与释放detach、Resize、命令、Esc、精确根关闭有实测；Raw代际未知不补归属。root实际HTCAPTION，枚举ContentBridge的HTCLIENT不等于真实投递路径，客户输入未发，类别仍FAIL。BMP实际显示cmd.exe启动0xd0000022，原因UNKNOWN，不能据此归因权限或改guard。
- [e21原行](v1-evidence/e21b-r10-interruption-preparation-failure-selected.jsonl)：37行合法连续，28配置未bootstrap导致准备失败，I08未运行，不是新Hotkey产品反例。[048原行](v1-evidence/048a-r10-interruption-hotkey-and-actor-foreground-abort-selected.jsonl)：338行合法连续，222实际WM_HOTKEY id910/count1在按下期观察；actor g13在8890ms实际FocusLost/end2，P418记录FG131910/source197416/shield786670但无新FG身份／起因，不能叫Deadline或NormalUp。失败后测试cleanupUP不补产品结束证据，整I08未过。

四批产品实际资源stop及自建client恢复分别有原始回执；wrapper依次wait0／exit1，desc0／1／0／0，forced=false／true／false／false，product_cleanup均false，不能由guest销毁补PASS。e7 Edge root关闭未确认；Explorer资格拒绝，Notepad／Code本包无合格root的BLOCKED不写永久环境缺失。精确guest／任务清理与证据未变另计。d2工具D/R构建及guest-path实际通过，048已部分实测；9724 FiniteSessionOnly已结束有限owned范围PASS：严格900.037841秒、资源无持续递增、实际产品资源停止及客户投递恢复有原件；wrapper的cleanup常量false不改写，单独按实际回执审查，不授整版。

## 212／0f2 Release／f54：两轮核心与组件已运行，I11读证据失败

包装3ea，fixture5553行JSON合法连续，原件SHA `F34BC988…6F36C6F8DA`；完整身份与11条[字面原行](v1-evidence/f54d-r10-release-two-core-rounds-reader-failure-and-components-selected.jsonl)见索引。296／310／322／336普通／Ctrl Move与右Resize四例均exact、native START／END各1并有实际LL-UP，零产品cancel／capture／placement，**仅native-only，不授Glue、增强Resize或NormalUp**。3046／3966同PID5292、未重启完成两轮owned核心；mixed slot14后4524记录attrs32、file_size system5／AccessDenied，4525／4547 I11 FAIL。实际错误原因UNKNOWN，不归结为rotation、不授900秒。4553独立60.0000332秒、CPU0%、WS30,949,376字节、handles282、GDI26／USER31前后相同，仅idle测量。2088 interruption、5051 visual／5279 inflight／5348 grid／5418 batch组件通过；设置与1250同7635 ZIP普通入口／保存后Move-UP／退出及字节子PASS，tray BLOCKED。

Explorer实际root RID8192但elevated=true，资格拒绝且未发输入，root清理未确认；不套用旧High例外。classic conhost／cmd实际Medium绑定，发送DOWN和阈值MOVE但未见真实START，单次清理UP后精确根关闭；不是现代Terminal全功能通过。Notepad／Code本批指定EXE缺失、未发应用输入而BLOCKED。Edge已绑定，仅发光标MOVE；暂停native基线hit-test超时1460，未发客户click或增强手势，根关闭未确认。5541产品实际退出0、资源stop与5546自建客户DOWN-UP恢复独立成立；wrapper wait0／exit1／desc2／forced=true／product_cleanup=false不改成整批cleanup PASS。23:55:48Z精确guest23940／任务清理、证据未变且无其他改动，guest销毁不替代产品清理。产品212未重建；0fc部署包NOT_RUN。后继driver `63a2df70f5877f4d9372fc5b8b0d8b0a872e9b64`实际以同一share-delete句柄核验并读取2MiB以内物理日志，不将权限错误认作rotation；D/R构建和guest-path定向文件检查通过（0.16/0.20、0.12/0.24秒），symlink因1314明确SKIP，哈希见验收表。新driver现场仍NOT_RUN，不预填I11或根因修复结果。

## 212／0f2 Debug／97a8：真实队列UP已观察，下一输入前置FAIL（历史时点，后继f54见上）

product212字节未改，driver0f2，包装3ea；fixture200行JSON合法连续。[两条字面原行](v1-evidence/97a8-r10-debug-native-only-real-queue-up-and-preinput-interval-selected.jsonl)187快速plain Move exact、native START／END各1、零产品cancel／capture／placement。独立source线程WH_GETMESSAGE观察到单个PM_REMOVE WM_LBUTTONUP，MSG前后相同、extra=PBV1，CallNext返回389142770早于source END389163043；LL也实际观察到UP，不能声称已证明“缺LL分支”，更不授增强NormalUp／Glue。191下一Ctrl例system500／request501／actual500ms，间隔门FAIL时尚未发送该手势输入，不是新增产品反例。后继未运行，原Full FAIL保留。193实际产品退出0、输入／capture／关联／writer资源停止，198实际客户DOWN-UP[0,0]→[1,1]；wrapper wait0／exit1／desc0／forced=false／product_cleanup=false，各自判定。23:34:48Z精确guest33468／任务清理且证据未变，无其他资源改动。Release f54d采用同212／0f2／包装3ea仍进行中。新0fc driver仅在三包快速手势之外增加32ms粗时钟余量，仍要求实测间隔超过系统双击时间；D/R构建及各1项guest-path通过（0.14/0.19、0.12/0.25秒），EXE哈希见验收表，现场NOT_RUN，不重建产品或换绑旧运行。

## 212 Release／477a：普通便携入口与组件已测，整批FAIL

同212产品／driver，Release EXE `E2F93AEC…AC69BE`／driver `ED8FBEBF…A0ABEB`，包装674b94c，fixture3500行合法连续。完整身份、原件哈希和13条[字面原行](v1-evidence/477a-r10-release-entry-components-and-real-grid-handoff-selected.jsonl)见索引。1219实际7635 ZIP普通GUI、保存配置后Move-UP、正常退出及字节检查仅入口scope通过，不等于整I12。设置GUI／诊断、startup、五键、导入、失败保存／诊断失败、损坏、排除与I08、visual3043／inflight3271／batch3365组件通过；tray BLOCKED。2545独立60.0119733秒测得CPU0%、WS27,480,064→27,504,640字节、handles284→278、GDI26／USER31，不授I11时长。

fast CtrlMove306实际最大化且START／END均0；两DOWN间隔437ms只作待验证driver双击线索，不定根因。dock1533／I11 2517首click终止未确认，本批没有完成两轮或900秒。grid3287旧driver报告START失败，但产品轮转原件PID7024的331→333→335→336→337已真实观察pending、START、cancel、END，继而`source_or_geometry_context_changed`拒绝且无新placement；不是平台无START。原始非17-grid合法起点及native前缀实际几何变化由driver修正，产品guard不放宽。Explorer资格FAIL；Terminal production START未观察，Notepad／VSCode无合格新root BLOCKED；Edge暂停native客户基线失败、root关闭未确认。wrapper wait0／exit1／desc2／forced=true／product_cleanup=false，正常资源退出不冒充应用清理。23:15:28Z精确guest32668／任务清理、证据未变、无其他资源改动；不是产品cleanup PASS。产品当前日志仅保留371–520，`.1`为此前多进程保留部分，不声称全部process原日志连续。后继driver-only修订未现场，不改绑本批身份。

## 212 Debug／c1f8：核心子链通过，快速右Resize原FAIL保留

后继仅测试驱动`0f2b949e81ce8013f6f2f5f6656ae487603ebad8`已D/R构建退出0、guest-path各1项通过；不重建产品。D driver `AC99788B52736E67B39EB820EE37C5DAD877FD36F4A5DCEB8A25DB087B02A21D`／R `4B35127B8C413C1EF052DC969D26A3B79AB4CBCE6142F847481DE8F6ADD3C87D`，实际self-identity均0f2。定向修正原生首击证据、独立路由双击间隔及grid合法起点；只在自建窗口线程观察真实queue UP，不改产品NormalUp门或任一旧FAIL。后继driver现场仍NOT_RUN，不能替换下述原driver212。

product／driver源码均`212e1979f9bf711569d63f0e371c6e7ff8375f2c`，Debug产品`8AF5BB9B…C4CBB6`／driver`315FBB1D…AC6C6E`，包装674b94c；原fixture216／产品276行均JSON合法且sequence连续。完整RunId、原hash、ZIP／manifest／WSB见索引。[fixture原行](v1-evidence/c1f8-r10-debug-owned-core-native-only-and-fast-resize-failure-selected.jsonl)178核心scope成立；[产品原行](v1-evidence/c1f8-r10-debug-native-geometry-group-and-up-boundaries-product-selected.jsonl)51／220两次Ctrl Move各3成员frozen，154→171普通Resize真实NormalUp，239另一Ctrl正常UP。快速plain185／Ctrl197仅原生位移与真实LL-UP保留，无产品cancel／capture／writer，**不授Glue或增强Resize PASS**。

207快速右Resize实际矩形exact、native START／END各1，但product270／272物理按键门false，271／273拒绝且零新placement；本次product／被动head UP callbacks均0，269 Raw UP generation0/source0不能归属g10，不补NormalUp或推根因。Full在此结束，Ctrl快速Resize及本Debug后继布局／设置／故障／应用未运行。产品自身资源退出与214客户DOWN／UP[0,0]→[1,1]另有实际证据；wrapper wait0／exit1／desc0／forced=false／product_cleanup=false，不抬整批cleanup PASS。23:08:10Z精确guest3652／任务清理、证据未变，无其他资源改动。后继Release477已完成，实际子范围与失败见上，不改变本Debug结论。

## 212运行前构建记录（后继c1f8／477a均已实测FAIL）

Debug product／driver实际self-identity均212，产品SHA `8AF5BB9BDF0555EB070E40E902D8DC951D122C8010AB0783DF9B28A878C4CBB6`、driver SHA `315FBB1D8217774736111EB40927B932F9BA14ED7357B364D47E3C5703AC6C6E`；11/11 offline PASS，processor3.27／wall3.32秒。原生CTest output-log `uat/v1-control/r10-212e197-Debug-offline-20261017.txt`／SHA `55C972BE0F869290DB88C901AE8A05E1F87A5A87C81C4B11E92F589F6F314FA1`。Release product／driver实际self-identity亦为212；产品 `E2F93AEC6714551E5F49A40CE07D8CC512F847AF77067145B46A4C57DBAC69BE`／driver `ED8FBEBFE7370E74E4BA16E35D8CBA4F39674E35326E1FDAAAD91F821BA0ABEB`；11/11 offline PASS processor1.69／wall2.99秒，原生CTest output-log `uat/v1-control/r10-212e197-Release-offline-20261017.txt`／SHA `686B310183EC35A1723FBB97CD641E1F48633948F5BA3E25F5BC03547EA363C7`。构建和offline不是现场PASS。

源码／实际product与driver身份`212e1979f9bf711569d63f0e371c6e7ff8375f2c`，构建目录`out/v1-product-native-geometry-r10-212e197`。普通／Ctrl单击Move及边角Resize走真实START→隔离／cancel→实际END／fresh B→共用writer；保留原DOWN意图及Ctrl组，不用B重起算。原DOWN limits仅已有系统/default测量，应用declared tracking limits实际在begin B查询，不能写成DOWN已实测。fast native-only透传只证明原生位移保留，不授Glue／增强Resize PASS。后继已实际运行c1f8／477a，均整批FAIL；旧356整对象保留为observed历史，不重绑定。

## 187 Release／356 Debug最终实际范围

[28d7 Release原行](v1-evidence/28d7-r9-release-owned-rounds-900s-components-and-app-boundaries-selected.jsonl)：product／driver均187，包装b2cc，5588行合法连续。2998／3912同PID3744完成两轮，20slot实际900.000199秒；4646原FINAL_REVIEW_PENDING输出保留，4653整I11仍BLOCKED，不把两轮／时长抬成整链PASS。4659独立60.013641秒、CPU0%、WS30,593,024字节、handles280→277、GDI23／USER30仅该测量scope。startup、五键、导入／失败保存／诊断失败／损坏／排除有子PASS，tray BLOCKED；新799d ZIP普通GUI／保存后Move-UP／正常退出与字节检查1208仅入口范围。I08 2047、视觉5157／在途5385／grid5450／batch5518组件通过；Explorer5533资格失败、5535清理E_ACCESSDENIED且root未消失。5550明确builtin conhost而非现代Terminal，ownerGTI未知、单阈值MOVE后未见START，后续Code／Notepad／Edge未运行，不继承别批。wrapper wait0／exit1／desc0／forced=false，product_cleanup=false，guest16220／专用任务22:32:57Z精确清理且证据未变。

[f706 Debug原行](v1-evidence/f706-r9-debug-owned-components-and-application-failures-selected.jsonl)：product／driver均356，包装08ff，2940行合法连续。174核心、686真实垂直／水平／T→L／四格／八窗、1691本屏dock/stretch、2053四cause与actor默认30秒、2533视觉、2750在途及后继恢复已运行；I08输出明确不授全合同，grid2805仍partial／original_UP_observed=false／whole_I10_PASS=false，batch真实清理UP子项保留。旧b1a3正常T早退未复现，不等于查明或修复缺UP。[产品6956/427原行](v1-evidence/f706-r9-debug-observed-foreground-termination-origin-selected.jsonl)实际记录winevent_foreground、g10、foreground2950050/source3932928/overlay4260670；与旧T不是同代／同场景，不猜触发原因或反推根因。Explorer资格FAIL；Notepad／Code本批无合格新root而BLOCKED；Edge暂停native客户基线失败未增强且root未确认关闭；classic conhost无START，Terminal根实际关闭。2931退出后客户投递未观察，wrapper wait0／exit1／desc2／forced=true／product_cleanup=false，不能以guest销毁补PASS。guest24232／专用任务22:38:01Z精确清理、证据未变。两批完整身份与原hash／ZIP／manifest在索引，不增加功能或B项。

## 356运行前构建记录（后继f706已实测FAIL）

product／driver两配置实际self-identity均`356d9e1b844c3f6d3efba41dd6ed39ca8f118996`，冻结`out/v1-product-termination-origin-r9-356d9e1`；D/R combined构建退出0、各10offline通过，D processor3.28／wall3.34秒，R processor1.44／wall2.63秒。D产品`5034AFD5E3672C7EE632DEBDA1880A18C76C2939ED1732139C09E0153FF37071`／driver`641DD0CD5FFFC31F7BAB01C59FA9C51469A2EA8F00FBEE07408EF812E868F5BD`；R产品`49D92161FB72179DBFE9B47236DE3CB1E8759B87E5BA427EFB8C57E8DAB94EBE`／driver`F898D300EADEDBAC324444A85A29CB7012C8766FB775DC06B76B9611A8AEDD1F`。原生CTest output-log SHA D `B9C964FB85AD64FF80D6F874FBCD3F36611BA4D74E5CBE5E6C416CF535054741`／R `646991EF391406CD0BA44054F67DA0F50F13FA2AD4E26311F3B6393F8D2FD4B7`。仅诊断、撤权先于观察／GUI日志，不改原三分支判定；不是逐测试原stdout或现场PASS。187/b1a3关键原行已入索引，FocusLost后实际LL-UP不补成NormalUp；28d7后继已完成且整批FAIL，各子范围见上，原现场身份不换绑。

## 187运行前构建记录（后继b1a3与28d7已实测FAIL）

源码／product与driver self-identity均为`187616742a1b7195ce46dce6d9df881b413975e6`，冻结目录`out/v1-product-input-display-r9-1876167`；D/R构建退出0、各10offline PASS，D processor2.98／wall3.04秒、R processor1.58／wall2.83秒。D产品`47C6B5ED47E7A469BF446F4BD006A34A2AAF3C92F39A05B14A51FCD01D7FDDB0`／driver`3EB70587CD05E7D735AB0A4BC9F0C6179FEAD5B65429BD9DA9C9640E0C231BCE`；R产品`D4E0EA86219FF2AC48A3F91C45A77A726622A7CF6576DA8584F4ECE5C075BE64`／driver`8FB159A6D264360275FB5EEFA80B4A6A20BFD2F81AF1117FF3D9EA901689B029`。原生CTest output-log SHA D `DF5436F78B79584C1CB898BBB4AB4A39971D258C52D3F884AE9D8A3AE84060CB`／R `722C2200CF0527853A66BADE82082FAEBC1E440B2610C96820D4754A0491CE58`；不是逐测试原stdout或现场功能证据。

V1-D26仅登记源码确认的captured实际MOVE／DOWN发布队列缺口及4f39→187修复，待定向runtime；**不是V1-D08缺UP根因已查明**。native只在真实CaptureReady后续行，UP后不新continuous写。187另接显式目的display proof／batch／late completion及accepted source上下文刷新，不重锚原DOWN；NextDisplay选择相邻而非Enum顺序、重复几何拒绝。多屏现场未取得，mixedDPI实际frame非exact时拒绝、不加第二次placement，不授PASS。新portable、Debug受影响路径与Release完整链待实际执行；2b／4c中间构建仍未部署，不再发布已知缺口的中间EXE。

## 0be2＋3fd3：旧82d组件定向推进，新R9待实际验证

两批产品仍为`82d66a0821e13d49903b9cfd1cf8186193ea5aba`／`C796DDA…ED0DE`。0be2 driver／包装`0260ce2`／EXE `20BADD9E…DEEB5`；3fd3 driver／包装`f3848e9`／EXE `06C90F6D…B95E8`。0be2为14条、3fd3为15条原字节摘录（含1359实际retired-pair LL-UP、release／detach／Gone判定，仍不授normalizedUP／NormalUp）；完整RunId／ZIP／manifest／原件哈希见索引，旧失败不改判。

[0be2原行](v1-evidence/0be2-r8-owned-components-and-retired-grid-up-verdict-selected.jsonl)：1720行合法连续。859 I08组件实际通过（source-close真实UP及恢复、actor默认30秒与后继恢复）；1350视觉、1578在途、1711 batch组件通过。在途1456只认可同代实际LL-UP先于abort的形态，1460真实late exact、1461 alpha173恢复、1474新Move／1481新UP有实测，不伪造NormalUp。grid原1602／1643 FAIL保留：唯一测试UP QPC区间1144310328..1144317458内实际观察到g7/button1回执1144314324，但已退休、未push；1600实际Medium/exact source。旧normalized判定漏接不是缺LL-UP，也不是正常UP通过。

[3fd3原行](v1-evidence/3fd3-r8-grid-scope-and-recovery-resize-up-failure-selected.jsonl)：1475行合法连续。1107视觉／1335在途／1398 grid／1466 batch组件通过；1392仍`original_UP_observed=false`、`whole_I10_PASS=false`。I08 616 FAIL：g13恢复Resize590 exact，593唯一UP QPC506510856..506527585，594未见匹配UP；599最新公开callback仍g12/500993717，不能套前批retired-pair解释。0be2 I08 PASS仅保留原身份。两批实际wrapper均wait0／exit1／desc0／forced=false、`product_cleanup_pass=false`；产品正常资源退出／真实客户恢复与精确guest32164／7024及任务清理分别成立，21:05:29Z／21:20:51Z证据未变，不以guest销毁补PASS。均跳过I11、idle60、设置、便携、Explorer及apps。

新生产候选`2b038dbd9f71cf02d27c629a8ed1a9f1c16393e8`已把普通Medium入口接到实际资源准入策略；资源不是source authority，High拒绝、WDAG原proof与失败不fallback保留，未向宿主发送测试输入。冻结构建`out/v1-product-resource-policy-r9-2b038db`，D/R self-identity均2b038、构建退出0；各10项offline通过，D processor3.12／wall3.18秒，R processor1.50／wall3.35秒。D EXE `422F9C1D8DDD3764E510B500972045347D3602B667D656DE68A8B32CC1767230`、R EXE `F0406FA35EEEA4CAF09C7A662C90002AD845D6BE37F5C5B8D315EFE64F7878F7`；原生CTest output-log哈希D `35357B87C6AB70DA962432F4DA3E2535E9C599778E22D981C81EDA07D0CA7106`／R `2CDEFE48344D91BED98A85B17E0EB4ED62B64ACE8049566ECE04E9781137E4AF`，非逐测试原stdout、非现场功能PASS。新便携／普通桌面／Release整链待新包实际验证，不清空旧证据、不涨功能／B分。

## 7019＋a27f：有限组件与Terminal实际推进，旧失败不改判

两批产品均`82d66a0821e13d49903b9cfd1cf8186193ea5aba`／Release `C796DDA…ED0DE`，driver`59c39b0adcd4baedd9051fde3b50bd4809c2335d`／`764C7D5A…CCB3A`，包装1444；完整ZIP／manifest／原件及每Run16条字面原行哈希见索引，不重建产品或重跑900秒。

[7019 owned原行](v1-evidence/7019-r8-owned-components-visual-batch-and-retained-up-failures-selected.jsonl)：1704行合法连续，1349视觉、1695第二成员真实native故障组件PASS；495 dock因单屏BLOCKED。730 source-close缺真实UP，760后继Move／Resize／UP和851 actor后恢复已执行。1455旧在途判定FAIL保留；1459迟到native exact／1460实际alpha173恢复通过，**1464下一Move-UP为NOT_RUN**，不拼完整恢复PASS。1584唯一原计划UP点实际High Progman，1586仍无本代UP；不由此猜系统根因。1696产品正常资源退出与1701客户投递独立成立；wrapper wait0／exit1／desc0／forced=false，但整批FAIL、product_cleanup_pass=false。20:49:04Z专用guest33004／任务清理、证据未变。

[a27f应用原行](v1-evidence/a27f-r8-terminal-geometry-and-app-limitations-selected.jsonl)：207行合法连续。Terminal55真实唯一dragbar(730,181)，五阶段Move／native START-END、79真实Raw／legacy NormalUp、88 Resize、命令及119 Escape恢复有实测；122客户点HTCAPTION=2，125拒绝且未发客户输入，124像素仍见cmd启动0xd0000022；136 exact根关闭成立。Code157本批指定EXE缺失，不能称再次复现旧GPU；Edge176暂停native基线NChit1460、未发click，191 root关闭未确认。197产品正常资源退出与202客户恢复独立成立；wrapper wait0／exit1／desc2／forced=true，不能写应用或整批cleanup PASS。20:51:43Z专用guest28368／任务清理、证据未变。

0260 driver仅修before-abort实际UP证据形态和exact清理UP点，现场仍NOT_RUN；85d仅部署前Suite脚本拒绝，无guest／输入，不是产品FAIL。保留1acb等原失败，不新增功能或B项。

## 1acb：应用真实像素与启动失败，driver退出与清理分别判定

[13条关键原行](v1-evidence/1acb-r8-applications-pixels-and-driver-exit-selected.jsonl)绑定产品`82d66a0821e13d49903b9cfd1cf8186193ea5aba`／`C796DDA…ED0DE`、driver`e3b8c826aee27aae0c65b857cf0d3a8097a82b4f`／`37BDEF79…72927`，包装47df；Run `1acb8c6bd6184c5bbf615497bf6bb57a`的完整ZIP／manifest、原件及像素哈希见索引。63仍缺真实START；67单次测试清理UP后69记录精确Terminal子窗、70采集实际矩形像素，已用view_image读取：终端内显示启动`cmd.exe /d /q /k`失败，错误`3489660962 (0xd0000022)`，不是可输入终端通过。Code109真实electron-debug.log有六次`GPU process launch failed: error_code=49`及FATAL，实际命令已有`--disable-gpu`；不由该号猜UIPI或GPU驱动根因。Edge133真实HTCLIENT点击无DOM效果，138实际像素为白区；不能认定与Terminal或Code同因，146正常关闭仍未确认。

156产品退出0及其自身资源回执独立成立。163／164同driver STA的Catalog释放调用边界完成，不声称WinEvent native Unhook返回成功；166只是结束marker，**不是退出证明**。本批另有Release-launch／guest-summary：实际wait0、fixture exit1，退出时仍有4个后代，`forced_test_cleanup=true`、`product_cleanup_pass=false`，不能写成成功退出或以guest销毁补清理PASS。20:33:53Z精确guest12240／任务清理且证据未变。后继driver`59c39b0adcd4baedd9051fde3b50bd4809c2335d`现场仍NOT_RUN，此处不填新构建哈希；产品82d字节不变，不增加功能或B项。

## 128f＋e5：同一产品的应用失败与入口复验分开

两批产品均`82d66a0821e13d49903b9cfd1cf8186193ea5aba`／EXE `C796DDA…ED0DE`，driver均`22d6e601a444d4af0ca853459abfb3168b661dd7`／`26542BC…9CE65`，包装717a1bb；完整guest ZIP／manifest与原日志哈希在索引，未重建或换绑产品。两份小摘录共39条原行，保留64位原字面量。

[128f应用原行](v1-evidence/128f-r8-applications-and-driver-exit-unknown-selected.jsonl)：Terminal50精确绑定，但61只有nativepending、63无真实START，未到增强五阶段／client截图；75 exact根关闭确认。Code96实际`--disable-gpu`，99实际Medium child退出0x80070005、100–103 hidden、107无合格根，原因UNKNOWN。Edge129已经真实HTCLIENT=1/error0并尝试一次客户click，DOM效果仍false；pause/resume真实，无增强Move；144/148根关闭未确认。150产品正常资源退出及155客户[0,0]→[1,1]独立成立，**158结束日志不是driver退出证明**：原launch／guest-summary均缺，driver进程退出UNKNOWN，跨STA析构仅候选解释。20:08:51Z专用guest20208／任务精确清理、证据未变，不补产品或driver PASS。

[e5入口原行](v1-evidence/e5f5-r8-startup-portable-rapid-scoped-selected.jsonl)：331实际disabled checkbox／334 Run命令／342登录模拟／345保存状态／347同实例／371恢复入口，388启动子项PASS；34bf301旧失败原样保留。294快速短Move是native-only passthrough，非增强或NormalUp。856保存失败、912诊断导出失败、980损坏恢复、1117排除及同一9281ZIP普通GUI／二启／默认和保存后实际Move-UP／退出1208子链通过。994托盘BLOCKED，1223/1225整批FAIL，不算整I12或新功能/B项。与128f不同，本批launch和summary均存在，实际wait0／exit1／无强制清理，driver退出已观察但不是成功退出0；20:14:31Z精确guest10692／任务清理、证据未变。后续仅定向修复退出生命周期和未过应用，不重跑900秒。

## 34bf：当前普通ZIP与两轮／15分钟实测推进，尾阻断是driver错误

Run `34bf39f969df4ce7b14cb1ab2d758bcd`仍使用产品82d／`C796DDA…ED0DE`、drivere138／`BA311956…D2CC`、包装dafed。ZIP `8D46E55171DAD98AF48C1C19074DA7E7797F2B9F654FF0CD76AB231ED884FA0D`／manifest `114F96DF52472536DFFF227C7E810CDDDD52F8116B13FE87B019BE8EE18BAE19`。稳定采集`collection-20261016T193948339Z-01f6e0a11ec04ec597b9e4e87a1e5a3b`含4007行合法连续fixture、原SHA `45E897ED5CE56912D72ACDE1AE04A22570E09BA3EF8A0A26557BD4878D40D112`。19:40:58Z确认guest12264／birth639277752443409911及专用任务清理、证据未变；不以guest销毁补产品PASS。

[入口原行](v1-evidence/34bf-r8-entry-and-ordinary-portable-fixture-selected.jsonl)：设置GUI／诊断、rapid组件、五键、导入导出、保存失败、诊断导出失败、损坏恢复与排除有实际子PASS；301启动保存后disabled checkbox仍FAIL、854托盘不可达仍BLOCKED。264实际三包快速普通Move精确完成(35,31)、原生START／END各1及真实LL UP，产品cancel／capture／placement均0；这是native-only保留，不是增强NormalUp或快速Ctrl／Resize通过。9281七文件ZIP逐项核验、空参正常GUI／无参二启、默认Move／真实UP、保存配置后实际Move／UP、正常退出／原字节与无profile均通过，1043仅是普通ZIP入口范围，不能将其内部`I12=PASS`当整版I12。

[有限连续原行](v1-evidence/34bf-r8-owned-two-rounds-15min-and-normal-exit-fixture-selected.jsonl)：同PID5260在2528／3304完成两轮owned核心I02–I06动作，真实postResize A及Ctrl C与UP均包含，未重启；20slot实际混合操作／空闲覆盖900.001415秒。五次handles284→279→286→282→282，GDI23→26→26→26→26、USER33→32→31→30→30，已审有限样本不严格单调增加，不宣无全部泄漏。4000实际正常资源退出、4005真实客户[8,8]→[9,9]独立成立。**3999是`invalid string position`驱动错误，不是I11资源断言失败**；后继视觉／在途／grid／batch／Explorer／apps和独立idle60未运行。bounded产品日志仅保留PID5260的2796–3441连续行；更早关键回执保留在fixture，不伪称原产品日志从1仍全在。

[保留失败](v1-evidence/34bf-r8-sourceclose-actor-and-driver-failures-fixture-selected.jsonl)：source-close1647队首实际UP0、1648产品缺匹配UP，**真实release／detach／Gone已成功，后继g21恢复成立**，不是产品清理失败或必须重启。actor真实30047ms默认Deadline和资源idle通过，但紧随g24仅见nativepending／未见START，1702失败，不把actor资源PASS升级为完整I08恢复PASS。随后以22d6e60修driver整份fixture 2MiB读取为阶段suffix，D/R构建及guest-path各1/1已通过（D processor0.15／wall0.18，R processor0.11／wall0.22秒；原生CTest output-log及哈希已归档，不是逐测试原stdout或现场PASS），修订现场NOT_RUN。继续未运行应用，不重跑900秒，不给功能／B项或整版加分。

## c17e：Release六段手势闭环，Ctrl C前置失败仍阻断整链

产品`82d66a0821e13d49903b9cfd1cf8186193ea5aba`／Release EXE `C796DDA39CF86370911FA0CD7A4FAEE876C2E87F221C798318FA0EAB8CAED0DE`，driver`e138b6d5e837f510468a48c60b5338540738d5f1`／EXE `BA3119560664110E26A020E0F31B47211BC2379395BACBE0BC4B189A1619D2CC`，包装`dafed9641746600b8c40f8b7436fee1b4433c191`。Run `c17e1f23d7bb4b9d9f10a0c19c471390`，ZIP `13B43ED386CD3300B3AC9E9DED11BF4652777655CB04D91AE4DBA6757C52116A`／manifest `E044A336FFB44CC552CB48C01340C1157CA766603DEA282C0B6A80C62998387C`。稳定采集`collection-20261016T191755242Z-2c51a23b3913419f8832548fd073eb2f`中fixture161／产品203行均合法连续；19:18:29Z专用guest／任务清理、证据未变，产品正常退出与实际客户输入另有回执。

[fixture原行](v1-evidence/c17e-r8-release-six-gestures-and-ctrlc-preinput-failure-fixture-selected.jsonl)与[产品UP回执](v1-evidence/c17e-r8-real-up-timestamp-and-normal-cleanup-product-selected.jsonl)：PID6408的g1 A五阶段、g2 Ctrl A真实带两follower、g3 B、g4 C、g5实际B Resize、g6 postResize A五阶段均exact，六段实际UP／NormalUp／release／detach／NormalGone成立。实际native数量4／3／4／4／1／4；按同机被动head UP QPC与native_before_qpc比较，各代UP后新增调用0，不以JSONL顺序冒充共同原生时序。原legacy派发QPC现为正值并进入normalizedUP，7149旧50FAIL保持原样，仅该时间戳缺口定向通过。149 postResize A正常终止通过，150／151 C前台与非客户命中成立；152 cursor到点后153`test_key_state_not_observed`，Ctrl C尚未发鼠标DOWN，无g7，不能记产品CtrlC拒绝或完整I02／Full通过。154实际正常退出、159客户DOWN／UP[0,0]→[1,1]与guest销毁分开。新便携9281e84f9cf844e281f91f7d0ca19037已生成（ZIP `EE1EF02D3E79A601D500706575B03E69514EA98F4EF5C10856AB6F6BC6F31C46`），普通GUI NOT_RUN；Core后继／设置／apps未由本批完成，不加功能或B项分数。

## R8／7149：真实native接管与正常UP已观察，原断言失败保留

后继仅driver修订`d06383562c2d7dc201066a8c5e35a90ebbf37ef5`补记现有单次修饰键测试包的真实发送QPC／返回、原始Async SHORT、500ms既有观察及首次前台变化；不重发输入、不加hook、不放宽原失败门，也不推断UIPI原因。独立`out/v1-driver-key-observation-d063835` D/R构建退出0、guest-path各1/1（D processor0.17／wall0.21，R processor0.15／wall0.26秒）；实际driver哈希D `DA5765C42384D6D7AFB6E6AE439B80263EDDE5945F6F65DBE55599452D736B55`／R `4DF572DD3E98B57062F2FBC4A9213A2177D53D8392EE0B1D49FAD555A5EA0CFC`。原生CTest日志`uat/v1-control/r8-d063835-{Debug,Release}-driver-offline-20261017.txt`哈希`7865D9FED29C882A7502D7641532598E24F7DCF75B726DE94BAADFABBB947405`／`AB1CFFD4B353AE29192C75A516C9EA6D289B0CA66A770D5333C7AA5DA7A96B13`。产品82d EXE未变，新driver现场NOT_RUN；e138的c17与后继分组运行不得换绑d063。

Run `7149a4d98b1a4a238cfb28956cb80ce4`绑定产品和driver源码`1759637eaabc604f5564801d70c20e837d573b78`，包装`778f4a421eb4eaa5744d73bd1eaec2deffcd9dd9`；Debug产品`64A111…5263`／driver`7E7E95…1196`，Release产品`60DA1C…ABB8`／driver`528D9C…4CDE`，完整哈希见验收表及索引。guest ZIP `1EF67570B7DA58B68DD13F8204FEB4413A32ED4D1DE78235E2B24A0E10E0A479`／manifest `9D851CBD5E6C9365FC9FD59B12428EBFFEDB36C4D6A2442ECD01B90C2A52D210`。稳定采集`collection-20261016T185142897Z-0876f791ae70450e8f003cd366065d17`：Debug fixture59／产品41、Release fixture41／产品8行均合法连续。18:53:10Z回执确认本批guest26360（birth ticks639277734378876101）及任务清理、证据未变；与产品真实退出分开记录，不以销毁guest补PASS。

[Debug实际产品原行](v1-evidence/7149-r8-debug-native-handoff-actual-up-and-cleanup-product-selected.jsonl)中PID6528/g1真实DOWN→START→isolation→一次100ms bounded cancel→实际native END→ATI／capture后进入同一writer。[fixture五阶段](v1-evidence/7149-r8-debug-five-stage-and-original-up-verdict-fixture-selected.jsonl)自由、XY吸附、保持、拉开、重吸均exact；四次native、保持已exact不调用native。随后实际Raw UP、本代accepted legacyUP、capture释放／detach／NormalUp Gone及gesture_end normal_up=true被观察。**原50 FAIL不改**：native legacy分支`legacy_up_dispatch_qpc=0`，规范化UP的`resource_observed_qpc=0`被实际证据门拒绝；该缺口不等于本次真实UP未到。后续Core／post-Resize／设置／apps未执行，首次A不算完整I02或功能PASS。

[Release前置原行](v1-evidence/7149-r8-release-preflight-no-product-gesture-fixture-selected.jsonl)：31的exact owned客户激活单次DOWN／UP虽各返回1，但真实计数仍[0,0]、foreground65792未变；32／33 FAIL，产品LL callbacks0，未形成产品DOWN／START／placement。原因UNKNOWN，不推console、权限或产品磁吸根因。新便携84ade69d76874b8c975927d0065b8304仅prepared（ZIP `4CD29CE82DD7723E559886A0CDB9F649155BCA36BB769681568B52D2D22E16C5`），普通GUI NOT_RUN。

修复候选`82d66a0821e13d49903b9cfd1cf8186193ea5aba`只修真实legacy派发时间戳，不将175旧记录换绑。独立目录`out/v1-product-native-move-up-r8-82d66a0` D/R产品构建退出0、各9项产品offline通过（D processor3.47／wall3.55；R processor1.44／wall2.66秒），不含另行修改的driver guest-path。Debug EXE `B9862A22A443B75ED4B2304592FE03A24D5C53E2B298C158C09CB8F557BF1EFC`，Release `C796DDA39CF86370911FA0CD7A4FAEE876C2E87F221C798318FA0EAB8CAED0DE`。原生CTest output-log为`uat/v1-control/r8-82d66a0-{debug,release}-product-offline-20261017.txt`，实际哈希已核入索引；不是各测试原stdout，不是现场PASS。构建当时未运行，后继真实c17e范围见上，不回写为175曾通过。

测试驱动另为`e138b6d5e837f510468a48c60b5338540738d5f1`，产品82d EXE不变。复用原actor事件／映射传递最多一次真实阈值MOVE，再由实际START／END／CaptureReady确认原续行MOVE；原始A与实际B分别核验，无UP退出合同保留。现有激活点击只增加已注册Raw observer的前后观察，不重发输入。`out/v1-driver-native-actor-48a4f5f`最终D/R driver构建退出0、自报e138（目录名是未部署中间稿，不是最终身份）；guest-path各1/1通过，D processor0.14／wall0.18、R processor0.12／wall0.22秒。Debug driver `A68563B356512A4BD10EDA0AE53C568349FABDCD6FF472BAEB7C5F4768F92944`／Release `BA3119560664110E26A020E0F31B47211BC2379395BACBE0BC4B189A1619D2CC`。原生CTest output-log为`uat/v1-control/r8-e138b6d-{Debug,Release}-driver-offline-20261017.txt`，hash分别`572A04B41B4D37BDCF2E0913818B6342436CC08078F927E45055F7EEF2605392`／`564C64C97A37D29D6518E673E37923F40D5C00F2C254D14613CC1080435697FA`。现场仍待新批真实记录，不继承175或R7结果。

## cb0b：入口与设置保护有实际子结果，后继仍未完成

后续R8产品／driver源码为`1759637eaabc604f5564801d70c20e837d573b78`，已接线并完成Debug／Release /MT构建（运行前快照，后继7149实际范围见上）：guarded guest 内普通非最大化、无修饰键的单击标题栏 Move 不再先吞 DOWN；短手势原生保留，增强须真实 START→隔离→一次 GUI 有界 cancel→真实 END／新鲜 B→实际 CaptureReady，再走原共用 writer。原 DOWN A 仍是意图锚，B 只校验连续性；停靠拖出描述在原 A 冻结。Ctrl／Resize／映射／最大化旧入口保持，不能推定这些快速操作也已修。真实 UP 撤权、不补连续写入及独立 F11／deadline 保留；callback 预建 pair、同一 START/END hook 和发布顺序已接入，不预填性能或功能 PASS。owned driver 增加真实阈值 MOVE 的有界适配，rapid 仍原三包输入并分别严格判断 native-only／增强；测试准备补齐三窗可见状态。Code 仅 guest 尝试官方软件渲染参数，旧失败原因仍 UNKNOWN。后继运行使用新的产品／driver 身份，绝不换绑本节旧原件。

R8构建目录`out/v1-product-native-move-r8-1759637`：产品Debug SHA256 `64A111A2584930CB71D5E6B7BBA8D1E98BC461266DA8D411CEAF260A13A65263`／Release `60DA1CB5EB5B914D09A92CEAE605CA122121162A4D94AA75CABE11B2C9F3ABB8`；driver Debug `7E7E95E09EF663CB85E80B4F8D4F14FA2E5BAF1B80E6BA83210C8F1527D11196`／Release `528D9C577A61A0F48D66B1F2CDB8EA349CDFE869A4E131B0BBD7506384734CDE`，各EXE自报源码1759637。初次测试编译因两处initializer-list不能传入span失败，随后仅改测试为显式array，原断言保留；此测试修订不链接产品／driver。受影响10项offline两配置均通过，Debug processor3.34／wall3.40秒，Release processor1.72／wall3.00秒。原生CTest output-log分别`uat/v1-control/r8-1759637-debug-offline-20261017.txt`（`B9A95327A2BC2B72A103BEACDBCB9E10EEE5059CB38218806ED4ED2DEBC2A668`）及Release同名前缀（`FB8DEA37D37699C9E22105DF7915C78DD758BFF3A14DEE02C7F89340FA34AA13`）；本段为工具结果转录，不是现场功能PASS。

Run `cb0b735bf4cd4ac0b46374df1bff0ab7`为Release IndependentEntry，产品b045的EXE保持`F78C5E…BD22`不变；driver／包装为`fc06fc9d476073d82eba0372eb37e5353e08e419`，Release driver `8E1A0EECACECC1E1383C54800503B84400E2E9C32671C3ECAF808A5D48532DF6`。本次driver D/R构建及guest-path各1/1通过（D processor0.17／wall0.24；R processor0.12／wall0.23秒），这是主执行者工具输出转录，不是归档原stdout或现场功能PASS。ZIP `4158155D73B9E96AA767F6DC59C4FE854FE6CA1104893ABEF436D3BFB733E8C8`／manifest `BBF4752E4165999BB7505D6FCAC712FF6C45A9E3947B24D669D2ED6B2386648F`。最终`collection-20261016T180807661Z-c54ce3e1d4b84a5aa08174a019b82554`含697行合法连续fixture；18:09:39Z确认guest23368／专用任务清理、证据未变，部署保留，不代替产品功能或cleanup通过。

[入口／设置原行](v1-evidence/cb0b-r7-entry-settings-partial-fixture-selected.jsonl)：253诊断、358启动、470五键映射、635导入导出子项PASS；201设置exact-idle激活门失败且未发点击，225GUI阶段FAIL。265快速三包为generation0、产品START／native均未形成，独立head实际UP1，不能记作已启动手势缺UP。652–671真实READONLY故障下原文件／草稿／代次保持、实际错误提示与确认、诊断导出及原属性恢复通过；但682旧策略预期snap失败，[产品PID6024原行](v1-evidence/cb0b-r7-old-policy-actual-placement-product-selected.jsonl)191实际free placement exact为`[134,83,534,443]`而非预期`[140,80,540,440]`，原因尚未确定。689随后因设置失前台gate终止。corrupt、tray、普通便携入口、Core／apps及新batch／Terminal capture修订本批NOT_RUN；不授本批完整M11-F05、设置功能或B项PASS，不取代4e9 Core与625时长证据。

## 新R7／4e9：actor和在途恢复推进，当前整链仍未通过

产品b0450da在`out/v1-product-idle-catalog-r7-b0450da`以/MT独立构建，Debug／Release退出0、各10项offline通过（D processor3.35／wall3.42秒；R processor1.56／wall2.79秒），CTest output-log哈希登记在验收表／索引。Release EXE `F78C5E95C6F9BC77993217AC6D5023878A210C75D7C27632B757704A060ABD22`。shipping driver7bec另经D/R guest-path各1/1（D processor0.18／wall0.22，R processor0.14／wall0.25秒），不把产品构建时guest-path与shipping身份混写。

新便携`364579589e454eb7a63931f68df49567`已生成，ZIP `046CE03ABB916BF4D114B899E95586AE927270AC1EAAEDC24F3D454BEBE99DC4`／manifest `60623F8D2CE7DFFC00F26714AFA81410DA6F743236FACB462B2BC4B1914F777B`，普通入口NOT_RUN。guest包`4e9eb4a7e1494054b2791c3797e78a37`绑定productb045／driver7bec／包装7bec，ZIP `F6FCEC6D5D370AB4FAED7D56EBF0BF06A5AB631675D0DD5F3CE7F65EFAAC8DE7`；本批Release CoreAndApplications整体FAIL，1836fixture行合法连续。最终收集`collection-20261016T173413819Z-65483a0d73584b9bb15ea589101d1f26`及17:36:31Z回执确认专用guest11332／任务清理、证据未变；Edge根关闭失败不由此改成PASS。R6下述实测绝不换绑R7。

[本批组件原行](v1-evidence/4e9-r7-actor-inflight-and-remaining-failures-fixture-selected.jsonl)：653实际默认Deadline30031ms，684同产品后继Move／Resize通过，不再沿用R6 actor失败作本批结论；616 source-close清理UP仍缺。1554临时视觉在途组件PASS，[产品原行](v1-evidence/4e9-r7-inflight-completion-and-restore-product-selected.jsonl)显示PID5636已发call33真实成功、迟到identity／geometry exact且不授新写入权限，实际alpha190→173恢复及后继g6 Move／NormalUp；该组件未额外验证Resize。1576 grid缺真实本代清理UP；1636 batch保留normalized UP为空的FAIL，实际status417已有同代LL观察，two native／third0不变，driver解析修订待实测。963 I11因`product_log_physical_size_unavailable_or_unbounded`失败、无errno，不推rotation根因，不授当前R7两轮／15分钟；983独立idle60 PASS不是I11。

[应用原行](v1-evidence/4e9-r7-application-subscopes-and-cleanup-fixture-selected.jsonl)：Explorer实际RID8192／elevated=true／medium=false，正式策略拒绝且未发输入，不能泛称已确认High。现代Terminal五阶段Move／实际UP、Resize、命令及Esc子项通过，1743客户点却是HTCAPTION=2、未发客户输入，故类别仍FAIL；1753 exact根关闭确认。Notepad指定官方EXE缺；Code仅hidden根／child0x80070005，原因UNKNOWN，不推权限根因。Edge实际暂停增强后单次native客户基线1804仍NC hit超时1460，未发click或增强Move，实际恢复enabled=true；1819／1821／1833关闭未确认。产品bounded轮转保留PID6604的1047–1108及PID5636的1–551连续记录，前进程更早原行已轮出，关键回执仍在fixture。后续只修已定位driver问题复用同R7产品，不给五应用／完整功能或B项加分。

## 历史R6／625：Release连续owned功能实际推进

后续仅driver修订（此提交时尚待新批运行，产品b045 EXE不变）：普通文件属性读取后若`file_size`实际返回系统not-found，仅消耗已有的一次成对重读；其他错误仍拒绝并记录真实errno/大小，不预判rotation。第二成员故障清理使用已有严格的同代、单次UP之后的公开LL回执，不要求已退休pair再生成normalized UP，也不把abort改NormalUp。source-close只在原计划唯一UP周围复用短时被动观察，原产品UP断言不放宽。Terminal仅在原客户区拒绝后保存一次精确guest根窗截图及有界子窗事实，不发送客户输入；仍须现场复核。

Run `62591991e03f406ba66ced2fff997329`，product baf27b1（/MT Release `A199FF…0DFF`）、driver b701、包装594aeae；guest ZIP `75F2A6F7EC8AD9DA3BD9A9585EAC5B0D07EB01C75352A00822A6C769AFC96073`。最终收集`collection-20261016T172501084Z-7fd31afc3911438e93879d9af11c7a8e`，3375fixture行全部合法连续。

[两轮与有限运行原行](v1-evidence/625-r6-release-owned-rounds-15min-and-idle-selected.jsonl)：同PID4156两轮owned核心I02–I06含实际post-Resize A五阶段、Ctrl C和匹配UP，1452／2202明确未重启。20个slot真实覆盖Move／Resize／Ctrl／命令与空闲，2864实测900.001661秒，不再把15分钟写NOT_RUN；跨屏仍单屏BLOCKED，不冒充Explorer／完整矩阵I11。handles五次采样291→291→286→288→288，GDI23→26→26→26→26、USER32→31→29→29→29：未出现这些采样严格单调增加，不等于证明无所有泄漏。独立2877测60.0125029秒、单核CPU0%、WS31,272,960→31,207,424字节；2969真实4156正常退出／资源回执，2974客户DOWN/UP计数[0,0]→[1,1]，不是guest销毁结论。

[视觉及保留失败](v1-evidence/625-r6-shake-repair-and-remaining-failures-fixture-selected.jsonl)与[实际产品](v1-evidence/625-r6-shake-restoration-and-inflight-product-selected.jsonl)：新PID2320第二shake原152／153实际发出native并exact恢复alpha173／topmost=false；随后真实legacyUP、NormalUp、release／detach／Gone与UP后几何保持，3275视觉组件PASS。D25据此定向关闭，M09-F05仍缺慢移／单反向不误触及系统Shake共存，B21仍0分，不再被旧D25错误一直标失败。616 source-close本代UP、651 actor及3361在途后验继续FAIL；3366 visual_restore_pending=true，不能用最终正常shutdown或客户可点击抹掉此前失败。

产品bounded轮转只保留PID4156的2698–3213及PID2320的1–240连续记录；早期4156关键回执仍以原字节嵌入fixture，不伪称全部原产品日志从1仍在两文件内。后继grid、第二成员故障、Explorer、四应用没有执行；CoreAndApplications也未重跑普通便携／startup/settings/rapid。专用guest／任务清理、证据未变均有回执，独立于两产品正常退出；不以销毁guest充当产品cleanup PASS。

## 历史R6构建身份（现场结果见上，旧/MD单独保留）

历史R6便携构建在`out/v1-product-visual-lease-r6-mt-baf27b1`明确使用`-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded`（/MT），产品自报构建源码baf27b1，Debug／Release均构建退出0、9项产品offline各9/9通过：D processor3.18／wall3.25秒，R processor1.46／wall2.69秒。Debug EXE `6DCAF5162A401F1FF7EB0BF0DF80F240B5BF4B929B880C604E19B6BCBA0B00F7`，Release `A199FF709F8A19395999D9B0A3F4CEF8F570D19E594E32102A77BC58322A0DFF`；原生CTest output-log及哈希绑定该目录，driver为b701。此段保留构建事实，后继625现场见上，不再把该R6产品写作从未运行。

历史非便携/MD构建（不换绑为/MT）：R6在`out/v1-product-visual-lease-r6-eee1b7c`构建Debug／Release产品退出0；既有9项产品offline（不含driver guest-path）各9/9通过：Debug processor1.46／wall1.55秒，Release processor1.05／wall1.79秒。原生CTest output-log及其哈希在验收表／索引；不是逐test原stdout或现场PASS。Debug EXE `35DE65EB194251C5A175C7CD44CAD50967564FF03F2C4D73FCBB0724E2C9A512`，Release `E13C106B0B63BE6B726AAC8A506D2C0E7499581186A1573AEDCB0EE40B12FCA5`。D25仍为`DEV_FIXED_PENDING_RUNTIME`；产品确有修改，不能复用afb旧EXE作为本修复候选。 初次打包尝试`7874d9fb68564598a4191f1189d8a570`因缺/MT在创建目录前被packager拒绝，目标不存在、未部署／未运行；这是打包保护，不是产品现场失败。

## 历史R5／90b8：单屏停靠拉伸子项通过，真实失败保留

Run `90b804141dfc41c9a655ff1c6a315b1d`产品仍afbca18（D/R原hash不变），driver／包装为`1b39d199d653e971dbfd01d586b0954446ed9274`；guest ZIP `AF40AB294DE05BFFBD2BC97FDF11C584350E990870AE1CC93207588BA7B93C26`／manifest `1527412A95B4E57A4F08FD7D39F12B34CC4310E9DE7DE8D268E070616BE3F44F`。另备冷Release包`0c612108c54c4ccc8eddd0c4ec9558e6`已生成，但因发现driver轮转查询缺口而**NOT_DEPLOYED／NOT_RUN**，不是失败运行；旧784e便携包普通入口仍NOT_RUN。

[Debug fixture](v1-evidence/90b8-r5-debug-dock-stretch-and-failures-selected.jsonl)的预览／撤销／角停靠／拖出／再吸附、测试的边／Shift边／角／Shift角拉伸、GUI改热键实际几何及邻窗不动均通过；408仍BLOCKED，明确缺项165为单实际屏，未授完整I04／I05。[产品边角回执](v1-evidence/90b8-r5-mapped-edge-up-and-corner-abort-product-selected.jsonl)证明284／303首点击实际同代LL UP＋capture前PhysicalButtons拒绝和Gone，而非NormalUp；mapped g6／g7有同key4 exact、mapped completion及188／202匹配LL UP。角g9／g10是contextual `double_click_stretch`，实际非NormalUp end4后release／detach／Gone；没有取得精确匹配的second-UP产品回执，不用test SendInput或g0 Raw补齐。

[轮转与shake实际产品原行](v1-evidence/90b8-r5-rotated-start-and-shake-restoration-failure-product-selected.jsonl)中，`.1`尾539 DOWN紧接current头540真实START g20；driver607却读成空，不能据此宣称产品没有START，失败后545真实legacyUP／553 NormalUp保留。第二shake152／153在实际UP156之前因空capture上下文拒绝native，actual仍190／topmost=true；fixture934写的173／false是预期常量而非回读，登记真实D25。1043的延迟native调用虽返回成功、矩形数值吻合，identity后验false且1048仍visual_restore_pending=true，保留失败，不把它改为exact PASS。

[Release原行](v1-evidence/90b8-r5-release-settings-cursor-failure-selected.jsonl)34 MOVE SendInput返回1但实际cursor仍旧点，35未发送点击、36前置FAIL；没有增强手势。本批没有执行应用矩阵、普通便携入口、完整I02／I03／I10／I11或idle60。Debug1057／Release41 fixture合法连续；Debug两个产品PID各连续，所有status健康，但未触发D24的capture-loss原分支。收集`collection-20261016T162735937Z-8a1283ea6e654f60b7fd8164f73d0719`与回执确认专用guest／任务清理及证据未变；不以guest销毁替代产品cleanup PASS。R6新driver修订尚未实测，不预填通过。

## 历史R5／cac：Debug组件已运行并保留失败，Release前置受阻

源码`afbca181ea9864eae197677b499cd29472465956`仅把真实资源owner的no-own-capture观察与实际detach／Gone用于abort清理，未把release=false伪装成API成功，NormalUp仍要求原严格事实。构建目录`out/v1-product-abort-cleanup-afbca18`，Debug／Release product、fixture及10offline目标构建退出0、CTest各10/10通过（D processor3.09／wall3.15秒；R processor1.88／wall3.20秒）。原生CTest output-log为该目录`Debug-offline.log`／`Release-offline.log`，不是各测试程序原stdout；实际产品／driver SHA-256见验收表，新Release产品`F4CE805ED64D2F3DEBC0BB9C532D5C39298001A02D90CAFF9CC9C3894BBD4A51`。新包已生成并实际运行，旧af0包和下述记录保持原身份。

Run `cac60b01c67448f0b383f188b55422b8`，产品／driver均afbca18，包装HEAD `13df19b16c094c4cfaade62a07a7ad95fa87e9a3`；guest ZIP `8327EAC44847E7BF92A39CD335EF5860502C77482033521841855862975C0CDB`／manifest `AFFE307C72152BBB007FD124B49E0B09F79B42A94BB2E24E2E44E004C1285F97`。新便携包`784e33f0d3b642d3aa629b9c9d945247`已生成（ZIP `4C0E267011C085DC7CDC2DE02E2569E97F39199D57E7D83506A875FE631B477D`），**普通入口未运行**，不继承旧便携包PASS。

[Debug fixture原行](v1-evidence/cac-r5-debug-components-and-failures-selected.jsonl)：135／143实际本代shield命中及preview矩形／alpha、149／150正常UP撤预览／角停靠、156拖出恢复原尺寸／DOWN锚点通过；396／447／497的Esc、合法失焦、真实F11中断与后继Move／Resize在同产品恢复通过。source-close资源与后继恢复成立，但514的清理UP实际快照仍是旧g14，未匹配本次g15，542保留失败；actor548实际收到START ack却因pre_MOVE_exact_context=false未发MOVE、exit1，不作driver断开功能PASS。[实际产品](v1-evidence/cac-r5-debug-resource-and-recovery-product-selected.jsonl)中g5在START前资源拒绝／Gone后health保持true，不能把284的短点击失败解释为已启动手势缺UP。D24原capture-loss分支没有本次证据，继续待定向验证。

[视觉产品原行](v1-evidence/cac-r5-debug-visual-product-selected.jsonl)与fixture751／765／844支持NormalUp恢复alpha、Esc清理UP和摇动tuple子项；855实际source／owner仍捕获本代shield，但release上下文检查失败，856／859明确未发planned或cleanup UP，861资源门FAIL。最终Debug正常shutdown字段及869独立真实客户点击有回执，不把它们升级成该手势NormalUp。[Release原行](v1-evidence/cac-r5-release-settings-foreground-failure-selected.jsonl)33观察settings_open但foreground_is_settings=false，未进入增强手势；35正常shutdown，36客户点击前台条件未成而未执行。Debug871／Release38 fixture各连续合法；Debug产品轮转合并后PID4164的1–579及PID6632的1–128各连续，Release产品1–9连续。最终收集`collection-20261016T160123707Z-2d17b32855f9403c84ec7baa49d32436`及cleanup回执确认专用guest／任务清理、证据未变，guest销毁不算产品cleanup PASS；两配置总FAIL，不提升完整功能／B项计分。

BBB `bbb795de85e643eca132c3575666c3bf`（af0／e86／Release CoreAndApplications）：[fixture原行](v1-evidence/bbb-r4-ready-and-shared-health-failure-fixture-selected.jsonl)33／36／39／43／45／46真实GUI-ready、命中、modal确认及保存通过；134预览actual矩形和alpha正确，但旧pointer-hit要求FAIL。随后[产品](v1-evidence/bbb-r4-capture-loss-abort-health-product-selected.jsonl)72实际capture_lost早于测试cleanup UP，75 no-own分支未调用ReleaseCapture、77真实detach、78撤罩，80起health字段false；源码确认旧strict清理误判，登记D24待新候选复验，不声称所有hook实际退出。后继START／应用准备不是各自独立根因。

BBB Terminal525仅精确绑定，532因healthfalse准备失败，没有客户命中／Move尝试；Edge584绑定、589进入暂停对照但healthy前置不成立，没有hit查询或客户点击／DOM结果，不能形成Edge原生客户可用性结论。603／616应用关闭未证仍FAIL，不用guest销毁替代。274的真实60.006秒／CPU0.0521%在healthfalse状态下测得，仅保留降级状态测量，不授正常idle或I11 PASS。619fixture／274product各合法连续，精确guest／task清理且证据未变；此段保留af0历史，R5另见上文，旧7项及B2不变。

## 历史R4：Release独立组件已运行，驱动假设与真实应用失败分开保留

候选源码`af0aa67ba3b4cb62d3ac2bece2c31533c8ba4ff6`已将InputService→GestureInputShield→共用writer实际接通；新路线保留精确guest／RunId／包与二进制身份保护，不在普通宿主启用。D23现为`DEV_FIXED_PENDING_TARGETED_RUNTIME`：匹配UP后不再允许新的continuous placement，已发出调用的完成和明确的release DockCommit保持独立语义；这不证明D08已修复。

该历史阶段功能Run为`f0726df0530e4735b5fc1d57e4ae070b`，Release CoreAndApplications，产品仍af0／`3CDDFF…C8AF`不变，driver edf／`8609BD…CD7E`；打包HEAD`94f0a01af8605adbca3d319fa08ea043f3e94070`，ZIP `80A320C1564C245A45031C349B66E211A0ABBFB23094673B706B2A8903C00AD2`／manifest `390DEA13050F231FDE9B92C39EC3C71D73EB519EF2C91DCB41972955C1B2CC4E`。fixture964行连续；产品轮转`.1`与current组合后PID6712的1–391、PID6608的1–393各连续，不把current首seq203误写成丢证据。

后继Run `7ff842bee0154a0c8c0eb1eeabd752ef`仅在初始设置／默认准备失败，**没有任何增强手势或placement，不是磁吸失败**。产品af0原EXE不变，driver b90；打包HEAD4c7dca6，ZIP `3F5BFF5824E402810947191BDC724C72A9CA3D6C0BCA70BB9FFE3D1C87463E09`／manifest `9A7C3DA088C0D12C478F0A0C19A37D1BDB5A002BDACBE223557D7C48688F3BED`。[driver原行](v1-evidence/7ff-r4-settings-ready-fixture-selected.jsonl)33窗口visible／FG tick46078，35／36真实点击tick46093；[产品](v1-evidence/7ff-r4-settings-ready-product-selected.jsonl)7的settings_open直到46125才在show／AnimateWindow返回后记录。37默认确认modal未出现FAIL保持；该时序支持driver准备竞态假设，等待完成回执及fresh控件命中修订尚待新Run，不预填根因已证或修复PASS。产品12实际正常退出、fixture44真实客户`[0,0]→[1,1]`，精确guest／task清理且证据未变另有回执。46／12行各合法连续；不改变f072实际功能、旧快速UP失败或7项功能／B2计分。

- [真实Release空闲测量](v1-evidence/f072-r4-idle60-actual-selected.jsonl)354／355／360独立PASS：实际60.0120913秒、单逻辑核CPU 0.02603641976%，工作集28,860,416→28,758,016字节、private commit 4,378,624→3,809,280字节；handles291→288、GDI26／USER28不变。原64位birth字面量保留。这不是I11或15分钟，也不证明功能通过。
- [视觉／在途driver失败](v1-evidence/f072-r4-visual-abort-inflight-fixture-selected.jsonl)550／551与649保留原FAIL；[实际产品轮转记录](v1-evidence/f072-r4-visual-abort-inflight-product-selected.jsonl)表明不能泛写为缺真实UP或cleanup失败。Escape g2已非正常结束并真实release／detach／Gone，之后唯一清理UP的fresh status80记录同代LL QPC1265213648，已退休故无规范化push。g3实际LL QPC1330403356／同代RawUP已到；legacy128因已abort被明确拒绝，不是accepted NormalUp。旧call11在UP前发出、源停泵1000毫秒后晚exact完成，release131／detach133／abortGone134、alpha173恢复136，656无UP后新增调用、657实际idle。现driver只查规范化UP的假设待修复复验；不得因此放宽正常NormalUp合同或把在途完成算迟到新调用。
- 布局88／155／342／747／768均有真实产品START和非零本代shield capture，旧postSTART要求capture==0导致FAIL；尚未完成后续布局、约束、I11链，不能补PASS。新postSTART与独立abort证据修订只记待定向复验，没有新driver现场结果。
- modern Terminal806实际绑定CASCADIA root，818–826五阶段、真实UP、Resize841、Center／DockLeft／Undo／Stretch／Escape已有子范围通过，871客户命中门失败使880类别FAIL；879实际关闭root。不是旧Console-host结果，不能标完整Terminal兼容PASS。Notepad882本guest官方EXE仍不可用；VSCode910只见hidden roots、无唯一合格新root，原因UNKNOWN。
- Edge922绑定后，932真实同source／FG的WM_NCHITTEST超时1460，933未见生产START；945／948／950真实WM_CLOSE后root仍存在，关闭FAIL保留。961的Catalog-unavailable是独立driver资源寿命问题，不证明关闭或Edge系统根因。Explorer789资格创建失败，混合组947 BLOCKED。后续只做有区分度的driver修复／一次原生客户对照，保留所有真实失败；不得以销毁guest替代应用正常关闭。

最终采集为`collection-20261016T150807071Z-1ca20b8f8a4f44d49ee795bc2998d052`，21文件／output unchanged。新增明确TEST_TOOL来源的只读VSCode诊断：当次三个known PID均已退出，本Run profile logs不存在，固定65秒窗口的精确Code路径Application Error／WER无匹配；原因仍UNKNOWN，不能据此填启动或兼容PASS。精确guest／临时任务已清理、证据不变、部署保留且其他资源未变；这与Edge原root关闭未证的产品cleanup FAIL分别记账。

以下39443／e1fc均为原身份历史，完整功能与对标分数不因本次独立结果增加。

专用目录`out/v1-captured-input-r4`实际Debug／Release构建均退出0。Debug的10项V1 offline通过，实际wall 2.87秒；Release的10项V1加explorer-mvp-flow共11项通过，实际wall 6.26秒、offline processor 2.12秒，二者不混用。Release产品SHA-256 `3CDDFF7C70FB6CE0A7DB141A3631B74F665D464ABDF910E7731D6FC44E79C8AF`，Debug产品`EBDD02BE05405D1BD17E429349F5377122FA945D7F4667F5C383801344615F7B`；driver Release `29083D75012005D27EFF423FDEAAEBF7C1AF04937E9DA2F50A181D6090E2F62B`／Debug `776152AD37E6365298E4808891D8911E720AE9C392D131260F1C545B31D14B03`。上述为主执行者实际工具结果转录，不是仓库归档原stdout，不替代现场结果。新7a0便携包已生成但普通入口尚未运行；首批e1fc的Debug／Release Full均被驱动缺口阻断如下，不授完整功能PASS。Explorer／整版连续链／普通Medium完整新路线／真人结果仍NOT_RUN，旧R3／R2证据及7项独立合同不改绑此EXE。

历史Run `39443acd23034e8fb7bd4eb8d1b2071a`复用原af0产品EXE，只换driver `b1dc41ba364de2282c5fc232bde34ae39d63d2ad`：Debug `3791D6EDB96A72098E4BAFA9C94BF66942D9FED9B31B7D8594DB8AA32CD35D03`／Release `DFB4D6CDF6FE8187FFB21B3534ABDDFB4A52FB4A678479587EB02497C2345671`；主ZIP `899366858A41383331CF1CF7D78BE305CCD293A97C864385729B23D9FBBEBC14`／manifest `3B3E1FC61953D6162F6F5EBD534FE0375EF65EA7A60349AC7C6CDE8C23647FD9`，packaging HEAD d74482a。原四日志hash与collection核对一致且JSON合法、sequence连续。

- [Debug owned原字段摘录](v1-evidence/39443-r4-debug-owned-core-selected.jsonl)：g1–7依次A五阶段、Ctrl A三成员、B／C五阶段、真实B Resize、postResize A五阶段、Ctrl C三成员。48／64／85／106／119／141／157每次planned单UP、实际几何和无capture均通过，162仅是owned core scope，不含真实Explorer／Release，不是完整I02。
- [产品实际资源与writer](v1-evidence/39443-r4-debug-input-resource-writer-selected.jsonl)：各代actual captureReady→accepted legacyUP→真实release／detach／NormalUp Gone→明确queued legacyUP→NormalEnd；23次该范围native发起QPC均早于各代真实passive-head UP及Raw／legacy观察。没有观察到UP后新增continuous；generation0的后继Center／DockLeft是独立命令，不误当旧手势迟到。按各producer实际QPC比较，不以JSONL行序推出跨线程原生全序。这证明Debug旧回环改善，**D08及完整D23验收仍不关闭**。
- 紧接168一次插入DOWN／MOVE／UP三包，实际head UP及产品UP撤权已到；产品g8在PreLive／PhysicalButtons拒绝，capture／placement均0，未产生NormalUp手势。169预期最终几何未实现而FAIL保留，不能删除该快速操作要求或用安全拒绝冒充移动功能PASS。171正常退出、176实际客户`[0,1]→[1,2]`另有事实，不以guest销毁补PASS。
- [Release前置失败](v1-evidence/39443-r4-release-foreground-failure-selected.jsonl)：28在一次SetForegroundWindow及有界实际等待后`actual_foreground_matches=false`，无DOWN／START／placement；31后继客户验证也未发送输入，不能声称客户恢复PASS。这不同于e1fc旧API短路问题。Release／跨应用仍须真实后续验证，Full仍FAIL。
- b1dc的Debug／Release driver实际构建退出0，guest-path定向测试各1/1，0.15／0.17秒。后继driver `edf4bc45054eec209e9588a3ab9a3e464d2df41d`现也实际D/R构建退出0、guest-path各1/1，0.17／0.20秒；两脚本AST为0错误。Debug driver `B0A167704F953E62414928F2BE3868178BC2644B2D277A1B048CEE2B8A072380`／Release `8609BD877DCB063DE2F3F25A4EA560AAF4B841EC13BF5BB4DC1E6A5AC1B2CD7E`，实际build-identity均edf，af0产品哈希不变。此处均为主执行者工具输出转录、非归档原stdout或现场；上述edf构建当时尚未运行，后继f072真实范围如上，不加功能／对标分数。 后继仅driver源码`b90bf4e2bd3af4f2c19779fb54517daca52b9cfc`实际D/R构建退出0；guest-path各1/1，Debug process 0.23／wall 0.27秒、Release process 0.11／wall 0.22秒。Debug driver `26F523E891AACB288DA5BFD958C36E9CF2854A2FBA5DBD228A869C9960A01D3D`／Release `3691C582A6BA138D93451C63AB51A3262E6C57CE2394C6DB5EF982CBD9870D5D`，产品af0／3CDD原哈希不变；仅工具结果转录，非原stdout或新现场PASS，该构建当时新Run尚未生成，其后7ff仅设置准备失败如上。

Run `e1fc79d4e1ca4c63b0572da1b4ef9150`，产品及driver均af0，打包HEAD`8f63bf46d2d1b832ce67a9af215af94985df8f28`；主ZIP `D9F9B3274F0B47EEFA4D83CA7109585B856B2D6DE59F04E3386A8CF2B68F6670`／manifest `52EC5A7B3AE2DDCF3CBFEFB5B50E5E316D9EBE86B1F868249F2C76DBDF8C64BE`，7a0便携ZIP `236FDF61F8529296639286158E28DD890E76EDA429E28A317396BF0E6F4152A5`／manifest `78ED29E69FA4B74E178096555257783B8AED8DB26F1354B2443B3A1E913CF486`。四份原日志均已独立核hash、JSON合法及sequence连续。

- [Debug driver](v1-evidence/e1fc-r4-debug-driver-selected.jsonl)34／36／38／40／42真实free、XY snap、hold、detach、resnap exact；产品4次native成功，hold仅already-exact无调用。43装passive hook后44明确`single_test_UP_attempted=false`，45因旧driver仍要求命中source而不是实际shield抛错，计划Normalcase未完成。
- 异常46仅一次测试清理UP后，[产品原回执](v1-evidence/e1fc-r4-debug-product-selected.jsonl)23实际RawUP、25accepted legacyUP、28实际capture释放、30解除关联、31 NormalUp Gone、32明确`shield_queued_legacy`、33正常结束；35资源退出。不能把异常路径的真实闭环改成planned场景PASS，亦不据此关闭D08或授D23定向验收。52后续实际客户计数`[0,0]→[1,1]`与guest销毁不同。
- [Release driver](v1-evidence/e1fc-r4-release-driver-selected.jsonl)28仅SetForegroundWindow返回false就短路，未等实际前台即29失败；产品无DOWN／START／placement。34后续同source实际foreground=true、客户计数`[0,0]→[1,1]`。只支持修正测试观察，不推测产品权限或系统根因。两个Full均FAIL；下一Run只改driver，继续复用af0原产品，不重烘EXE或改绑旧证据。

## 现有验收表的推进状态与仓库内证据

继续整版开发时，已将60个功能、12个模块、B01–B32、I01–I12、环境和应用行的既有实测逐项回填，不再让已经失败／受阻的主状态全停在 `NOT_RUN`。当前验收功能为7项 `PASS`、12项 `FAIL`、41项 `BLOCKED`；`FAIL`表示原身份绑定的实际反例尚未关闭，`BLOCKED`表示该行写明的完整合同／环境／当前候选复验条件尚缺，而不是声称所有子项未运行。原R408与R2/0c710独立B32 PASS保留历史，新R3/a00b/866另有真实新证据，不换绑旧EXE／ZIP／Run。B29只因其完整M01-F05／M11-F05合同实证齐备加分，不能外推B27／B28／B31；B33–B40继续固定DEFERRED／0分。

每行有 `observed`、`remaining_conditions`、`next_action`，以及 `partial_observation_ids`→原观察的产品SHA／二进制哈希／RunId／证据关联；`candidate_identity_ref`仅指待验当前候选，不把旧观察改绑新EXE。模块／对标条件直接引用功能行，未另建一套任务书。后续产品修复或现场新结果按实际身份增量登记；驱动／文档变更不得清空既有产品证据。

[仓库内证据索引](v1-evidence/index.json)和[摘录说明](v1-evidence/README.md)保留152条关键原始观察的脱敏摘录，含dd60／714c真实普通ZIP与诊断、b60／c01组件通过及缺UP／恢复／应用失败、50df／bdab owned回环及T/L/四格、dd60／97顺序Release客户激活失败。已独立核对四份原日志整文件哈希及对应16个原EXE哈希；每条摘录另带原行UTF-8 digest。原始大文件和包仍在索引所列ignored本地`uat/`位置，无需为源码复核反复上传整个目录。

本节只是既有结果映射，不是重新运行现场或结束整版任务。实际缺UP、快速拖动消费时序风险、托盘、官方应用准备和显示环境分别推进；新的确定性反例与修复不被误写成既有LL-UP缺失根因。完整功能合同未齐时不计PASS，不以PARTIAL作为主动停止条件。

M12-F04的独立机器证据合同现按实际校验通过：[工具输出转录](v1-evidence/acceptance-metadata-contract-transcript.txt)明确非原stdout或现场；固定ID唯一、PASS绑定／真实分数、5项反例、当时29原件与摘录hash、当时当前包hash均实际核实。它证明元数据能力，不授其他产品功能PASS；完整整版链不是此项额外门槛。随后f9已实际运行M11-F03真实冲突／命令保护／此页默认与动作帮助，按其独立合同通过，不覆盖其他设置或整版门槛。

## 新确定的输入消费时序缺陷：代码修复与实际定向复验

用户提出的 `InputService` 真实UP先清active、GUI消费迟到MOVE仍要求active的风险，已用实际共用生产判定最小提取复现：旧版定向Debug为226 checks／1 failure。这是已确定的独立时序缺陷，不是此前实际LL UP缺失的根因证明。

产品修复源码为 `408ac527a6bfda7b01a5e83249bd5ed0eca2714d`：排队旧MOVE仍拒绝；只有实际匹配NormalUp的UP点可以提交一次独立terminal计划，按原DOWN／窗口锚点求绝对几何，native发起前仍复核权限。后继DOWN、停止、abort或真实上下文失效不能恢复旧代写权限；旧代finish只退休自身，不能撤销较新代。修复后同一反例及相关代际／一次性／原锚点定向Debug为239 checks／0 failures。

[before／after小型转录](v1-evidence/input-up-dispatch-directed.txt)明确是`exec_command`输出转录，**不是程序原stdout、真实Windows输入投递或guest功能证据**；原转录与脱敏文件哈希分别入索引。随后8189取得下述真实快速UP点terminal定向结果，V1-D15已按该精确范围关闭，不再扩展专项。原D08缺LL-UP S1及完整连续链门槛保持未关闭，两者不是同一根因；dd60旧已测包也不换绑为新R。

新R产品在专用`out/v1-product-408ac52`完成Debug／Release构建及各10项offline实际PASS（4.14s／3.49s）。Release EXE为836608字节、SHA-256 `48F96C2A46A259979D484E0585C471C3FC8F9880EA1239B99C7A83D89CE3924E`；Debug为3287552字节、`BC75138C4E52B215ABA3BAD65643DDF192D0A1B295309C2F280BA9D131A39345`，独立只读哈希与两份`--build-identity`均核实R。另一个driver／脚本提交D=`83bf90362faafdfd431669b2ca545364d3898bcd`只构建测试工具，不重烘R产品。详见[构建与offline转录](v1-evidence/408ac52-build-offline-transcript.txt)；CTest当时未tee，转录不冒充原stdout或现场，当前`LastTest.log`只代表后执行的Debug。新Run8189的真实范围另列如下，不能升级正式DEV或关闭D08。

## f9：真实映射冲突／实际命令／页默认通过；a623首场未到capture

Run `f9b6012a43e345648d52c067ff9594f8`，同R2／458A62、原98fc便携ZIP不变；driver／packaging HEAD=`9a77cd755b08edb5e0a38be567e437d664959be4`，Release driver `EB3C2F76F680ADCF5F838740C3BF7BC5ECC82D53C62266B0E4FDB950B4AF6F97`。主ZIP `49EB7A5E14E7A669678ED707D6A90B423D7BCB1E75F2F78EA91F1C6F30107CD6`／manifest `56602FF600FBF10594874576B424887EF277DA3EEAD8E5CE6FD5E6D2143868C7`。见[实际fixture](v1-evidence/f9-mapping-fixture-selected.jsonl)及[独立产品回执](v1-evidence/f9-mapping-product-selected.jsonl)，不把driver变化重绑成新产品EXE。

- **M11-F03独立PASS**：442测试线程仅成功注册自己的Ctrl+Alt+F10；448实际只读动作legend与Center／MoveRight一致。461真实产品中文冲突notice，原bytes、draft及generation保持；463原Ctrl+Alt+C实际Center exact `[1285,638,1765,958]`。464仅注销成功创建的registration，真实API返回true；469同失败draft仍在，472保存后474新F10实际MoveRight exact `[1300,638,1780,958]`。483实际点击此页默认、鼠标／键盘片段和步长／help一致且未保存前旧配置不动，486保存恢复原选项；489默认Center再次actual exact，490精确测试窗实际销毁。产品PID6272 seq64冲突notice、65／69／74真实native调用7／8／9及66／70／75真实command记录相互对应；不是伪造WM_COMMAND或调用handler。437五键及客户透传、配置／视觉恢复另有对应范围通过。仅授本映射编辑／冲突／页默认／动作帮助合同，不授M09全部动作、托盘帮助、整个M11或B28。
- startup329、import/export588（含新映射检查）、saveFailure696、corrupt749、exclusion845和portable919均有实际阶段范围结果；仅tray755／761 BLOCKED导致932／934整体FAIL，Core I11／应用矩阵／Full NOT_RUN。912正常产品退出、916客户`[7,7]→[8,8]`，927另一个run-copy退出及931客户`[8,8]→[9,9]`，与精确guest／task已清理／证据未变分开记账。
- **a623是另一个新输入平台探针，不是当前R2产品**：[原始小摘录](v1-evidence/a623-consumed-down-probe-selected.jsonl)，Run `a623eafefe2a470eb5877f876da37077`，源码9a77、Debug探针 `F7C81FDB88C16CF6BF8AC2DECAFC51B191C9F2C517DFF4F00A482EF2537AC406`；原日志SHA `DBC8F42DBD5BD797152F93406EAB7F389FA7059F6E7F446567C97BE22B106115`。seq15真实exact LL DOWN已consume，随后final_guarded_cleanup；seq24 capture_attempts=0、native_attempts=0、writer_started=false、completed_gestures=0，V1 InputService／MoveSession／writer均未arm。清理单次UP后seq19真实LL UP但未推断legacy投递，不能记新机制NormalUp。scenario_pass=false；未运行capture建立／丢失／独立退出风险或Explorer，不给PASS。当前具体拒绝项尚未细分，不推测系统、权限或根因，也不凭首场就宣布整机制不可能；旧D08不关闭。
- 用户已确认合法Windows ISO／VM，但RAM不足不能新建VM；不是无介质或未回复，不调整宿主资源／其他任务。所有输入仍限专用guest，未来新Run结果不预填。

## 866：新R3真实可见错误与完整导出合同通过，整批仍非Full

产品／driver源码均为`37e5ffce1290f3c8e07056083e42666859eebe0d`。Run `866d2342e2994233a48c8a97106d660f`，Release产品 `658C12B9550F6F699E2A349F36E6560A897530FEA81DF598DA1E3267244CD067`／driver `0D7F1659DF312B94A9CE91F36A3BB27C37DF9D6FDC5486ED1FB3B8E9C81C522F`；Debug产品 `59D90E1B266ADCEF0C1EE2521FCC6BE6922FDD8866B2EAE6012DCBF7D705D9F7`，本批未执行Debug现场。主ZIP `862BA10029FBACC8540D90ED4ECFBFE2DE816100922559120B29748075CFBC4A`／manifest `934D5DCE05D82116974C7EEEA896ADF7FB3BDDAF667DFC17C5B44B2FD1BC0788`，打包HEAD5a8e746。原R2 current package和98fc整对象已转historical，旧日志／包不改标签。

- **M12-F02独立PASS／D22定向关闭**：[fixture](v1-evidence/866-r3-entry-diagnostic-portable-selected.jsonl)719在真实本地同SID DACL拒5后观察到自有中文错误modal，实际Static文本查询2次；720真实Enter确认。0假包／配置未变；722／723恢复精确原DACL，727／728恢复后实际导出并解析／脱敏，739原实例／配置／证据恢复，740／745通过。[产品PID5488原回执](v1-evidence/866-r3-local-export-failure-product-selected.jsonl)seq6返回1另有独立GUI观察，不以API返回冒充可见结果；seq7恢复后新导出，seq9正常退出。
- [真实故障包原件](v1-evidence/866-r3-actual-fault-diagnostic.txt)1628字节、SHA-256 `AE1201EA6DA9EAD071A9B764D6F07291C4BC494A3572BCD6685A6C17552414C2`与原文件相同，含R3版本、OS26100.9711、DPI144、实际fixture类别、脱敏设置、真实配置保存错误及终止原因。不是seed错误文本或普遍隐私证明。86a旧失败原样保留；本项合同无需无关托盘／整版通过作为额外门槛，也不自签独立review。
- 新便携a00b ZIP `30B2C32DB7C54E931F77600B82247A0A90FF20730EBF6F74B8D4DC87EDB19719`／manifest `719F58EE9B77753DC867986C541E155123CBA40627A536B5C2A6C9242AADB03C`：900核七文件；907／909真实空参中文GUI，911／912同实例二启；927／932默认效果和实际UP，945保存后955／960实际效果／UP，961／965／966正常退出及客户恢复，967原字节／无profile，968仅便携子链。旧工具负例继承未改工具的原身份，不冒充本批重跑。
- R3相应startup329、五键437、映射／页默认588、保存故障696、损坏恢复798、排除894均实际子流程通过；既有六项完整合同的原生记录保持原SHA，R3 UI-only变更影响及新实测单列。**tray810 BLOCKED使981／983整体FAIL**；Core I11／应用／Full NOT_RUN，没有真实完整V1输入Release闭环。
- **D23源码确认开放**：R408 terminal方案仍可能在匹配UP后发起新continuous native，而非已发出调用的在途完成，违反guide5.2；旧251 scoped PASS不覆盖该缺口。M08明确的release DockCommit例外不放行continuous terminal。后继修复／候选尚待实际验证，不能宣称当前R3可试用。
- R3 Debug／Release已构建、各10offline实际通过；本快照仅取得Debug4.83秒，Release时间未知，不猜测。构建不是上述现场证据。精确guest／任务已清理，证据未变，产品退出／DACL恢复不以guest销毁替代。

## f481：owned资源四场景通过，不是Move或Explorer通过

另一个资源探针Run `f4814a3b2fc24894975adbce91a5721f`，源码同37e5但**不是V1产品EXE**，Debug探针 `4A9BB4205FFD6C54221C62D04C03E2318920AEA0D8FA4A508DB6DE9FF2CCF203`；原JSONL分别87／36／49／62行，均合法连续，整文件／原行digest见索引。

- [normal-repeat](v1-evidence/f481-consumed-down-normal-repeat-selected.jsonl)：两代实际legacy UP、自有capture释放、关联解除和NormalUp撤罩，44／83通过。
- [early-UP](v1-evidence/f481-consumed-down-early-up-selected.jsonl)：真实LL UP先于资源请求；30实际PreLive／PhysicalButtons拒绝，ATI／capture均0，不以queued=true授权限；32真实exact source清理UP及队列／几何不变通过，明确非NormalUp。
- [F11停止](v1-evidence/f481-consumed-down-stop-selected.jsonl)：capture就绪后25实际发送Ctrl+Shift+F11，27实际WM_HOTKEY，未预先发送或观察鼠标UP即释放／解除／ExplicitStop撤罩；随后仅一次测试清理UP由exact source实收，37／45通过。
- [source停泵](v1-evidence/f481-consumed-down-source-pause-selected.jsonl)：自有source实际停泵5000毫秒；真实3000毫秒Deadline／Gone tick33687，早于实际resume35703，实际ready→gone2984毫秒。50／58验证独立收尾；明确不是默认30秒实测，也不是writer阻塞试验。
- 四场景均`writer_started=false/native_attempts=0/Move_function_PASS=false`，没有原生START／END或cancel、没有V1 InputService／MoveSession。LL／Raw观察不补NormalUp；物理按住不从driver账本或async左键0推断。只关闭这些新资源风险范围，旧owned九项保持原scope；Explorer／真实writer及其阻塞／新路线Release／真人仍NOT_RUN。精确guest／任务已清理，原始证据不变，销毁guest不代替产品cleanup。

## 历史86a：真实导出失败无可见反馈，后继866已定向修复

Run `86a42fd21f544a0dba0db056be424946`，实际Release／Medium／EntryAndSettings；仍为原R2／458A62，driver源码`4cd9f269ebf619d11860c506d5c52f15bb37bce8`／Release `75F221E5AD111455EB967D994388D72834321F854F3C0CADA3B7391D36BCB3FA`，打包HEAD27e47d2。主ZIP `6F63604221423DA664877C66D82CADD71CAF541788852D3AA5D182017DF2A08E`／manifest `8B20DFEC5FC9E170941DB24B45EDC68F3F8FAECC2D4A4C21EEF74DEBBB2C2FA1`。关键[fixture摘录](v1-evidence/86a-diagnostic-export-failure-fixture-selected.jsonl)及[本地产品原回执](v1-evidence/86a-diagnostic-export-failure-product-selected.jsonl)分别绑定原日志SHA `2061EFE008127154AF852F11907A1A4CF2B58DC6E805591DFD45E9E2420D8040`／`FC84A77DEFBA515D2FD4B3AD951A5DD86FC2D7A34D8CDC54D1CE1B82F54127FD`。

- **M12-F02真实FAIL／V1-D22**：701核原EXE哈希，710／711在精确新guest-local目录空参启动同R2；713精确设置PID7064。714双方实际SID相同，只给该本地目录增加不继承的FILE_ADD_FILE deny，未改映射Output；715实际CreateFile拒绝5。真实诊断按钮点击后719仅有产品seq5错误notice，有界1秒核13个自有线程窗无错误modal；0个diagnostic final/tmp、配置字节不变，720无假包成立。日志错误不是用户可见报错，不把本负例写成环境阻断或继续NOT_RUN。
- 721／722按同一目录身份恢复精确原DACL字节；726／727恢复后真实再次点击，产生新包并完成字段／配置解析及实际标题／路径脱敏专项。产品seq6成功notice、seq8正常退出，733实际客户计数`[6,6]→[7,7]`；738原DACL／证据归档／原实例与配置恢复均true后，739／740／746仍如实保留唯一可见反馈断言FAIL。原损坏配置799、排除895继续有实际阶段PASS，tray811 BLOCKED，982／984整批FAIL，不拼Full。
- 最小产品修复`37e5ffce1290f3c8e07056083e42666859eebe0d`仅在明确导出失败时显示自有中文MessageBox、保留原notice／无假成功；不改变输入／几何或目标窗权限。**当前仅源码修复，构建身份与实际可见反馈尚待新候选复验**；旧R2的6项独立PASS及旧包／Run不换绑新EXE，不授M12-F02或整版PASS。
- 首份部分收集因本地白名单未含精确新增诊断子目录中止，原部分副本保留；后继仅补与远端一致的精确范围后取得完整收集，不把这次工具问题写成产品／权限失败。精确guest／任务清理和证据哈希不变另有回执；guest销毁不替代上述产品退出或DACL恢复。

## d546：新候选owned资源两代正常抬键闭环，不是V1 Move通过

Run `d546969fd0f649fcb878db5079e05184`，源码`4cd9f269ebf619d11860c506d5c52f15bb37bce8`，Debug owned探针EXE `A2058B6E19570FF0CA9DD7D87B3CD9E17996752F20D09C95F2149727BD414546`，不是冻结R2产品。原JSONL为86行、sequence连续且全部合法，62493字节／SHA-256 `5AAD6BF20595E215AE06181375B1EE77486687E437FB572F6DACC2A1749F093E`；[小型关键摘录](v1-evidence/d546-consumed-down-resource-normal-repeat-selected.jsonl)对重复capture／release／detach仅保留必要字段，值不改，原行digest绑定完整原件。

- 两代`28810927835600/601`均有真实exact消费DOWN（15／54）。16／55实际pair授权成立，async左键0只作诊断；21／60记录AttachThreadInput真实成功、自有SetCapture及回读各本代overlay（66166／66194），前台仍source197216。**该候选没有原生START／END或cancel，不能写成已验证native END交接**。
- 33／71实际LL UP观察true并透传；35／73同代实际legacy LEFTUP由资源接受，非PostMessage、非重发UP、非Raw计数代填。39／77由所属资源线程释放本代capture、API成功且回读0；41／79本代实际关联解除成功，42／80 reason=NormalUp、真实撤罩／hotkey注销。44／82资源判定接受；45只在旧资源清理后开始下一代，86正常完成两代并销毁receiver／Raw registration／自有UI。每个producer原时间事实独立解释，不以JSONL排序建立跨线程全序。
- **只授owned资源normal＋repeat范围PASS**。两代均无source placement、native START／END为0，V1 InputService／MoveSession／writer未启动；这不是普通Move磁吸、原D08修复、Explorer跨进程、三窗或Release通过。捕获后实际热键、短Deadline、source线程有界停泵独立退出及其他风险场景仍NOT_RUN；实现中或准备中也不预填。
- 精确guest／任务已清理、证据前后不变、部署保留且未改其他资源；其销毁与已观察到的资源正常退出分开。当前6/60功能、2/40对标不变。下一步先运行新增关联机制的必要退出检查，满足后才继续共用writer与Explorer功能接线。
- [4cd构建／offline小转录](v1-evidence/4cd-consumed-down-build-offline-transcript.txt)：真实Debug／Release构建退出0，各4项受影响offline通过，1.37s／2.07s；明确是exec session24215工具输出转录，不是归档原stdout或现场证据。没有为生成转录重跑。

## f63：明确拒绝在旧异步左键门；修订未预填现场结果

Run `f63d6bc443534f5c94ca637a60b412ae`，另一个Debug平台探针源码`6685a08d8ff3ade1f6714603eee1edf5beb4f619`／EXE `8CE9E00706CB55D01F4A947EF37F7498297E4BEAE9BAF1D1A2C5F33DAD705656`，不是R2产品。见[关键原始摘录](v1-evidence/f63-consumed-down-pre-capture-selected.jsonl)，5410字节原日志SHA-256 `E50916B144E1FF8061329A6E9B1952D466F51E620566D63D37F6D32DC7BF4CD4`。

- seq15真实exact LL DOWN已消费；seq16 receipt／published、同代／单DOWN、未UP／未退休及全部上下文成立，而`GetAsyncKeyState_LBUTTON_return=0`／`left_high=false`。结合该源码入场判定，此次唯一拒绝项是消费DOWN候选仍要求旧物理异步左键high；不证明实际物理UP、系统根因或原D08根因。记录本身明确不是跨线程原子快照，不把JSONL顺序升级为共同原生时序。
- seq25仍为`capture_attempts=0`、`native_attempts=0`、`writer_started=false`、`completed_gestures=0`；未到capture／关联／writer。单次测试清理UP后的seq20实际LL-UP观察为false，不补造NormalUp，也不以清理资源flags证明尚未建立资源的生命周期通过。a623的当时UNKNOWN和原身份不改写。
- 修订`bd53b68629abc69e05a088f929b41379fd3fff2b`让ConsumedDown按真实receipt／generation／active／未UP／未撤权／deadline入场，其他鼠标键仍须实际clear；旧native Move物理门保留。当前标`DEV_FIXED_PENDING_TARGETED_RUNTIME_REVALIDATION`，不是正式产品替换或新路线通过。
- 收集及精确guest／任务清理前后证据哈希一致；部署保留、其他资源未改。guest销毁不算产品cleanup PASS。下一步只复验该修订的真实capture、Raw／legacy UP、解除关联与退出；满足前不外推Explorer，6/60及2/40分数不变。

## 历史0a8f：登录模拟与普通二启合同通过，故障诊断仍缺导出失败负例

Run `0a8f1f0e477a46c1b1160291c3acb53a`，Release／Medium／`EntryAndSettings`，仍为原R2／458A62与原98fc ZIP。driver／packaging HEAD=`e81d7bdcd289aa6b18f949757b19f543f914b142`，Release driver `D7401664865A586132DF17E9D983F8261B01E452E997C14CDF0948754984E569`；主guest ZIP `BF85A23E8D4C871B80394F2B7183DF24BD8E552DD29FDCEFAC8D9477FCC6901D`／manifest `F9690BA6AEB4515069E18A2CD9ECCAF6F7F1EFB83C9A978F51FDAFA9BE77C4BB`。见[fixture摘录](v1-evidence/0a8f-fixture-selected.jsonl)、[普通产品摘录](v1-evidence/0a8f-portable-product-selected.jsonl)与[真实故障包小件](v1-evidence/0a8f-fault-diagnostic.json)，原件及hash另见索引。

- **M01-F05独立PASS**：267实际建立精确本EXE的HKCU `--startup`值；276／279保存enabled=false／startup=true，285原实例正常退出及真实客户恢复。286以该实际命令参数模拟登录，287新身份；288／289实际status为disabled、无自动设置且配置字节未变。290–292手工空参二启退出0，原PID5732／birth、唯一owner及暂停状态不变，打开同主设置。313／314删除自己的入口及恢复原配置，320正常退出，324／329范围PASS。明确是模拟登录，不宣称实际Windows注销／登录；未在开发宿主启用。
- **M01-F01独立PASS**：801原七文件包、808空参中文空格路径启动、810唯一可见中文设置；811–813第二次同EXE空参启动退出0，原PID5688／birth／设置HWND1180494不变、唯一行为owner及实际status保持。GUI子系统／asInvoker／实际Medium身份与生产mutex分支在资源创建前退出共同支持不重复初始化；不声称实测了全桌面hook数量。658–663同R2实际损坏配置notice／exact备份／默认GUI补齐恢复合同。托盘不是这一独立入口功能的额外门槛；97旧顺序配置激活失败保留历史，不宣其根因关闭。
- **M12-F02仍PARTIAL**：553真实readonly配置失败、557／558实际中文modal、560旧bytes／draft保留后，561实际点击诊断，567取得本批新final文件，568真实字段／配置解析和实际owned标题／Run路径省略，569包含先前真实错误。文件SHA `81A9BC6BD06EF212E88B57FC88A9413E731F7929AA3FC49B57A064CA33F180C1`已独立重算，无seed故障文本。这验证“发生配置失败后成功导出故障包”，**尚未验证诊断导出自身失败时明确报错、无假完整文件**；不把两种失败混同，也不要求无关托盘或普遍隐私矩阵。
- 同98fc普通实例828默认吸引10、833真实NormalUp；845／846保存仅解压目录配置，856吸引0、861真实NormalUp；862／867正常退出与自有资源、866客户`[7,7]→[8,8]`、868交付字节／无profile均成立，869仅本子链PASS。329登录、646保存故障、699损坏恢复及795排除阶段通过。仅705／711托盘BLOCKED使882／884整体FAIL，Core I11／应用矩阵／Full NOT_RUN，不授整版通过。
- 877另一个run-copy正常退出、881客户`[8,8]→[9,9]`；精确guest／task已清理、证据未变／部署保留、其他资源未改另有回执，不以guest销毁授产品清理PASS。外部普通完整桌面／多屏仍缺可用隔离资源：用户已有ISO／VM但RAM不足，不写成缺ISO或等待答复。

## 历史9d74：原98fc普通便携闭环，真实配置写失败保护通过

Run `9d74d68f8a95441fa6344d5f9a73d0b2`，`EntryAndSettings`／Release／Medium；产品同R2／458A62不变，driver／packaging HEAD=`a01eb94db1b92fc28e32bed4b25dd4df7d4a9691`，Release driver `5B1170A2BEE1BD3EE71ADE03539504A0B349714C98CDE418FF4FB74AD9C86327`。主guest ZIP `0DA05EF7632C78D43C9BE00FAE6818DFFECB1D2F900B30EA89B318D57226A102`／manifest `38DF97C4002A07DC723B1DE6B09C089444143D043F2B7D3B306877E127AE91D0`；98fc便携ZIP／R2 EXE均未改字节。关键[fixture](v1-evidence/9d74-fixture-selected.jsonl)、[普通产品](v1-evidence/9d74-portable-product-selected.jsonl)、[普通桌面对照](v1-evidence/9d74-medium-desktop-probe-selected.json)可直接复核，原件及digest见[索引](v1-evidence/index.json)。

- 配置写失败：508仅对本轮精确配置文件设置只读属性；512／513实际中文“设置未更改”modal，515原文件hash、有效代际和未保存draft均保留，528／533实际旧磁吸行为及NormalUp成立。516原属性恢复后540同draft真实保存成功，547／552重启及新值、564／569实际新自由Move与NormalUp成立，583原设置恢复。没有修改DACL，不虚构未记录的`ReplaceFile`错误码。结合493真实导入导出、645损坏exact备份／默认恢复、原同R2 L1对partial／旧新schema／未知字段拒绝、明确portable／user路径，**M11-F05完整独立合同PASS**；不额外要求一种DACL故障机制，也不授整个M11／I07通过。
- 登录开关264／267实际建立精确本EXE `--startup`的HKCU入口，276／279关闭移除，284组件PASS。但此函数只循环true→false，**尚未以实际Run值模拟登录，再手工二启核唯一实例及保存enabled状态恢复**。M01-F05保留这一项具体缺口，不拿托盘、I11或应用矩阵作额外阻断。
- 747原98fc七文件核验；754真正空参启动，756在任何`--settings`前唯一中文GUI。771默认吸引10实际`[140,80,540,440]`；775／776既有public status核同PID／source／g1、实际UP callback QPC不早于本次发送、consumed／未abort／matching branch／push均成立，NormalUp／capture0／exact成立。788／789真实保存只改解压目录配置；799吸引0实际`[134,83,534,443]`，803／804 g2同样闭环。805／810正常退出和自有资源、809真实客户`[5,5]→[6,6]`、811原字节及无profile成立，812为该子链PASS。**D21按此定向关闭**；diagnostic路径仍要求实际详细event，没有产品fallback、重发UP或Raw归属伪造，不关闭D08。
- 独立阶段493／592／645／741及便携812通过；本批只有托盘651／657 `BLOCKED`，故825／827整体仍FAIL，owned Core I11、应用矩阵和Full明确NOT_RUN。不把“仅因托盘阻断”改写成整体PASS。820另一个run-copy正常退出、824客户`[6,6]→[7,7]`；精确guest／task清理及证据未变另核，非产品cleanup替代物。
- guest本地文件副本对照的5项尺寸／hash均一致，实际child token Medium8192／non-elevated／nonAdmin／session1成立；ResumeThread=1后有界20秒仍无退出、phase `NOT_PRESENT`／无receipt，随后仅精确testchild终止、账号及DACL清理确认。**本地副本假设不足，普通Shell GUI仍UNKNOWN**，不是产品FAIL或已证明映射IO根因；同路线有限调查停止，不追加API、timeout或无改动重试。用户已有ISO／VM但RAM不足的外部条件不变，不创建新VM／改宿主资源。

## 历史5017：设置行为补齐，更新中文便携真实NormalUp与driver错误分开

Run `5017e5a10bdd47bfae97026f68adee3b`，`EntryAndSettings`／Release／Medium，产品仍为同一R2／458A62；driver=`bc5eb9a4396b1aa1de30f1566233be670afd30f6`／`3F20B5E64057175D9D99E3986189E9246508FD0B6789BBF25B21D99121B44EE6`，packaging HEAD=`ecab19190196365211a1ce22e76cf2316240c899`。原件与逐行digest在[fixture摘录](v1-evidence/5017-fixture-selected.jsonl)、[普通产品摘录](v1-evidence/5017-portable-product-selected.jsonl)及[索引](v1-evidence/index.json)，不把新driver绑定旧Run。

- 493导入／导出、546损坏配置恢复、642排除／移除三个阶段实际PASS：原生export420；import draft／取消438–439、save459、吸引0的实际Move470／真实NormalUp475；实际restart／notice504–505、exact损坏备份506、默认Move522／NormalUp527；规则595、原生START／实际几何／END605–609且无产品writer、移除后产品Move632／NormalUp637。窄标题布局D20按原受阻操作定向关闭，不回写d85／36fd。M11-F05仍缺主方案明确要求的实际写失败不假报保存／原配置保留及重启保护证据，不因这三阶段通过便授整个M11／I07 PASS。
- 新便携`98fc5fb109a740aba7c27baa0cd29825`仅纠正说明、未重建EXE；ZIP `E10AB78556070603432A7979E89EC3CDAEE00C9DD256205D72CBF9C74A80A236`／manifest `940B3629E60EE31C895FEDA7302DD2E91090AD0E79B2187D81044878D82A06F3`。648七文件核验，655真正空参启动，657在任何`--settings`前出现唯一中文GUI，662 Esc、670 g1 START、672默认Move实际exact `[140,80,540,440]`成立。旧0c710的独立包PASS保留其原身份，不换绑本包。
- 674只发送一次最终MOVE＋UP（2/2插入）；675被动队首实际UP=1／CallNext=1／Unhook成功；676已观察真实NormalUp。普通产品seq7几何重新验证、零新增native，seq8 `normal_up=true`／pending退休，shutdown seq10记录同g1实际UP callback一次、consumed／matching branch／push均true。**原677仍FAIL不是缺UP**：普通模式没有diagnostic专属`input_button_observed`，driver把缺该详细事件误判为失败。D21属于测试合同缺口，修复及新Run尚未预填PASS，原683／686失败原样保留；Raw49→50仍无独立source／generation归属，不用它补证据。
- 678本次普通PID6996正常退出、自有资源flags全true，682真实客户计数`[4,4]→[5,5]`；精确guest／临时任务退出、证据哈希未变、部署保留另有cleanup回执，不以销毁guest替代产品结果。新包保存设置后行为等未走完部分仍待定向复验，不把这次普通Move扩成Full／I11／I12。

## d85／36fd／022：复用同R2，新便携独立通过，测试前置与应用失败分别保留

这些批次不重建产品R2，不把driver源码当产品源码。d85 driver=`7e174bf83ae461ed24c0c7c144a9cd8615706285`／Release `6B83975D5015A1A2F8408A59A48352E369BED13A6AF252E7F5C1820BC5A00A2A`；36fd和022 driver=`660cf6ffc84761cb1d1f2cec832eb01700811caa`／Release `B99E9258C614FDF4B4BCAC9D2DB767957EB83E2D2FAE6497CFC180E909E65CBE`。各自原包、manifest、原日志及摘录哈希在[索引](v1-evidence/index.json)，不能拼成Full。

[d85实际摘录](v1-evidence/d85-fixture-selected.jsonl)：GUI208、诊断232、真实快速UP／terminal251、登录开关／实际HKCU284、五键392分别有范围PASS。原生导出420、import draft438／456、save459成立；损坏配置正常stop493、真实restart／notice494／495、exact备份496、GUI默认500成立。导入后行为464、损坏恢复后行为506在caption候选为空时尚未发DOWN，排除543更在规则操作前受阻；这些失败保留，不写成整个阶段从未运行，也不认定为产品规则或输入失败。日志`hit:2`仅为所请求HTCAPTION，非实际回读。

[36fd新增事实](v1-evidence/36fd-fixture-selected.jsonl)将同类前检缩到24次真实查询：positioning `[111,80,369,449]`、DPI144、DWM按钮带`[28,0,248,44]`，464／507／576各24个候选全部非caption，样本返回13／12／3，无查询失败、超时、foreign命中或测试DOWN。只修自建settings／portable测试布局留出标题空间，产品保护不放宽；本批当时未通过，后由5017定向关闭D20。不能用新Run补造d85旧日志的细节或覆盖旧失败。

### 当前0c710便携功能：按其完整独立合同PASS，不宣额外几何链通过

d85的555原七文件核验、562真正空参启动、564在任何`--settings`前的唯一可见中文GUI成立；575该正常实例PID6532退出／资源flags全true，579实际客户计数`[4,4]→[5,5]`。581只是未请求额外检查、未发input，不是另一清理失败。原ZIP `0235C5…5946F`及EXE未重包／替换。574额外Move前检失败保持FAIL，未宣称本子链有Move或保存设置后的动作效果。

[只读包合同复核转录](v1-evidence/d85-portable-contract-readonly.txt)明确不是新guest或原stdout：收集后七项实际尺寸／SHA全部匹配原manifest；真实dumpbin的11系统imports与manifest一致，无漏带外部CRT。packager／helper／25项工具测试文件哈希与原负例完全相同，实际`Prepare-PortableTestPackage` AST也未变，故复用原生产缺文件／外部依赖拒绝证据，不冒充新执行。结合真实干净guest普通入口，**只授M12-F03／B32 PASS（2.5分）**；M12模块、I01／I12、托盘、完整指南操作链和整版均未因此通过。旧R408／2632／9e5完整保留其历史身份。

原0c710内quickstart仍含旧R408／2632身份文案。以上独立包运行合同PASS**不代表中文说明准确或最终交付通过**；后继98fc说明包已纠正并由5017实际普通启动，但完整说明操作链仍待D21修复后的定向复验。旧ZIP／运行身份不修改，不因纯driver变化反复重包。

### 022应用：Terminal通过；Edge实际START后零placement失败，原因UNKNOWN

[fixture摘录](v1-evidence/022-fixture-selected.jsonl)与[product摘录](v1-evidence/022-product-selected.jsonl)绑定同R2、D660的`ApplicationsOnly`，不重跑owned I11或伪绑定Explorer环境。Terminal45精确绑定、五阶段57–65、70真实NormalUp几何保持、Resize79／84真实UP，离散命令／Esc／实际终端客户输入、114 exact根关闭与115类别范围PASS。Notepad117仍无取得的指定官方程序。Code三个Medium目录RW检查成立，但launcher／一child仍259、另一child `0x80070005`；仅4个hidden根，144本次ownlog不可用／0目录0文件、145无合格窗，原因UNKNOWN；不回填30bf曾有log目录的事实。

Edge（155.0.4283.45）157精确绑定，162 caption前检实际PASS，165真实`WM_NCHITTEST=2`／前台exact／capture0／move-size0，167取得g4 START。169首个free目标`[90,210,994,870]`，实际仍`[150,150,1054,810]`、无placement回执；产品PID6564 seq78真实DOWN→79 START→80 `end=3,normal_up=false`，tick70140已在任何placement之前中止，前后UP callback count仍6。**原因尚未记录，不能断言权限、焦点、capture、UIPI或Windows根因**；不再把最新失败写成旧caption超时／19k查询风暴。179实际根仍活／`WM_NULL=false,error=0`，180／184／188 exact关闭未确认，182混合组BLOCKED；不操作未知popup或强关。189产品正常退出、193客户`[0,0]→[1,1]`与专用guest／任务销毁分开，196整批FAIL，不以销毁guest关闭Edge清理未知。

### 当前外部条件

用户已回复：**已有Windows ISO／VM，但内存不足，当前不能建立新VM**；不是缺ISO、未回复或PaneBind不支持Home。未创建新VM，未调整宿主内存、其他项目任务或设置。普通完整桌面与多屏／混合DPI目前仍缺可用隔离交互环境及足够资源；继续当前Sandbox能运行的项目，不要求购买Pro，也不把该环境缺口当作产品功能PASS。

## 30bf／新R2：D17定向关闭，post-Resize释放仍失败，Terminal类别通过

Run `30bf79c65e5b421aa1087963787898b2`，产品／driver源码均`4d07ef1fe3d3888ac7fbdb9b77afccd6d5a2cf62`，Release产品 `458A62CAEA26F76827BFF84AECDE4F5273EE6876809C411B525B8BF110F5F14C`、driver `B2195564DDD10EA22F5915EA1C0669B2B34FCDEE3CCF9C39FF00D7664B44B4A4`。主包ZIP `7137540B7B9059A268CC7A4B3E3E878F06709043F73593AC94317BF137C707D5`／manifest `B0A44B1EC8C45B61E36618C6149C9E83F51AB21B6678C3D53CCE48CC17E2C1ED`；实际Medium RID8192 non-elevated／session1／Default／150%。[fixture摘录](v1-evidence/30bf-fixture-selected.jsonl)、[product摘录](v1-evidence/30bf-product-selected.jsonl)绑定原件与行digest，不拼为Full。

- g26 A五阶段及760真实NormalUp／几何保持通过。product PID6980 seq303 resnap exact；304重复点只回读、305实际LL UP、306 `normal_up_motion_already_exact`再次核几何且零新native、307 NormalUp／pending退休。Ctrl A（775）、B（795）、C（815）、真实Resize（822／827）均取得实际UP及正确几何，**D17按原反例定向关闭**，不追加历史专项。
- 真实Resize后A回环g31的free／XY／hold／detach／resnap（834–842）均exact；844最终MOVE＋单个UP插入2/2成功，但845队首实际UP=0，846产品UP／end为空，Raw54→55仍无source／generation归属。847／868 FAIL，Pause853只是abort；尚无完整两轮／15分钟，D08继续OPEN。不能用前五手势闭环抵消本反例。
- 视觉永久alpha／置顶、实际注册热键、暂停与退出恢复原tuple173／true（953／967–969），应用改alpha199不被覆盖（977／979）有真实子PASS。拖动alpha190／Move1047 exact后实际UP1052缺失，1073整体FAIL，后续摇动未由本批执行。I08 sourceclose602、临时在途1153、grid1258的原UP失败保留；第二成员真实失败无第三调用、后续新Move／接触／Ctrl及各UP，1352组件PASS。NF01独立60.004483秒／CPU0%／工作集27,435,008字节通过，private commit4,194,304不是驻留，不代替I11。
- **Terminal本次类别PASS**：1382精确新Medium根绑定；五阶段Move／1407实际UP、Resize1416／1421实际UP、Center／DockLeft／Undo／Stretch／Undo、Esc恢复、终端客户输入／不误触Move均实际通过，1451 exact根关闭确认、1452类别PASS。不是借旧b60结果，也不外推其余四类或混合组。
- VSCode三个Medium目录RW探针PASS，launcher和一child仍259，另child `0x80070005`；4个hidden根、无合格根或窗口输入。**本批ownlog根已存在、2目录／0文件**，与旧b49无根不同，原因仍UNKNOWN。Edge精确绑定但23,920点搜索／19,410实际hit查询中18,839失败，样本1460超时；1500 caption FAIL、1510／1514／1516 exact关闭未确认，不操作未知popup或强关。
- 1520产品正常退出、自有资源flags全true、unknown native=false；1524真实客户计数`[1,1]→[2,2]`，1527精确自有Raw注销确认。任务／专用guest清理及证据未变独立记账；不以guest销毁关闭Edge清理未知，1528整批FAIL保留。

[普通桌面probe](v1-evidence/30bf-medium-desktop-probe-selected.json)已实际取得QUERY-only child token的Medium8192／non-elevated／nonAdmin／session1，D18原工具SecurityException定向关闭。但ResumeThread后有界20秒无child退出／回执，仍**UNKNOWN而不是普通GUI成立**；精确测试child终止、账号及DACL清理确认，未启动产品／Explorer或输入，不预判为环境DENIED。

[1b5／99f官方准备摘录](v1-evidence/99f-1b5-notepad-preparation-selected.json)保留两次不同事实：1b5因allowed Geo38970误值保护拒绝、无改变／下载；99f实际39070→244已核，MarketUS manifest200／找到Notepad后官方下载需要Entra交互认证，`-1978335114 / 0x8A150076`，0字节无包、不绕凭据或许可。恢复cmdlet完成但回读仍244／`restore_verified=false`，准备工具恢复**FAIL（D19）**；guest销毁只是隔离影响，不能写配置恢复PASS。产品／Notepad功能仍NOT_RUN。

新R2实际构建目录`out/v1-product-4d07ef1`，Debug EXE `7D503C7CB639698627ADFEE519851757B89BA0A7AE677673FD7247EBE3AC895E`；主执行工具转录（非原stdout归档，chunks `505e83 / 23335e`）记录Debug／Release各10/10 offline（总real4.07s／3.01s，label4.03s／1.65s），各直接input-route252／0。不用这些离线数授功能PASS。新便携`0c7107367dfa4f48b7466455b0fda8ab`的ZIP `0235C5C1C8DAC0E072E51DE2AC99FB7EF7D3A96470BF51CEC17EC6DF8F05946F`／manifest `25978E28E35AD04CE28C5540BAE49BBEB33B392B70AD74ACF5F4A6A793D56003`只已提取/ready，普通无参入口尚待定向运行；旧R408/2632原ZIP及9e5历史PASS整对象保留。

## 历史7cd／R408 serial MOVE＋单次UP：真实释放闭环，但终端几何失败

Run `7cd4f252d8b942e89cb9b3c3eb10a1b4`仍用产品R408／Release `48F96C…3924E`，driver为`209f20222d111b64f99c5330aab157214a8d88c5`／`2E51C539FF8838BBFC3063280EA58FB9885035637CD40FFA991C9B67C91DEB79`，packaging HEAD `c4dc45f7666aac23c1d030f95cd39691d1c5fd46`。主包ZIP `6DE7BEADFA1041C180FA2649575E772470496781BE3C173A57E7AECEA667CB1B`／manifest `2C46A4AAC7FA7021CB0EFD8DD55CDABE232E32AB59B034566C0801D722C7CD16`。这是`CoreAndApplications`分组，不是Full或重新绑定9e5。

- 747／749／751／753／755的自由／XY／保持／拉开／重吸几何成立。757一次SendInput插入最终绝对MOVE再单个UP（2/2返回，不重复旧UP）；758队首observer实际UP=1、CallNext返回1、Unhook成功；759产品PID2112同g26实际UP seq304与NormalUp seq306成立，Raw49→50仍不自行授归属。
- 但[产品摘录](v1-evidence/7cd-product-selected.jsonl)显示seq302真实resnap exact `[120,80,380,440]`；303旧排队MOVE因撤权而零native；实际UP后305独立terminal真实调用50却写入`[115,84,375,444]`，对错误自由计划exact，`authority_after=false`。760／767因此仍FAIL，不是“正常UP未到”。Terminal1491同样已有实际UP／NormalUp，1492仍从期望`[150,150,1050,810]`变为`[143,154,1043,814]`，类别未通过。
- 新缺陷**V1-D17**与D08区分：产品修复`e22cd41f80c52ba5f0311961cbdd38c38e82ff50`已提交，构建HEAD `7385a094852a27f1f8b0c541acc3622b16c0ffec`另含测试probe修复；新产品尚未guest运行，不能用7cd给新EXE授PASS。原7cd没有前一input tick，不能据此宣布equal-tick或旧缺UP根因已查明；D08继续OPEN，D15原定向关闭不回退。
- [fixture摘录](v1-evidence/7cd-fixture-selected.jsonl)保留独立visual1158／第二成员故障1437子项PASS、60.0001793秒idle／CPU0%／工作集27,185,152字节；临时在途1238、grid1343与Edge命中1549仍FAIL。1568产品正常退出／自有资源全true、1572实际客户计数`[1,1]→[2,2]`；专用guest／任务退出和证据未变单独记账，不关闭Edge原清理未知，1576整批FAIL。

[7cd普通桌面probe](v1-evidence/7cd-medium-desktop-probe-selected.json)中credential确为Medium8192／non-elevated／session1，`CreateProcessWithTokenW`实际成功，但QUERY-only child token的工具查询抛`SecurityException`，child从未resume，原结果为**UNKNOWN，不是环境DENIED**。主执行实际本地工具调用复现（未归档stdout，不补造原文），工具修复7385已提交、guest复验NOT_RUN，记V1-D18；精确child／本次新账号已清理、DACL哈希未变、未启动产品／Explorer或发送输入。b49原只读DACL DENIED保留其历史事实，不能由本工具异常外推必须新VM。

[e0e4官方Notepad准备摘录](v1-evidence/e0e4-notepad-market-selected.json)保留真实WinGet verbose：信息端点200，包manifest请求`Market=ZZ`返回400，随后CLI解码异常。它是官方应用准备失败，产品／应用功能NOT_RUN，不是网络全断、产品FAIL或永久缺Notepad；未改区域／语言，未安装或请求凭据。精确测试market配置修复393e5a2已提交，实际新准备仍待验证。

## 9e5／同R同2632原ZIP：便携功能PASS，整版门槛独立继续

Run `9e5a86c7f7864aa9ad6569b2ce5fbd92`，产品R／EXE48F96未改、driverD129／EXE `B317E71FC71D2110568688B5E98A4294C44368EA17AB233211776A50E7CB3AAD`，packaging HEAD=`718d8f59f2b3156152cec48875c01811573265d7`，Release／Medium RID8192 non-elevated／session1／Default／144DPI。主包ZIP `9CD06B8B6CB58E783EA3CB6998535094822045AD103E2AF4FE074FAC15EC09E7`、manifest `54D9A9AAB2F4FAF43893CF7FA95A251D2E206DCB7376A8BD769E90EDE57F0007`；[实际摘录](v1-evidence/9e5-fixture-selected.jsonl)及索引绑定原件位置。2632仍是原ZIP字节，不因改driver重新构建／重包。

- PID6316真实设置底部诊断→1579字节`diagnostic-30187.txt`→parser／隐私40→Esc／配置未改41通过，诊断哈希独立重算 `DF70F71462EA9E7FDA255EC59356E3F95AC8E334AEDF6057ACE85C88A0D52041`；正常stop43、实际客户投递47成立。它只验证本次owned标题／Run路径不泄漏，不外推普遍隐私或整个M12。
- 2632独立解压中文空格目录，49空参正常启动；51在任何`--settings`前出现唯一可见中文GUI PID6932。66默认实际Move／71真实NormalUp；83实际Enter保存、84只改解压目录配置；94新设置改变实际Move／99真实NormalUp；100正常退出／资源全true、104客户计数`[2,2]→[3,3]`、106原字节及无profile检查通过。108是未请求的额外检查，不记失败或第二次stop PASS。109／111为该范围PASS，不替代完整I01／I12。
- 独立只读核验ZIP七项路径、尺寸和逐文件SHA-256全部匹配，包括唯一主EXE、portable标记、MIT许可、中文说明／配置／knownissues与manifest。实际`dumpbin /DEPENDENTS`与manifest的11项系统imports一致，没有漏带外部C++运行库。没有开发目录、网络或driver运行依赖。
- [真实工具负例转录](v1-evidence/portable-contract-negatives-transcript.txt)来自实际25/25工具输出（不是原stdout或现场）：生产packager实际依赖函数拒绝`msvcp140.dll`；从AST仅加载真实guest `Prepare-PortableTestPackage`函数，缺一文件Spec在提取／建目录／启动前被七文件合同拒绝。源是当时工作区测试／脚本哈希，不伪绑定到新产品提交或编造guest缺文件运行。

以上按**便携功能本身**合同给M12-F03／B32 PASS（2.5分）；不是因文件存在／退出0或日志里的`I12=PASS`就授整链。M12其他功能、模块DEV和I12完整入口链仍独立未过。V1-D16三个product-source pin由真实parser和普通ZIP入口定向关闭，8189原失败保留。精确guest／临时任务已退出、证据未变、部署保留，与两产品PID的正常stop／客户投递分别记账。之后新driver仍复用同R，不把9e5运行改绑未来driver。

## b49／同R核心与应用：恢复／视觉有实际子结果，缺UP边界仍失败

Run `b49deace80fe49899375bf8d36481ea1`，产品R同一48F96，driverD129同一B317，`CoreAndApplications`／Release／Medium。主包ZIP `C2AFF10198625D99A81EB77D7E963F610C5234398737D02D5702BEAEC140F39F`、manifest `9B8C7759EACA703BF3B1056CF4F8C188C728D3B29B6480417F6F70BF16C32208`，本地重算一致；原fixture1604行，[关键摘录](v1-evidence/b49-fixture-selected.jsonl)。此分组明确不重跑既有GUI／portable／rapid，也不将分组PASS拼成Full。

- 首个I11 g26五阶段747／749／751／753／755几何成立；756独立guest队首observer安装成功，757单次测试UP插入返回1，758观察期实际UP callback=0且Unhook成功；759 Raw左UP49→50，但source／generation归属UNKNOWN，产品实际UP／end仍null，760／781 FAIL。这是**独立低级observer也缺UP**的新边界事实，不再把本反例简单归为GUI消费；也未证明hook永久移除、UIPI、系统原因或真实物理UP。D15快速UP点定向成功不关闭D08。
- Esc、合法失焦、实际Ctrl+Shift+F11分别有本代真实UP及后续Move／Resize通过子项；sourceclose有撤权／无新写／新Move612及Resize622，但原代UP602失败。actor真实START／exact637、默认Deadline638（child发送前→end为30031ms）、后续Move651／Resize661通过；642清理UP的产品匹配仍null，不补NormalUp，I08整体712 FAIL。
- 单屏停靠／拉伸核心已执行，159跨屏环境阻断，396保持BLOCKED。视觉1172实际原alpha173／正常UP恢复、应用自行改199不覆盖、摇动190置顶与再次恢复173等子项PASS；临时在途1252／1336仍FAIL。grid真实单次non-exact拒绝但原代UP1357缺失，1393保持FAIL；第二成员实际API TRUE后identity失效、无第三调用及幸存新Move／Ctrl／实际UP组件1451 PASS，不把API TRUE当作API FALSE，也不由此授完整I10。
- 当前R独立空闲786／787实际60.0122137秒、单逻辑核CPU0%、末工作集27,287,552字节，满足该环境NF01独立预算；private commit3,923,968不是驻留。NF01可独立PASS，不能替代I11两轮／15分钟或跨机性能承诺。
- [普通新账号环境探针](v1-evidence/b49-medium-probe-selected.json)实际credential Medium8192／non-elevated／session1成立，Input／Output DACL ALLOW，但WinSta0／Default DACL DENIED；只读DACL检查不证明MIC或实际GUI访问。该本次新账号按SID精确移除，未改既有ACL／token／账户，未以它运行产品／Explorer或发送输入。它是**具体guest桌面条件阻断，不是产品FAIL，更不是绝对需要新VM的结论**。
- Terminal真实绑定，五阶段几何正确但实际UP／NormalUp1506缺失，队首observer也0、Raw107→108归属UNKNOWN，当前类别FAIL，不用旧b60类别通过抵消。Notepad本批仍缺指定官方程序，继续准备而非永久跳过。VS Code已携官方1.142.0.0，Medium UserData／Extensions／TEMP真实新文件写／flush／读回／删除均PASS；launcher仍259，一个已持有子进程退出`0x80070005`，只有hidden根且ownlog目录不可用，无唯一合格根、未发窗口输入；不是“整个Code退出”或已证明profile无权限。Edge绑定但标题命中1577 FAIL，精确关闭1591未观察，登记保留，原因UNKNOWN。
- 1596产品正常退出／自有资源flags全true、unknown native=false；1600真实客户计数`[1,1]→[2,2]`，与精确guest／任务退出、证据哈希未变分开。Edge清理未确认不因销毁guest关闭，1604整批FAIL保留。

## 8189／新R：快速UP点terminal闭环，两个driver错pin失败保留

RunId `81896dad13d5423e5de14a8363f7bcc6`，Release／真实Medium RID8192、non-elevated、session1、Default／WinSta0、单屏144DPI。产品R=`408ac527a6bfda7b01a5e83249bd5ed0eca2714d`／EXE `48F96C…3924E`，driver D=`83bf90362faafdfd431669b2ca545364d3898bcd`／EXE `B7E0A77896E0DB3B841C66396AFEC93ADB4FA1F2A89BE9CA31CFEB1B7F609160`；packaging HEAD=`665b8289e36eac93e0ea485a79e52f541dd8120f`。主包ZIP `BA27609500A64AE2EEE0C4070F7064DC0435EF97398C6086DAB8E4DD0DAC0FFC`／manifest `99AE0E4A2AFCE93155CFFF90C97C9D980B946F0505973619E333DC1E2BF6E4D8`均独立重算一致。原件在`uat/v1-control/81896dad13d5423e5de14a8363f7bcc6/collection-20261016T075702366Z-19db097048e24dcb8b4604baadcb14fb/`；[fixture摘录](v1-evidence/8189-fixture-selected.jsonl)与[product摘录](v1-evidence/8189-product-selected.jsonl)已入库。

- 243单次SendInput数组插入DOWN／MOVE／UP三个元素，未等产品START再放UP；插入成功本身不作投递证明。244取得同g2实际产品LL UP和NormalUp，独立actual矩形`[195,271,595,611]` exact；普通native attempt=0、terminal=1。产品PID4148实际seq33 UP→34真实terminal placement（`normal_up_completion=true,authority_after=false`）→35 NormalUp／pending退休。没有恢复active权限或重放旧MOVE；测试也没有从插入方式推断GUI实际落后时长。
- 本次被动队首hook实际观察UP一次、CallNext返回1、Unhook成功；仅为guest测试观测，不是产品Raw替代归属，也不证明旧缺LL-UP根因。后继同批导入设置的真实Move470／NormalUp475，以及重启后移除排除的Move625／NormalUp630成立。V1-D15定向关闭，D08仍OPEN；这些片段不拼成Full／I02／I11。
- 当前R设置GUI208、登录启动285、五键393、导入导出493、损坏重启545、排除635各组件有实际阶段PASS。托盘551／557仍BLOCKED，实际NIM_ADD=false，未发送未知菜单输入。
- 225实际导出1579字节`diagnostic-51359.txt`，其真实header为产品R，但D driver仍用自身源码SHA核对；226／233保持FAIL，未把文件存在算parser通过。641的portable-ready marker也正确写R而被错用D核验，保持原FAIL，**新2632 ZIP的普通空参入口本批NOT_RUN**，不是产品启动或几何失败。V1-D16登记这三处driver产品SHA pin缺陷；工作区修复用guard已核验的product_source，待新driver以同R复验，不重建产品。
- 642产品正常退出、资源flags全true、unknown native=false；646实际客户计数`[3,3]→[4,4]`，与后续精确guest／临时任务退出分别记账。cleanup回执确认证据未变、部署保留、其他资源未改，不能以guest销毁替代产品结论。driver648整体FAIL保留。

后续先由修正driver的`CoreAndApplications`推进未跑核心／应用，再定向`SettingsDiagnostic`复验两个pin失败；复用同R EXE，不重复已通过GUI，不将分组子项拼成Full。portable2632 ZIP哈希为`4D7CDAEFC22208233716A24DC2DE6B98FAFEF14A83DCB6B450FF2DF20E5602A2`、manifest `090EFBA44ED960CBB9A16DA75A780A14B38A74D4AFDBAB04F3584B189E1B401C`，只作已核构建身份，不宣普通入口已验。

## 源码与包身份

本稿是持续开发快照，不固定最终交付 SHA，不预填下一批结果。

| 项目 | 当前准确状态 |
| --- | --- |
| 开发分支 | `codex/r1c4b-live-magnet` |
| 当前R3实际候选 | `37e5ffce1290f3c8e07056083e42666859eebe0d`／Release658C12、Debug59D90；866仅entry/settings/diagnostic/portable实际范围。旧R2/458、98fc及全部旧包原身份转history，D08/D23与整版仍未闭合 |
| 方案入库 | `c1e8f82f8156db7c4778473f6b4689327f771013`；变更限 `AGENTS.md`、`README.md`、`docs/PANEBIND_V1_DEVELOPMENT_GUIDE.md`，主执行者已完成标准推送 |
| 当前实测包／入口 | 866／R3的EntryAndSettings与新a00b便携；M12F02定向PASS，tray BLOCKED令总FAIL；另f481为资源探针四场景，不是V1 Move |
| 历史原2632便携 | ZIP `4D7CDAEFC22208233716A24DC2DE6B98FAFEF14A83DCB6B450FF2DF20E5602A2`；manifest `090EFBA44ED960CBB9A16DA75A780A14B38A74D4AFDBAB04F3584B189E1B401C`；产品R／EXE48F96未改；便携原packaging HEAD665b828，9e5 guest包HEAD718d8f5分别记录 |
| 历史dd60 runtime／build | `dd603a79aeba64c816e13ba49265db342c14e7e3`；Debug／Release fresh构建及各10项offline实际PASS；旧结果原身份保留，不迁移绑定当前R |
| 历史e2abc打包 HEAD | `dd603a79aeba64c816e13ba49265db342c14e7e3`；旧已测ZIP不重包，整对象已归入historical_packages；当前R/2632另立真实身份 |
| 历史714c现场 | `714c69f6feba4a0f81678c314eb1747e`，干净guest、Release-only／Medium／`SettingsDiagnostic`，106行scoped PASS；97 Debug专项PASS及后继Release失败同时保留，不外推Full／I01／I12／整版 |
| 历史b60主要功能实测 | `b60e9c343c29f75007df7bf1833fd5d4ab8d3cf8`／Run `c01ffe67d3004961bd00765312869c8a`。不把历史结果换dd60或当前R的新EXE标签；当前R另有8189/b49/9e5真实范围 |
| 714c实际退出 | ZIP实例PID5696正常退出，资源flags全true、unknown native=false；实际post-stop客户计数`[2,2]→[3,3]`。精确guest／任务已清理、证据未变、部署保留。旧c01 Edge清理未知不因本次退出或guest销毁关闭 |
| 714c guest ZIP／manifest SHA-256 | `805A0F93BA6643E3ACFD4E891421D1878EC848C6F47240064229B3758B829F2C`／`FB2A8AA99EDB8CF343EC5383FACFE53C7179439253A9EA1A3F2D224F2B32B11F` |
| 历史e2abc真便携包 | PackageId `e2abc1ec0dcf422088d62c53bc633d24`；ZIP `FACE7CD67969582B53901EBC91C63AC2C473AD982F86D0F774E3904563C0B556`；manifest `7764C9B0561FB96B11A8B06B9A0A2A3DF5205038460C2BD299C4DC6A66C7FC8B`；Release EXE `0CEA6E75FE171F472A020290C157EAE5A154D44B0AECC75B1E4AC60A8A7786FE`；原字节与原scope保留，不换绑2632 |
| 历史 aae 已收证 | `a2eb05f7e76d05ec9378e0d12631dfd22c326fbb`／`aae67076ccda4bdb9300c9015108231a`，整体 FAIL；原始证据及清理回执保留 |
| aae ZIP／manifest SHA-256 | `31132A5BCBB4EAB8F488BCE647B80A5A36A6E57540B5DA83FF41FC5BE2D8B8EF`／`DF95AEEA2336F7217A051A5341B8BE85F5E379147B48CC377B369ABCEF4BC793` |
| aae Release／Debug EXE SHA-256 | `A1F92E7773607A6DB8967E0AE10B47257474C3947B3F3B96B643A639574C9E56`／`6C1B5EB112CC8169E166A13A627AE16B2A354BAB63882224FCC65301E0C2C596` |
| 已实际运行历史批 88e | 源码 `fcca29306ded7f483f8381901bbc7fb706cdf1c6`；RunId `88e4574bac3c4eba801431a559d58a3a` |
| 已实际运行历史批 8cc | 源码 `be011280b0d8dc7715163655642964cb112f5656`；RunId `8cc920c8b1584531b248e602b672d186` |
| 8cc ZIP SHA-256 | `902FBFFED8028C3B0DD2C1ED69A9C3099BB53F7E6B87B5DCB449C9EDBADA0479` |
| 8cc manifest SHA-256 | `F907191C43BC13423890EC5EEC4B417F7546BA7FDA5C29C015F5284D74BCC5AD` |
| 8cc Release／Debug EXE SHA-256 | `E64087A761163A9A1BA4293291997098B822F177C9BC369DF35BEF4993FB4A39`／`CE5B232741824E5BA68F47F43867AB3EB400658F572C3FE2760A00A3EA22475B` |

每批实际身份保存在 `uat/v1-sandbox/<RunId>/package-receipt.json` 与 `input/manifest.json`，便携包在 `uat/v1-portable/<PackageId>/package-receipt.json`。历史包与原始失败继续保留。`source_commit`／EXE self identity 为 RUNTIME，`packaging_head` 为实际打包HEAD；允许的三个交付文档已提交差异与源码变化严格分开。此次更新不重新打包已测e2abc：本报告和仓库最新版中文指南是独立附交付材料，不能声称ZIP内的文档也已更新／实测。打包receipt的部署字段不是当前执行结果，现场状态以实际回执为准。

## 已运行的功能与明确失败

### dd60／714c与97：设置GUI诊断和普通ZIP专项闭环，不覆盖旧功能整链

714c原始证据：`uat/v1-control/714c69f6feba4a0f81678c314eb1747e/collection-20261015T194851747Z-315d6753dfb64d8e94079d2cb3c49f5c/`。97原始证据：`uat/v1-control/97a0fa7ebb39452188674011891963d9/collection-20261015T194554881Z-1ac68b9eca0646b7af9c4fb9c7eeac22/`。均为自动guest driver，不是真人UAT。

- 97 Debug实际点击设置窗口底部“诊断 (&D)”，生成1590字节`diagnostic-30546.txt`，解析／隐私专项39、Esc与配置未写40、正常退出及post-stop真实计数`[1,1]→[2,2]`通过，48为该专项PASS。没有因托盘不可达而省略已有GUI导出路径。
- 同guest后继Release29真实客户激活FAIL：DOWN／UP发送各返回1，但实际客户计数仍`[0,0]`、前台不匹配；未到设置、parser或普通ZIP。产品0operations／0callbacks；正常stop后的客户检查因前台条件失败根本没有再发DOWN／UP，不能写“投递失败”。这一顺序配置差异原因UNKNOWN；下列干净guest成功不把它改为通过，也不证明系统根因。
- 干净guest714c只运行Release：设置精确身份、实际诊断按钮点击、新final诊断文件与parser／隐私39、Esc／配置未写40通过。收件`diagnostic-30359.txt`为1590字节，SHA-256 `D35D5EBC610AB887A16F9C732084F69416C7EDBB9B16ABFC52162F8654BB252F`，本地独立重算一致。本次只证明实际owned标题／Run路径不在导出中，不声称所有文档／所有配置的普遍隐私保证。
- 同一714c真实解压e2abc普通ZIP：48空参数启动，50在任何`--settings`命令之前出现唯一可见中文设置；默认吸引10DIP时实际Move目标`[140,80,380,440]`、68同g1 NormalUp。80真实Enter保存、81仅解压目录配置改变，吸引0时下一实际Move目标`[134,83,374,443]`、94同g2 NormalUp。95 ZIP实例正常退出／资源全true，99真实post-stop计数`[2,2]→[3,3]`；101交付字节与无profile检查通过，102是该普通入口子流程PASS。
- 104 scoped summary与106 driver PASS明确仅设置诊断＋普通ZIP，不授托盘／Full／I01／I12／M12／整版通过；103是未请求的额外poststop检查，不是另一次投递失败，也不是第二次stop通过。精确guest／任务清理与证据前后哈希一致独立记账，不能代替产品事实。主要功能的b60/c01观察仍按其原始源码／二进制保存，未换绑到dd60。

### 590／1133：原子临时文件清单缺陷已修，旧失败保留

原始证据：`uat/v1-control/590650e822c34312ad51e172c93f2fce/collection-20261015T193727214Z-95fcc4b1b059456982ca2cf81ad50aed/`。

- Debug真实GUI/click已生成1506字节final诊断文件，但driver误将原子rename中的`.tmp`纳入inventory，随后`file_size`抛错；parser未运行，原41行专项FAIL保持。
- Release精确GUI身份成立而前台不匹配，未完成parser或普通ZIP，32行FAIL保持。单有文件、退出码或guest销毁均不算功能通过。
- driver只清点本次精确final文件的修复由dd60 Debug97和Release714c真实导出／parser复验通过，**这个确定driver问题已专项关闭**，不列作当前产品S1。顺序Debug→Release输入／前台差异仍UNKNOWN，未因此关闭。

### c01／b60：单屏停靠／拉伸、默认截止和失败后恢复取得实际子结果

原始证据：`uat/v1-control/c01ffe67d3004961bd00765312869c8a/collection-20261015T192211498Z-87901c575af04496bcfdf88477990983/`。Release／Medium／`IndependentEntry`，只计实际独立范围，不拼成 Full 或整版通过。

- 设置 GUI／登录启动／五键／导入导出／损坏恢复／排除及还原、七文件 ZIP 普通无参可见设置／真实保存效果／退出、Terminal 本次类别、owned 视觉再次取得对应实际子结果。portable647、Terminal2154、visual1779均只绑定本源码／包；托盘498／504仍因精确 UID1 图标不可达 BLOCKED。
- I04／I05 seq700–1005 单屏核心实际通过：自由移动／XY、屏幕边缘预览与离开取消、角停靠真实 UP 提交、拖离恢复原尺寸与 DOWN 锚点、实际注册半屏热键、停靠后自由移动806／重吸附810、smartDock 开关的不同实际矩形；智能拉伸开启下的边／角双击与 Shift 组合、微移／调高、真实 GUI 更改热键的几何效果及邻居不改。原配置恢复1003与自有窗关闭1004通过。774实际仅单屏，跨屏为 `ENVIRONMENT_BLOCKED`，phase1011保持 BLOCKED，不称整个 I04 或所有智能拉伸模式通过。
- actor g23／call43 真实 DOWN、START 和 exact Move1252通过；默认 deadline=30000ms，产品实际 Deadline end／pending退休1253。30031ms 是 child **发送前 tick87906 到产品 end117937**，不是 actor 退出后或 START 后整30秒。1255无新增 native 调用；1257仅测试 cleanup UP 已发／native idle，同 g实际产品UP仍为null，不补成NormalUp。随后同 PID6356 新 g24 Move、g25 Resize exact及各实际 LL UP／NormalUp通过。I08另一个 sourceclose g20虽撤权、无新调用且后续恢复成立，其原匹配UP1217仍FAIL，整个I08不通过。
- 第二成员故障2017：第一个 exact，第二个原生 API 返回 TRUE 后身份丢失，第三个没有调用；不是观察到 API FALSE。status2020、原代实际UP且无新批2022、存活两窗新 Move2029／形成接触2039／Ctrl2049及匹配UP和资源关闭2053均通过，component2058 PASS。该真实失败与恢复符合本次子场景，不新增必须另取 API FALSE 的门槛，也不升整个I10。
- grid1957／1958实际单次API TRUE／non-exact拒绝、1961 status、1965撤权后无新增placement，以及后续新 Move／Ctrl／真实UP和资源关闭成立；但原g6UP1964为null，grid原断言FAIL、phase2000仍FAIL。临时在途1859的原失败及安全继续记录也保留。
- I11 g26的五阶段实际几何正确，hold为`already_exact_no_native_call`，不是五次 native调用；1372／1373实际LL UP/end仍null，Raw独立计数97→98的source／generation归属UNKNOWN。显式Pause为异常收尾，不是NormalUp；两轮／15分钟未完成。没有可靠共享事件身份，产品未增加 RawNormalUp fallback，S1保持OPEN，不无新证据重复同路线。
- 独立空闲1399实测60.0035803秒、单逻辑核CPU0%，末工作集26,562,560字节、private commit3,981,312字节；private不是驻留，idle不代替I11。Explorer正式资格、Notepad／VSCode缺项及Edge命中／exact关闭问题仍保留。产品正常退出和post-stop计数`[5,5]→[6,6]`成立，精确guest／任务已清理、证据未变；整批仍FAIL，销毁guest不代替未确认的Edge或产品整版cleanup。

### e4／593：普通便携入口、Terminal 与独立组件有实际进展，整批仍 FAIL

原始证据：`uat/v1-control/e4a9f65c5034440f9822babf794956db/collection-20261015T185026239Z-b2e503d862f240b7b630d979e3892d5d/`。这是 `IndependentEntry`，不是绕开失败后补记 Full 通过；每一独立阶段之间都核验当前产品身份、真实 idle／pending 状态、driver 账本及原三窗资源归属，失败保留后才在安全条件成立时继续。

- 真实设置 GUI、登录启动开关／自己的实际 Run 值、导入导出、损坏配置备份／默认恢复、排除及还原的 phase 分别在 seq203、236、442、492、580 通过。标题五键实际映射／同代 UP／置顶反转和客户区五键透传子流程 seq344 通过；不是把映射 UP 当作拖动 NormalUp。
- 真七文件 ZIP 在带中文／空格的独立解压位置以空参数正常启动（seq593）；首次同 PID 唯一中文设置实际可见（595），没有发送 `--settings` 促成首次出现。默认设置、Esc、实际 Move、GUI 保存后下一 Move 效果、正常退出／真实客户输入恢复、交付字节不变及无用户 profile 写入均有回执，647 为该便携子流程 PASS。它不是 `--build-identity`、构建或 driver 注入产品权限的替代入口；真人 UAT 和整版验收仍未通过。
- Release 独立空闲实测 seq1234 为 60.0009748 秒、单逻辑核 CPU 平均 0%，结束工作集 26,361,856 字节、private commit 3,645,440 字节。后者是提交量，不是驻留内存；这不是两轮／15 分钟 I11 证据。owned 视觉映射、原属性及布局恢复组件 seq1614 通过。
- 实际 Terminal 的五阶段 Move、匹配真实 UP、Resize、Center／DockLeft／Undo／Stretch、Esc 恢复与客户区输入 seq1874–1925 通过；1927 精确根关闭确认，1928 为该类别本次范围 PASS。只证明此真实终端类别，不外推 Explorer／编辑器或混合组。
- 托盘 seq498／504 为 BLOCKED：精确当前 UID1 图标不可达。dock 后自由移动806、I08 匹配 UP1032／actor START1064、I11 普通 UP1208、临时在途1694、grid status1796、第二成员 status1824 各保留 FAIL；没有把 `FAIL_SAFE_TO_CONTINUE` 改成场景通过。单屏的跨屏项目774明确为 `ENVIRONMENT_BLOCKED`。
- Explorer 实际根 `integrity_rid=8192,elevated=true,medium=false`，资格拒绝且没有发送该目标输入；Notepad／VSCode 本批指定 EXE 缺失，BLOCKED。Edge 在 actual hit 检查1958失败，随后 exact 根关闭未确认，登记保留；未知身份不被强关，整个混合组未完成。
- 产品正常退出的真实资源 flags、post-stop 客户计数增长成立，但 Edge 清理未确认且整批 `driver_shutdown=FAIL`。这些结果与 guest 后续销毁分别记账，不把影响被隔离等同产品 cleanup 通过。后继修复及最终候选身份尚由主执行继续，不预填结果。

### bdab／50df：owned I02 子链及 T／L／四格通过，八窗因失焦中止

源码 `50dfdaa69352ad86f3b8da73c05badd3f361f73e`；原始证据：`uat/v1-control/bdab2e5e8aa648caa7bfe50124caa89d/collection-20261015T183312474Z-cf8ba2b9a13f455cac309ec73f6fbb83/`。

- Release `Full` fixture seq30–134 实际完成 owned A/B/C 普通五阶段、Ctrl A、B 原生 Resize 后 A 回环、Ctrl C、匹配 UP 及 Center／Dock 命令；随后 T／L／四格对应共享缩放和 source 切换通过子场景。它是 owned 子链，不替代正式 I02 的 Explorer／混合应用范围。
- 八窗 source7 的真实 Ctrl DOWN／g15 后，前台短暂变为未知 HWND3146652；产品按 FocusLost 撤权，本代 placement=0。驱动新的光标插入返回1，但独立 GetCursorPos 未到目标，seq512 `test_cursor_not_exact`，整批 FAIL。不操作未知窗口、不重试原八窗链，也不把它预判成产品写入错误。
- 正常 stop 与 post-stop 客户输入通过各自子项，不能关闭八窗失败。后面的应用和普通便携入口未由本批运行；不能把 e4 的独立分段结果拼接成 bdab 的 Full 通过。

### 945c／3a65：Terminal 真实正常 UP，旧 fixture 的 Console 查询分支误拒

源码 `3a65a41e7e4c4382ddde4a343b7df30d301a4857`；原始证据：`uat/v1-control/945c28c08a4a43d4b6801b15706aa33d/collection-20261015T182256411Z-b77926536cb34647a4b701dda2338002/`。

- Terminal 五阶段实际几何成立，最终 `[150,150,1050,810]`。fixture681 单次 UP 后，682 引出产品 PID5852 seq22 的同 g1 实际 LL UP，seq23 明确 `normal_up=true,pending_retired=true`。
- fixture683 整体仍 FAIL；旧 `up()` 直接按 source TID 查询 Console 的 GUI 队列，与生产精确前台 Console fallback 不一致。原683没有该查询的具体错误码，不能补造；686以生产 helper 实际确认 `foreground_console`／GUI idle。此确定性驱动差异已修，e4 以新身份重新验证 Terminal 完整本次类别范围。
- 原945c整批失败与当时未完成的 Resize／命令／客户输入保持原样；这一真实 NormalUp 不关闭其他构建中普通或异常场景的实际 UP 缺失。

### fdfe／783：GUI、五键与损坏恢复通过子流程，其余失败不隐藏

源码 `783078111ebee3d17cdaa80244180e0f4b4f155b`；原始证据：`uat/v1-control/fdfe8a79403342259401aad1b6a45b7c/collection-20261015T180117207Z-cb793400912e4c7bade308ff7387196a/`。

- IndependentEntry 的真实六页 GUI（201）、登录启动开关与实际 Run 值恢复（232）、五键客户区透传／标题映射及资源关闭（338）通过各自子流程。损坏配置前真实正常退出与客户输入恢复、重启提示、exact 备份字节、默认值和原选项恢复377–416通过，未把历史快照充作本次结果。
- 原生文件名键入361失败；实际 local Esc 取消不使导入导出通过。托盘421／425 BLOCKED。排除 native Move 的起手预检 HTCAPTION=2 与实际 `WM_NCLBUTTONDOWN` wparam=8 冲突，485失败，不能写成已进入原生 Move。
- Edge 绑定后 DOWN／START531失败；旧 catch 的 outstanding DOWN 尚未清理，安全门534拒绝继续；exact 根关闭未确认，登记保留，536／548 FAIL。后继已修一次有账本依据的测试清理及 caption 几何判断，旧失败不改写。Terminal 和普通七文件 ZIP 入口本批未到达。

### 8cee／f626：普通 C 五阶段几何通过，正常 UP 收尾失败

原始证据：`uat/v1-control/8cee44b3378f41998e8e5dcf9635f5f3/collection-20261015T163804486Z-3dda0c7311f849bbab49bbefc9f3ee8f/`。

- fixture seq69–83、产品 PID7108 seq37–43 记录普通 C 的真实 DOWN／START gen4，以及 free、XY snap、hold、detach、resnap 实际几何均 exact。hold 本来已 exact，回执明确未调用 native；其余对应实际单次调用，不能把五阶段都说成各有原生调用。
- seq84 单次测试 UP 的 SendInput 返回 1；约 3500ms 等待内无本代 actual UP／gesture_end，seq85 的 `normal_up_and_no_late_geometry` FAIL。最终产品 seq44 仅在 stop 时记录 end=8／Exit、`normal_up=false`；shutdown seq46 的 UP callback count=3、last generation=3。不得以停止终止代替正常松键。
- 松键前实际 cursor `[440,273]` 位于实际 C `[414,260,674,440]` 内，旧窗外轨迹不能解释全部 UP 失败；本批未证明 UIPI、hook 被移除或系统根因，原因 UNKNOWN。普通收尾现在阻断实时手势整链，不再仅是异常注入路线的问题。
- seq87 正常退出、资源 flags true、无未知 native 完成；seq88–91 真正发送 cursor／DOWN／UP，窗口计数 `[0,0]→[1,1]`，post-stop 投递子场景 PASS。seq92 整体 FAIL，精确 guest／任务已清理且证据未变，不由此关闭正常 UP 问题。
- 本批未到设置生命周期、托盘、五键、I11、应用矩阵或真 ZIP 入口，保留本批 NOT_RUN。后继 IndependentEntry 和 fixture 只读 Raw 观察已实施并按各自新 SHA／RunId 运行，详见上文；不改写8cee失败，不将只读 Raw 计数当作某 source／generation 的授权或真实 NormalUp。

### 7cac／0fa：真实在途超时、迟到完成与临时属性恢复，实际 UP 仍未观察

原始证据：`uat/v1-control/7cac94a0561349c3ba1fde679cd27c11/collection-20261015T162312746Z-12c84270720a4e3ab136d02478f86151/`。

- fixture seq565–577 与产品 PID6868 seq143–147 记录真实 OpacityDown `205→173`，随后实际 DOWN／START gen16 与临时 alpha `173→190`。不是诊断许可代替实际修改。
- 自建 source 在 `WM_WINDOWPOSCHANGING` 中有界暂停 1000ms；call44 的实际 `native_before_qpc=519183982`、`native_after_qpc=529210003` 及迟到 exact 回读证实原生调用真的在途约 1 秒。250ms writer 等待预算耗尽后，产品 seq148 记 completion unknown，seq150 以 SourceLost／end=3 退休 pending，不能把超时说成调用未发出或已取消。
- 单次测试 UP（fixture seq580／581，SendInput 返回 1）在 source 仍阻塞时发出；产品未观察到本代 hook UP，shutdown seq156 最后 callback 仍为 gen15。seq583 的完整终止检查 FAIL；尽管产品已有 SourceLost 退休记录，也不能补成 NormalUp，缺失 UP 的原因 UNKNOWN。
- seq151 记录 call44 迟到 exact、`new_write_authority_granted=false`；seq152／153 通过实际 alpha API 将 `190→173` 并取得 helper commit ACK、退休临时持有状态；seq155 正常退出前又恢复原 tuple `173→205`。这些具体恢复事实成立，但不是整个异常链通过。
- fixture seq579 的 `native_entry_qpc=0`、flags／positioning 零值是日志 payload 实参与 `wait(...)` 实参求值顺序造成的确定性证据缺陷，不能推断 native 未进入；原日志不改。后继 `f626b6ee8c934da08497172898f6d93d155d5964` 已修并通过双配置构建／各10项 offline，8cee 实际运行但更早在普通 C 收尾失败、未到该在途场景，故这项定向复验仍 NOT_RUN。
- seq585 正常退出，产品 seq156 资源字段全 true、无未知在途完成；seq586–589 真实发送 cursor／DOWN／UP 并观察窗口计数 `[1,1]→[2,2]`，post-stop 投递子场景 PASS。seq590 整体 FAIL。精确 guest／任务已清理、证据保持不变，不以销毁 guest 代替清理通过。
- 设置生命周期、托盘、五键、I11、应用矩阵及 I12 均未到达，NOT_RUN。后续仅调整测试顺序让未受阻功能先运行，不无改动重复同一缺 UP 路线，不将后续未运行包写成通过。

### 1bee／cf：I10 发 DOWN 前对象改变，真实 post-stop 点击通过

原始证据：`uat/v1-control/1bee4d83fa2d4786ad0319a69d3227c6/collection-20261015T161147990Z-6fe4611acd8b4b148eb258a15c99dbcb/`。

- fixture seq507 已完成本批 owned T／L／四格／八窗子链；seq509–513 准备下一精确 source 与实际 cursor。产品 PID3604 seq136 曾确认 source HWND459520／HTRIGHT，随后 seq137–139 记录前台及 hover 对象改变、资格／命中不满足；fixture seq514 明确 `test_DOWN_sent=false, test_UP_sent=false, generation=0`，seq515 为 `layout_source_or_hover_hit_changed_before_DOWN`。本次 I10 没有 DOWN、START 或 native 调用，不是 grid 非 exact 或缺失 UP 的新复现，改变对象的根因未定。
- seq516 正常退出，产品 seq140／141 为 timing／shutdown；所有资源字段 true、无 unknown native completion。seq517–520 确有实际 cursor、DOWN／UP 发送及窗口计数 `[1,1]→[2,2]`，post-stop 投递子场景 PASS；seq521 整体仍 FAIL。collection 与 cleanup 回执确认精确 guest／任务清理、证据未变，不由 guest 销毁推定产品 cleanup。
- 既有配对修复已提交：`85c1599172f946ef86b9e84838e0a068fcda3341` 退休旧 aborted pair；`b092f01ab18ba9624b4d483c9e2404a844ac6309` 修复 authority=0 下新配对及 hover 准备刷新；`2c37573ae7afc160b00094474eabef411c03949c` 增加 mapped 配对精确 generation CAS。不能据此认定历史实际 UP 缺失根因已证实或闭环。2c 的 ea7 包仅部署、未启动，不算现场运行。
- 托盘真实菜单、五键与映射用例已接生产测试入口，1bee 未到达，保持 NOT_RUN；后续 I10 恢复、设置生命周期、I11、应用矩阵及真便携 ZIP 普通入口均未由本批验证。不预填后续源码／包结果。

### eae／2a：首写入前 FocusLost，真实 post-stop 点击通过

原始证据：`uat/v1-control/eae17bbdfc6740789cd8c06b602c1be4/collection-20261015T152931313Z-b28a5b4cf7b640058713ddc85354334b/`。

- 产品 PID976 的 seq138 为本代真实 DOWN，seq139 为 gen16 START，seq140 立即以 end=2／FocusLost 终止，未发生本代 native placement。fixture seq517 的 grid 检查 FAIL，实际仍为原矩形 `[140,140,540,540]`；其 `observed_native_result` 固定文案不是 API 回执，不能据此宣称已调用或再次复现 grid 非 exact。
- 失败后仅一笔测试清理 UP（seq519，SendInput 返回 1）；shutdown seq145 的实际 UP callback 快照 count=16、last generation=15，没有本代 UP 进入现有 hook 的观察。不能据此断定 UIPI、系统根因或制造 UP／NormalUp；d49 曾真正执行 grid 后拒绝及缺失 UP 的历史保持原样。
- post-stop 此次确有独立 cursor、DOWN、UP（seq522–524），seq525 明确 `executed=true`、两笔 SendInput 实际调用、窗口计数 `[1,1]→[2,2]`，该真实投递子场景 PASS。与 d49 未走到发送、仅有旧 raw `executed=true` 的情形区分；退出字段 true、guest 销毁或本次点击都不把整批 FAIL 改为 PASS。
- 当时发现确定性源码反例：旧 aborted 配对残留时，新 DOWN 透传但对应新 UP 被旧配对吞掉；后续 85c／b092 与 2c 的已提交修复见上节。该反例不自动证明 eae FocusLost 或未观察 UP 的根因，历史失败保留；恢复、在途失败和 I12 未运行仍保持 NOT_RUN。

### d49／ff：grid 单次调用成立，abort 后实际 UP 未观察

原始证据：`uat/v1-control/d49ec6c63b5341b8b70f054106a4c74d/collection-20261015T151212261Z-f94be534497241cfb01ea55a64b9a124/`。

- owned T／L／四格／八窗子链再次完成（seq507），本批真实 grid17 目标 `[140,140,577,540]`、实际 `[140,140,564,539]`，SetWindowPos 返回 true，产品记 non-exact 并 abort，不假称 API FALSE（fixture seq517，产品 PID5832／gen16／call42）。
- seq518 的单次 native 调用子项通过；driver outstanding=true、Async=0 与最后 native 回执 authority_after=true 分别记录。最后一项是原生回执当时的值，不表示 abort 后继续拥有写入权限；任何一项都不代替真实抬键。
- abort 后 status 真实响应（seq520／521）；测试 UP 的 SendInput 返回 1（seq522），但 seq523 未观察到本代产品实际 UP，`I10_abort_real_UP_not_NormalUp=FAIL`。这只确定观察合同未满足，尚不确定消息在哪一层中断，也不制造 NormalUp。
- 正常退出码 0、shutdown 全部资源 flags true、unknown completion=false；seq525→526 间没有 test_cursor／test_button_send，post-stop 未执行到点击发送，具体未满足的前置条件 UNKNOWN。保留 seq526 的 raw `executed=true, actual_down_up_delivered=false`；其中 executed 来自资源 flags，不证明已点击，因此本批不构成“产品吞输入／投递失败”证据，也不判整体 cleanup PASS。guest／临时任务按精确归属清理。grid 后新 Move／Ctrl、第二成员在途失败、设置生命周期、中断／15 分钟链、应用矩阵及 I12 便携入口均未到达，继续 NOT_RUN；普通接线／fixture 缺陷仍在授权范围内定位修复。

### cb807／8453：T→L、四格和八窗通过子场景，grid 后联合断言失败

- cb807：源码 `48bd08fa2f8202399f77d9f28d47433fa79167fd`，collection `uat/v1-control/cb80769a1ae24298a1f7ccc5946d48d3/collection-20261015T143649465Z-d5c83e648265495ba700b49af4ba84d2/`。T 两轴共享缩放、成员最小值／原始 DOWN 拉回、实际 Resize 后普通 Move 重建接触、L 布局换 source Ctrl Move、四格双轴及后续普通 Move 均通过对应检查（seq303–397）。八窗初始独立布局 seq398 失败；整体仍 FAIL，不能继承为八窗通过。
- 8453：源码 e811，collection `uat/v1-control/8453f79385e64ca29c2a73eeb4bc305c/collection-20261015T144744444Z-042358dfed054ee0bb830e0709f2bf4d/`。按实际 native 最小尺寸准备八窗后，末成员作为 Ctrl source、全部成员几何及该组资源销毁通过（seq419–430）；此前 T／L／四格对应操作也完成。
- 8453 原生 grid17 实际产生目标与回读不一致，产品拒绝且未伪称 API FALSE，seq440 通过。但紧接的 `I10_native_grid_single_attempt_before_revoke` 联合断言 seq441 失败；原记录不能区分次数、driver 账本、键状态等哪一子项失败，不预写“UP 已坏”“多次写入”或具体根因。后续 Move／Ctrl 恢复、I10 其余项目与整条链不计 PASS。
- 两批正常 stop 的资源字段与独立 client 投递分别保留；最终投递检查 cb807 seq401／8453 seq445 均为 false，不把进程退出或编排 guest 清理替代整体产品 cleanup。旧 aae 的 T START 失败仍保留，但不再作为“尚未越过 T”的最新进度。

### aae：双窗共享 Resize 局部通过，T 布局 START 失败

原始证据目录：

`uat/v1-control/aae67076ccda4bdb9300c9015108231a/collection-20261015T140152142Z-ddf706cc15c248109b5ed8df484de5e5/`

- Release／Medium 的双窗竖向共享边、相反侧 source、横向共享边实际缩放及 min 交集统一钳制通过（fixture seq548–582）；参与窗口实际矩形与产品 native exact 回执对应，例如 `process_id=6704` 的 call35–40。固定尺寸邻居场景在真实 DOWN／Move／UP 后保持几何且没有 placement（seq583–591）。这些是 M07／约束处理子场景，不是完整三窗／所有 source 或整条 I10 通过。
- 随后新建 T 布局已到实际前台／命中准备；产品 seq89 是 `prepared_actual_source_and_hit`，seq90 读到 source 自身 capture／move-size=`1835770`，fixture seq598 为 `layout_real_DOWN_production_start=FAIL`。该次未领取产品 START，不能写作原生观测到 `clicks=2`，也不能以准备快照推定产品已接管。
- 后续 caf4 已补“拖动过的 press 不再保留为双击首击候选”，支持返回原锚点后仍失效；cb807／8453 已越过 T 并完成上述子场景。该复验不补造 aae 缺失的 clicks=2 观察，也不宣布系统根因。
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
- Catalog 使用确定的实际矩形遮挡判定；吸附边线与 Ctrl 共享边从实际回执／几何绘制。e4 的 owned 视觉组件有实际结果，但未外推所有 DPI／应用或真人观感。
- Edge driver 消除 DOWN 前重复 bounds 查询；Terminal 精确根映像适配为实际 cmd，Console GUI 查询也与生产严格 fallback 对齐。Terminal 已由 e4 重新运行本次类别；Edge 仍有实际命中及收尾失败，不能共同写成通过。
- 已接原生 grid17 拒绝与后续 Move／Ctrl 恢复入口；8453 已真实观察 non-exact 后拒绝，但当时后续联合断言失败、恢复未通过。c01 后继 Move／Ctrl 恢复子项已通过，原 g6 实际 UP 仍缺失，完整 I10 未通过。grid 由自有应用在 `WM_WINDOWPOSCHANGING` 实施，不伪造产品已知 grid。
- Code 去掉可能覆盖显式命令行的 bootstrap data 设置，仅作为可区分假设；尚不能据此宣布根因或修复成功。

原始 DOWN、单 writer、撤权后无新调用、在途调用与完成未知分开、actual postverify 及精确视觉所有权合同继续保留。构建、L1/core 测试与局部 owned 结果都不能替代当前 Release 的整功能／模块验收。

ff 以来的七文件便携 ZIP、精确源身份／哈希、普通无参入口及 I10 在途失败用例均已接线；测试 driver 账本、实际低级鼠标回调和 GetAsyncKeyState 分开记录。后继实际结果见各批，不再用“未到达全部后续功能”概括当前状态。M12 的便携组件、固定表／校验器／证据绑定、中文可执行指南／集中缺陷／review入口均已落地；指南的实际命令、六页设置及托盘名称已与源码核对，**60/60 IMPLEMENTED仅表示已实施，不代表实现正确、说明完整现场走查或DEV通过**。当前独立完整合同为7/60功能、B29／B32共2/40（5%）；模块与整链仍未通过，S1不因实施数或独立单项关闭。

## 集中缺口与待核验项

### 已修或已有接线，等待对应现场闭环

| 项目 | 证据与现状 | 下一项明确验证 |
| --- | --- | --- |
| 设置与生命周期 | 269b／88e等旧失败保留；当前212／dd02十个设置／生命周期阶段及7635便携普通入口有实际子PASS；托盘BLOCKED | 完整M11／I07及所有设置项／高DPI走查未齐；不把子流程拼成整模块PASS |
| 布局 START／click 候选 | aae／cb807／8453保留历史身份；当前212／cea真实四窗双轴Resize及后继Move、八窗末成员Ctrl Move和三代NormalUp通过 | 不重跑G06／T-L旧专项；组件不关闭Full未知前台失败，不替代真实混合应用或整I03 |
| Edge／Terminal adapter | 744源快照修补已实际触达；fe0无PaneBind产品时Terminal仍HTCAPTION及shell启动失败，Edge实际HTCLIENT／单击／焦点通过但DOM无响应、根未closed | 不再把Native对照列待跑，也不判唯一系统根因；应用实际原生可用后再测产品，不以枚举child或放宽安全门取代 |
| grid 与成员失败 | 当前212／f54 grid5348与第二成员故障5418组件PASS；477a严格geometry拒绝、c01旧g6UP缺失等原FAIL保持身份 | 整I10合同仍未齐，组件PASS不补旧NormalUp；不把identity loss改叫API FALSE |
| 视觉／恢复与边线 | 当前212／f54 visual5051与temporary inflight5279组件PASS；7cac／e4原失败及迟到exact保留历史身份 | 不再写当前在途从未通过；组件结果不推普遍隐私／硬崩溃或全应用恢复PASS |
| post-stop 实际输入恢复 | 8cee 真实计数0→1、7cac／1bee／eae 真实计数1→2通过；d49 未执行到发送，不能判投递失败 | post-stop 投递不能关闭普通手势本代 UP 缺失，也不能推成整版 cleanup 验收 |
| 普通 Move 正常收尾 | 当前212／f54两轮owned核心已完成；四快速case实际LL-UP但仅native-only。c01、8cee／e4与c1f8原缺UP失败保留，成功不能补旧缺失 | D08仍OPEN；不由队列UP或Raw计数授增强NormalUp、不推已查明缺失根因 |
| ConsumedDown候选入场门 | f63旧门拒绝保留；d546及f481为历史资源可行性和退出风险实测，后继V1已接实际writer并有owned手势结果 | 旧owned可行性不外推Explorer；当前正式Explorer资格及五应用未齐，不升级宿主试用 |
| aborted／mapped 配对与准备 | 85c／b092 已修旧配对、authority=0 新配对与 hover 准备，2c 已接 mapped 精确 generation CAS | 历史实际 UP 缺失原因 UNKNOWN；1bee 未发本次 DOWN，不作为配对现场复验 PASS |
| 托盘与五键入口 | dd02当前212五键实际阶段PASS；1029行精确当前UID1图标不可达、菜单输入未发送，托盘仍BLOCKED，旧批次原身份保留 | 五键不等于整个M09或托盘菜单通过；仍需可达的真实精确托盘入口，不向未知对象点击 |
| 在途日志与实际异常 | 7cac／e4原失败保留；当前212／f54在途组件5279及batch5418已实测通过各自判定 | 不再以旧在途FAIL概括当前；仍不授整个I10或抹去历史UP失败 |
| GUI诊断与driver临时文件 | 590误纳`.tmp`的确定driver问题已由dd60 Debug97／Release714c真实final导出及parser专项复验关闭 | 设置底部“诊断”入口已实测，不因此授托盘／普遍隐私／完整M12通过 |
| 导出失败可见反馈 | 86a原R2缺实际modal的FAIL保留；866新R3真实modal/ack、0假包、恢复与重导出实际通过 | D22仅定向关闭／M12F02独立PASS，不抬M12模块或整版，不换绑旧R2 |

### 仍缺的实际环境或尚未闭合能力

本轮严格既有SSH别名只读核ZS-WORKSTATION：Professional／build26200；允许的安装注册表／PATH／已知命令位置未确认普通虚拟化程序，Get-VM不可用、Hyper-V CIM为无效命名空间（非权限拒绝），未确认可用注册VM。**不是全盘无VM证明**，未扫VM磁盘／用户数据、未启动或复用任何VM，未改host／CrossRec。瞬时空闲约12259MiB不推翻用户已有ISO／VM但不足新建VM的资源条件，也不构成资源分配许可；具体工具输出来源见验收表环境备注。

| 项目 | 准确边界 |
| --- | --- |
| 正式 Medium Explorer | 当前f54实际root RID8192但elevated=true，medium=false，正式资格拒绝且未输入；root清理未确认。不套旧High例外，不改token事实。 |
| Code | 7c6c／744已携官方Code并实际尝试Medium启动，但未取得唯一合格新root，startup BLOCKED、增强NOT_RUN；不再称本批未带EXE，也不把历史GPU／hidden子进程错误直接当本次唯一原因。 |
| Notepad | 744仍无明确可运行的官方EXE／合格新root，无应用输入、增强NOT_RUN；旧官方准备／认证和payload限制保留，不在宿主改账户或UAC，不将软件缺口当产品不兼容。 |
| E03／E04 | 当前没有真实双屏／负坐标与混合 DPI 环境证据，属于具体环境缺项；纯模拟坐标不代替。 |
| 200% UI、虚拟桌面、普通用户真机 | 尚未执行，分别保留 `NOT_RUN`；当前截图或 guest 局部结果不外推。 |
| I08／I10／I11 | f54旧file_size FAIL保留；e7两轮／20slot已完成但实际资源间隔899.9940725秒，严格900门FAIL。048真实F11观察通过，actor被实际失焦提前撤权，不算Deadline；I08未整体验收。9724严格900.037841秒及有限owned退出通过，视觉／在途／grid／batch子PASS不授整I10／I11或全应用／跨屏。 |
| 顺序配置输入差异 | 97 Debug专项成功后，同guest的Release实际客户激活失败，未到GUI/parser/ZIP；原因UNKNOWN。714c独立Release-only成功不证明根因，也不抹去原失败；不操作未知前台或追加无依据API。 |
| 本阶段便携交付 | 当前212／7635 ZIP由dd02再次实际验证普通GUI、保存后Move-UP、正常退出与字节；1250行原I12=PASS仅按入口scope评价，全部入口未齐，整I12仍BLOCKED。旧包和历史结果不换绑。 |

## 持续验证与交付边界

验收全集见 [V1_ACCEPTANCE.json](V1_ACCEPTANCE.json)，产品操作见[中文快速入门](../V1_QUICK_START.md)。当前为 **2/40（5%）**：独立登录启动／默认恢复B29及便携B32通过，不能称整版80%；B01–B32 全部必过，B33–B40 固定延后但保留在分母中。整版 DEV 门槛尚未满足，真人 UAT 为 `PENDING`。文档、机器表与后续证据更新应继续区分源码接通、局部实际结果和完整门槛，不能用测试数或旧 High 局部证据凑分。

本稿集中交付已测原ZIP、独立最新版中文指南、机器表与历史证据索引，供整体review；它是PARTIAL阶段交付，不代表满足B01–B32或整版完成。缺可靠共享事件身份时不增加RawNormalUp，不无新证据重复同一缺UP路线。最终文档commit／push及远端SHA由主执行者实际核验后交接，本文不预填自引用FinalSHA。原失败与历史包保留，`uat/`、EXE、ZIP 不入Git；不创建PR、merge、tag或公开release。

现场自动输入仍仅在本任务专属可丢弃 guest；远端文件仅在 `I:\PaneBindMVP1Runs\` 本任务目录，不触碰 CrossRec、其他用户数据、账户、token、UAC 或宿主设置。普通宿主试用和真人手感不能由自动 DEV 自签通过。
