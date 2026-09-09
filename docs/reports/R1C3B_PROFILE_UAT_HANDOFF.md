# R1-C3B Phase 1 — First Debug Profile UAT

Only one positive Debug Ctrl+Move profile run is requested. No Release run,
ablation, internal-only comparison or extra manual profiling action is needed.
The implemented path preserves C3A interaction and C2B safety behavior.

## Command

From D:\repository\panebind:

~~~powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run-r1c3b-smoothness-profile.ps1 -BuildDirectory out/r1c3b-debug -Configuration Debug -GlueTimeoutSeconds 120 -ObserveSeconds 300
~~~

External R0 Observer is ON. The reserved ExternalObserverEnabled=false option
is rejected in this phase; no independent evidence is silently disabled.

## Human steps

1. At the Leader prompt, create a new Explorer window after its baseline,
   navigate to the displayed Leader nonce directory, then ENTER.
2. At the Follower prompt, create a second new Explorer window, navigate to its
   separate nonce directory, then ENTER.
3. Wait for readiness FIT; if prompted, resize only these test windows until
   they fit. The fixture then prepares the zero-gap layout automatically.
4. Wait for the drag prompt. Hold Ctrl **before** pressing the Leader title bar.
5. Drag normally for approximately one second, away from screen edges/Snap.
6. Release the mouse first, then Ctrl.
7. Wait for unhook, exact restore and evidence analysis. There is no Y+ENTER
   Glue activation and no new profiling gesture.

Do not reuse existing Explorer windows, drag the Follower, use another app,
resize while activated, change monitor/DPI, or close unrelated applications.
Automatic restore is UAT fixture safety, not product Glue semantics.

## Return these results

Return the prefix under ignored uat/r1c3b/, the runner outcome and one rating:

| Rating | Observation |
| --- | --- |
| A | Very smooth |
| B | Continuous follow with slight stepping/lag |
| C | Visibly stepped/laggy |
| D | Only jumps at the end |
| E | Jitter/abnormal behavior |

Phase 1 does **not** require improvement to B. Existing C3A was C/C.
The new output retains Ctrl/START/LOCATION/END/applies/feedback/final/restore
gates and adds scheduling, backlog, Leader/Follower capture, Shell/location,
process/security, geometry/DWM, monitor/DPI, Core, prepare/pending, native,
postverify and feedback costs. It prints data-driven hotspot rankings and
per-operation exclusive breakdowns. Missing private-message dispatch is
explicitly NOT OBSERVED, not synthesized from drain time.

TIMING_PROFILE_GATE=FAIL means the profile cannot support a timing conclusion,
even if Glue correctness completed. Preserve the files; do not infer a root
cause or edit the evidence. Raw logs are local only and must not enter Git.
CPU-side intervals are not display latency/FPS, and different-grain percentiles
must not be added.

No Phase 2 optimization, Z-order change, larger component or mixed-DPI support
is authorized by this handoff. Real profile interpretation follows this run.
