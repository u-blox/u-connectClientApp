/* Portable command-line entry point for Windows and POSIX hosts. */

#include <stdio.h>
#include <string.h>

#include "ucxapp_cmd.h"
#include "ucxapp_platform.h"
#ifdef APP_VERSION_MAJOR
#include "version.h"
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4200 4201)
#endif
#include "u_cx_system.h"
#include "u_port.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#define MAX_EXEC 16
#define UART_BAUD 115200

static uCxAtClient_t gAtClient;
static uCxAtClientConfig_t gAtConfig;
static uCxHandle_t gUcxHandle;
static char gRxBuffer[8192];
static char gUrcBuffer[4096];
static bool gConnected;

static bool connectModule(const char *port)
{
    uPortInit();
    memset(&gAtConfig, 0, sizeof(gAtConfig));
    gAtConfig.pRxBuffer = gRxBuffer;
    gAtConfig.rxBufferLen = sizeof(gRxBuffer);
    gAtConfig.pUrcBuffer = gUrcBuffer;
    gAtConfig.urcBufferLen = sizeof(gUrcBuffer);
    gAtConfig.pUartDevName = port;

    uCxAtClientInit(&gAtConfig, &gAtClient);
    int32_t result = uCxAtClientOpen(&gAtClient, UART_BAUD, false);
    if (result != 0) {
        ucxapp_print("ERROR: cannot open %s (%d)\n", port, (int)result);
        uCxAtClientDeinit(&gAtClient);
        uPortDeinit();
        return false;
    }
    uCxInit(&gAtClient, &gUcxHandle);
    ucxapp_events_init();
    ucxapp_urc_register(&gUcxHandle);

    if (uCxSystemSetEchoOff(&gUcxHandle) != 0) {
        ucxapp_print("warning: could not disable echo\n");
    }
    if (uCxSystemSetExtendedError(&gUcxHandle, U_SYS_EXTENDED_ERRORS_ON) != 0) {
        ucxapp_print("warning: could not enable extended error codes\n");
    }
    gConnected = true;
    return true;
}

static void disconnectModule(void)
{
    if (!gConnected) {
        return;
    }
    ucxapp_events_deinit();
    uCxAtClientClose(&gAtClient);
    uCxAtClientDeinit(&gAtClient);
    uPortDeinit();
    gConnected = false;
}

static int cmdDeviceConnect(ucxapp_t *app, int argc, char *argv[])
{
    if (argc != 2) {
        return UCXAPP_ERR_USAGE;
    }
    if (gConnected) {
        ucxapp_print("already connected\n");
        return UCXAPP_OK;
    }
    if (!connectModule(argv[1])) {
        return UCXAPP_ERR_FAILED;
    }
    app->ucx = &gUcxHandle;
    ucxapp_print("connected to %s\n", argv[1]);
    return UCXAPP_OK;
}

static int cmdDeviceDisconnect(ucxapp_t *app, int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    app->ucx = NULL;
    disconnectModule();
    return UCXAPP_OK;
}

static const ucxapp_cmd_t gConnectCmds[] = {
    { "device.connect",    "<port>", "Connect to the module on a serial port", 0, cmdDeviceConnect },
    { "device.disconnect", "",       "Disconnect from the module",             0, cmdDeviceDisconnect },
};

static void printUsage(const char *prog)
{
    ucxapp_print("usage: %s [port] [--exec \"<command>\"]... [--shell] [--list-ports] [--list-modules] [--version]\n", prog);
    ucxapp_print("  without a port, --exec/--shell use the first NORA EVK found\n");
    ucxapp_print("  with no --exec/--shell the command shell starts; 'help' lists the commands\n");
}

static void listPorts(void)
{
    ucxapp_serial_port_t ports[32];
    int count = ucxapp_platform_list_serial_ports(ports, (int)(sizeof(ports) / sizeof(ports[0])));
    for (int i = 0; i < count; i++) {
        ucxapp_print("%-16s %-10s %s\n", ports[i].name, ports[i].available ? "available" : "no access",
                     ports[i].desc);
    }
    if (count == 0) {
        ucxapp_print("no serial ports found\n");
    }
}

int main(int argc, char *argv[])
{
    const char *port = NULL;
    const char *execCmds[MAX_EXEC];
    int execCount = 0;
    bool shellMode = false;

    ucxapp_platform_init();
    ucxapp_io_set(&ucxapp_io_stdio);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--exec") == 0 && i + 1 < argc && execCount < MAX_EXEC) {
            execCmds[execCount++] = argv[++i];
        } else if (strcmp(argv[i], "--shell") == 0) {
            shellMode = true;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
#ifdef APP_VERSION_STRING
            ucxapp_print("%s\n", APP_VERSION_STRING);
#else
            ucxapp_print("unknown\n");
#endif
            return 0;
        } else if (strcmp(argv[i], "--ini") == 0 && i + 1 < argc) {
            i++;   /* accepted for compatibility; the app keeps no settings file */
        } else if (strcmp(argv[i], "--list-modules") == 0) {
            char names[8][32];
            int n = ucxapp_find_modules(names, 8);
            for (int k = 0; k < n; k++) {
                ucxapp_print("%s\n", names[k]);
            }
            return 0;
        } else if (strcmp(argv[i], "--list-ports") == 0) {
            listPorts();
            return 0;
        } else if (argv[i][0] != '-' && port == NULL) {
            port = argv[i];
        } else {
            printUsage(argv[0]);
            return 2;
        }
    }

    ucxapp_cmd_register(gConnectCmds, sizeof(gConnectCmds) / sizeof(gConnectCmds[0]));
    ucxapp_register_device_cmds();
    ucxapp_register_wifi_cmds();
    ucxapp_register_sock_cmds();
    ucxapp_register_net_cmds();
    ucxapp_register_sys_cmds();
    ucxapp_register_mqtt_cmds();
    ucxapp_register_bt_cmds();
    ucxapp_register_cert_cmds();
    ucxapp_register_fw_cmds();
    ucxapp_register_gatt_cmds();
    ucxapp_register_qr_cmds();
    ucxapp_register_hid_cmds();
    ucxapp_register_web_cmds();

    ucxapp_t app = { NULL };
    int rc = 0;
    char detected[8][32];
    if (port == NULL && ucxapp_find_modules(detected, 8) > 0) {
        port = detected[0];
        ucxapp_print("auto-detected module on %s\n", port);
    }
    if (port != NULL) {
        char line[128];
        snprintf(line, sizeof(line), "device.connect %s", port);
        if (ucxapp_cmd_exec(&app, line) != UCXAPP_OK) {
            ucxapp_print("continuing without a device\n");
        }
    }
    for (int i = 0; i < execCount && rc == 0; i++) {
        char line[256];
        snprintf(line, sizeof(line), "%s", execCmds[i]);
        if (ucxapp_cmd_exec(&app, line) != UCXAPP_OK) {
            rc = 1;
        }
    }
    if (rc == 0 && (shellMode || execCount == 0)) {
        ucxapp_shell_run(&app);
    }
    {
        char stopLine[] = "web.stop";
        ucxapp_cmd_exec(&app, stopLine);
    }
    disconnectModule();
    return rc;
}
