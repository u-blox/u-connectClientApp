#include "ucxapp_cmd.h"

#include <stdio.h>
#include <string.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4200 4201)
#endif
#include "u_cx_bluetooth.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#define BT_SCAN_MAX_DEVICES 32
#define BT_SCAN_TIMEOUT_MS  30000

typedef struct {
    uBtLeAddress_t addr;
    int32_t rssi;
    char name[32];
} scanEntry_t;

static void printAddr(const uBtLeAddress_t *a)
{
    ucxapp_print("%02X:%02X:%02X:%02X:%02X:%02X%s", a->address[0], a->address[1], a->address[2],
                 a->address[3], a->address[4], a->address[5],
                 a->type == U_BD_ADDRESS_TYPE_RANDOM ? ",random" : "");
}

/* "XX:XX:XX:XX:XX:XX" or "XX:XX:XX:XX:XX:XX,random"; public if no type is given. */
static bool parseAddr(const char *s, uBtLeAddress_t *addr)
{
    unsigned int v[6];
    int used = 0;

    if (sscanf(s, "%2x:%2x:%2x:%2x:%2x:%2x%n", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &used) != 6) {
        return false;
    }
    for (int i = 0; i < 6; i++) {
        addr->address[i] = (uint8_t)v[i];
    }
    addr->type = U_BD_ADDRESS_TYPE_PUBLIC;
    if (s[used] == ',') {
        if (strcmp(&s[used + 1], "random") == 0) {
            addr->type = U_BD_ADDRESS_TYPE_RANDOM;
        } else if (strcmp(&s[used + 1], "public") != 0) {
            return false;
        }
    } else if (s[used] != '\0') {
        return false;
    }
    return true;
}

static int cmdBtScan(ucxapp_t *app, int argc, char *argv[])
{
    scanEntry_t list[BT_SCAN_MAX_DEVICES];
    int count = 0;
    uCxBtDiscoveryDefault_t dev;
    (void)argc;
    (void)argv;

    ucxapp_print("scanning...\n");
    uCxAtClientSetCommandTimeout(app->ucx->pAtClient, BT_SCAN_TIMEOUT_MS, false);
    uCxBluetoothDiscoveryDefaultBegin(app->ucx);
    while (uCxBluetoothDiscoveryDefaultGetNext(app->ucx, &dev)) {
        int i = 0;
        while (i < count && !(memcmp(list[i].addr.address, dev.bd_addr.address, 6) == 0 &&
                              list[i].addr.type == dev.bd_addr.type)) {
            i++;
        }
        if (i == count) {
            if (count == BT_SCAN_MAX_DEVICES) {
                continue;
            }
            list[count].addr = dev.bd_addr;
            list[count].rssi = dev.rssi;
            list[count].name[0] = '\0';
            count++;
        } else if (dev.rssi > list[i].rssi) {
            list[i].rssi = dev.rssi;
        }
        if (dev.device_name != NULL && dev.device_name[0] != '\0' && list[i].name[0] == '\0') {
            snprintf(list[i].name, sizeof(list[i].name), "%s", dev.device_name);
        }
    }
    uCxEnd(app->ucx);

    for (int i = 0; i < count; i++) {
        printAddr(&list[i].addr);
        ucxapp_print("  %d dBm  %s\n", (int)list[i].rssi, list[i].name[0] ? list[i].name : "(no name)");
    }
    ucxapp_print("%d device(s)\n", count);
    return UCXAPP_OK;
}

static int cmdBtConnect(ucxapp_t *app, int argc, char *argv[])
{
    uBtLeAddress_t addr;

    if (argc != 2 || !parseAddr(argv[1], &addr)) {
        return UCXAPP_ERR_USAGE;
    }
    int32_t handle = uCxBluetoothConnect(app->ucx, &addr);
    if (handle < 0) {
        ucxapp_print("ERROR: connect failed (%d)\n", (int)handle);
        return UCXAPP_ERR_FAILED;
    }
    ucxapp_print("connection %d\n", (int)handle);
    return UCXAPP_OK;
}

static int cmdBtDisconnect(ucxapp_t *app, int argc, char *argv[])
{
    int32_t handle;

    if (argc != 2 || !ucxapp_parse_int(argv[1], &handle) || handle < 0) {
        return UCXAPP_ERR_USAGE;
    }
    int32_t result = uCxBluetoothDisconnect(app->ucx, handle);
    if (result != 0) {
        ucxapp_print("ERROR: disconnect failed (%d)\n", (int)result);
        return UCXAPP_ERR_FAILED;
    }
    ucxapp_print("connection %d closed\n", (int)handle);
    return UCXAPP_OK;
}

static int cmdBtList(ucxapp_t *app, int argc, char *argv[])
{
    uCxBtListConnections_t conn;
    int count = 0;
    (void)argc;
    (void)argv;

    uCxBluetoothListConnectionsBegin(app->ucx);
    while (uCxBluetoothListConnectionsGetNext(app->ucx, &conn)) {
        count++;
        ucxapp_print("connection %d: ", (int)conn.conn_handle);
        printAddr(&conn.bd_addr);
        ucxapp_print("\n");
    }
    uCxEnd(app->ucx);
    ucxapp_print("%d connection(s)\n", count);
    return UCXAPP_OK;
}

static const ucxapp_cmd_t gBtCmds[] = {
    { "bt.scan",       "",                    "Scan for BLE devices (up to 30 s)",              UCXAPP_CMD_NEEDS_DEVICE, cmdBtScan },
    { "bt.connect",    "<addr[,random]>",     "Connect to a BLE device, e.g. 11:22:33:44:55:66", UCXAPP_CMD_NEEDS_DEVICE, cmdBtConnect },
    { "bt.disconnect", "<handle>",            "Close a BLE connection",                         UCXAPP_CMD_NEEDS_DEVICE, cmdBtDisconnect },
    { "bt.list",       "",                    "List BLE connections",                           UCXAPP_CMD_NEEDS_DEVICE, cmdBtList },
};

void ucxapp_register_bt_cmds(void)
{
    ucxapp_cmd_register(gBtCmds, sizeof(gBtCmds) / sizeof(gBtCmds[0]));
}
