#include "ucxapp_cmd.h"
#include "ucxapp_platform.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4200 4201)
#endif
#include "u_cx_general.h"
#include "u_cx_security.h"
#include "u_cx_system.h"
#include "u_port.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

static int cmdDeviceInfo(ucxapp_t *app, int argc, char *argv[])
{
    const char *text = NULL;
    (void)argc;
    (void)argv;

    if (uCxGeneralGetManufacturerIdentificationBegin(app->ucx, &text) && text != NULL) {
        ucxapp_print("Manufacturer:     %s\n", text);
    }
    uCxEnd(app->ucx);

    text = NULL;
    if (uCxGeneralGetDeviceModelIdentificationBegin(app->ucx, &text) && text != NULL) {
        ucxapp_print("Model:            %s\n", text);
    }
    uCxEnd(app->ucx);

    text = NULL;
    if (uCxGeneralGetSoftwareVersionBegin(app->ucx, &text) && text != NULL) {
        ucxapp_print("Firmware Version: %s\n", text);
    }
    uCxEnd(app->ucx);

    return UCXAPP_OK;
}

static int cmdDeviceStatus(ucxapp_t *app, int argc, char *argv[])
{
    char ssid[64], ip[48];
    int32_t handles[16];
    int certs = 0;
    uCxSecListCertificates_t cert;

    cmdDeviceInfo(app, argc, argv);
    if (ucxapp_wifi_get_status(app->ucx, ssid, sizeof(ssid), ip, sizeof(ip))) {
        ucxapp_print("Wi-Fi:            connected to '%s', IP %s\n", ssid, ip);
    } else {
        ucxapp_print("Wi-Fi:            not connected\n");
    }
    ucxapp_print("Sockets:          %d open\n", ucxapp_handle_list(UCXAPP_HANDLE_SOCK, handles, 16));
    ucxapp_print("BLE connections:  %d\n", ucxapp_handle_list(UCXAPP_HANDLE_CONN, handles, 16));
    uCxSecurityListCertificatesBegin(app->ucx);
    while (uCxSecurityListCertificatesGetNext(app->ucx, &cert)) {
        certs++;
    }
    uCxEnd(app->ucx);
    ucxapp_print("Certificates:     %d\n", certs);
    return UCXAPP_OK;
}

static int cmdDeviceReboot(ucxapp_t *app, int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    int32_t r = uCxSystemReboot(app->ucx);
    if (r != 0) {
        ucxapp_print("ERROR: reboot failed (%d%s)\n", (int)r, ucxapp_errname(r));
        return UCXAPP_ERR_FAILED;
    }
    U_CX_PORT_SLEEP_MS(2000);
    ucxapp_print("module rebooted\n");
    return UCXAPP_OK;
}

static int cmdDeviceFactory(ucxapp_t *app, int argc, char *argv[])
{
    if (argc != 2 || strcmp(argv[1], "confirm") != 0) {
        return UCXAPP_ERR_USAGE;
    }
    int32_t r = uCxSystemFactoryReset(app->ucx);
    if (r != 0) {
        ucxapp_print("ERROR: factory reset failed (%d%s)\n", (int)r, ucxapp_errname(r));
        return UCXAPP_ERR_FAILED;
    }
    r = uCxSystemReboot(app->ucx);
    if (r != 0) {
        ucxapp_print("ERROR: reboot failed (%d%s)\n", (int)r, ucxapp_errname(r));
        return UCXAPP_ERR_FAILED;
    }
    U_CX_PORT_SLEEP_MS(2000);
    ucxapp_print("module reset to factory settings and rebooted\n");
    return UCXAPP_OK;
}

static const ucxapp_cmd_t gDeviceCmds[] = {
    { "device.reboot", "", "Reboot the module (clears GATT services and connections); use as the last command", UCXAPP_CMD_NEEDS_DEVICE, cmdDeviceReboot },
    { "device.factory", "<confirm>", "Erase all settings (Wi-Fi, Bluetooth, certificates) and reboot; type 'confirm' to proceed", UCXAPP_CMD_NEEDS_DEVICE, cmdDeviceFactory },
    { "device.info", "", "Show manufacturer, model and firmware version", UCXAPP_CMD_NEEDS_DEVICE, cmdDeviceInfo },
    { "device.status", "", "Overview: module info, Wi-Fi, open sockets, BLE connections, certificates", UCXAPP_CMD_NEEDS_DEVICE, cmdDeviceStatus },
};

int ucxapp_find_modules(char names[][UCXAPP_PLATFORM_PORT_NAME_LEN], int maxNames)
{
    ucxapp_serial_port_t ports[64];
    int found = 0;
    int count = ucxapp_platform_list_serial_ports(ports, (int)(sizeof(ports) / sizeof(ports[0])));

    for (int i = 0; i < count && found < maxNames; i++) {
        const ucxapp_serial_port_t *p = &ports[i];
        bool nora = strstr(p->desc, "NORA-W36") != NULL || strstr(p->desc, "NORA-B26") != NULL;
        if (nora && p->available && (p->label[0] == '\0' || strcmp(p->label, "AT") == 0)) {
            snprintf(names[found++], UCXAPP_PLATFORM_PORT_NAME_LEN, "%.*s", UCXAPP_PLATFORM_PORT_NAME_LEN - 1, p->name);
        }
    }
    return found;
}

bool ucxapp_port_skip(const ucxapp_serial_port_t *sp)
{
    static const char *const kSkip[] = { "Virtual", "Bluetooth", "Management", "SOL", "NORA-" };

    if (!sp->available || sp->label[0] != '\0') {
        return true;
    }
    for (size_t i = 0; i < sizeof(kSkip) / sizeof(kSkip[0]); i++) {
        if (strstr(sp->desc, kSkip[i]) != NULL) {
            return true;
        }
    }
    return false;
}

bool ucxapp_port_is_gnss(const ucxapp_serial_port_t *sp)
{
    static const char *const kGnss[] = { "u-blox", "GNSS", "GPS", "F9P", "F9R", "ZED", "NEO-", "MAX-", "LEA-" };

    if (sp->vid == 0x1546) {
        return true;
    }
    for (size_t i = 0; i < sizeof(kGnss) / sizeof(kGnss[0]); i++) {
        if (strstr(sp->desc, kGnss[i]) != NULL) {
            return true;
        }
    }
    return false;
}

static void lastPath(const char *kind, char *path, size_t size)
{
    const char *dir = getenv("TEMP");

    if (dir == NULL) {
        dir = getenv("TMPDIR");
    }
    snprintf(path, size, "%s/ucxapp_last_%s.txt", dir != NULL ? dir : "/tmp", kind);
}

bool ucxapp_last_port_load(const char *kind, char *port, size_t size, int32_t *baud)
{
    char path[300], line[64];
    FILE *f;
    int b = 0;
    bool ok = false;

    lastPath(kind, path, sizeof(path));
    f = fopen(path, "r");
    if (f == NULL) {
        return false;
    }
    if (fgets(line, sizeof(line), f) != NULL) {
        char name[UCXAPP_PLATFORM_PORT_NAME_LEN];
        if (sscanf(line, "%31s %d", name, &b) == 2 && b > 0) {
            snprintf(port, size, "%s", name);
            *baud = b;
            ok = true;
        }
    }
    fclose(f);
    return ok;
}

void ucxapp_last_port_save(const char *kind, const char *port, int32_t baud)
{
    char path[300];
    FILE *f;

    lastPath(kind, path, sizeof(path));
    f = fopen(path, "w");
    if (f != NULL) {
        fprintf(f, "%s %d\n", port, (int)baud);
        fclose(f);
    }
}

void ucxapp_register_device_cmds(void)
{
    ucxapp_cmd_register(gDeviceCmds, sizeof(gDeviceCmds) / sizeof(gDeviceCmds[0]));
}
