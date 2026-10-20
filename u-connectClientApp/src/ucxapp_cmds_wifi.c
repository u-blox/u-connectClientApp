#include "ucxapp_cmd.h"
#include "ucxapp_store.h"
#include "ucx_obfuscate.h"
#include "u_port.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

bool ucxapp_wifi_get_status(uCxHandle_t *ucx, char *ssid, size_t ssidSize, char *ip, size_t ipSize)
{
    uCxWifiStationStatus_t status;
    uSockIpAddress_t addr;

    ssid[0] = '\0';
    ip[0] = '\0';
    if (!isConnected(ucx)) {
        return false;
    }
    if (uCxWifiStationStatusBegin(ucx, U_WIFI_STATUS_ID_SSID, &status) && status.rsp.StatusIdStr.ssid != NULL) {
        snprintf(ssid, ssidSize, "%s", status.rsp.StatusIdStr.ssid);
    }
    uCxEnd(ucx);
    if (uCxWifiStationGetNetworkStatus(ucx, U_WIFI_NET_STATUS_ID_IPV4, &addr) != 0 ||
        uCxIpAddressToString(&addr, ip, ipSize) <= 0) {
        ip[0] = '\0';
    }
    return true;
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

static int connectTo(ucxapp_t *app, const char *ssid, const char *password, uint32_t timeoutS)
{
    uCxHandle_t *ucx = app->ucx;

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
    if (!ucxapp_events_wait(URC_FLAG_NETWORK_UP, timeoutS)) {
        ucxapp_print("ERROR: timeout waiting for network up\n");
        uCxWifiStationDisconnect(ucx);   /* abort the pending attempt, else the next connect gets ERROR:15 */
        U_CX_PORT_SLEEP_MS(1000);
        return UCXAPP_ERR_FAILED;
    }
    ucxapp_print("Connected to '%s'\n", ssid);
    printIpAddress(ucx);
    return UCXAPP_OK;
}

static int cmdWifiConnect(ucxapp_t *app, int argc, char *argv[])
{
    if (argc < 2 || argc > 3) {
        return UCXAPP_ERR_USAGE;
    }
    return connectTo(app, argv[1], argc > 2 ? argv[2] : "", 20);
}

/* ---- saved profiles: one "name<TAB>ssid<TAB>obfuscated password" line each ---- */

#define PROFILES_FILE  "u-connectClientApp.wifi"
#define MAX_PROFILES   16
#define NAME_LEN       32
#define SSID_LEN       33
#define PASS_LEN       64
#define OBF_LEN        (PASS_LEN * 2 + 1)

typedef struct {
    char name[NAME_LEN];
    char ssid[SSID_LEN];
    char password[PASS_LEN];
} profile_t;

static bool fieldOk(const char *s, size_t maxLen)
{
    return s[0] != '\0' && strlen(s) < maxLen && strpbrk(s, "\t\r\n") == NULL;
}

static int loadProfiles(profile_t *list)
{
    size_t len = 0;
    uint8_t *data = ucxapp_store_load(PROFILES_FILE, &len);
    int count = 0;
    char *line = (char *)data;

    if (data == NULL) {
        return 0;
    }
    while (*line != '\0' && count < MAX_PROFILES) {
        char *end = line + strcspn(line, "\r\n");
        char *next = end;
        char *tab1;
        char *tab2;
        profile_t *p = &list[count];

        while (*next == '\r' || *next == '\n') {
            *next++ = '\0';
        }
        tab1 = strchr(line, '\t');
        tab2 = tab1 != NULL ? strchr(tab1 + 1, '\t') : NULL;
        if (tab2 != NULL) {
            *tab1 = '\0';
            *tab2 = '\0';
            if (strlen(line) < NAME_LEN && strlen(tab1 + 1) < SSID_LEN && strlen(tab2 + 1) < OBF_LEN) {
                snprintf(p->name, sizeof(p->name), "%s", line);
                snprintf(p->ssid, sizeof(p->ssid), "%s", tab1 + 1);
                deobfuscatePassword(tab2 + 1, p->password, sizeof(p->password));
                count++;
            }
        }
        line = next;
    }
    free(data);
    return count;
}

static bool saveProfiles(const profile_t *list, int count)
{
    char *text = (char *)calloc((size_t)count + 1, NAME_LEN + SSID_LEN + OBF_LEN + 4);
    bool ok;

    if (text == NULL) {
        return false;
    }
    for (int i = 0; i < count; i++) {
        char obf[OBF_LEN];
        size_t used = strlen(text);

        obfuscatePassword(list[i].password, obf, sizeof(obf));
        snprintf(text + used, NAME_LEN + SSID_LEN + OBF_LEN + 4, "%s\t%s\t%s\n", list[i].name, list[i].ssid, obf);
    }
    ok = ucxapp_store_save(PROFILES_FILE, text, strlen(text));
    free(text);
    return ok;
}

static int findProfile(const profile_t *list, int count, const char *name)
{
    for (int i = 0; i < count; i++) {
        if (strcmp(list[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

#define LAST_FILE       "u-connectClientApp.wifilast"
#define REGDOMAIN_FILE  "u-connectClientApp.regdomain"

static const struct { const char *name; uWifiRegDomain_t value; } gRegDomains[] = {
    { "world", U_WIFI_REG_DOMAIN_WORLD }, { "etsi", U_WIFI_REG_DOMAIN_ETSI }, { "fcc", U_WIFI_REG_DOMAIN_FCC },
    { "ic", U_WIFI_REG_DOMAIN_IC }, { "nz", U_WIFI_REG_DOMAIN_NZ }, { "mkk", U_WIFI_REG_DOMAIN_MKK },
    { "ncc", U_WIFI_REG_DOMAIN_NCC }, { "acma", U_WIFI_REG_DOMAIN_ACMA }, { "kcc", U_WIFI_REG_DOMAIN_KCC },
    { "sa", U_WIFI_REG_DOMAIN_SA }, { "br", U_WIFI_REG_DOMAIN_BR },
};

static int findRegDomain(const char *name)
{
    for (size_t i = 0; i < sizeof(gRegDomains) / sizeof(gRegDomains[0]); i++) {
        if (strcmp(gRegDomains[i].name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

/* Applies the saved regulatory domain; defaults to ETSI so channels 1-13 are scanned. */
static void applyRegDomain(uCxHandle_t *ucx)
{
    size_t len = 0;
    char *saved = (char *)ucxapp_store_load(REGDOMAIN_FILE, &len);
    int dom = saved != NULL ? findRegDomain(saved) : -1;

    free(saved);
    if (dom < 0) {
        dom = findRegDomain("etsi");
    }
    if (uCxWifiSetRegulatoryDomain(ucx, gRegDomains[dom].value) != 0) {
        ucxapp_print("WARNING: could not set regulatory domain '%s'\n", gRegDomains[dom].name);
    }
}

static bool loadLastUsed(char *name, size_t size)
{
    size_t len = 0;
    char *data = (char *)ucxapp_store_load(LAST_FILE, &len);

    if (data == NULL) {
        return false;
    }
    data[strcspn(data, "\r\n")] = '\0';
    snprintf(name, size, "%s", data);
    free(data);
    return name[0] != '\0';
}

static int connectProfile(ucxapp_t *app, const profile_t *p, uint32_t timeoutS)
{
    int rc = connectTo(app, p->ssid, p->password, timeoutS);

    if (rc == UCXAPP_OK) {
        ucxapp_store_save(LAST_FILE, p->name, strlen(p->name));
    }
    return rc;
}

static int cmdProfileAdd(ucxapp_t *app, int argc, char *argv[])
{
    profile_t list[MAX_PROFILES];
    const char *password = argc > 3 ? argv[3] : "";
    int count;
    int idx;

    (void)app;
    if (argc < 3 || argc > 4 || !fieldOk(argv[1], NAME_LEN) || !fieldOk(argv[2], SSID_LEN) ||
        strlen(password) >= PASS_LEN || strpbrk(password, "\t\r\n") != NULL) {
        return UCXAPP_ERR_USAGE;
    }
    count = loadProfiles(list);
    idx = findProfile(list, count, argv[1]);
    if (idx < 0) {
        if (count >= MAX_PROFILES) {
            ucxapp_print("ERROR: at most %d profiles\n", MAX_PROFILES);
            return UCXAPP_ERR_FAILED;
        }
        idx = count++;
    }
    snprintf(list[idx].name, sizeof(list[idx].name), "%s", argv[1]);
    snprintf(list[idx].ssid, sizeof(list[idx].ssid), "%s", argv[2]);
    snprintf(list[idx].password, sizeof(list[idx].password), "%s", password);
    if (!saveProfiles(list, count)) {
        ucxapp_print("ERROR: cannot write %s\n", PROFILES_FILE);
        return UCXAPP_ERR_FAILED;
    }
    ucxapp_print("profile '%s' saved (ssid '%s')\n", argv[1], argv[2]);
    return UCXAPP_OK;
}

static int cmdProfileList(ucxapp_t *app, int argc, char *argv[])
{
    profile_t list[MAX_PROFILES];
    int count = loadProfiles(list);

    (void)app;
    (void)argc;
    (void)argv;
    for (int i = 0; i < count; i++) {
        ucxapp_print("%s: ssid=%s %s\n", list[i].name, list[i].ssid, list[i].password[0] != '\0' ? "WPA" : "open");
    }
    ucxapp_print("%d profile(s)\n", count);
    return UCXAPP_OK;
}

static int cmdProfileRemove(ucxapp_t *app, int argc, char *argv[])
{
    profile_t list[MAX_PROFILES];
    int count;
    int idx;

    (void)app;
    if (argc != 2) {
        return UCXAPP_ERR_USAGE;
    }
    count = loadProfiles(list);
    idx = findProfile(list, count, argv[1]);
    if (idx < 0) {
        ucxapp_print("ERROR: no profile '%s'\n", argv[1]);
        return UCXAPP_ERR_FAILED;
    }
    list[idx] = list[--count];
    if (!saveProfiles(list, count)) {
        ucxapp_print("ERROR: cannot write %s\n", PROFILES_FILE);
        return UCXAPP_ERR_FAILED;
    }
    ucxapp_print("profile '%s' removed\n", argv[1]);
    return UCXAPP_OK;
}

static int cmdProfileConnect(ucxapp_t *app, int argc, char *argv[])
{
    profile_t list[MAX_PROFILES];
    int count;
    int idx;

    if (argc != 2) {
        return UCXAPP_ERR_USAGE;
    }
    count = loadProfiles(list);
    idx = findProfile(list, count, argv[1]);
    if (idx < 0) {
        ucxapp_print("ERROR: no profile '%s' (see wifi.profile.list)\n", argv[1]);
        return UCXAPP_ERR_FAILED;
    }
    return connectProfile(app, &list[idx], 20);
}

#define AUTO_FIRST_TRY_S  10

static int cmdProfileAuto(ucxapp_t *app, int argc, char *argv[])
{
    profile_t list[MAX_PROFILES];
    char last[NAME_LEN];
    int count = loadProfiles(list);
    int idx;
    int best = -1;
    int bestRssi = -1000;
    uCxWifiStationScanDefault_t net;

    (void)argc;
    (void)argv;
    if (count == 0) {
        ucxapp_print("ERROR: no profiles (see wifi.profile.add)\n");
        return UCXAPP_ERR_FAILED;
    }

    if (loadLastUsed(last, sizeof(last)) && (idx = findProfile(list, count, last)) >= 0) {
        ucxapp_print("Trying last used profile '%s'...\n", last);
        if (connectProfile(app, &list[idx], AUTO_FIRST_TRY_S) == UCXAPP_OK) {
            return UCXAPP_OK;
        }
    }

    applyRegDomain(app->ucx);
    ucxapp_print("Scanning for known networks (can take up to 40 s)...\n");
    uCxAtClientSetCommandTimeout(app->ucx->pAtClient, 60000, false);
    uCxWifiStationScanDefaultBegin(app->ucx);
    while (uCxWifiStationScanDefaultGetNext(app->ucx, &net)) {
        for (int i = 0; i < count; i++) {
            if (net.ssid != NULL && strcmp(net.ssid, list[i].ssid) == 0 && net.rssi > bestRssi) {
                best = i;
                bestRssi = (int)net.rssi;
            }
        }
    }
    uCxEnd(app->ucx);
    if (best < 0) {
        ucxapp_print("ERROR: no known network found\n");
        return UCXAPP_ERR_FAILED;
    }
    ucxapp_print("Strongest known network: '%s' (%d dBm)\n", list[best].name, bestRssi);
    return connectProfile(app, &list[best], 20);
}

static int cmdWifiRegDomain(ucxapp_t *app, int argc, char *argv[])
{
    uWifiRegDomain_t current;
    int dom;

    if (argc > 2) {
        return UCXAPP_ERR_USAGE;
    }
    if (argc == 2) {
        dom = findRegDomain(argv[1]);
        if (dom < 0) {
            return UCXAPP_ERR_USAGE;
        }
        if (!ucxapp_store_save(REGDOMAIN_FILE, gRegDomains[dom].name, strlen(gRegDomains[dom].name))) {
            ucxapp_print("ERROR: cannot write %s\n", REGDOMAIN_FILE);
            return UCXAPP_ERR_FAILED;
        }
        if (uCxWifiSetRegulatoryDomain(app->ucx, gRegDomains[dom].value) != 0) {
            ucxapp_print("ERROR: module rejected the regulatory domain\n");
            return UCXAPP_ERR_FAILED;
        }
    }
    if (uCxWifiGetRegulatoryDomain(app->ucx, &current) != 0) {
        ucxapp_print("ERROR: cannot read the regulatory domain\n");
        return UCXAPP_ERR_FAILED;
    }
    for (size_t i = 0; i < sizeof(gRegDomains) / sizeof(gRegDomains[0]); i++) {
        if (gRegDomains[i].value == current) {
            ucxapp_print("regdomain=%s\n", gRegDomains[i].name);
            return UCXAPP_OK;
        }
    }
    ucxapp_print("regdomain=%d\n", (int)current);
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

static int cmdWifiApEnable(ucxapp_t *app, int argc, char *argv[])
{
    uCxHandle_t *ucx = app->ucx;
    const char *ssid = argc > 1 ? argv[1] : "ucxclient Access Point";
    const char *password = argc > 2 ? argv[2] : "12345678";
    int32_t channel = 6;

    if (argc > 4 || (argc > 3 && (!ucxapp_parse_int(argv[3], &channel) || channel < 1 || channel > 13))) {
        return UCXAPP_ERR_USAGE;
    }
    if (strlen(password) < 8) {
        ucxapp_print("ERROR: password must be at least 8 characters\n");
        return UCXAPP_ERR_FAILED;
    }
    if (uCxWifiApSetConnectionParams2(ucx, ssid, channel) != 0 ||
        uCxWifiApSetSecurityWpa2(ucx, password, U_WIFI_WPA_VERSION_WPA2) != 0) {
        ucxapp_print("ERROR: failed to configure the access point\n");
        return UCXAPP_ERR_FAILED;
    }
    if (uCxWifiApActivate(ucx) != 0) {
        ucxapp_print("ERROR: failed to activate AP (disconnect the Wi-Fi station first)\n");
        return UCXAPP_ERR_FAILED;
    }
    ucxapp_print("Access point enabled: ssid='%s' channel=%d WPA2\n", ssid, (int)channel);
    ucxapp_qr_print_wifi(ssid, password, "WPA", false);
    return UCXAPP_OK;
}

static int cmdWifiApDisable(ucxapp_t *app, int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (uCxWifiApDeactivate(app->ucx) != 0) {
        ucxapp_print("ERROR: failed to deactivate AP\n");
        return UCXAPP_ERR_FAILED;
    }
    ucxapp_print("Access point disabled\n");
    return UCXAPP_OK;
}

static int cmdWifiApStatus(ucxapp_t *app, int argc, char *argv[])
{
    uCxHandle_t *ucx = app->ucx;
    uCxWifiApGetConnectionParams_t params;
    uCxWifiApGetSecurity_t sec;
    uCxWifiApListNetworkStatus_t net;
    uMacAddress_t mac;
    int count = 0;
    (void)argc;
    (void)argv;

    if (uCxWifiApGetConnectionParamsBegin(ucx, &params)) {
        ucxapp_print("ssid=%s channel=%d\n", params.ssid, (int)params.channel);
    }
    uCxEnd(ucx);
    if (uCxWifiApGetSecurity(ucx, &sec) == 0) {
        if (sec.type == U_CX_WIFI_AP_GET_SECURITY_RSP_TYPE_SECURITY_MODE_WPA_VERSION) {
            ucxapp_print("security: wpa_version=%d mode=%d\n", (int)sec.rsp.SecurityModeWpaVersion.wpa_version,
                         (int)sec.rsp.SecurityModeWpaVersion.security_mode);
        } else if (sec.type == U_CX_WIFI_AP_GET_SECURITY_RSP_TYPE_SECURITY_MODE) {
            ucxapp_print("security: mode=%d\n", (int)sec.rsp.SecurityMode.security_mode);
        }
    }
    uCxWifiApListNetworkStatusBegin(ucx);
    while (uCxWifiApListNetworkStatusGetNext(ucx, &net)) {
        static const char *const names[] = { "ip", "subnet", "gateway", "dns" };
        char str[40];
        if (net.net_status_id >= 0 && net.net_status_id <= 3 &&
            uCxIpAddressToString(&net.net_status_val, str, sizeof(str)) > 0) {
            ucxapp_print("%s=%s\n", names[net.net_status_id], str);
        }
    }
    uCxEnd(ucx);
    uCxWifiApListStationsBegin(ucx);
    while (uCxWifiApListStationsGetNext(ucx, &mac)) {
        ucxapp_print("station %02X:%02X:%02X:%02X:%02X:%02X\n", mac.address[0], mac.address[1], mac.address[2],
                     mac.address[3], mac.address[4], mac.address[5]);
        count++;
    }
    uCxEnd(ucx);
    ucxapp_print("%d station(s)\n", count);
    return UCXAPP_OK;
}

static const ucxapp_cmd_t gWifiCmds[] = {
    { "wifi.scan",       "",                    "Scan for Wi-Fi networks",          UCXAPP_CMD_NEEDS_DEVICE, cmdWifiScan },
    { "wifi.connect",    "<ssid> [password]",   "Connect to a Wi-Fi network",       UCXAPP_CMD_NEEDS_DEVICE, cmdWifiConnect },
    { "wifi.status",     "",                    "Show connection, SSID, RSSI, IP",  UCXAPP_CMD_NEEDS_DEVICE, cmdWifiStatus },
    { "wifi.disconnect", "",                    "Disconnect from Wi-Fi",            UCXAPP_CMD_NEEDS_DEVICE, cmdWifiDisconnect },
    { "wifi.profile.add", "<name> <ssid> [password]", "Save or update a Wi-Fi profile", 0, cmdProfileAdd },
    { "wifi.profile.list", "",                   "List saved Wi-Fi profiles",        0, cmdProfileList },
    { "wifi.profile.remove", "<name>",           "Delete a saved Wi-Fi profile",     0, cmdProfileRemove },
    { "wifi.profile.connect", "<name>",          "Connect using a saved Wi-Fi profile", UCXAPP_CMD_NEEDS_DEVICE, cmdProfileConnect },
    { "wifi.profile.auto", "",                   "Connect: last used profile, else scan and pick the strongest known", UCXAPP_CMD_NEEDS_DEVICE, cmdProfileAuto },
    { "wifi.regdomain",  "[world|etsi|fcc|ic|nz|mkk|ncc|acma|kcc|sa|br]", "Show or set (and save) the regulatory domain", UCXAPP_CMD_NEEDS_DEVICE, cmdWifiRegDomain },
    { "wifi.ap.enable",  "[ssid] [password] [channel]", "Start a WPA2 access point and show its join QR code", UCXAPP_CMD_NEEDS_DEVICE, cmdWifiApEnable },
    { "wifi.ap.disable", "",                    "Stop the access point",            UCXAPP_CMD_NEEDS_DEVICE, cmdWifiApDisable },
    { "wifi.ap.status",  "",                    "Show AP parameters, addresses and stations", UCXAPP_CMD_NEEDS_DEVICE, cmdWifiApStatus },
};

void ucxapp_register_wifi_cmds(void)
{
    ucxapp_cmd_register(gWifiCmds, sizeof(gWifiCmds) / sizeof(gWifiCmds[0]));
}
