#include "ucxapp_cmd.h"
#include "ucxapp_platform.h"
#include <string.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4200 4201)
#endif
#include "u_cx_general.h"
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

static int cmdDeviceReboot(ucxapp_t *app, int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    int32_t r = uCxSystemReboot(app->ucx);
    if (r != 0) {
        ucxapp_print("ERROR: reboot failed (%d)\n", (int)r);
        return UCXAPP_ERR_FAILED;
    }
    U_CX_PORT_SLEEP_MS(2000);
    ucxapp_print("module rebooted\n");
    return UCXAPP_OK;
}

static const ucxapp_cmd_t gDeviceCmds[] = {
    { "device.reboot", "", "Reboot the module (clears GATT services and connections); use as the last command", UCXAPP_CMD_NEEDS_DEVICE, cmdDeviceReboot },
    { "device.info", "", "Show manufacturer, model and firmware version", UCXAPP_CMD_NEEDS_DEVICE, cmdDeviceInfo },
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
            snprintf(names[found++], UCXAPP_PLATFORM_PORT_NAME_LEN, "%s", p->name);
        }
    }
    return found;
}

void ucxapp_register_device_cmds(void)
{
    ucxapp_cmd_register(gDeviceCmds, sizeof(gDeviceCmds) / sizeof(gDeviceCmds[0]));
}
