## What's in this release

u-connectXpressApp is the command shell for u-connectXpress modules (NORA-W36, NORA-B26). It runs on Windows and Linux, and as firmware on the NUCLEO-H753ZI, NUCLEO-F767ZI, NUCLEO-F439ZI, NUCLEO-F401RE, NUCLEO-F446RE, STM32F4DISCOVERY, NUCLEO-F103RB and NUCLEO-G071RB (lite) boards.

- One launcher, `ucxapp.py`, for Windows, Linux (also WSL) and STM32
- Hardware detection with `--detect`, including NORA modules, GNSS receiver and IMU
- New command modules for the app, GNSS, IMU, cellular and hardware
- Streaming and a terminal UI
- Embedded web page for the module
- STM32 board configurations in `app/boards/`

## Downloads

`ucxapp-<version>.zip` holds every target, with SHA-256 checksums next to each file:

```
windows/ucxapp.exe            signed; run it with the module's COM port
linux/ucxapp                  run it with the serial port, e.g. /dev/ttyUSB0
stm32/stm32_h753/             .bin/.hex/.elf; flash with STM32CubeProgrammer at 0x08000000
stm32/stm32_f767/
stm32/stm32_f439/
stm32/stm32_f407/             STM32F4DISCOVERY
stm32/stm32_f401/
stm32/stm32_f446/
stm32/stm32_f103/             lite build
stm32/stm32_g071/             lite build
```

`ucxapp.exe` (Windows, signed) and `ucxapp` (Linux x64) are also attached on their own.

## Connecting a NORA-W36 to a NUCLEO board

All boards use the Arduino header CN10. The shell runs on the ST-Link virtual COM port (115200 baud).

| NORA-W36 pin | Arduino pin | NUCLEO-H753ZI | NUCLEO-F767ZI / F439ZI | Function |
|---|---|---|---|---|
| TX | D0 (CN10) | PB7 (USART1_RX) | PG9 (USART6_RX) | Board receives |
| RX | D1 (CN10) | PB6 (USART1_TX) | PG14 (USART6_TX) | Board transmits |
| GND | GND (CN10) | GND | GND | Ground |
| VCC | 3V3 (CN10) | 3.3V | 3.3V | Power |

The module UART runs at 115200 baud. Use `bridge` in the shell to talk to the module directly from a PC terminal.
