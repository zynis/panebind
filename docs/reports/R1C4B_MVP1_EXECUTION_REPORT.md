# R1-C4B MVP1 — Partial execution and stop decision

用户现在可以做什么：复核已提交的纯 Move 意图算法与一份新的 owned-only DOWN 锚点证据；**还不能试用三扇 Explorer 窗口的实时磁吸原型**。没有启用 Explorer cancellation、产品 Raw Input、输入隔离或窗口写入。本报告不把自动 owned 诊断当成人类物理输入 UAT 或 Explorer 产品验收。

## Scope and implementation checkpoint

- Branch: `codex/r1c4b-live-magnet`; starting HEAD: `5459133b00740e74f6be2f752eb96227760d370b`; implementation checkpoint: `56e984ba8d6791d21006f616f46306293afbc7ca`.
- Standard `git fetch origin` passed at round start; then `origin/main = 81e40facf52ffdb96f76e4d737740167d485f4a7`, `origin/main...HEAD = 0 behind / 36 ahead` before this round's commit.
- Added pure `CursorMoveMagnetIntent` and 37 deterministic checks: original cursor/window anchor, one solver per changed cursor, free intent even with no candidate/fast suppression/detach, X/Y proposal, hysteresis, duplicate cursor, UP retirement and checked overflow. This model does **not** grant native write authority.
- Added an explicit test-only owned DOWN-anchor diagnostic CLI in the existing probe. Old v5 CLI and validator verdict semantics are unchanged. Added the new core test to the fail-closed offline positive selection.
- Recorded the cross-process/input-isolation research boundary in [MVP1 input handoff decision](../architecture/R1C4B_MVP1_INPUT_HANDOFF_DECISION.md). No source code from AltSnap/FancyZones was copied or adapted.

## Automated and empirical evidence

Environment: Windows, VS 18 2026 x64, MSVC, Windows SDK 10.0.26100.0, CTest 4.2.3-msvc3. The configured CMake executable was `D:/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe`.

- `scripts/test-r1c4b-offline-selection.ps1`: 75 synthetic audit checks PASS, with no GUI/input.
- `scripts/run-r1c4b-offline-tests.ps1 -Configuration Debug -BuildDirectory out/r1c4b-live-magnet-debug`: 25/25 PASS.
- Same positive offline selection with `Release` and `out/r1c4b-live-magnet-release`: 25/25 PASS. No old interactive GUI CTest list was run.
- One default-sandbox read-only input preflight returned `UNKNOWN`: Codex sandbox desktop, foreground HWND 0; it sent no input. A distinct read-only preflight on the real host desktop returned `READY` (all required buttons/modifiers observed UP; capture/menu/move-size clear).
- Exactly one host-desktop owned Move DOWN-anchor diagnostic was run from implementation checkpoint `56e984b` with Debug probe SHA256 `A86CEE4D778CE33BB207641F9E4FB16EED7AB3333FFAF529D498951511BDBE70`. Probe exit 0, historical v5 offline validator `Result=PASS`, and post-run read-only host preflight `READY`. The v5 raw `shutdown.result=CAPTURED_NOT_ACCEPTED` remains its historical runtime label; it is not rewritten into product acceptance.
- Within that one owned synthetic gesture, Raw `WM_INPUT` LEFT_DOWN record 64 had available `MSG.pt=[1339,641]`; actual self-owned `WM_NCLBUTTONDOWN` record 67 had `lParam` screen point `[1339,641]`. This is a single empirical match, **not** proof that Raw DOWN identifies a foreign Explorer HWND or that cross-queue ordering is always reliable.
- New ignored local evidence: `uat/r1c4b-mvp1-anchor/56e984b-debug-01/`, containing both preflights, JSONL and metadata. Raw JSONL SHA256 `215C10DA9E013BBD1D19AE85306897D9F10C947F7B70C5BDEDD7973BEDE089A6`. Small review ZIP: `uat/r1c4b-mvp1-anchor/56e984b-debug-01-review.zip`, SHA256 `63E450CCFE2A948737931A2C7C3604C95D9AC60E2F9FE4376EFD71EEC385A62E` (30,273 bytes). None is tracked by Git or uploaded.

## Why live integration stops here

Cross-process cancellation releases Explorer's capture, so the physical legacy mouse stream can then reach Explorer controls or other applications. The existing Fix D post-END, planned-path shield cannot cover the cancel-to-END gap or an arbitrary real cursor path. Background `SetCapture` and `RIDEV_NOLEGACY` do not solve foreign-process legacy delivery. A candidate topmost, nearly transparent, nonactivating PaneBind-owned overlay would need to exist before cancel and, for an unrestricted trajectory, cover the virtual desktop until the physical/legacy UP handoff is proven. That temporarily intercepts input over the user's **pre-existing other windows**, broader than exact three-Explorer control. Its scope requires the user's explicit choice; no product overlay or Explorer cancellation was run. The second gate remains empirical: the one owned anchor match cannot prove Explorer original DOWN identity, real END authority, or safe legacy-UP teardown.

If the user authorizes the broad gesture-scoped isolation experiment, the smallest next sequence is: owned overlay hit/foreground/UP/cleanup evidence; read-only exact test-created Explorer DOWN/START association; then exact Explorer cancel/END/Raw continuation with no writer until fresh authority, followed by integrated Debug/Release tests. If not authorized, keep Explorer live work stopped and choose a different product interaction design in a later round. This report requests neither a human trial nor a change to the three Recovery Index files.

```text
FIXH_CORRECTED_REPLAY = PASS (historical accepted evidence)
MVP1_SHARED_MOVE_TAKEOVER = PARTIAL_CORE_ONLY
MVP1_OWNED_LIVE_MAGNET = NOT_RUN
MVP1_EXPLORER_MOVE_MAGNET = NOT_RUN
MVP1_CTRL_GLUE_SAME_ENTRY = NOT_RUN
MVP1_INPUT_HANDOFF_AND_CLEANUP = BLOCKED_BY_SCOPE_DECISION_AND_UNPROVEN_MECHANISM
MVP1_NATIVE_RESIZE_PRESERVED = NOT_REVALIDATED (product path unchanged)
MVP1_RELEASE_FUNCTIONAL_RUN = NOT_RUN
MVP1_TRYOUT_CANDIDATE = BLOCKED
PRODUCT_RAW_INPUT = NOT_IMPLEMENTED
PRODUCT_INPUT_ISOLATION = NONE
PRODUCT_SENDINPUT_DEPENDENCY = NONE (owned test driver only)
PRODUCT_GLOBAL_INPUT_HOOK = NONE
PRODUCT_DLL_INJECTION = NONE
PRODUCT_POLLING = NONE
PHYSICAL_INPUT_HUMAN_UAT = NOT_RUN
VISUAL_TERMINAL_RESTORE_FLICKER = NOT_HUMAN_TESTED
FULL_R1C4B_ACCEPTANCE = NOT_CLAIMED
PR / MERGE / TAG / RELEASE = NO
```
