# u-connectClientApp - Windows Console Application for u-connectXpress

A native C Windows application for testing and configuring u-connectXpress devices (NORA-W36, NORA-B26).

This repo contains https://github.com/u-blox/u-connectClient a small footprint AT command client for talking to the following u-blox u-connectXpress short-range modules:

* NORA-W36
* NORA-B26

## Prerequisites

### System Requirements
- **Windows 10 or Windows 11** (64-bit)
- **FTDI USB device** (NORA-W36 or NORA-B26 module)

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
git clone https://github.com/u-blox/ucx-windows-app.git
cd ucx-windows-app

# Initialize only the u-connectClient submodule (without recursing into its submodules)
git submodule update --init u-connectClient

# Launch (auto-builds on first run)
.\u-connectClientApp.cmd
```

## Launch Script Commands

```powershell
# Run application (default: auto-build if needed)
.\u-connectClientApp.cmd

# Firmware update mode (goes directly to firmware menu)
.\u-connectClientApp.cmd flash

# Debug build
.\u-connectClientApp.cmd debug

# Update u-connectClient submodule
.\u-connectClientApp.cmd update

# Clean build artifacts
.\u-connectClientApp.cmd clean

# Rebuild from scratch
.\u-connectClientApp.cmd rebuild

# Code sign for distribution
.\u-connectClientApp.cmd sign YOUR_CERT_THUMBPRINT

# Workspace validation
.\u-connectClientApp.cmd selftest
```

## Key Features

- **Bluetooth**: Scan, connect, bond, SPS, GATT Client/Server
- **Wi-Fi** (NORA-W36): Station/AP modes, network scanning, auto-reconnect
- **Network Services**: Socket, HTTP, MQTT, TLS/Security
- **Firmware Updates**: XMODEM protocol with SHA256 verification
- **Profile Management**: Save Wi-Fi and Bluetooth device profiles
- **Dual Menu Modes**: Detailed and compact views
- **Smart Auto-Detection**: FTDI device and COM port discovery

## Command Layer (scriptable, portable)

Besides the menu, the app has a small command layer (`u-connectClientApp/src/ucxapp_*.c`)
with no Windows dependencies, intended to be reused on Linux and embedded targets.

```powershell
# Run commands non-interactively and exit (exit code 1 on first failure)
u-connectClientApp.exe COM13 --exec "device.info" --exec "wifi.scan"
u-connectClientApp.exe COM13 --exec "wifi.connect MySsid MyPassword" --exec "sock.open tcp"

# Interactive command shell (or press [:] in the menu); type 'help'
u-connectClientApp.exe COM13 --shell
```

Commands: `device.info`, `wifi.scan|connect|status|disconnect`,
`sock.open|connect|send|read|close|list`, plus Windows-only `device.connect|disconnect`.
A command is a table entry (`name`, `usage`, `help`, `run()`); see `src/ucxapp_cmds_wifi.c`.
Full reference: [u-connectClientApp/UCXAPP_COMMANDS.md](u-connectClientApp/UCXAPP_COMMANDS.md).

## File Structure

```
u-connectClientApp/
├── u-connectClientApp.cmd      # Launch script
├── u-connectClientApp.ini             # Settings (auto-created)
├── README.md
├── u-connectClientApp/
│   ├── u-connectClientApp.c           # Main application
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
.\u-connectClientApp.cmd

# Code sign (creates u-connectClientApp-signed.exe in release/)
.\u-connectClientApp.cmd sign YOUR_CERT_THUMBPRINT
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
