---
name: ucx-stm32
description: Build, flash and use u-connectClientApp as firmware on NUCLEO-H753ZI, NUCLEO-F767ZI or NUCLEO-F439ZI boards driving a u-connectXpress module. Use for STM32 setup, HAL paths, flashing over ST-Link, the console and UART bridge mode.
---

# u-connectClientApp on STM32 Nucleo

## Prerequisites
- Arm GNU toolchain (`arm-none-eabi-gcc`), CMake, Ninja, STM32CubeProgrammer (ST-Link flashing).
- The matching STM32Cube HAL: set `STM32CUBEH7_PATH`, `STM32CUBEF7_PATH` or `STM32CUBEF4_PATH`, or init the u-connectClient submodules.

## Build, flash, log
```powershell
.\u-connectClientAppSTM32.cmd --board h753 --build --flash --log   # h753 (default), f767 or f439
```
Other options: `--rebuild`, `--clean`, `--erase`, `--recover`, `--log COMx`, `--help`. Prebuilt `.bin/.hex/.elf` images for each board are attached to every GitHub release; flash a `.bin` at `0x08000000` with STM32CubeProgrammer.

## Wiring
NORA-W36 on the Arduino header CN10, 115200 baud:

| NORA-W36 | Arduino | H753 | F767 / F439 |
|---|---|---|---|
| TX | D0 | PB7 (USART1_RX) | PG9 (USART6_RX) |
| RX | D1 | PB6 (USART1_TX) | PG14 (USART6_TX) |
| GND | GND | GND | GND |
| VCC | 3V3 | 3.3V | 3.3V |
- The shell runs on the ST-Link virtual COM port (USART3, 115200).

## Use
- The shell connects to the module at startup; `device.connect [uart] [baud]` reconnects.
- `bridge [baud]` makes the board a transparent console-to-module bridge (AT debugging, XMODEM updates). Press Ctrl-] with 1 s of silence before and after to return to the shell.
- Command list: `COMMANDS.md`.

## Module documentation
Look up AT commands and behaviour with the `u-blox-docs` MCP server (`.vscode/mcp.json`).
