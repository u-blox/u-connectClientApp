## What's in this release

u-connectClientApp is the command shell for u-connectXpress modules (NORA-W36, NORA-B26). It runs on Windows and Linux, and as firmware on the NUCLEO-H753ZI, NUCLEO-F767ZI and NUCLEO-F439ZI boards.

- One launcher, `ucxapp.py`, for Windows, Linux (also WSL) and STM32
- Hardware detection with `--detect`, including NORA modules, GNSS receiver and IMU
- New command modules for the app, GNSS, IMU, cellular and hardware
- Streaming and a terminal UI
- Embedded web page for the module
- STM32 board configurations in `app/boards/`

## Downloads

Files are grouped by platform. Verify the Windows exe against the attached `.sha256` file.

### Windows

| File | How to use |
|---|---|
| `ucxapp-windows-x64.exe` (signed) | Run it and connect to the module's COM port |

### Linux

| File | How to use |
|---|---|
| `ucxapp-linux-x64` | `chmod +x`, then run it with the serial port, e.g. `/dev/ttyUSB0` |

### STM32

| File | How to use |
|---|---|
| `ucxapp_nucleo_h753zi.bin/.hex/.elf` | Flash with STM32CubeProgrammer at `0x08000000` |
| `ucxapp_nucleo_f767zi.bin/.hex/.elf` | Same |
| `ucxapp_nucleo_f439zi.bin/.hex/.elf` | Same |

## Connecting a NORA-W36 to a NUCLEO board

All boards use the Arduino header CN10. The shell runs on the ST-Link virtual COM port (115200 baud).

| NORA-W36 pin | Arduino pin | NUCLEO-H753ZI | NUCLEO-F767ZI / F439ZI | Function |
|---|---|---|---|---|
| TX | D0 (CN10) | PB7 (USART1_RX) | PG9 (USART6_RX) | Board receives |
| RX | D1 (CN10) | PB6 (USART1_TX) | PG14 (USART6_TX) | Board transmits |
| GND | GND (CN10) | GND | GND | Ground |
| VCC | 3V3 (CN10) | 3.3V | 3.3V | Power |

The module UART runs at 115200 baud. Use `bridge` in the shell to talk to the module directly from a PC terminal.
