#include "ucxapp_cmd.h"

#include <string.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4200 4201)
#endif
#include "u_cx_diagnostics.h"
#include "u_cx_socket.h"
#include "u_cx_http.h"
#include "u_cx_wifi.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#define HTTP_SESSION      0
#define HTTP_CHUNK        512
#define HTTP_MAX_BODY     (64 * 1024)   /* bytes read from the module before giving up */
#define HTTP_DEFAULT_SHOW 512

static int cmdNetPing(ucxapp_t *app, int argc, char *argv[])
{
    int32_t count = 4;

    if (argc < 2 || argc > 3 ||
        (argc > 2 && (!ucxapp_parse_int(argv[2], &count) || count < 1 || count > 10))) {
        return UCXAPP_ERR_USAGE;
    }
    ucxapp_events_clear(URC_FLAG_PING_COMPLETE);
    int32_t result = uCxDiagnosticsPing2(app->ucx, argv[1], count);
    if (result != 0) {
        ucxapp_print("ERROR: failed to start ping (%d)\n", (int)result);
        return UCXAPP_ERR_FAILED;
    }
    if (!ucxapp_events_wait(URC_FLAG_PING_COMPLETE, (uint32_t)count * 2 + 5)) {
        ucxapp_print("ERROR: ping timeout\n");
        return UCXAPP_ERR_FAILED;
    }
    ucxapp_print("%s: sent %d, received %d, average %d ms\n", argv[1],
                 (int)gUcxappPingSent, (int)gUcxappPingReceived, (int)gUcxappPingAvgMs);
    return gUcxappPingReceived > 0 ? UCXAPP_OK : UCXAPP_ERR_FAILED;
}

static int cmdNetDns(ucxapp_t *app, int argc, char *argv[])
{
    uSockIpAddress_t addr;
    char text[64];

    if (argc != 2) {
        return UCXAPP_ERR_USAGE;
    }
    int32_t result = uCxSocketGetHostByName(app->ucx, argv[1], &addr);
    if (result != 0) {
        ucxapp_print("ERROR: cannot resolve %s (%d)\n", argv[1], (int)result);
        return UCXAPP_ERR_FAILED;
    }
    uCxIpAddressToString(&addr, text, sizeof(text));
    ucxapp_print("%s = %s\n", argv[1], text);
    return UCXAPP_OK;
}

/* Splits "[http[s]://]host[/path]" in place. */
static bool splitUrl(char *url, bool *https, const char **host, const char **path)
{
    *https = false;
    if (strncmp(url, "https://", 8) == 0) {
        *https = true;
        url += 8;
    } else if (strncmp(url, "http://", 7) == 0) {
        url += 7;
    }
    char *slash = strchr(url, '/');
    if (slash != NULL) {
        memmove(slash + 1, slash, strlen(slash) + 1);   /* room for the host terminator */
        *slash = '\0';
        *path = slash + 1;
    } else {
        *path = "/";
    }
    *host = url;
    return url[0] != '\0';
}

static void printPrintable(const uint8_t *data, int32_t len)
{
    for (int32_t i = 0; i < len; i++) {
        char c = (char)data[i];
        if ((data[i] < 0x20 && c != '\n' && c != '\r') || data[i] > 0x7e) {
            c = '.';
        }
        ucxapp_print("%c", c);
    }
}

static int cmdHttpGet(ucxapp_t *app, int argc, char *argv[])
{
    bool https;
    const char *host;
    const char *path;
    char url[256];
    char hostUrl[200];
    int32_t show = HTTP_DEFAULT_SHOW;
    int32_t err;
    int rc = UCXAPP_ERR_FAILED;

    if (argc < 2 || argc > 3 || strlen(argv[1]) >= sizeof(url) / 2 ||
        (argc > 2 && (!ucxapp_parse_int(argv[2], &show) || show < 0))) {
        return UCXAPP_ERR_USAGE;
    }
    strcpy(url, argv[1]);
    if (!splitUrl(url, &https, &host, &path)) {
        return UCXAPP_ERR_USAGE;
    }

    if (https) {
        snprintf(hostUrl, sizeof(hostUrl), "https://%s", host);
        err = uCxHttpSetTLS2(app->ucx, HTTP_SESSION, U_WIFI_TLS_VERSION_TLS1_2);
        if (err >= 0) {
            err = uCxHttpSetConnectionParams3(app->ucx, HTTP_SESSION, hostUrl, 443);
        }
    } else {
        snprintf(hostUrl, sizeof(hostUrl), "http://%s", host);
        err = uCxHttpSetConnectionParams2(app->ucx, HTTP_SESSION, hostUrl);
    }
    if (err >= 0) {
        err = uCxHttpSetRequestPath(app->ucx, HTTP_SESSION, path);
    }
    if (err < 0) {
        ucxapp_print("ERROR: cannot configure HTTP session (%d)\n", (int)err);
        return UCXAPP_ERR_FAILED;
    }

    ucxapp_events_clear(URC_FLAG_HTTP_RESPONSE_READY);
    err = uCxHttpGetRequest(app->ucx, HTTP_SESSION);
    if (err < 0) {
        ucxapp_print("ERROR: GET request failed (%d)\n", (int)err);
        return UCXAPP_ERR_FAILED;
    }
    if (!ucxapp_events_wait(URC_FLAG_HTTP_RESPONSE_READY, 15)) {
        ucxapp_print("ERROR: timeout waiting for the HTTP response\n");
        goto done;
    }

    /* The first header chunk starts with the status line. */
    uCxHttpGetHeader_t header;
    bool first = true;
    bool more = true;
    while (more) {
        memset(&header, 0, sizeof(header));
        if (!uCxHttpGetHeader2Begin(app->ucx, HTTP_SESSION, HTTP_CHUNK, &header)) {
            ucxapp_print("ERROR: cannot read HTTP headers\n");
            goto done;
        }
        more = header.more_to_read != 0;
        if (first && header.byte_array_data.pData != NULL) {
            const uint8_t *d = header.byte_array_data.pData;
            size_t n = 0;
            while (n < header.byte_array_data.length && d[n] != '\r' && d[n] != '\n') {
                n++;
            }
            ucxapp_print("%.*s\n", (int)n, (const char *)d);
        }
        first = false;
        uCxEnd(app->ucx);
    }

    uint8_t buf[HTTP_CHUNK];
    int32_t total = 0;
    int32_t shown = 0;
    int32_t moreToRead = 1;
    while (moreToRead && total < HTTP_MAX_BODY) {
        int32_t n = uCxHttpGetBody(app->ucx, HTTP_SESSION, HTTP_CHUNK, buf, &moreToRead);
        if (n < 0) {
            ucxapp_print("\nERROR: reading body failed (%d)\n", (int)n);
            goto done;
        }
        total += n;
        if (shown < show) {
            int32_t part = (show - shown < n) ? show - shown : n;
            printPrintable(buf, part);
            shown += part;
        }
    }
    ucxapp_print("\n[%d bytes%s]\n", (int)total, total > shown ? ", output truncated" : "");
    rc = UCXAPP_OK;

done:
    uCxHttpDisconnect(app->ucx, HTTP_SESSION);   /* may fail if the server already closed */
    return rc;
}

static const ucxapp_cmd_t gNetCmds[] = {
    { "net.ping", "<host> [count]",    "ICMP ping (count 1-10, default 4)",     UCXAPP_CMD_NEEDS_DEVICE, cmdNetPing },
    { "net.dns",  "<host>",            "Resolve a host name to an IP address",  UCXAPP_CMD_NEEDS_DEVICE, cmdNetDns },
    { "http.get", "<url> [show_bytes]", "HTTP/HTTPS GET, prints status and body", UCXAPP_CMD_NEEDS_DEVICE, cmdHttpGet },
};

void ucxapp_register_net_cmds(void)
{
    ucxapp_cmd_register(gNetCmds, sizeof(gNetCmds) / sizeof(gNetCmds[0]));
}
