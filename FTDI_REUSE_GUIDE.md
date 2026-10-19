# FTDI D2XX: Embedded DLL Extraction & COM Port Detection

Reusable patterns from ucx-windows-app for any Windows application using FTDI USB-serial devices.

---

## 1. Embed FTDI DLL as Win32 Resource

**Resource file (.rc):**
```rc
// Resource ID 101, type RT_RCDATA
IDR_FTD2XX_DLL 101 "third-party/ftdi/ftd2xx64.dll"
```

**resource.h:**
```c
#define IDR_FTD2XX_DLL 101
```

This embeds the DLL inside the exe so users don't need a separate DLL file.

---

## 2. Extract DLL with Size-Based Caching

Extracts the embedded DLL to `%TEMP%` on first run, skips on subsequent runs if the file already exists with the correct size.

```c
#include <windows.h>

static HMODULE gFtd2xxModule = NULL;

static bool extractAndLoadFtd2xx(void)
{
    char dllPath[MAX_PATH] = "";
    bool dllExtracted = false;

    // --- Step 1: Extract from embedded resource ---
    HRSRC hResource = FindResource(NULL, MAKEINTRESOURCE(101), RT_RCDATA);
    if (hResource) {
        HGLOBAL hGlobal = LoadResource(NULL, hResource);
        if (hGlobal) {
            void *pData = LockResource(hGlobal);
            DWORD dwSize = SizeofResource(NULL, hResource);

            if (pData && dwSize > 0) {
                GetTempPathA(MAX_PATH, dllPath);
                strcat_s(dllPath, MAX_PATH, "ftd2xx64_embedded.dll");

                // Cache check: skip extraction if file exists with correct size
                WIN32_FILE_ATTRIBUTE_DATA fileAttr;
                bool needsExtract = true;
                if (GetFileAttributesExA(dllPath, GetFileExInfoStandard, &fileAttr)) {
                    if (fileAttr.nFileSizeLow == dwSize && fileAttr.nFileSizeHigh == 0) {
                        needsExtract = false;
                        dllExtracted = true;
                    }
                }

                if (needsExtract) {
                    HANDLE hFile = CreateFileA(dllPath, GENERIC_WRITE, 0, NULL,
                                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                    if (hFile != INVALID_HANDLE_VALUE) {
                        DWORD written;
                        if (WriteFile(hFile, pData, dwSize, &written, NULL) &&
                            written == dwSize) {
                            dllExtracted = true;
                        }
                        CloseHandle(hFile);
                    }
                }
            }
        }
    }

    // --- Step 2: Load extracted DLL ---
    if (dllExtracted) {
        gFtd2xxModule = LoadLibraryA(dllPath);
    }

    // --- Step 3: Fallback to system/local paths ---
    if (!gFtd2xxModule) gFtd2xxModule = LoadLibraryA("ftd2xx64.dll");
    if (!gFtd2xxModule) gFtd2xxModule = LoadLibraryA("ftd2xx.dll");
    if (!gFtd2xxModule) {
        // Try next to the executable
        char exeDir[MAX_PATH];
        GetModuleFileNameA(NULL, exeDir, MAX_PATH);
        char *lastSlash = strrchr(exeDir, '\\');
        if (lastSlash) {
            *(lastSlash + 1) = '\0';
            char tryPath[MAX_PATH];
            snprintf(tryPath, MAX_PATH, "%sftd2xx64.dll", exeDir);
            gFtd2xxModule = LoadLibraryA(tryPath);
            if (!gFtd2xxModule) {
                snprintf(tryPath, MAX_PATH, "%sftd2xx.dll", exeDir);
                gFtd2xxModule = LoadLibraryA(tryPath);
            }
        }
    }

    return gFtd2xxModule != NULL;
}
```

---

## 3. Dynamic Function Loading

Load D2XX functions at runtime (no link-time dependency on ftd2xx.lib):

```c
typedef PVOID  FT_HANDLE;
typedef ULONG  FT_STATUS;
#define FT_OK  0
#define FT_LIST_NUMBER_ONLY  0x80000000
#define FT_OPEN_BY_SERIAL_NUMBER 1
#define FT_OPEN_BY_DESCRIPTION   2

typedef FT_STATUS (WINAPI *PFN_FT_ListDevices)(PVOID, PVOID, DWORD);
typedef FT_STATUS (WINAPI *PFN_FT_Open)(int, FT_HANDLE*);
typedef FT_STATUS (WINAPI *PFN_FT_OpenEx)(PVOID, DWORD, FT_HANDLE*);
typedef FT_STATUS (WINAPI *PFN_FT_GetComPortNumber)(FT_HANDLE, LPLONG);
typedef FT_STATUS (WINAPI *PFN_FT_Close)(FT_HANDLE);

static PFN_FT_ListDevices       gpFT_ListDevices;
static PFN_FT_Open              gpFT_Open;
static PFN_FT_OpenEx            gpFT_OpenEx;
static PFN_FT_GetComPortNumber  gpFT_GetComPortNumber;
static PFN_FT_Close             gpFT_Close;

static bool resolveFtd2xxFunctions(void)
{
    if (!gFtd2xxModule) return false;

    gpFT_ListDevices      = (PFN_FT_ListDevices)     GetProcAddress(gFtd2xxModule, "FT_ListDevices");
    gpFT_Open             = (PFN_FT_Open)            GetProcAddress(gFtd2xxModule, "FT_Open");
    gpFT_OpenEx           = (PFN_FT_OpenEx)          GetProcAddress(gFtd2xxModule, "FT_OpenEx");
    gpFT_GetComPortNumber = (PFN_FT_GetComPortNumber)GetProcAddress(gFtd2xxModule, "FT_GetComPortNumber");
    gpFT_Close            = (PFN_FT_Close)           GetProcAddress(gFtd2xxModule, "FT_Close");

    if (!gpFT_ListDevices || !gpFT_Open || !gpFT_GetComPortNumber || !gpFT_Close) {
        FreeLibrary(gFtd2xxModule);
        gFtd2xxModule = NULL;
        return false;
    }
    return true;
}
```

---

## 4. COM Port Detection via USB Device Tree Walk

When the FTDI VCP driver is installed, D2XX cannot open devices and SetupAPI only shows "USB Serial Port". This walks up the USB device tree to find the real device name from `BusReportedDeviceDesc`.

**Required headers and libs:**
```c
#include <setupapi.h>
#include <cfgmgr32.h>
#include <devpropdef.h>
#include <devguid.h>    // GUID_DEVCLASS_PORTS
#include <regstr.h>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")

// Define manually to avoid INITGUID linker issues with devpkey.h
static const DEVPROPKEY MY_DEVPKEY_BusReportedDeviceDesc = {
    {0x540b947e, 0x8b40, 0x45bc,
     {0xa8, 0xa2, 0x6a, 0x0b, 0x89, 0x4c, 0xbd, 0xa2}}, 4
};
```

**Full detection function:**
```c
// Returns true if device info was found for the given COM port.
// deviceName: receives real device name (e.g. "EVK-NORA-W36")
// portFunction: receives port function (e.g. "AT", "LOG") for multi-channel FTDI
static bool getDeviceInfoForComPort(const char *portName,
                                     char *deviceName, size_t deviceNameSize,
                                     char *portFunction, size_t portFunctionSize)
{
    deviceName[0] = '\0';
    portFunction[0] = '\0';
    bool found = false;

    HDEVINFO devInfoSet = SetupDiGetClassDevs(
        &GUID_DEVCLASS_PORTS, NULL, NULL, DIGCF_PRESENT);
    if (devInfoSet == INVALID_HANDLE_VALUE) return false;

    SP_DEVINFO_DATA devInfoData;
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(devInfoSet, i, &devInfoData); i++) {
        // Read the registry port name (e.g. "COM22")
        HKEY hKey = SetupDiOpenDevRegKey(devInfoSet, &devInfoData,
                                          DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
        if (hKey == INVALID_HANDLE_VALUE) continue;

        char regPortName[16] = "";
        DWORD regSize = sizeof(regPortName);
        DWORD regType;
        RegQueryValueExA(hKey, "PortName", NULL, &regType,
                         (LPBYTE)regPortName, &regSize);
        RegCloseKey(hKey);

        if (_stricmp(regPortName, portName) != 0) continue;

        // Get the friendly name from registry
        char friendlyName[256] = "";
        DWORD fnSize = sizeof(friendlyName);
        SetupDiGetDeviceRegistryPropertyA(devInfoSet, &devInfoData,
            SPDRP_FRIENDLYNAME, NULL, (PBYTE)friendlyName, fnSize, &fnSize);

        // Default: use friendly name
        strncpy_s(deviceName, deviceNameSize, friendlyName, _TRUNCATE);

        // If generic "USB Serial Port", walk up the device tree
        if (strstr(friendlyName, "USB Serial Port")) {
            DEVINST currentInst = devInfoData.DevInst;

            for (int level = 0; level < 3; level++) {
                DEVINST parentInst;
                if (CM_Get_Parent(&parentInst, currentInst, 0) != CR_SUCCESS)
                    break;

                WCHAR busDesc[256];
                ULONG busDescSize = sizeof(busDesc);
                DEVPROPTYPE propType;

                if (CM_Get_DevNode_PropertyW(parentInst,
                        &MY_DEVPKEY_BusReportedDeviceDesc,
                        &propType, (PBYTE)busDesc, &busDescSize, 0) == CR_SUCCESS
                    && propType == DEVPROP_TYPE_STRING) {

                    // Convert wide → narrow
                    char busDescA[256];
                    WideCharToMultiByte(CP_UTF8, 0, busDesc, -1,
                                        busDescA, sizeof(busDescA), NULL, NULL);

                    // Check for your known device identifiers
                    if (strstr(busDescA, "NORA") || strstr(busDescA, "EVK") ||
                        strstr(busDescA, "FTDI") /* add your own */) {
                        strncpy_s(deviceName, deviceNameSize, busDescA, _TRUNCATE);

                        // Extract FTDI channel letter for port function
                        // Instance ID: FTDIBUS\VID_0403+PID_6011+SERIAL_A\0000
                        //                                   channel ─────┘
                        char instanceId[256];
                        DWORD idSize = sizeof(instanceId);
                        if (SetupDiGetDeviceInstanceIdA(devInfoSet, &devInfoData,
                                instanceId, idSize, &idSize)) {
                            char *bs = strrchr(instanceId, '\\');
                            if (bs && bs > instanceId) {
                                char ch = *(bs - 1);
                                // FT4232H: A=port0, B=port1, C=port2, D=port3
                                // Customize mapping for your hardware:
                                if (ch == 'A')
                                    strncpy_s(portFunction, portFunctionSize, "AT", _TRUNCATE);
                                else if (ch == 'C')
                                    strncpy_s(portFunction, portFunctionSize, "LOG", _TRUNCATE);
                                else {
                                    char label[4] = { 'C', 'H', ch, '\0' };
                                    strncpy_s(portFunction, portFunctionSize, label, _TRUNCATE);
                                }
                            }
                        }
                        break;
                    }
                }
                currentInst = parentInst;
            }
        }

        found = true;
        break;
    }

    SetupDiDestroyDeviceInfoList(devInfoSet);
    return found;
}
```

---

## 5. Enumerate All COM Ports

```c
static void listAllComPorts(void)
{
    HDEVINFO devInfoSet = SetupDiGetClassDevs(
        &GUID_DEVCLASS_PORTS, NULL, NULL, DIGCF_PRESENT);
    if (devInfoSet == INVALID_HANDLE_VALUE) return;

    SP_DEVINFO_DATA devInfoData;
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(devInfoSet, i, &devInfoData); i++) {
        HKEY hKey = SetupDiOpenDevRegKey(devInfoSet, &devInfoData,
                                          DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
        if (hKey == INVALID_HANDLE_VALUE) continue;

        char portName[16] = "";
        DWORD regSize = sizeof(portName);
        DWORD regType;
        RegQueryValueExA(hKey, "PortName", NULL, &regType,
                         (LPBYTE)portName, &regSize);
        RegCloseKey(hKey);

        if (strncmp(portName, "COM", 3) != 0) continue;

        char deviceName[256], portFunc[16];
        getDeviceInfoForComPort(portName, deviceName, sizeof(deviceName),
                                portFunc, sizeof(portFunc));

        if (portFunc[0])
            printf("  %s = %s [%s]\n", portName, deviceName, portFunc);
        else
            printf("  %s = %s\n", portName, deviceName);
    }

    SetupDiDestroyDeviceInfoList(devInfoSet);
}
```

**Example output:**
```
  COM22 = EVK-NORA-W36 [AT]
  COM23 = EVK-NORA-W36 [LOG]
  COM1  = Communications Port
```

---

## Notes

- **Why not use `devpkey.h` directly?** Including `devpkey.h` requires `INITGUID` to be defined before `windows.h` in exactly one translation unit, or you get linker errors (`LNK2001: unresolved external symbol`). Defining the DEVPROPKEY manually avoids this.
- **Why walk the device tree?** When the FTDI VCP driver is installed, `FT_OpenEx()` returns `FT_DEVICE_NOT_FOUND`, and the SetupAPI friendly name is just "USB Serial Port (COMxx)". The real device name (from FTDI EEPROM) is only available as `BusReportedDeviceDesc` on the parent USB composite device.
- **Channel mapping is hardware-specific.** FT4232H has 4 channels (A-D). The mapping (A=AT, C=LOG) is specific to u-blox EVK boards. Adjust for your hardware.
