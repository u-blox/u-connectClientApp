# ucx-windows-app - Windows Console Application for u-connectXpress

A native C Windows application for testing and configuring u-connectXpress devices (NORA-W36, NORA-B26).

This repo contains https://github.com/u-blox/ucxclient a small footprint AT command client for talking to the following u-blox u-connectXpress short-range modules:

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

# Initialize only the ucxclient submodule (without recursing into its submodules)
git submodule update --init ucxclient

# Launch (auto-builds on first run)
.\launch-ucx-windows-app.cmd
```

## Launch Script Commands

```powershell
# Run application (default: auto-build if needed)
.\launch-ucx-windows-app.cmd

# Debug build
.\launch-ucx-windows-app.cmd debug

# Update ucxclient submodule
.\launch-ucx-windows-app.cmd update

# Clean build artifacts
.\launch-ucx-windows-app.cmd clean

# Rebuild from scratch
.\launch-ucx-windows-app.cmd rebuild

# Code sign for distribution
.\launch-ucx-windows-app.cmd sign YOUR_CERT_THUMBPRINT

# Workspace validation
.\launch-ucx-windows-app.cmd selftest
```

## Key Features

- **Bluetooth**: Scan, connect, bond, SPS, GATT Client/Server
- **Wi-Fi** (NORA-W36): Station/AP modes, network scanning, auto-reconnect
- **Network Services**: Socket, HTTP, MQTT, TLS/Security
- **Firmware Updates**: XMODEM protocol with SHA256 verification
- **Profile Management**: Save Wi-Fi and Bluetooth device profiles
- **Dual Menu Modes**: Detailed and compact views
- **Smart Auto-Detection**: FTDI device and COM port discovery

## File Structure

```
ucx-windows-app/
├── launch-ucx-windows-app.cmd      # Launch script
├── ucx-windows-app.ini             # Settings (auto-created)
├── README.md
├── ucx-windows-app/
│   ├── ucx-windows-app.c           # Main application
│   ├── CMakeLists.txt
│   ├── bin/                        # Build outputs (gitignored)
│   ├── release/                    # Signed releases
│   └── third-party/                # FTDI DLL, Bluetooth SIG, etc.
├── ucxclient/                      # Git submodule
│   ├── src/                        # UCX client library
│   ├── inc/                        # Headers
│   ├── ucx_api/                    # UCX API definitions
│   └── ports/                      # Platform ports
└── build/                          # CMake artifacts (gitignored)
```

## Manual Building

```powershell
# Using CMake
cmake -S ucx-windows-app -B build
cmake --build build --config Release

# Using Visual Studio
# Open build/ucx-windows-app.sln
```

## Production Deployment

```powershell
# Build release version
.\launch-ucx-windows-app.cmd

# Code sign (creates ucx-windows-app-signed.exe in release/)
.\launch-ucx-windows-app.cmd sign YOUR_CERT_THUMBPRINT
```

**Distribution**: `ucx-windows-app/release/ucx-windows-app-signed.exe` is self-contained (~2.5 MB) with FTDI DLL embedded.

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
