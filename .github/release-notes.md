## Downloads

| Target | File | How to use |
|---|---|---|
| Windows x64 | `u-connectClientApp-windows-x64.exe` (signed) | Run it, connect to the COM port of the module |
| Linux x64 | `u-connectClientApp-linux-x64` | `chmod +x`, run with the serial port, e.g. `/dev/ttyUSB0` |
| NUCLEO-H753ZI | `ucxapp_nucleo_h753zi.bin/.hex/.elf` | Flash with STM32CubeProgrammer at `0x08000000` |
| NUCLEO-F767ZI | `ucxapp_nucleo_f767zi.bin/.hex/.elf` | Same |
| NUCLEO-F439ZI | `ucxapp_nucleo_f439zi.bin/.hex/.elf` | Same |

## Connecting a NORA-W36 to a NUCLEO board

All boards use the Arduino header CN10. The shell runs on the ST-Link virtual COM port (115200 baud).

| NORA-W36 pin | Arduino pin | NUCLEO-H753ZI | NUCLEO-F767ZI / F439ZI | Function |
|---|---|---|---|---|
| TX | D0 (CN10) | PB7 (USART1_RX) | PG9 (USART6_RX) | Board receives |
| RX | D1 (CN10) | PB6 (USART1_TX) | PG14 (USART6_TX) | Board transmits |
| GND | GND (CN10) | GND | GND | Ground |
| VCC | 3V3 (CN10) | 3.3V | 3.3V | Power |

The module UART runs at 115200 baud. Use `bridge` in the shell to talk to the module directly from a PC terminal.
