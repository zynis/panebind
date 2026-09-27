# R1-C4B Pivot 1 Fix E — 64位证据数值审计

本轮是 evidence pipeline repair，不是窗口行为设计。沿用 Fix D 已审查的
ProductGestureAuthority / TestSyntheticInputIsolation / shield / writer 模型；
不修改 native、Core、product 或任何 acceptance predicate。无需重新调查并
修改已接受的窗口模型；Fix D prior-art/license记录保留，无新的外部代码复用。

## 平台 contract 与实测根因

官方 [QueryPerformanceCounter](https://learn.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancecounter)
返回 LARGE_INTEGER；其 [QuadPart](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-large_integer-r1)
为 signed64。[Windows PowerShell5.1 numeric literals](https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.core/about/about_numeric_literals?view=powershell-5.1)
说明小整数默认 Int32、更大值为 long，并且浮点/decimal会有其他类型。

本机 Windows PowerShell5.1 中，原 JSONL `native_return_qpc=1134816073663`
解析为 Int64；`$lastReturn=0` 为 Int32，原 `Math.Max` 调用选择了需转换
第二参数为 Int32 的重载并溢出。这是本机已复现的 binder 行为，不宣称
所有 PowerShell/.NET 版本的所有等价调用都如此。

```powershell
# OLD
$lastReturn=0
$lastReturn=[Math]::Max($lastReturn,$r.native_return_qpc)

# NEW（本轮实际实现）
[long]$lastReturn=0
[long]$currentReturn=$r.native_return_qpc
if($currentReturn -gt $lastReturn){$lastReturn=$currentReturn}
```

## 审计结果与范围

| 项目 | 数值处理 |
| --- | --- |
| 主/嵌套 `qpc`、`*_qpc`、frequency、sequence / `*_sequence`、watermark | 原值先断言 Int32或Int64且非负，再按 signed Int64 使用；不接受 string/bool/array/float/decimal偷渡或取整 |
| firstWrite/lastReturn/handoffStart、source/quantum/Raw sequence累加器 | 显式 `[long]`；Max使用比较赋值 |
| handoff/native/Raw duration、七类 latency | `[long]` stamps相减，输出signed Int64 ticks；在转换ms之前不降为Int32 |
| WinEvent arm/event tick | 原值先检查合法整数与DWORD范围；保持原wraparound matcher |
| Math.Max/Min剩余调用 | 只用于有界geometry，不接收QPC；不改变shield/anchor arithmetic |
| `[int]`剩余用途 | 数组index、Bool→0/1等非QPC用途；不是时钟/sequence下转 |
| percentile pipeline | 原ticks/frequency保留64位；ms/interpolation使用double或decimal；index可Int32；空样本不填假0 |

不能以“typed `[long]`参数”先隐式转换原输入：PowerShell会对fraction取整、
coerce数组等。公用timing helpers先逐参数检查原值；QPC delta先校验两个
原stamps再做Int64 subtraction。正负cross-stream latency仍合法；negative
stamp不合法。既有 row monotonic、native start≤return、receipt≤record等
时钟顺序条件保留，不新增 native EXIT≤WinEvent END 这一跨流 gate。

## Regression fixtures

完整 Move/BottomResize wire均对以下首个native return值平移所有非零QPC：

```text
2147483647
2147483648
3000000000
900000000000
1134816073663
1500000000000
9000000000000
```

每个量级比较原完整 verdict、authority、geometry、cleanup和operation count
signature不变，所有七类latency保持相同值且Int64。另测最大return accumulator、
大于Int32的duration与合法负跨流latency；负值、fraction、Double整数、超过
signed64、string/bool/null/array、fractional sequence/watermark、DWORD越界、
主/native/input/shield/callback时钟逆序均须拒绝。

保留原87项fixtures，不删弱原负例。Replay assessment额外测试完整gate、历史
INVALID/false不改、hash/SHA/contract binding、missing/type/unknown exception
与semantic assertion分型、禁止child CLI和覆盖写、原双hash及source/binary前后
核对。实际测试数量/命令/结果由本轮 execution report记录，不以fixture充当实测。

## 不可变 replay 与后续 gate

原460条JSONL与metadata只读，固定expectedSHA256前后核对。完整corrected
validator重新计算全部contract；不读旧报告中的PASS，不重定义函数去跳过
断言。新 `.revalidation.json` 使用 CreateNew，记录原implementation SHA、
validator fix SHA/file hash、original INVALID与新replay result；不覆盖原metadata。
Exception/missing/type为INVALID，semantic FAIL或任何失败即STOP，不启动GUI。

只有完整replay PASS后才当前Fix D runtime DebugMove×1；再分别计Debug/Release
Move/Resize四组smoke，均5/5后才四组20/20formal。首失败不重试。统计/operation
storm继续区分source与shield，既有每quantum最多一次source write及每source
result最多一次required shield placement不变。不前移Explorer或产品输入接管。
