<!-- TRELLIS:START -->
# Trellis Instructions

These instructions are for AI assistants working in this project.

This project is managed by Trellis. The working knowledge you need lives under `.trellis/`:

- `.trellis/workflow.md` — development phases, when to create tasks, skill routing
- `.trellis/spec/` — package- and layer-scoped coding guidelines (read before writing code in a given layer)
- `.trellis/workspace/` — per-developer journals and session traces
- `.trellis/tasks/` — active and archived tasks (PRDs, research, jsonl context)

If a Trellis command is available on your platform (e.g. `/trellis:finish-work`, `/trellis:continue`), prefer it over manual steps. Not every platform exposes every command.

If you're using Codex or another agent-capable tool, additional project-scoped helpers may live in:
- `.agents/skills/` — reusable Trellis skills
- `.codex/agents/` — optional custom subagents

Managed by Trellis. Edits outside this block are preserved; edits inside may be overwritten by a future `trellis update`.

<!-- TRELLIS:END -->


<!-- USER-RULES:START -->
- 尽量每次修改代码都要做一次 git 企业级规范提交，注明关键信息，方便回溯和代码回滚。

- 执行 west 相关命令必须先加载项目 NCS 环境，再显式使用 NCS 工具链 Python；不要调用裸 `west`、裸 `python` 或 `E:\anaconda3\python.exe`。

  推荐直接使用项目脚本：

  ```powershell
  .\scripts\build.ps1 -Pristine -BuildDir "D:\b_ip5306_rtt_usb" -Board "mini-keyboard/nrf52840" -Snippet "rtt-console"
  ```

  如必须手写 west 命令，使用下面的 PowerShell 形式，注意 `BOARD_ROOT` 必须是解析后的项目根目录字符串，不要写成字面量 `$PWD`：

  ```powershell
  $ProjectRoot = (Resolve-Path ".").Path
  . .\scripts\ncs-env.ps1
  & "E:\ncs\toolchains\fd21892d0f\opt\bin\python.exe" -m west build --build-dir "D:\b_ip5306_rtt_usb" $ProjectRoot --pristine=always --board "mini-keyboard/nrf52840" -- -DSNIPPET=rtt-console "-DBOARD_ROOT=$ProjectRoot"
  ```

  如果出现 `west: unknown command "build"; do you need to run this inside a workspace?`，优先检查是否已执行 `. .\scripts\ncs-env.ps1`，不要改用系统 Python 或 Anaconda Python。
<!-- USER-RULES:END -->
