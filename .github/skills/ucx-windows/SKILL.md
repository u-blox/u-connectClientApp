---
name: ucx-windows
description: Build and run u-connectClientApp on Windows to test and configure a u-connectXpress module (NORA-W36, NORA-B26). Use for Windows setup, COM port connection, scripted --exec runs and firmware updates.
---

# u-connectClientApp on Windows

## Build and run
1. Install Visual Studio 2022 Build Tools (Desktop development with C++), CMake 3.15+ and Git.
2. `git submodule update --init u-connectClient`
3. `.\u-connectClientAppWin.cmd` builds on first run and opens the shell. Other modes: `debug`, `clean`, `rebuild`, `selftest`.

## Use
- Connect an EVK (FTDI USB). The app auto-detects the COM port, or pass it: `u-connectClientApp.exe COM13`.
- Script: `u-connectClientApp.exe COM13 --exec "device.info" --exec "wifi.scan"` (exit code 1 on first failure).
- Firmware update: `--exec "fw.flash <file>"`.
- Type `help` in the shell for all commands; see `u-connectClientApp/UCXAPP_COMMANDS.md`.

## Module documentation
Look up AT commands and behaviour with the `u-blox-docs` MCP server (`.vscode/mcp.json`).
