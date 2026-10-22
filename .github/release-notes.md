## What's in this release

u-connectXpressApp is the command shell for u-connectXpress modules (NORA-W36, NORA-B26). It runs on Windows and Linux, and as firmware on the NUCLEO-H753ZI, NUCLEO-F767ZI, NUCLEO-F439ZI, NUCLEO-F401RE, NUCLEO-F446RE, STM32F4DISCOVERY and Blue Pill (lite) boards.

- One launcher, `ucxapp.py`, for Windows, Linux (also WSL) and STM32
- Hardware detection with `--detect`, including NORA modules, GNSS receiver and IMU
- New command modules for the app, GNSS, IMU, cellular and hardware
- Streaming and a terminal UI
- Embedded web page for the module
- STM32 board configurations in `app/boards/`

## Downloads

There is one archive per target. Each unpacks to a folder of the same name that also holds the SHA-256 checksums.

| Archive | How to use |
|---|---|
| `ucxapp-windows-x64.zip` | Signed `ucxapp.exe`; run it and connect to the module's COM port |
| `ucxapp-linux-x64.tar.gz` | `tar -xzf`, then run `ucxapp` with the serial port, e.g. `/dev/ttyUSB0` |
| `ucxapp-stm32-nucleo_h753zi.zip` | `.bin/.hex/.elf`; flash with STM32CubeProgrammer at `0x08000000` |
| `ucxapp-stm32-nucleo_f767zi.zip` | Same |
| `ucxapp-stm32-nucleo_f439zi.zip` | Same |
| `ucxapp-stm32-f407_disc.zip` | Same (STM32F4DISCOVERY) |
| `ucxapp-stm32-nucleo_f401re.zip` | Same (NUCLEO-F401RE) |
| `ucxapp-stm32-nucleo_f446re.zip` | Same (NUCLEO-F446RE) |
| `ucxapp-stm32-bluepill_f103c8.zip` | Same (Blue Pill, lite build, needs 128 KB flash) |

## Connecting a NORA-W36 to a NUCLEO board

All boards use the Arduino header CN10. The shell runs on the ST-Link virtual COM port (115200 baud).

| NORA-W36 pin | Arduino pin | NUCLEO-H753ZI | NUCLEO-F767ZI / F439ZI | Function |
|---|---|---|---|---|
| TX | D0 (CN10) | PB7 (USART1_RX) | PG9 (USART6_RX) | Board receives |
| RX | D1 (CN10) | PB6 (USART1_TX) | PG14 (USART6_TX) | Board transmits |
| GND | GND (CN10) | GND | GND | Ground |
| VCC | 3V3 (CN10) | 3.3V | 3.3V | Power |

The module UART runs at 115200 baud. Use `bridge` in the shell to talk to the module directly from a PC terminal.
