#include "ucxapp_cmd.h"
#include "u_port.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4200 4201)
#endif
#include "u_cx_wifi.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#define WIFI_STATE_CONNECTED 2   /* Wi-Fi connection status id: 1 = not connected, 2 = connected */

static bool isConnected(uCxHandle_t *ucx)
{
    uCxWifiStationStatus_t status;
    bool connected = false;

    if (uCxWifiStationStatusBegin(ucx, U_WIFI_STATUS_ID_CONNECTION, &status)) {
        connected = status.rsp.StatusIdInt.int_val == WIFI_STATE_CONNECTED;
    }
    uCxEnd(ucx);
    return connected;
}

static void printIpAddress(uCxHandle_t *ucx)
{
    uSockIpAddress_t ip;
    char str[40];

    if (uCxWifiStationGetNetworkStatus(ucx, U_WIFI_NET_STATUS_ID_IPV4, &ip) == 0 &&
        uCxIpAddressToString(&ip, str, sizeof(str)) > 0) {
        ucxapp_print("ip=%s\n", str);
    }
}

static int cmdWifiScan(ucxapp_t *app, int argc, char *argv[])
{
    uCxWifiStationScanDefault_t net;
    int count = 0;
    (void)argc;
    (void)argv;

    ucxapp_print("Scanning (can take up to 40 s)...\n");
    uCxAtClientSetCommandTimeout(app->ucx->pAtClient, 60000, false);
    uCxWifiStationScanDefaultBegin(app->ucx);
    while (uCxWifiStationScanDefaultGetNext(app->ucx, &net)) {
        const char *sec = net.authentication_suites == 0 ? "Open" :
                          (net.authentication_suites & (1 << 5)) ? "WPA3" :
                          (net.authentication_suites & (1 << 4)) ? "WPA2" : "WPA";
        count++;
        ucxapp_print("%-32s %02X:%02X:%02X:%02X:%02X:%02X ch%-3d %4d dBm %s\n",
                     (net.ssid != NULL && net.ssid[0] != '\0') ? net.ssid : "<hidden>",
                     net.bssid.address[0], net.bssid.address[1], net.bssid.address[2],
                     net.bssid.address[3], net.bssid.address[4], net.bssid.address[5],
                     (int)net.channel, (int)net.rssi, sec);
    }
    uCxEnd(app->ucx);
    ucxapp_print("%d network(s)\n", count);
    return UCXAPP_OK;
}

static int cmdWifiConnect(ucxapp_t *app, int argc, char *argv[])
{
    uCxHandle_t *ucx = app->ucx;
    const char *ssid;
    const char *password;

    if (argc < 2 || argc > 3) {
        return UCXAPP_ERR_USAGE;
    }
    ssid = argv[1];
    password = argc > 2 ? argv[2] : "";

    if (isConnected(ucx)) {
        ucxapp_print("Already connected, disconnecting first...\n");
        uCxWifiStationDisconnect(ucx);
        U_CX_PORT_SLEEP_MS(1000);
    }

    if (uCxWifiStationSetConnectionParams(ucx, 0, ssid) != 0) {
        ucxapp_print("ERROR: failed to set connection parameters\n");
        return UCXAPP_ERR_FAILED;
    }
    if (password[0] != '\0') {
        if (uCxWifiStationSetSecurityWpa(ucx, 0, password, U_WIFI_WPA_THRESHOLD_WPA2) != 0) {
            ucxapp_print("ERROR: failed to set WPA security\n");
            return UCXAPP_ERR_FAILED;
        }
    } else if (uCxWifiStationSetSecurityOpen(ucx, 0) != 0) {
        ucxapp_print("ERROR: failed to set open security\n");
        return UCXAPP_ERR_FAILED;
    }

    ucxapp_events_clear(URC_FLAG_NETWORK_UP | URC_FLAG_NETWORK_DOWN);
    if (uCxWifiStationConnect(ucx, 0) != 0) {
        ucxapp_print("ERROR: failed to start connection\n");
        return UCXAPP_ERR_FAILED;
    }
    if (!ucxapp_events_wait(URC_FLAG_NETWORK_UP, 20)) {
        ucxapp_print("ERROR: timeout waiting for network up\n");
        return UCXAPP_ERR_FAILED;
    }
    ucxapp_print("Connected to '%s'\n", ssid);
    printIpAddress(ucx);
    return UCXAPP_OK;
}

static int cmdWifiStatus(ucxapp_t *app, int argc, char *argv[])
{
    uCxHandle_t *ucx = app->ucx;
    uCxWifiStationStatus_t status;
    (void)argc;
    (void)argv;

    if (!isConnected(ucx)) {
        ucxapp_print("wifi: disconnected\n");
        return UCXAPP_OK;
    }
    ucxapp_print("wifi: connected\n");
    if (uCxWifiStationStatusBegin(ucx, U_WIFI_STATUS_ID_SSID, &status)) {
        ucxapp_print("ssid=%s\n", status.rsp.StatusIdStr.ssid != NULL ? status.rsp.StatusIdStr.ssid : "");
    }
    uCxEnd(ucx);
    if (uCxWifiStationStatusBegin(ucx, U_WIFI_STATUS_ID_RSSI, &status)) {
        ucxapp_print("rssi=%d\n", (int)status.rsp.StatusIdInt.int_val);
    }
    uCxEnd(ucx);
    printIpAddress(ucx);
    return UCXAPP_OK;
}

static int cmdWifiDisconnect(ucxapp_t *app, int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    ucxapp_events_clear(URC_FLAG_NETWORK_DOWN | URC_FLAG_WIFI_LINK_DOWN);
    if (uCxWifiStationDisconnect(app->ucx) != 0) {
        ucxapp_print("ERROR: failed to disconnect\n");
        return UCXAPP_ERR_FAILED;
    }
    ucxapp_events_wait(URC_FLAG_WIFI_LINK_DOWN, 3);
    ucxapp_print("Disconnected\n");
    return UCXAPP_OK;
}

static const ucxapp_cmd_t gWifiCmds[] = {
    { "wifi.scan",       "",                    "Scan for Wi-Fi networks",          UCXAPP_CMD_NEEDS_DEVICE, cmdWifiScan },
    { "wifi.connect",    "<ssid> [password]",   "Connect to a Wi-Fi network",       UCXAPP_CMD_NEEDS_DEVICE, cmdWifiConnect },
    { "wifi.status",     "",                    "Show connection, SSID, RSSI, IP",  UCXAPP_CMD_NEEDS_DEVICE, cmdWifiStatus },
    { "wifi.disconnect", "",                    "Disconnect from Wi-Fi",            UCXAPP_CMD_NEEDS_DEVICE, cmdWifiDisconnect },
};

void ucxapp_register_wifi_cmds(void)
{
    ucxapp_cmd_register(gWifiCmds, sizeof(gWifiCmds) / sizeof(gWifiCmds[0]));
}
