# ucx-windows-app Build Instructions

## Project Overview

ucx-windows-app is a Windows console application for testing u-connectXpress devices (NORA-W36, NORA-B26). It uses the ucxclient library as a git submodule.

## Building

### Quick Build (Recommended)
```powershell
.\launch-ucx-windows-app.cmd
```
This auto-builds if needed and launches the application.

### Build Commands
```powershell
# Standard Release build
.\launch-ucx-windows-app.cmd

# Debug build
.\launch-ucx-windows-app.cmd debug

# Clean build artifacts
.\launch-ucx-windows-app.cmd clean

# Full rebuild from scratch
.\launch-ucx-windows-app.cmd rebuild
```

### Manual CMake Build
```powershell
cmake -S ucx-windows-app -B build -A x64
cmake --build build --config Release
```

**DO NOT USE:**
- Direct Visual Studio builds without CMake configuration
- Building from wrong directory (must be from repo root)

## Project Structure

- `ucx-windows-app/` - Main application source
  - `ucx-windows-app.c` - Monolithic application code
  - `CMakeLists.txt` - Build configuration
  - `bin/` - Build outputs (gitignored)
  - `release/` - Signed releases
  - `third-party/` - FTDI DLL, QR code generator
- `ucxclient/` - AT command client library (git submodule)
- `build/` - CMake artifacts (gitignored)

## Submodule Management

```powershell
# Initialize submodule (first time)
git submodule update --init ucxclient

# Update submodule to latest
.\launch-ucx-windows-app.cmd update
```

## Code Style

- C11 standard
- MSVC compiler with /W4 warning level
- Functions prefixed with module name (e.g., `uCxAtClient_`)
- Use `_CRT_SECURE_NO_WARNINGS` for Windows APIs

## Testing

```powershell
# Run application with self-test
.\launch-ucx-windows-app.cmd selftest

# Firmware update mode
.\launch-ucx-windows-app.cmd flash
```

## Release Process

1. Clean rebuild: `.\launch-ucx-windows-app.cmd rebuild`
2. Sign with certificate: `.\launch-ucx-windows-app.cmd sign CERT_THUMBPRINT`
3. Create git tag: `git tag -a v3.2.0.YYMMDD -m "Release message"`
4. Push tag: `git push origin v3.2.0.YYMMDD`

## Version Numbering

- Format: `MAJOR.MINOR.PATCH.BUILD` (e.g., 3.2.0.260219)
- BUILD = YYMMDD (CalVer, auto-generated from build date)
- Major.Minor.Patch follows UCX API version

## Platform Requirements

- Windows 10/11 (64-bit)
- Visual Studio 2022 Build Tools
- CMake 3.15+
- FTDI USB device (NORA-W36 or NORA-B26)
