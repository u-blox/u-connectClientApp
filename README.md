# u-connectClientApp - Command Shell for u-connectXpress (Windows, Linux, STM32)

A native C command-line application for testing and configuring u-connectXpress devices (NORA-W36, NORA-B26). It runs on Windows and Linux hosts and as FreeRTOS firmware on STM32 Nucleo boards (NUCLEO-H753ZI, NUCLEO-F767ZI, NUCLEO-F439ZI).

This repo contains https://github.com/u-blox/u-connectClient a small footprint AT command client for talking to the following u-blox u-connectXpress short-range modules:

* NORA-W36
* NORA-B26

## Prerequisites

### System Requirements
- **Windows 10 or Windows 11** (64-bit) for the Windows build; Linux for the Linux build; an STM32 Nucleo board for the firmware build
- **FTDI USB device** or a UART connection to a NORA module (NORA-W36 or NORA-B26)

### Required Software
1. **Visual Studio 2022 Build Tools** (or full Visual Studio 2022)
   - Download: https://aka.ms/vs/17/release/vs_BuildTools.exe
   - Install "Desktop development with C++" workload

2. **CMake 3.15 or later**
   - Download: https://cmake.org/download/
   - Add to system PATH during installation

3. **Git**
   - Download: https://git-scm.com/download/win

## Quick Start

```powershell
# Clone main repository only
git clone https://github.com/u-blox/u-connectClientApp.git
cd u-connectClientApp

# Initialize only the u-connectClient submodule (without recursing into its submodules)
git submodule update --init u-connectClient

# Launch (auto-builds on first run)
.\u-connectClientAppWin.cmd
```

## Launch Script Commands

```powershell
# Run the command shell (auto-detects a NORA EVK)
.\u-connectClientAppWin.cmd

# Debug build
.\u-connectClientAppWin.cmd debug

# Update u-connectClient submodule
.\u-connectClientAppWin.cmd update

# Clean build artifacts
.\u-connectClientAppWin.cmd clean

# Rebuild from scratch
.\u-connectClientAppWin.cmd rebuild

# Code sign for distribution
.\u-connectClientAppWin.cmd sign YOUR_CERT_THUMBPRINT

# Workspace validation
.\u-connectClientAppWin.cmd selftest
```

Launchers per target: `u-connectClientAppWin.cmd` (Windows), `u-connectClientAppLinux.sh` (Linux), `u-connectClientAppSTM32.cmd` (STM32 firmware, see below).

## STM32 (NUCLEO-H753ZI, NUCLEO-F767ZI, NUCLEO-F439ZI)

```powershell
# Build, flash over the ST-Link and open the console on its virtual COM port (default board: H753)
.\u-connectClientAppSTM32.cmd --build --flash --log

# Select another board: h753 (default), f767 or f439
.\u-connectClientAppSTM32.cmd --board f767 --build --flash --log

# Other options: --rebuild, --clean, --erase, --recover, --log COMx, --help
```

The module is wired to USART1 (H753) or USART6 (F767, F439); the shell runs on the ST-Link VCP. `bridge [baud]` turns the board into a
transparent console-to-module bridge (e.g. for AT debugging or XMODEM firmware update); press Ctrl-] with
1 s of silence around it to return to the shell. Needs the matching STM32Cube HAL (`STM32CUBEH7_PATH`, `STM32CUBEF7_PATH` or
`STM32CUBEF4_PATH`, or the u-connectClient submodule). Release builds of all three boards are published with each tag.

## Key Features

- **Bluetooth**: Scan, connect, bond, SPS, GATT Client/Server
- **Wi-Fi** (NORA-W36): Station/AP modes, network scanning, auto-reconnect
- **Network Services**: Socket, HTTP, MQTT, TLS/Security
- **Firmware Updates**: XMODEM protocol with SHA256 verification (`fw.download`, `fw.flash`)
- **Smart Auto-Detection**: FTDI device and COM port discovery

## Command Layer (scriptable, portable)

The app is a command layer (`u-connectClientApp/src/ucxapp_*.c`)
with no Windows dependencies, shared by the Windows and Linux builds.

```powershell
# Run commands non-interactively and exit (exit code 1 on first failure)
u-connectClientApp.exe COM13 --exec "device.info" --exec "wifi.scan"
u-connectClientApp.exe COM13 --exec "wifi.connect MySsid MyPassword" --exec "sock.open tcp"

# Interactive command shell; type 'help'
u-connectClientApp.exe COM13 --shell
```

### Web UI (hosted on the module)

```powershell
# The NORA-W36 serves the page over Wi-Fi (TCP listener); anyone on the network can open it
u-connectClientApp.exe COM13 --exec "wifi.connect MySsid MyPassword" --exec "web.start" --shell
```

`web.start [port] [token]` prints `http://<module-ip>:<port>/?t=<token>` (default port 80). The token is
required for every request; a random one is generated unless you pass it. The page has a command box and
live output; the app must keep running (shell or `--shell`) because commands run on the PC.
Routes: `GET /`, `GET /events` (SSE: `out`, `done`), `POST /api/exec?cmd=...`. `web.stop`, `web.status`.
The HTTP engine is `src/u_webserver.c` from u-connectMatter; routes are in `src/ucxapp_cmds_web.c`.

Commands: `device.info`, `wifi.scan|connect|status|disconnect`,
`sock.open|connect|send|read|close|list`, `net.ping|dns`, `http.get`, `mqtt.*`, `bt.*` (incl. `bt.profile.*`), `wifi.profile.*`, `at`, plus Windows-only `device.connect|disconnect`.
A command is a table entry (`name`, `usage`, `help`, `run()`); see `src/ucxapp_cmds_wifi.c`.

Host-OS services (COM-port discovery, HTTPS download, key input) sit behind `src/ucxapp_platform.h`,

with the Windows backend in `src/ucxapp_platform_win.c`.
Full reference: [u-connectClientApp/UCXAPP_COMMANDS.md](u-connectClientApp/UCXAPP_COMMANDS.md).

## File Structure

```
u-connectClientApp/
├── u-connectClientAppWin.cmd      # Windows launch script
├── u-connectClientAppLinux.sh     # Linux launch script
├── u-connectClientAppSTM32.cmd    # STM32 build/flash/log script
├── README.md
├── u-connectClientApp/
│   ├── src/                           # Command layer and entry point (ucxapp_main.c)
│   ├── stm32/                      # STM32 firmware (NUCLEO-H753ZI) entry, platform and CMake
│   ├── CMakeLists.txt
│   ├── bin/                        # Build outputs (gitignored)
│   ├── release/                    # Signed releases
│   └── third-party/                # FTDI DLL, Bluetooth SIG, etc.
├── u-connectClient/                      # Git submodule
│   ├── src/                        # UCX client library
│   ├── inc/                        # Headers
│   ├── ucx_api/                    # UCX API definitions
│   └── ports/                      # Platform ports
└── build/                          # CMake artifacts (gitignored)
```

## Manual Building

```powershell
# Using CMake
cmake -S u-connectClientApp -B build
cmake --build build --config Release

# Using Visual Studio
# Open build/u-connectClientApp.sln
```

## Production Deployment

```powershell
# Build release version
.\u-connectClientAppWin.cmd

# Code sign (creates u-connectClientApp-signed.exe in release/)
.\u-connectClientAppWin.cmd sign YOUR_CERT_THUMBPRINT
```

**Distribution**: `u-connectClientApp/release/u-connectClientApp-signed.exe` is self-contained (~2.5 MB) with FTDI DLL embedded.

---

**Version**: 3.2.0 (aligned with UCX API)  
**Platform**: Windows 10/11 (64-bit)  



# Disclaimer
Copyright &#x00a9; u-blox

u-blox reserves all rights in this deliverable (documentation, software, etc.,
hereafter “Deliverable”).

u-blox grants you the right to use, copy, modify and distribute the
Deliverable provided hereunder for any purpose without fee.

THIS DELIVERABLE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR IMPLIED
WARRANTY. IN PARTICULAR, NEITHER THE AUTHOR NOR U-BLOX MAKES ANY
REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE MERCHANTABILITY OF THIS
DELIVERABLE OR ITS FITNESS FOR ANY PARTICULAR PURPOSE.

In case you provide us a feedback or make a contribution in the form of a
further development of the Deliverable (“Contribution”), u-blox will have the
same rights as granted to you, namely to use, copy, modify and distribute the
Contribution provided to us for any purpose without fee.
