# Git transport resilience repair — 2026-09-12

用户明确授权建立 SSH-over-443 primary、HTTPS secondary，并修订 Remote Git
policy；不是 GitHub API object/ref reconstruction。

Starting HEAD: 0623c7ded7c5bc9ecfec195e0a00bfa907140708。
Branch: codex/r1c3b-smoothness-profile；初始 working tree clean。

专用 Ed25519 key 在上一轮生成并完成 private/public pair 校验，passphrase 为
NONE；本轮复用，用户已手动向 zynis 注册公钥。没有覆盖或使用其他项目 key，
没有打印私钥，没有把 SSH 文件或 key 内容提交仓库。当前登录 gh 缺少
admin:public_key scope 的历史已明确，未扩大 scope 或重新登录。

用户 SSH config 仅追加专用 block，匹配 github-443 和实际 ssh.github.com，
使用 IdentityFile、IdentitiesOnly yes 与端口 443；原有配置保留。
Host key 按 GitHub 官方公布指纹核对，认证验证使用 StrictHostKeyChecking=yes。
Windows ssh-agent 未能在非管理员环境持久启动；IdentityFile 直接使用 key，
不是 transport blocker。未改代理、TLS verification 或持久 HTTP version。

| 检查 | 结果 |
| --- | --- |
| ssh -T github-443 | Hi zynis；认证 PASS。GitHub 无 shell access 的 exit 1 为预期 |
| SSH URL git ls-remote | main=d901094404f04772e1d641842df76d13c551a734；工作分支=0623c7ded7c5bc9ecfec195e0a00bfa907140708 |
| origin fetch / ls-remote / push --dry-run | 全部 PASS |
| github-https fetch / ls-remote | 本次全部 PASS；与 origin 返回相同 SHA |
| Git config | origin fetch/push 为 SSH URL；保留 github-https fetch/push 为 HTTPS URL |
| Runtime changes in repair commit | NONE |

Exact URLs:

~~~text
origin = ssh://git@ssh.github.com:443/zynis/panebind.git
github-https = https://github.com/zynis/panebind.git
~~~

AGENTS.md 现在要求：明确瞬时错误最多 3 次短退避重试，primary 失败后尝试
secondary；标明成功 remote，不伪造 origin refs；push 后通过成功 transport
核对远端 SHA。认证/主机/仓库身份、冲突 refs 或完整性错误仍必须 STOP。

这不保证网络永不瞬断；修复的是单链路脆弱性和过早停止策略。基础设施提交后，
通过标准 push + ls-remote 再核对该分支，再继续用户已批准的 Phase 3。

官方依据：[SSH over HTTPS port](https://docs.github.com/en/authentication/troubleshooting-ssh/using-ssh-over-the-https-port)、
[GitHub host fingerprints](https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/githubs-ssh-key-fingerprints)。
