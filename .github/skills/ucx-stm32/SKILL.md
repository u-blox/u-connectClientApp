---
name: ucx-stm32
description: Build, flash and use u-connectXpressApp as firmware on NUCLEO-H753ZI, NUCLEO-F767ZI, NUCLEO-F439ZI, NUCLEO-F401RE, NUCLEO-F446RE, STM32F4DISCOVERY (STM32F407VG) or Blue Pill (STM32F103C8, lite build) boards driving a u-connectXpress module. Use for STM32 setup, HAL paths, flashing over ST-Link, the console and UART bridge mode.
---

# u-connectXpressApp on STM32 Nucleo

## Prerequisites
- Arm GNU toolchain (`arm-none-eabi-gcc`), CMake, Ninja, STM32CubeProgrammer (ST-Link flashing).
- The matching STM32Cube HAL: set `STM32CUBEH7_PATH`, `STM32CUBEF7_PATH`, `STM32CUBEF4_PATH` or `STM32CUBEF1_PATH`, or init the u-connectClient submodules. A `STM32CubeF1` checkout next to this repo is found automatically.

## Build, flash, log
```powershell
.\ucxapp --stm32 --board h753 --build --flash --log   # h753 (default), f767, f439, f407, f401, f446, bluepill or all
```
On Linux: `./ucxapp.py --stm32 ...`. Other options: `--rebuild`, `--clean`, `--erase`, `--recover`, `--log COMx`, `--help`. Prebuilt `.bin/.hex/.elf` images for each board are attached to every GitHub release; flash a `.bin` at `0x08000000` with STM32CubeProgrammer.

## Wiring
NORA-W36 on the Arduino header CN10, 115200 baud:

| NORA-W36 | Arduino | H753 | F767 / F439 |
|---|---|---|---|
| TX | D0 | PB7 (USART1_RX) | PG9 (USART6_RX) |
| RX | D1 | PB6 (USART1_TX) | PG14 (USART6_TX) |
| GND | GND | GND | GND |
| VCC | 3V3 | 3.3V | 3.3V |
- The shell runs on the ST-Link virtual COM port (USART3, 115200).

### STM32F4DISCOVERY (STM32F407VG, `--board f407`)
No Arduino header and the ST-Link VCP is not routed to the MCU, so wire it by hand:

| Signal | Pin |
|---|---|
| NORA-W36 TX -> USART3_RX | PB11 |
| NORA-W36 RX <- USART3_TX | PB10 |
| Shell console USART2 TX / RX (USB-UART adapter, 115200) | PA2 / PA3 |
| GND, 3V3 | GND, 3V |

### NUCLEO-F401RE / NUCLEO-F446RE (`--board f401` / `--board f446`)
Arduino D0/D1 is USART2, which is the ST-Link VCP, so the module goes on USART1:

| Signal | Pin |
|---|---|
| NORA-W36 TX -> USART1_RX | D2 (PA10) |
| NORA-W36 RX <- USART1_TX | D8 (PA9) |
| GND, 3V3 | GND, 3V3 |
- The shell runs on the ST-Link virtual COM port (USART2, 115200). The F401 runs at 84 MHz, the F446 at 168 MHz.

### Blue Pill (STM32F103C8, `--board bluepill`)
Lite build for 20 KB RAM: device, wifi, sock, sys, app, mqtt, bt and gatt commands (no net, cert or TLS). Scan and GATT discovery tables are small (8 devices, 8 services, 24 characteristics) and Bluetooth company, appearance and characteristic names are shown as numbers only. The image is about 118 KB, so it needs a part with 128 KB of usable flash (most C8 chips, or a CB). Flash with an ST-Link V2 on the SWD header.

| Signal | Pin |
|---|---|
| NORA-W36 TX -> USART2_RX | PA3 |
| NORA-W36 RX <- USART2_TX | PA2 |
| NORA-W36 RTS / CTS (only with flow control) | PA0 (CTS) / PA1 (RTS) |
| Shell console USART1 TX / RX (USB-UART adapter, 115200) | PA9 / PA10 |
| GND, 3V3 | G, 3.3 |

## Use
- The shell connects to the module at startup; `device.connect [uart] [baud]` reconnects.
- `bridge [baud]` makes the board a transparent console-to-module bridge (AT debugging, XMODEM updates). Press Ctrl-] with 1 s of silence before and after to return to the shell.
- Command list: `COMMANDS.md`.

## Module documentation
Look up AT commands and behaviour with the `u-blox-docs` MCP server (`.vscode/mcp.json`).
