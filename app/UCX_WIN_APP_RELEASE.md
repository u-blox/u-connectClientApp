# Release Process for u-connectClientApp

## Overview

Releases are **GitHub-only** with signed executables. Users clone the repository to build Debug or custom configurations.

## Version Numbering

**Automatic versioning** based on build date:
- Format: `3.2.0.BUILD` where BUILD = YYMMDD (CalVer format)
- Example: `3.2.0.251204` (December 4, 2025)
- Major.Minor.Patch follows UCX API version (currently 3.2.0)
- Build number updates daily based on current date

Version is generated at build time by CMake from `app/version.h.in`.

## Creating a Release

### 1. Ensure Code is Ready
- [ ] All features tested and working
- [ ] Compiler warnings fixed
- [ ] Documentation updated
- [ ] All changes committed to Git

### 2. Build and Sign Release

**Important:** Always clean build before release to ensure correct version number:

```powershell
# Clean rebuild Release configuration (forces version regeneration)
.\u-connectClientAppWin.cmd rebuild

# Build Release configuration and sign with certificate
.\u-connectClientAppWin.cmd sign EF3FD135F1CD669E0D7F4F2CF14FE1334EECD16E

# This will:
# - Generate version.h with date-based build number (3.2.0.YYMMDD)
# - Build Release configuration
# - Sign with u-blox AG GlobalSign EV CodeSigning certificate
# - Timestamp with DigiCert server (valid until Dec 2027)
# - Verify signature and certificate chain
# - Create u-connectClientApp-signed.exe in app/release/
```

**Verify Version Before Release:**
```powershell
# Check the version number displayed in CMake output
# Should show: "Build version: 3.2.0.YYMMDD (date-based)"

# Or run the executable to verify
.\app\release\u-connectClientApp-signed.exe
# Menu header should show: "Application v3.2.0.YYMMDD"
```

**Note:** The signed executable has ftd2xx64.dll embedded as a resource - no separate DLL needed.

### 3. Create Git Tag
```bash
# Tag format: v3.2.0.BUILD
git tag -a v3.2.0.251204 -m "Release ucx Windows App v3.2.0.251204"
git push origin master
git push origin v3.2.0.251204
```

### 4. Create GitHub Release
Automatic: `.\scripts\release.ps1 -Publish` does steps 2-4 below (`-DryRun` only builds and signs). The tag is `vX.Y.Z.YYMMDD`, with X.Y.Z from `app/CMakeLists.txt`.

Pushing the `v*` tag runs `.github/workflows/release.yml`, which builds Linux and the three STM32 images and creates a **draft** release using `.github/release-notes.md`. The Windows exe is signed locally (the EV certificate is not available to CI):

```powershell
.\u-connectClientAppWin.cmd rebuild
.\u-connectClientAppWin.cmd sign EF3FD135F1CD669E0D7F4F2CF14FE1334EECD16E
gh release upload v3.2.0.251204 "app\release\u-connectClientApp-signed.exe#u-connectClientApp-windows-x64.exe"
```
Then review the draft on GitHub and publish it. The manual steps below are the fallback.

#### Manual alternative
1. Go to: https://github.com/u-blox/u-connectClientApp/releases
2. Click "Draft a new release"
3. **Tag:** v3.2.0.251204 (must match Git tag)
4. **Title:** u-connectClientApp v3.2.0.251204
4. **Description:** Write release notes including:
   ```markdown
   ## Features
   - Bluetooth: Scan, connect, GATT client/server, SPS
   - Wi-Fi: Station/AP mode, HTTP, MQTT, NTP, location services
   - Firmware update via XMODEM with SHA256 verification
   - Note: HID over GATT (HoG) keyboard is experimental
   
   ## Requirements
   - Windows 10/11 (64-bit)
   - NORA-W36 or NORA-B26 module
   - USB connection (auto-detects EVK boards)
   
   ## SHA256 Checksum
   sha256:PASTE_HASH_HERE_AFTER_CALCULATING
   
   ## Installation
   Download u-connectClientApp-signed.exe and run - no installation required.
   FTDI drivers are embedded in the executable.
   ```
6. **Attach file:** `u-connectClientApp-signed.exe` (from app/release/)
7. **Calculate SHA256** and add to release notes:
   ```powershell
   Get-FileHash app\release\u-connectClientApp-signed.exe -Algorithm SHA256
   ```
8. Click "Publish release"

## After Release

Users who need Debug builds or modifications:
```bash
# Clone repository with submodule
git clone --recurse-submodules https://github.com/u-blox/u-connectClientApp.git
cd u-connectClientApp

# Build and run (auto-builds if needed)
.\u-connectClientAppWin.cmd

# Build Debug configuration
.\u-connectClientAppWin.cmd debug

# Build all configurations
.\u-connectClientAppWin.cmd all
```

## Directory Structure

```
u-connectClientApp/
├── u-connectClientAppWin.cmd              (Launch script)
├── u-connectClientApp.ini                     (Settings, auto-created, gitignored)
├── app/
│   ├── src/                               (Command layer and entry point)
│   ├── u-connectClientApp.rc              (Resource file - icon, DLL, version)
│   ├── version.h.in                    (Version template)
│   ├── CMakeLists.txt                  (Build configuration)
│   ├── bin/
│   │   ├── u-connectClientApp.exe         (Release, unsigned, gitignored)
│   │   └── u-connectClientApp-debug.exe   (Debug, gitignored)
│   └── release/
│       └── u-connectClientApp-signed.exe  (Signed release, tracked in git)
├── u-connectClient/                              (Git submodule)
│   ├── src/                            (UCX client library source)
│   ├── inc/                            (UCX client library headers)
│   └── ucx_api/                        (UCX API definitions)
└── build/
    ├── u-connectClientApp.sln                 (Visual Studio solution)
    ├── u-connectClientApp.vcxproj             (Visual Studio project)
    └── version.h                        (Generated by CMake, gitignored)
```

**Only signed releases** go to GitHub. Everything else is built from source.

## Release Checklist

- [ ] All tests passing
- [ ] Version reflects current date (YYMMDD format)
- [ ] Built and signed with certificate
- [ ] SHA256 hash calculated and added to release notes
- [ ] Git tag created and pushed
- [ ] GitHub release created with signed executable
- [ ] Release notes include SHA256 hash in format: `sha256:HASH`

