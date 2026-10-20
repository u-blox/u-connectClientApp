# Building and deploying u-connectClientApp

Technical reference for building, flashing, signing and releasing. For an overview and examples see [README.md](README.md).

## Prerequisites

- Windows 10/11 (64-bit) for the Windows build; Linux for the Linux build; an STM32 Nucleo board for the firmware build
- FTDI USB device or a UART connection to a NORA module (NORA-W36 or NORA-B26)

Windows build tools:
1. Visual Studio 2022 Build Tools with the "Desktop development with C++" workload: https://aka.ms/vs/17/release/vs_BuildTools.exe
2. CMake 3.15 or later (on PATH): https://cmake.org/download/
3. Git: https://git-scm.com/download/win

STM32 builds additionally need the Arm GNU toolchain, Ninja and STM32CubeProgrammer.

## Get the sources

```powershell
git clone https://github.com/u-blox/u-connectClientApp.git
cd u-connectClientApp

# Only the u-connectClient submodule (without recursing into its submodules)
git submodule update --init u-connectClient
```

## Launchers

| Target | Launcher |
|---|---|
| Windows | `.\u-connectClientAppWin.cmd` |
| Linux | `./u-connectClientAppLinux.sh` |
| STM32 | `.\u-connectClientAppSTM32.cmd` |

### Windows

```powershell
.\u-connectClientAppWin.cmd            # build if needed and run (auto-detects a NORA EVK)
.\u-connectClientAppWin.cmd debug      # debug build
.\u-connectClientAppWin.cmd update     # update the u-connectClient submodule
.\u-connectClientAppWin.cmd clean      # clean build artifacts
.\u-connectClientAppWin.cmd rebuild    # rebuild from scratch
.\u-connectClientAppWin.cmd sign YOUR_CERT_THUMBPRINT
.\u-connectClientAppWin.cmd selftest   # workspace validation
```

Manual build:

```powershell
cmake -S app -B build/windows
cmake --build build/windows --config Release
```

### STM32 (NUCLEO-H753ZI, NUCLEO-F767ZI, NUCLEO-F439ZI)

```powershell
# Build, flash over the ST-Link and open the console on its virtual COM port (default board: H753)
.\u-connectClientAppSTM32.cmd --build --flash --log

# Select another board: h753 (default), f767 or f439
.\u-connectClientAppSTM32.cmd --board f767 --build --flash --log

# Other options: --rebuild, --clean, --erase, --recover, --log COMx, --help
```

The module is wired to USART1 (H753) or USART6 (F767, F439); the shell runs on the ST-Link VCP. `bridge [baud]` turns the board into a
transparent console-to-module bridge (for AT debugging or XMODEM firmware update); press Ctrl-] with 1 s of silence around it to return to the shell.

The build needs the matching STM32Cube HAL (`STM32CUBEH7_PATH`, `STM32CUBEF7_PATH` or `STM32CUBEF4_PATH`, or the u-connectClient submodule).
Release builds of all three boards are published with each tag.

## File structure

```
u-connectClientApp/
├── u-connectClientAppWin.cmd      # Windows launch script
├── u-connectClientAppLinux.sh     # Linux launch script
├── u-connectClientAppSTM32.cmd    # STM32 build/flash/log script
├── app/
│   ├── src/                        # Command layer and entry point (ucxapp_main.c)
│   ├── stm32/                      # STM32 firmware entry, platform and CMake
│   ├── CMakeLists.txt
│   ├── bin/                        # Build outputs (gitignored)
│   ├── release/                    # Signed releases
│   └── third-party/                # FTDI DLL, Bluetooth SIG, etc.
├── u-connectClient/                # Git submodule: AT client library
└── build/                          # CMake artifacts per target (gitignored)
```

## Architecture notes

- The command layer (`app/src/ucxapp_*.c`) has no OS dependencies and is shared by Windows, Linux and STM32.
- Host services (COM-port discovery, HTTPS download, key input) sit behind `app/src/ucxapp_platform.h`; the Windows backend is `ucxapp_platform_win.c`.
- A command is a table entry (`name`, `usage`, `help`, `run()`); see `app/src/ucxapp_cmds_wifi.c`.
- The web UI engine is `app/src/u_webserver.c`; routes are in `app/src/ucxapp_cmds_web.c`. Routes: `GET /`, `GET /events` (SSE), `POST /api/exec?cmd=...`.
- Full command reference: [COMMANDS.md](COMMANDS.md).

## Production deployment

```powershell
.\u-connectClientAppWin.cmd                                  # release build
.\u-connectClientAppWin.cmd sign YOUR_CERT_THUMBPRINT        # creates u-connectClientApp-signed.exe in release/
```

`app/release/u-connectClientApp-signed.exe` is self-contained (~2.5 MB) with the FTDI DLL embedded.
See [app/UCX_WIN_APP_RELEASE.md](app/UCX_WIN_APP_RELEASE.md) and `scripts/release.py` for the full release flow.
