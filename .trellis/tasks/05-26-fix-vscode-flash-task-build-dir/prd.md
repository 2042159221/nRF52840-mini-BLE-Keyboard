# fix vscode flash task build directory

## Goal

Fix the VS Code flash task so `west flash` uses the Zephyr application-domain build directory instead of the sysbuild top-level build directory. This prevents `Error: could not find CMAKE_PROJECT_NAME in Cache` during the rebuild step before flashing.

## What I already know

* The current `nRF: West Flash Erase` task passes `-d '${workspaceFolder}/build'`.
* `build/CMakeCache.txt` is the sysbuild top-level cache and does not contain the normal Zephyr application project cache entries expected by `west flash` rebuild logic.
* `build/PRO_Ble_mini_keyboard/CMakeCache.txt` is the actual application-domain cache and contains board and Zephyr byproduct entries.
* Project instructions require loading `scripts/ncs-env.ps1` and invoking west through `E:\ncs\toolchains\fd21892d0f\opt\bin\python.exe`.

## Requirements

* Update the VS Code `nRF: West Flash Erase` task to flash from the application-domain build directory.
* Keep the existing NCS environment loading and explicit NCS toolchain Python usage.
* Do not change unrelated build tasks or project behavior.

## Acceptance Criteria

* [ ] `.vscode/tasks.json` flash task uses `${workspaceFolder}/build/PRO_Ble_mini_keyboard` as the west flash build directory.
* [ ] The task remains a PowerShell command that sources `scripts/ncs-env.ps1` and runs `python.exe -m west flash`.
* [ ] JSON syntax remains valid.

## Definition of Done

* Minimal scoped edit completed.
* JSON validity checked.
* User is told how to run the corrected task.

## Out of Scope

* Reworking the build script.
* Changing board, snippets, or build output location.
* Flashing the device unless explicitly requested.

## Technical Notes

* Inspected `.vscode/tasks.json` and both `build/CMakeCache.txt` and `build/PRO_Ble_mini_keyboard/CMakeCache.txt`.
