#include "ucxapp_cmd.h"

#include <stdio.h>
#include <string.h>

#include "qrcodegen/qrcodegen.h"

#define QR_BORDER 2

/* Escape characters that are special in a WIFI: payload. */
static size_t escapeWifi(const char *in, char *out, size_t size)
{
    size_t n = 0;

    for (; *in != '\0'; in++) {
        bool special = strchr("\\;,:\"", *in) != NULL;
        if (n + (special ? 2 : 1) + 1 > size) {
            return 0;
        }
        if (special) {
            out[n++] = '\\';
        }
        out[n++] = *in;
    }
    out[n] = '\0';
    return n;
}

bool ucxapp_qr_print(const char *text)
{
    static uint8_t qr[qrcodegen_BUFFER_LEN_MAX];
    static uint8_t tmp[qrcodegen_BUFFER_LEN_MAX];

    if (!qrcodegen_encodeText(text, tmp, qr, qrcodegen_Ecc_LOW, qrcodegen_VERSION_MIN,
                              qrcodegen_VERSION_MAX, qrcodegen_Mask_AUTO, true)) {
        ucxapp_print("ERROR: text too long for a QR code\n");
        return false;
    }
    int size = qrcodegen_getSize(qr);
    for (int y = -QR_BORDER; y < size + QR_BORDER; y++) {
        for (int x = -QR_BORDER; x < size + QR_BORDER; x++) {
            ucxapp_print(qrcodegen_getModule(qr, x, y) ? "\xE2\x96\x88\xE2\x96\x88" : "  ");
        }
        ucxapp_print("\n");
    }
    return true;
}

bool ucxapp_qr_print_wifi(const char *ssid, const char *password, const char *security, bool hidden)
{
    char s[128];
    char p[128];
    char payload[320];

    if (escapeWifi(ssid, s, sizeof(s)) == 0 || escapeWifi(password, p, sizeof(p)) == 0) {
        ucxapp_print("ERROR: SSID or password too long\n");
        return false;
    }
    if (strcmp(security, "nopass") == 0) {
        snprintf(payload, sizeof(payload), "WIFI:T:nopass;S:%s;%s;", s, hidden ? "H:true;" : "");
    } else {
        snprintf(payload, sizeof(payload), "WIFI:T:%s;S:%s;P:%s;%s;", security, s, p, hidden ? "H:true;" : "");
    }
    ucxapp_print("%s\n", payload);
    return ucxapp_qr_print(payload);
}

static int cmdQrText(ucxapp_t *app, int argc, char *argv[])
{
    (void)app;
    if (argc != 2) {
        return UCXAPP_ERR_USAGE;
    }
    return ucxapp_qr_print(argv[1]) ? UCXAPP_OK : UCXAPP_ERR_FAILED;
}

static int cmdQrWifi(ucxapp_t *app, int argc, char *argv[])
{
    const char *security = "WPA";
    bool hidden = false;
    (void)app;

    if (argc < 2 || argc > 5) {
        return UCXAPP_ERR_USAGE;
    }
    if (argc > 3) {
        security = argv[3];
        if (strcmp(security, "WPA") != 0 && strcmp(security, "WEP") != 0 && strcmp(security, "nopass") != 0) {
            return UCXAPP_ERR_USAGE;
        }
    }
    if (argc > 4) {
        if (strcmp(argv[4], "hidden") != 0) {
            return UCXAPP_ERR_USAGE;
        }
        hidden = true;
    }
    return ucxapp_qr_print_wifi(argv[1], argc > 2 ? argv[2] : "", argc > 2 ? security : "nopass", hidden)
               ? UCXAPP_OK : UCXAPP_ERR_FAILED;
}

static int cmdQrGeo(ucxapp_t *app, int argc, char *argv[])
{
    char url[128];
    (void)app;

    if (argc != 3) {
        return UCXAPP_ERR_USAGE;
    }
    snprintf(url, sizeof(url), "https://www.google.com/maps?q=%s,%s", argv[1], argv[2]);
    ucxapp_print("%s\n", url);
    return ucxapp_qr_print(url) ? UCXAPP_OK : UCXAPP_ERR_FAILED;
}

static const ucxapp_cmd_t gQrCmds[] = {
    { "utils.qr.text", "<text>",                                   "Print any text as a QR code",                    0, cmdQrText },
    { "utils.qr.wifi", "<ssid> [password] [WPA|WEP|nopass] [hidden]", "Print a Wi-Fi join QR code (WIFI: payload)",  0, cmdQrWifi },
    { "utils.qr.geo",  "<lat> <lon>",                              "Print a Google Maps location QR code",           0, cmdQrGeo },
};

void ucxapp_register_qr_cmds(void)
{
    ucxapp_cmd_register(gQrCmds, sizeof(gQrCmds) / sizeof(gQrCmds[0]));
}
