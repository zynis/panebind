# R1-C3A — Human Ctrl + Move UAT Completion Record

The first Debug and subsequent Release positive human runs are complete and
accepted on `e56202d03f49721370c704c63e41976907779c2d`. Both have Ctrl activation,
multi-step follow, exact feedback/final geometry/restore PASS. See the
[human report](R1C3A_HUMAN_VALIDATION_REPORT.md) and [execution](R1C3A_EXECUTION_REPORT.md).
No new human UAT is requested by this seal. The instructions below are retained
as the historical Debug handoff, not an instruction to rerun it.

Both subjective smoothness observations are C (visibly stepped/laggy).
Functionality is accepted; product smoothness is not. Optimization is required
in a future R1-C3B, which has not started. Automatic restore remains fixture-only.

## Historical first Debug command

From `D:\repository\panebind`, after the implementation-ready gate passes:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\run-r1c3a-explorer-ctrl-glue-evidence.ps1 `
  -BuildDirectory out/r1c3a-debug `
  -Configuration Debug `
  -GlueTimeoutSeconds 120 `
  -ObserveSeconds 300
```

## Human actions

1. Follow the Leader prompt: create a **new** Explorer test window after the
   recorded baseline, navigate to the shown Leader nonce directory, then ENTER.
2. Follow the Follower prompt: create a second **new** Explorer test window,
   navigate to its distinct nonce directory, then ENTER.
3. Wait for readiness FIT. If the preview asks for smaller windows, manually
   resize only these two test windows as directed and retry readiness.
4. Wait for automatic zero-gap test layout and the **Ctrl+Move** prompt.
   There is no Y+ENTER Glue confirmation.
5. Hold either Ctrl **before** pressing the Leader title bar; drag the Leader
   for about one second, away from screen edges, Snap Layouts and zones.
6. Release the mouse **first**, then Ctrl. Wait for unhook, exact restore and
   evidence completion. Do not perform another drag in the same run.
7. Return the runner outcome/prefix and a short
   `HUMAN_SMOOTHNESS_OBSERVATION`: noticeable lag, stepped motion, jitter,
   or only an END-time jump? Severe/unusable stutter should be stated plainly.

Do not use preexisting Explorer windows, other applications, elevated windows,
extra third-party windows, Ctrl+Resize or mid-drag Ctrl attach/release tests.
Do not close another application to make the test work. No user input is
automated by Codex. An up sample is safely blocked, not retried mid-drag.

## Evidence contract

Raw logs remain ignored under `uat/r1c3a/`; do not commit them. The external
R0 Observer is an independent recorder, never a control bus. The validator
checks full JSONL/lifecycle/authority/geometry/restore/queue/feedback integrity
alongside exactly one callback-down activation and generation linkage.
Positive evidence needs at least two distinct active Follower targets/applies
and at least one apply before END **callback delivery**; this is not physical
mouse-release timing. CPU-side QPC metrics are diagnostic, not smoothness SLA.

`SAFE_BLOCKED: CTRL_NOT_DOWN_AT_START` requires no activation, no active
Follower write, unchanged Follower during drag, complete unhook and restore.
Setup/restore counts are separate. Do not convert incomplete/unsafe evidence
into SAFE_BLOCKED or a PASS. Too few progressive samples cannot pass realtime
evidence even if final geometry is exact.

This round does not claim product completion, dynamic Ctrl-release detachment,
physical-key provenance, AltGr/remapper compatibility, Snap coexistence,
cross-DPI/monitor/elevation behavior, 3+ windows or arbitrary application
eligibility. Automatic restore is **UAT fixture safety**, not product Glue
semantics. Debug human review must precede any decision about Release UAT.
