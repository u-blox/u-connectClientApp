#include "ucxapp_adv.h"

#include <stdio.h>
#include <string.h>

#include "ucxapp_cmd.h"
#include "../third-party/bluetooth-sig/bt_service_uuids.h"
#include "../third-party/bluetooth-sig/bt_company_identifiers.h"
#include "../third-party/bluetooth-sig/bt_appearance_values.h"

#define AD_FLAGS         0x01
#define AD_UUID16_MORE   0x02
#define AD_UUID16_ALL    0x03
#define AD_UUID32_MORE   0x04
#define AD_UUID32_ALL    0x05
#define AD_UUID128_MORE  0x06
#define AD_UUID128_ALL   0x07
#define AD_NAME_SHORT    0x08
#define AD_NAME_FULL     0x09
#define AD_TX_POWER      0x0A
#define AD_SERVICE_DATA  0x16
#define AD_APPEARANCE    0x19
#define AD_URI           0x24
#define AD_MANUFACTURER  0xFF

#define COMPANY_APPLE    0x004C
#define UUID_EDDYSTONE   0xFEAA
#define HEX_SHOWN        16

/* Walks the structures; false at the end or on a malformed length. */
static bool nextAd(const uint8_t *d, size_t len, size_t *off, uint8_t *type, const uint8_t **val, size_t *vlen)
{
    size_t l;

    if (*off >= len || d[*off] == 0) {
        return false;
    }
    l = d[*off];
    if (*off + l >= len) {
        return false;
    }
    *type = d[*off + 1];
    *val = &d[*off + 2];
    *vlen = l - 1;
    *off += l + 1;
    return true;
}

static uint16_t le16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static void printHex(const uint8_t *p, size_t n)
{
    for (size_t i = 0; i < n && i < HEX_SHOWN; i++) {
        ucxapp_print("%02X ", p[i]);
    }
    if (n > HEX_SHOWN) {
        ucxapp_print("...");
    }
}

static void printUuid128(const uint8_t *p)
{
    for (int j = 15; j >= 0; j--) {
        ucxapp_print("%02X%s", p[j], (j == 12 || j == 10 || j == 8 || j == 6) ? "-" : "");
    }
}

static void printUuidList(const uint8_t *v, size_t n, size_t width, const char *indent)
{
    for (size_t i = 0; i + width <= n; i += width) {
        ucxapp_print("%s  ", indent);
        if (width == 2) {
            const char *name = btGetServiceName(le16(&v[i]));

            ucxapp_print("0x%04X%s%s%s\n", le16(&v[i]), name != NULL ? " (" : "", name != NULL ? name : "",
                         name != NULL ? ")" : "");
        } else if (width == 4) {
            ucxapp_print("0x%04X%04X\n", le16(&v[i + 2]), le16(&v[i]));
        } else {
            printUuid128(&v[i]);
            ucxapp_print("\n");
        }
    }
}

static void printFlags(uint8_t f)
{
    ucxapp_print("0x%02X", f);
    if (f & 0x01) {
        ucxapp_print(" [limited discoverable]");
    }
    if (f & 0x02) {
        ucxapp_print(" [general discoverable]");
    }
    if (f & 0x04) {
        ucxapp_print(" [no BR/EDR]");
    }
    if (f & 0x18) {
        ucxapp_print(" [LE and BR/EDR]");
    }
}

static void printUri(const uint8_t *v, size_t n)
{
    static const char *const scheme[] = { "aaa:", "aaas:", "about:", "acap:", "acct:", "cap:", "cid:", "coap:", "coaps:",
        "crid:", "data:", "dav:", "dict:", "dns:", "file:", "ftp:", "geo:", "go:", "gopher:", "h323:", "http:", "https:" };

    if (n > 0 && v[0] < sizeof(scheme) / sizeof(scheme[0])) {
        ucxapp_print("%s", scheme[v[0]]);
    }
    for (size_t i = 1; i < n; i++) {
        ucxapp_print("%c", v[i] >= 0x20 && v[i] < 0x7F ? v[i] : '.');
    }
}

void ucxapp_adv_print(const uint8_t *data, size_t len, const char *indent)
{
    size_t off = 0;
    uint8_t type;
    const uint8_t *v;
    size_t n;

    while (nextAd(data, len, &off, &type, &v, &n)) {
        switch (type) {
        case AD_FLAGS:
            ucxapp_print("%sFlags: ", indent);
            if (n > 0) {
                printFlags(v[0]);
            }
            ucxapp_print("\n");
            break;
        case AD_UUID16_MORE:
        case AD_UUID16_ALL:
            ucxapp_print("%s%s16-bit services:\n", indent, type == AD_UUID16_ALL ? "" : "Some ");
            printUuidList(v, n, 2, indent);
            break;
        case AD_UUID32_MORE:
        case AD_UUID32_ALL:
            ucxapp_print("%s%s32-bit services:\n", indent, type == AD_UUID32_ALL ? "" : "Some ");
            printUuidList(v, n, 4, indent);
            break;
        case AD_UUID128_MORE:
        case AD_UUID128_ALL:
            ucxapp_print("%s%s128-bit services:\n", indent, type == AD_UUID128_ALL ? "" : "Some ");
            printUuidList(v, n, 16, indent);
            break;
        case AD_NAME_SHORT:
        case AD_NAME_FULL:
            ucxapp_print("%s%s name: %.*s\n", indent, type == AD_NAME_FULL ? "Complete" : "Short", (int)n, (const char *)v);
            break;
        case AD_TX_POWER:
            if (n > 0) {
                ucxapp_print("%sTX power: %d dBm\n", indent, (int)(int8_t)v[0]);
            }
            break;
        case AD_APPEARANCE:
            if (n >= 2) {
                const char *name = btGetAppearanceName(le16(v));

                ucxapp_print("%sAppearance: 0x%04X%s%s%s\n", indent, le16(v), name != NULL ? " (" : "",
                             name != NULL ? name : "", name != NULL ? ")" : "");
            }
            break;
        case AD_SERVICE_DATA:
            if (n >= 2) {
                const char *name = btGetServiceName(le16(v));

                ucxapp_print("%sService data 0x%04X%s%s%s: ", indent, le16(v), name != NULL ? " (" : "",
                             name != NULL ? name : "", name != NULL ? ")" : "");
                printHex(v + 2, n - 2);
                ucxapp_print("\n");
            }
            break;
        case AD_URI:
            ucxapp_print("%sURI: ", indent);
            printUri(v, n);
            ucxapp_print("\n");
            break;
        case AD_MANUFACTURER:
            if (n >= 2) {
                const char *name = btGetCompanyName(le16(v));

                ucxapp_print("%sManufacturer 0x%04X%s%s%s: ", indent, le16(v), name != NULL ? " (" : "",
                             name != NULL ? name : "", name != NULL ? ")" : "");
                printHex(v + 2, n - 2);
                ucxapp_print("\n");
            }
            break;
        default:
            ucxapp_print("%sType 0x%02X (%d bytes): ", indent, type, (int)n);
            printHex(v, n);
            ucxapp_print("\n");
            break;
        }
    }
}

static void append(char *out, size_t size, const char *sep, const char *text)
{
    size_t used = strlen(out);

    if (text[0] == '\0') {
        return;
    }
    if (used + 1 < size) {
        snprintf(out + used, size - used, "%s%s", used > 0 ? sep : "", text);
    }
}

void ucxapp_adv_summary(const uint8_t *data, size_t len, char *out, size_t size)
{
    char services[96] = "", who[64] = "", what[48] = "";
    int serviceCount = 0;
    size_t off = 0;
    uint8_t type;
    const uint8_t *v;
    size_t n;

    out[0] = '\0';
    while (nextAd(data, len, &off, &type, &v, &n)) {
        if (type == AD_UUID16_MORE || type == AD_UUID16_ALL) {
            for (size_t i = 0; i + 2 <= n; i += 2) {
                const char *name = btGetServiceName(le16(&v[i]));
                char hex[8];

                if (name == NULL) {
                    snprintf(hex, sizeof(hex), "0x%04X", le16(&v[i]));
                }
                if (serviceCount++ < 3) {
                    append(services, sizeof(services), ", ", name != NULL ? name : hex);
                }
            }
        } else if (type == AD_MANUFACTURER && n >= 2) {
            const char *name = btGetCompanyName(le16(v));

            snprintf(who, sizeof(who), "%s", name != NULL ? name : "");
            if (le16(v) == COMPANY_APPLE && n >= 4 && v[2] == 0x02 && v[3] == 0x15) {
                append(who, sizeof(who), " ", "iBeacon");
            }
        } else if (type == AD_SERVICE_DATA && n >= 2 && le16(v) == UUID_EDDYSTONE) {
            snprintf(who, sizeof(who), "Eddystone");
        } else if (type == AD_APPEARANCE && n >= 2) {
            const char *name = btGetAppearanceName(le16(v));

            snprintf(what, sizeof(what), "%s", name != NULL ? name : "");
        }
    }
    if (serviceCount > 3) {
        append(services, sizeof(services), "", " ...");
    }
    append(out, size, " | ", services);
    append(out, size, " | ", who);
    append(out, size, " | ", what);
}

bool ucxapp_adv_local_name(const uint8_t *data, size_t len, char *out, size_t size)
{
    size_t off = 0;
    uint8_t type;
    const uint8_t *v;
    size_t n;
    bool found = false;

    out[0] = '\0';
    while (nextAd(data, len, &off, &type, &v, &n)) {
        if ((type == AD_NAME_FULL || (type == AD_NAME_SHORT && !found)) && n > 0) {
            snprintf(out, size, "%.*s", (int)n, (const char *)v);
            found = true;
            if (type == AD_NAME_FULL) {
                break;
            }
        }
    }
    return found;
}
