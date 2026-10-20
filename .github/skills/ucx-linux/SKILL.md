---
name: ucx-linux
description: Build and run u-connectClientApp on Linux to test and configure a u-connectXpress module (NORA-W36, NORA-B26) over a serial port. Use for Linux setup, serial permissions and scripted runs.
---

# u-connectClientApp on Linux

## Build and run
1. Install `cmake`, `gcc` and `git`.
2. `git submodule update --init u-connectClient`
3. `./u-connectClientAppLinux.sh` builds and starts the shell (the binary is `u-connectClientApp/bin/ucxapp`).

## Use
- Serial access: add your user to the `dialout` group (`sudo usermod -aG dialout $USER`, then log in again).
- Connect to a module: `device.connect /dev/ttyUSB0` in the shell, or pass the port on the command line.
- Script commands non-interactively with `--exec "<command>"`; type `help` for the full list (see `u-connectClientApp/UCXAPP_COMMANDS.md`).

## Module documentation
Look up AT commands and behaviour with the `u-blox-docs` MCP server (`.vscode/mcp.json`).
