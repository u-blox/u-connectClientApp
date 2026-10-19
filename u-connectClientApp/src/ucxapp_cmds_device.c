#include "ucxapp_cmd.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4200 4201)
#endif
#include "u_cx_general.h"
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

static const ucxapp_cmd_t gDeviceCmds[] = {
    { "device.info", "", "Show manufacturer, model and firmware version", UCXAPP_CMD_NEEDS_DEVICE, cmdDeviceInfo },
};

void ucxapp_register_device_cmds(void)
{
    ucxapp_cmd_register(gDeviceCmds, sizeof(gDeviceCmds) / sizeof(gDeviceCmds[0]));
}
