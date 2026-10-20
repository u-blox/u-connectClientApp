#include "ucxapp_cmd.h"

#include <stdio.h>
#include <string.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4200 4201)
#endif
#include "u_cx_bluetooth.h"
#include "u_cx_gatt_server.h"
#include "u_port.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#define PROP_READ      0x02
#define PROP_WRITE_NR  0x04
#define PROP_WRITE     0x08
#define PROP_NOTIFY    0x10

#define HIDMOD_CTRL  0x01
#define HIDMOD_SHIFT 0x02
#define HIDMOD_ALT   0x04
#define HIDMOD_GUI   0x08

/* Standard keyboard with LEDs, report ID 1. */
static const uint8_t kReportMap[] = {
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x85, 0x01,
    0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x08, 0x81, 0x01,
    0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65, 0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00,
    0x05, 0x08, 0x95, 0x05, 0x75, 0x01, 0x19, 0x01, 0x29, 0x05, 0x91, 0x02,
    0x95, 0x01, 0x75, 0x03, 0x91, 0x03,
    0xC0
};

/* Version 1.11, country 0, normally connectable + remote wake. */
static const uint8_t kHidInfo[] = { 0x11, 0x01, 0x00, 0x03 };

static bool gDefined;
static int32_t gService = -1;
static int32_t gBootIn = -1;
static int32_t gBootInCccd = -1;
static int32_t gBootOut = -1;
static int32_t gKbdIn = -1;
static int32_t gKbdInCccd = -1;
static int32_t gKbdOut = -1;
static int32_t gProtocolMode = -1;
static int32_t gControlPoint = -1;
static volatile int32_t gConn = -1;
static volatile bool gBootNotify;
static volatile bool gKbdNotify;
static volatile bool gProtocolBoot;

bool ucxapp_hid_server_write(int32_t conn, int32_t handle, const uint8_t *data, size_t len)
{
    if (!gDefined || len == 0) {
        return false;
    }
    if (handle == gBootInCccd || handle == gKbdInCccd) {
        bool on = len >= 2 && (data[0] & 0x01) != 0;
        if (handle == gBootInCccd) {
            gBootNotify = on;
        } else {
            gKbdNotify = on;
        }
        if (on) {
            gConn = conn;
        }
        ucxapp_print("[HID] %s input report notifications %s (conn %d)\n", handle == gBootInCccd ? "boot keyboard" : "keyboard",
                     on ? "enabled" : "disabled", (int)conn);
        return true;
    }
    if (handle == gProtocolMode) {
        gProtocolBoot = data[0] == 0;
        ucxapp_print("[HID] protocol mode: %s\n", gProtocolBoot ? "boot" : "report");
        return true;
    }
    if (handle == gKbdOut || handle == gBootOut) {
        ucxapp_print("[HID] keyboard LEDs 0x%02X (num=%d caps=%d scroll=%d)\n", (unsigned)data[0], (data[0] & 1) != 0,
                     (data[0] & 2) != 0, (data[0] & 4) != 0);
        return true;
    }
    if (handle == gControlPoint) {
        ucxapp_print("[HID] control point: %s\n", data[0] == 0 ? "suspend" : "exit suspend");
        return true;
    }
    return false;
}

/* Defines a characteristic and, if the firmware did not add one, its CCCD. Returns the value handle or -1. */
static int32_t defineChar(ucxapp_t *app, uint16_t uuid, uint8_t props, uGattServerWriteSecurity_t wsec,
                          const uint8_t *init, int32_t initLen, int32_t *cccd)
{
    uint8_t u[2] = { (uint8_t)(uuid >> 8), (uint8_t)(uuid & 0xFF) };
    uCxGattServerCharDefine_t rsp;

    int32_t r = uCxGattServerCharDefine6(app->ucx, u, 2, &props, 1, U_GATT_SERVER_READ_SECURITY_NONE, wsec,
                                         init, initLen, initLen, &rsp);
    if (r != 0) {
        ucxapp_print("ERROR: cannot add characteristic 0x%04X (%d)\n", (unsigned)uuid, (int)r);
        return -1;
    }
    if (cccd != NULL) {
        *cccd = rsp.cccd_handle;
        if (rsp.cccd_handle <= 0) {
            static const uint8_t cccdUuid[2] = { 0x29, 0x02 };
            static const uint8_t off[2] = { 0, 0 };
            if (uCxGattServerDescriptorDefine4(app->ucx, cccdUuid, 2, U_GATT_SERVER_READ_SECURITY_NONE,
                                               U_GATT_SERVER_WRITE_SECURITY_NONE, off, 2, cccd) != 0) {
                ucxapp_print("WARNING: cannot add CCCD for 0x%04X\n", (unsigned)uuid);
            }
        }
    }
    return rsp.value_handle;
}

static bool defineDescriptor(ucxapp_t *app, uint16_t uuid, const uint8_t *value, int32_t len)
{
    uint8_t u[2] = { (uint8_t)(uuid >> 8), (uint8_t)(uuid & 0xFF) };
    int32_t handle;
    int32_t r = uCxGattServerDescriptorDefine4(app->ucx, u, 2, U_GATT_SERVER_READ_SECURITY_NONE,
                                               U_GATT_SERVER_WRITE_SECURITY_NONE, value, len, &handle);
    if (r != 0) {
        ucxapp_print("WARNING: cannot add descriptor 0x%04X (%d)\n", (unsigned)uuid, (int)r);
    }
    return r == 0;
}

static int cmdHidSetup(ucxapp_t *app, int argc, char *argv[])
{
    static const uint8_t kSvcUuid[2] = { 0x18, 0x12 };
    static const uint8_t kExtRef[2] = { 0x2A, 0x19 };   /* External Report Reference: Battery Level */
    static const uint8_t kInRef[2] = { 0x01, 0x01 };    /* report id 1, input */
    static const uint8_t kOutRef[2] = { 0x01, 0x02 };   /* report id 1, output */
    static const uint8_t kProtoReport[1] = { 0x01 };
    static const uint8_t kZero8[8] = { 0 };
    static const uint8_t kZero1[1] = { 0 };
    uCxGattServerCharDefine_t rsp;
    uint8_t props;
    int32_t r;
    bool auth = argc > 1 && strcmp(argv[1], "auth") == 0;
    uGattServerWriteSecurity_t wsec = auth ? U_GATT_SERVER_WRITE_SECURITY_AUTHENTICATED
                                           : U_GATT_SERVER_WRITE_SECURITY_NONE;

    if (argc > 1 && !auth) {
        ucxapp_print("usage: hid.setup [auth]\n");
        return UCXAPP_ERR_USAGE;
    }
    if (gDefined) {
        ucxapp_print("HID service is already defined; restart the module to define it again\n");
        return UCXAPP_ERR_FAILED;
    }

    ucxapp_gatt_ensure_advertising(app);
    uCxBluetoothSetSecurityMode(app->ucx, auth ? U_BT_SECURITY_MODE_AUTHENTICATED : U_BT_SECURITY_MODE_UNAUTHENTICATED);
    uCxBluetoothSetLocalName(app->ucx, "u-blox HID Keyboard");
    uCxBluetoothSetLegacyAdvertisementConfig(app->ucx, 160, 240);

    r = uCxGattServerServiceDefine(app->ucx, kSvcUuid, 2, &gService);
    if (r != 0) {
        ucxapp_print("ERROR: cannot define HID service (%d)\n", (int)r);
        return UCXAPP_ERR_FAILED;
    }

    props = PROP_READ;
    if (uCxGattServerCharDefine6(app->ucx, (const uint8_t[]){ 0x2A, 0x4A }, 2, &props, 1,
                                 U_GATT_SERVER_READ_SECURITY_NONE, U_GATT_SERVER_WRITE_SECURITY_NONE,
                                 kHidInfo, sizeof(kHidInfo), sizeof(kHidInfo), &rsp) != 0) {
        ucxapp_print("ERROR: cannot add HID information\n");
        return UCXAPP_ERR_FAILED;
    }

    gProtocolMode = defineChar(app, 0x2A4E, PROP_READ | PROP_WRITE_NR, wsec,
                               kProtoReport, 1, NULL);

    props = PROP_READ;
    r = uCxGattServerCharDefine6(app->ucx, (const uint8_t[]){ 0x2A, 0x4B }, 2, &props, 1,
                                 U_GATT_SERVER_READ_SECURITY_NONE, U_GATT_SERVER_WRITE_SECURITY_NONE,
                                 kReportMap, sizeof(kReportMap), sizeof(kReportMap), &rsp);
    if (r != 0) {
        ucxapp_print("ERROR: cannot add report map (%d)\n", (int)r);
        return UCXAPP_ERR_FAILED;
    }
    defineDescriptor(app, 0x2907, kExtRef, 2);

    gBootIn = defineChar(app, 0x2A22, PROP_READ | PROP_NOTIFY, wsec, kZero8, 8, &gBootInCccd);
    defineDescriptor(app, 0x2908, kInRef, 2);

    gBootOut = defineChar(app, 0x2A32, PROP_READ | PROP_WRITE_NR | PROP_WRITE, wsec,
                          kZero1, 1, NULL);
    defineDescriptor(app, 0x2908, kOutRef, 2);

    gKbdIn = defineChar(app, 0x2A4D, PROP_READ | PROP_NOTIFY, wsec, kZero8, 8, &gKbdInCccd);
    defineDescriptor(app, 0x2908, kInRef, 2);

    gKbdOut = defineChar(app, 0x2A4D, PROP_READ | PROP_WRITE_NR | PROP_WRITE, wsec,
                         kZero1, 1, NULL);
    defineDescriptor(app, 0x2908, kOutRef, 2);

    gControlPoint = defineChar(app, 0x2A4C, PROP_WRITE_NR, wsec, kZero1, 1, NULL);

    r = uCxGattServerServiceActivate(app->ucx);
    if (r != 0) {
        ucxapp_print("ERROR: cannot activate HID service (%d)\n", (int)r);
        return UCXAPP_ERR_FAILED;
    }
    gDefined = true;
    ucxapp_print("HID keyboard service active\n");

    /* The report map refers to the Battery Level characteristic. */
    if (ucxapp_gatt_server_ensure(app, "bas") != UCXAPP_OK) {
        ucxapp_print("WARNING: battery service not available\n");
    }
    ucxapp_print("pair from the host (Just Works), wait for '[HID] ... notifications enabled', then use hid.key or hid.text\n");
    return UCXAPP_OK;
}

static int cmdHidStatus(ucxapp_t *app, int argc, char *argv[])
{
    (void)app;
    (void)argc;
    (void)argv;

    if (!gDefined) {
        ucxapp_print("hid: not set up (run hid.setup)\n");
        return UCXAPP_OK;
    }
    ucxapp_print("hid: service %d, boot input %d (CCCD %d), keyboard input %d (CCCD %d)\n", (int)gService, (int)gBootIn,
                 (int)gBootInCccd, (int)gKbdIn, (int)gKbdInCccd);
    ucxapp_print("notifications: boot=%s keyboard=%s, protocol=%s, client conn=%d\n", gBootNotify ? "on" : "off",
                 gKbdNotify ? "on" : "off", gProtocolBoot ? "boot" : "report", (int)gConn);
    return UCXAPP_OK;
}

static bool keyFor(char c, uint8_t *mod, uint8_t *code)
{
    *mod = 0;
    if (c >= 'a' && c <= 'z') {
        *code = (uint8_t)(0x04 + (c - 'a'));
    } else if (c >= 'A' && c <= 'Z') {
        *code = (uint8_t)(0x04 + (c - 'A'));
        *mod = HIDMOD_SHIFT;
    } else if (c >= '1' && c <= '9') {
        *code = (uint8_t)(0x1E + (c - '1'));
    } else {
        switch (c) {
        case '0':  *code = 0x27; break;
        case ' ':  *code = 0x2C; break;
        case '\n': *code = 0x28; break;
        case '.':  *code = 0x37; break;
        case ',':  *code = 0x36; break;
        case '-':  *code = 0x2D; break;
        case '/':  *code = 0x38; break;
        case ';':  *code = 0x33; break;
        case '\'': *code = 0x34; break;
        case '!':  *code = 0x1E; *mod = HIDMOD_SHIFT; break;
        case '@':  *code = 0x1F; *mod = HIDMOD_SHIFT; break;
        case '?':  *code = 0x38; *mod = HIDMOD_SHIFT; break;
        case ':':  *code = 0x33; *mod = HIDMOD_SHIFT; break;
        case '_':  *code = 0x2D; *mod = HIDMOD_SHIFT; break;
        default:   return false;
        }
    }
    return true;
}

static bool reportsEnabled(void)
{
    if (!gDefined) {
        ucxapp_print("ERROR: HID not set up (run hid.setup)\n");
        return false;
    }
    if (gConn < 0 || (!gBootNotify && !gKbdNotify)) {
        ucxapp_print("waiting up to 60 s for the host to connect and subscribe...\n");
        int connectedTicks = 0;
        for (int i = 0; i < 120 && (gConn < 0 || (!gBootNotify && !gKbdNotify)); i++) {
            U_CX_PORT_SLEEP_MS(500);
            /* A bonded host reconnects without rewriting its CCCDs, so no URC reports the subscription. */
            if (gConn >= 0 && ++connectedTicks >= 10) {
                ucxapp_print("host connected without a subscribe event; sending anyway\n");
                gKbdNotify = true;
                break;
            }
        }
    }
    if (gConn < 0 || (!gBootNotify && !gKbdNotify)) {
        ucxapp_print("ERROR: the host has not enabled notifications on the input report yet\n");
        return false;
    }
    return true;
}

/* Press and release one key; boot protocol clients get the boot input report. */
static bool sendKey(ucxapp_t *app, uint8_t mod, uint8_t code)
{
    int32_t handle = (gBootNotify && (gProtocolBoot || !gKbdNotify)) ? gBootIn : gKbdIn;
    uint8_t down[8] = { mod, 0, code, 0, 0, 0, 0, 0 };
    uint8_t up[8] = { 0 };

    if (uCxGattServerSendNotification(app->ucx, gConn, handle, down, sizeof(down)) != 0) {
        ucxapp_print("ERROR: key press notification failed\n");
        return false;
    }
    U_CX_PORT_SLEEP_MS(50);
    if (uCxGattServerSendNotification(app->ucx, gConn, handle, up, sizeof(up)) != 0) {
        ucxapp_print("ERROR: key release notification failed\n");
        return false;
    }
    return true;
}

/* hid.key <key> [ctrl|shift|alt|gui ...]: key is one character or enter, esc, tab, backspace, space */
static int cmdHidKey(ucxapp_t *app, int argc, char *argv[])
{
    uint8_t mod = 0;
    uint8_t code = 0;

    if (argc < 2) {
        return UCXAPP_ERR_USAGE;
    }
    if (strcmp(argv[1], "enter") == 0) {
        code = 0x28;
    } else if (strcmp(argv[1], "esc") == 0) {
        code = 0x29;
    } else if (strcmp(argv[1], "backspace") == 0) {
        code = 0x2A;
    } else if (strcmp(argv[1], "tab") == 0) {
        code = 0x2B;
    } else if (strcmp(argv[1], "space") == 0) {
        code = 0x2C;
    } else if (strlen(argv[1]) != 1 || !keyFor(argv[1][0], &mod, &code)) {
        return UCXAPP_ERR_USAGE;
    }
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "ctrl") == 0) {
            mod |= HIDMOD_CTRL;
        } else if (strcmp(argv[i], "shift") == 0) {
            mod |= HIDMOD_SHIFT;
        } else if (strcmp(argv[i], "alt") == 0) {
            mod |= HIDMOD_ALT;
        } else if (strcmp(argv[i], "gui") == 0) {
            mod |= HIDMOD_GUI;
        } else {
            return UCXAPP_ERR_USAGE;
        }
    }
    if (!reportsEnabled() || !sendKey(app, mod, code)) {
        return UCXAPP_ERR_FAILED;
    }
    ucxapp_print("key sent\n");
    return UCXAPP_OK;
}

/* hid.text <text...>: types the text; words are joined with spaces. */
static int cmdHidText(ucxapp_t *app, int argc, char *argv[])
{
    char text[200];
    size_t pos = 0;

    if (argc < 2) {
        return UCXAPP_ERR_USAGE;
    }
    for (int i = 1; i < argc; i++) {
        int n = snprintf(text + pos, sizeof(text) - pos, "%s%s", i > 1 ? " " : "", argv[i]);
        if (n < 0 || (size_t)n >= sizeof(text) - pos) {
            ucxapp_print("ERROR: text too long\n");
            return UCXAPP_ERR_FAILED;
        }
        pos += (size_t)n;
    }
    for (size_t i = 0; i < pos; i++) {
        uint8_t m;
        uint8_t k;
        if (!keyFor(text[i], &m, &k)) {
            ucxapp_print("ERROR: no key mapping for '%c'\n", text[i]);
            return UCXAPP_ERR_FAILED;
        }
    }
    if (!reportsEnabled()) {
        return UCXAPP_ERR_FAILED;
    }
    for (size_t i = 0; i < pos; i++) {
        uint8_t m;
        uint8_t k;
        keyFor(text[i], &m, &k);
        if (!sendKey(app, m, k)) {
            return UCXAPP_ERR_FAILED;
        }
        U_CX_PORT_SLEEP_MS(50);
    }
    ucxapp_print("typed %u character(s)\n", (unsigned)pos);
    return UCXAPP_OK;
}

static const ucxapp_cmd_t gHidCmds[] = {
    { "hid.setup",  "[auth]",                               "Define the HID keyboard service (and battery); 'auth' requires authenticated writes", UCXAPP_CMD_NEEDS_DEVICE, cmdHidSetup },
    { "hid.status", "",                                     "Show HID handles, notification state and protocol mode",               UCXAPP_CMD_NEEDS_DEVICE, cmdHidStatus },
    { "hid.key",    "<key> [ctrl|shift|alt|gui ...]",       "Send one key press to the connected host",                              UCXAPP_CMD_NEEDS_DEVICE, cmdHidKey },
    { "hid.text",   "<text...>",                            "Type text on the connected host",                                       UCXAPP_CMD_NEEDS_DEVICE, cmdHidText },
};

void ucxapp_register_hid_cmds(void)
{
    ucxapp_cmd_register(gHidCmds, sizeof(gHidCmds) / sizeof(gHidCmds[0]));
}
