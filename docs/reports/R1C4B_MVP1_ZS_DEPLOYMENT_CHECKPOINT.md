# R1-C4B MVP1 — ZS-Workstation deployment checkpoint (2026-09-28)

This is a checkpoint before any Windows feature change or guest input. It is not an owned, Explorer, or three-window acceptance report.

## Identity and verified deployment

- Starting branch `codex/r1c4b-live-magnet`, HEAD `55e3c2b242a813d8466ce704aad45d484e05c48b`, clean worktree; standard `git fetch origin` succeeded and local/upstream divergence was `0/0`. `origin/main=81e40facf52ffdb96f76e4d737740167d485f4a7` (`0 behind / 43 ahead`).
- Existing SSH alias with `BatchMode=yes`, `StrictHostKeyChecking=yes` and the already trusted key confirmed hostname `ZS-Workstation`; no credential or SSH setting was changed. The remote SSH token is elevated, but elevation does not establish a maintenance window.
- New PaneBind-only host directories created: `C:\Users\zs\AppData\Local\PaneBindMVP1Runs\` and its new child `370eb9f51b0a4838b3d10da7829bcec2\`. Before creation, both were absent. No other host directories or existing files were edited, moved, deleted, or overwritten.
- Via SCP, only `desktop-deploy.zip` was placed in that RunId directory. Remote SHA-256 matched local: `70DEF9C34B677526F7342D060AD13C7F03870E7A34568BAE94E37DDFB8614384`. It was expanded into the same new directory without overwrite. Input, output, and `desktop-owned-mvp1.wsb` were verified; the manifest identifies implementation SHA `328a6a1b0c5f240afb2b30a39c9e1f112dd87449` (the later documentation HEAD does not invalidate this binary identity).
- Verified remote SHA-256: probe `23E142785ACC36AA832BC73F4D88AED900D303463885475ED61FD90F210A4E35`; input preflight `545696E53851C19EE0B0DE8AB98695A78695947E02F46505C282C99839D513A4`; guest script `7478A743033BA510B8B3D92D4D1B6575C5F43727F0E5348F8F3BD99FD951C282`; manifest `27C95A06B3349AA4750BF166FBCBECAB6C6198069744FC7D7E946E7340ACEE1B`; `.wsb` `01191E85BD7CACF7556BCCE73F7C62192C60DCAE0265580C919FFB3F01952E70`. Input/output RunId markers match. The `.wsb` HostFolder paths point at these remote directories, not the development machine; input is read-only, output writable, networking and clipboard disabled.
- The local ignored `uat/.../RESUME.md` records exact deployment/start target, evidence path, hash list, SSH recovery conditions and continuation sequence. No `uat/`, ZIP or EXE entered Git.

## System and maintenance gate

- ZS-Workstation is Windows Professional 25H2 build 26200.9168. Before change, `Containers-DisposableClientVM=Disabled`; **no** Windows feature has yet been enabled and **no** restart has occurred. Existing `sshd` is Running with StartMode Auto. Console Session 1 is Active, but its saved-work state is unknown. Read-only checks found no definite current CrossRec maintenance approval; absence of a matching task/process name is not proof CrossRec has ended or is safe to interrupt.
- Read-only resource check: 31.9 GB RAM, 8 logical processors, hypervisor present, system `C:` free space about 1.3 GB. Microsoft lists at least 1 GB free for Windows Sandbox, so this nominally meets the published floor but leaves little margin. No existing files will be deleted or moved to make space. Free space must be checked again after any official `-NoRestart` enablement and before a boot attempt; a low-space failure or unsafe margin is a new explicit environment blocker, not a product failure.
- The only planned system mutation is Microsoft's `Enable-WindowsOptionalFeature -Online -FeatureName Containers-DisposableClientVM -All -NoRestart` with the existing lawful elevated token and Windows-required dependencies. Because maintenance and unsaved work are not confirmed, it has **not** been run. The user has been asked once whether a controlled, non-forced restart is currently allowed with CrossRec in maintenance and desktop work saved. No CrossRec repository, data, environment, task, or process was modified, paused, stopped, or rerun.
- No Sandbox instance, scheduled task, GUI test, synthetic input, or cleanup has been executed. On resumption, check the current feature/host identity, then only after the maintenance answer enable without immediate restart; record the returned RestartNeeded and feature state. If a restart is needed, do not force-close applications. After restart revalidate SSH identity, feature, console session and package hash, then launch only this `.wsb` in the logged-in user's interactive session and review the guest evidence in the RunId `output` directory.

Official references: [Sandbox installation and prerequisites](https://learn.microsoft.com/en-us/windows/security/application-security/application-isolation/windows-sandbox/windows-sandbox-install), [`.wsb` mappings and logon command](https://learn.microsoft.com/en-us/windows/security/application-security/application-isolation/windows-sandbox/windows-sandbox-configure-using-wsb-file), [`Enable-WindowsOptionalFeature -NoRestart`](https://learn.microsoft.com/en-us/powershell/module/dism/enable-windowsoptionalfeature).

```text
DEPLOYMENT = VERIFIED
SYSTEM_FEATURE_CHANGE = NONE
RESTART = NOT_PERFORMED
GUEST = NOT_STARTED
OWNED / EXPLORER / THREE_WINDOW_UAT = NOT_RUN
MAINTENANCE_AND_SAVED_WORK = UNKNOWN
```

## 2026-09-29 I: storage amendment

The section above is the historical pre-migration checkpoint. The user then directed all PaneBind deployment and future evidence to `I:`. Read-only checks found `I:` to be a 931.5 GB fixed volume with 927.6 GB free; `I:\PaneBindMVP1Runs` and the exact RunId child were absent. Only those two directories were created. No other I: files or projects were touched.

The original, unchanged `desktop-deploy.zip` was copied by SCP into `I:\PaneBindMVP1Runs\370eb9f51b0a4838b3d10da7829bcec2\` and verified with the same SHA-256 `70DEF9C34B677526F7342D060AD13C7F03870E7A34568BAE94E37DDFB8614384`. The unpacked EXEs, script, manifest, run markers and implementation SHA `328a6a1b0c5f240afb2b30a39c9e1f112dd87449` all matched the original package. A separate `owned-mvp1-I.wsb` was generated and remote-verified at SHA-256 `DD61529AD4CA6D52375727FB8F711C38BC53A633824F43ECF6A597315E008FC8`. Its only host mappings are this RunId's `input` (read-only) and `output` (writable); the **guest** paths remain `C:\PaneBindMVP1\Input` and `C:\PaneBindMVP1\Output`. Networking, clipboard and unnecessary device redirection remain disabled. Launch only the I-specific `.wsb`, not the archived C-host configuration.

After the I copy and its recovery note were SHA-verified, the known extracted C-host `.wsb` in the I directory was removed; it is recoverable from the unchanged ZIP. A one-purpose cleanup script (SHA-256 `098AEEEA5D7C7419BC58CDA525B6571A6EEEED0AA4465ADC3C1D8CAA80B9DC7A`) checked the I copy and recovery note, the exact C RunId directory inventory, each of eight old file hashes and reparse-point safety. It then removed only those eight C RunId files and the three resulting empty `input`, `output` and RunId directories non-recursively. The C `PaneBindMVP1Runs` parent remains, and no unknown file was deleted. These removed files are not in the Recycle Bin but can be restored from the I copy or local ignored ZIP. The I recovery note's current SHA-256 is `0DD071C71968361E371B3B1D92ED46D19149AEE6ED1FC346B5C29C286730D207`.

Post-cleanup read-only check: I RunId exists, old C RunId does not, C parent remains, `C:` free space is about **1.1 GB**, `I:` free space 927.56 GB, and `Containers-DisposableClientVM` remains `Disabled`. Moving files to I does **not** move Sandbox system storage. Per the user's explicit limit, no feature enablement, guest launch or restart will occur at this C: margin; no existing file, system directory, partition, global TEMP, symlink, or other project is modified to make room. The CrossRec maintenance/saved-work confirmation is still outstanding. Owned, Explorer, and three-window UAT remain `NOT_RUN`; no prior regression was rerun for this path-only migration.
