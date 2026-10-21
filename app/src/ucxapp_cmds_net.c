#include "ucxapp_cmd.h"
#include "ucxapp_stats.h"
#include "ucxapp_store.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4200 4201)
#endif
#include "u_cx_diagnostics.h"
#include "u_cx_socket.h"
#include "u_cx_http.h"
#include "u_cx_wifi.h"
#include "u_port.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#define HTTP_SESSION      0
#define HTTP_CHUNK        2048          /* AT+UHTCGBB, binary body read (3.5.0) */
#define HTTP_HDR_CHUNK    1000          /* AT+UHTCGH is a string read, max 1000 */
#define HTTP_MAX_BODY     (64 * 1024)   /* bytes read from the module before giving up */
#define HTTP_DEFAULT_SHOW 512
#define HTTP_MAX_URL      2048          /* u-connectXpress 3.5.0 and later */

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
        ucxapp_print("ERROR: failed to start ping (%d%s)\n", (int)result, ucxapp_errname(result));
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
        ucxapp_print("ERROR: cannot resolve %s (%d%s)\n", argv[1], (int)result, ucxapp_errname(result));
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

static int32_t configureSession(ucxapp_t *app, bool https, const char *host, const char *path)
{
    static char hostUrl[HTTP_MAX_URL + 16];   /* static: keeps the STM32 stack small */
    int32_t err;

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
    return err;
}

static int cmdHttpGet(ucxapp_t *app, int argc, char *argv[])
{
    bool https;
    const char *host;
    const char *path;
    static char url[HTTP_MAX_URL + 2];
    int32_t show = HTTP_DEFAULT_SHOW;
    int32_t err;
    int rc = UCXAPP_ERR_FAILED;

    if (argc < 2 || argc > 3 || strlen(argv[1]) > HTTP_MAX_URL ||
        (argc > 2 && (!ucxapp_parse_int(argv[2], &show) || show < 0))) {
        return UCXAPP_ERR_USAGE;
    }
    strcpy(url, argv[1]);
    if (!splitUrl(url, &https, &host, &path)) {
        return UCXAPP_ERR_USAGE;
    }

    err = configureSession(app, https, host, path);
    if (err < 0) {
        ucxapp_print("ERROR: cannot configure HTTP session (%d%s)\n", (int)err, ucxapp_errname(err));
        return UCXAPP_ERR_FAILED;
    }

    ucxapp_events_clear(URC_FLAG_HTTP_RESPONSE_READY);
    err = uCxHttpGetRequest(app->ucx, HTTP_SESSION);
    if (err < 0) {
        ucxapp_print("ERROR: GET request failed (%d%s)\n", (int)err, ucxapp_errname(err));
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
        if (!uCxHttpGetHeader2Begin(app->ucx, HTTP_SESSION, HTTP_HDR_CHUNK, &header)) {
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
            ucxapp_print("\nERROR: reading body failed (%d%s)\n", (int)n, ucxapp_errname(n));
            goto done;
        }
        total += n;
        ucxapp_stats_add(UCXAPP_STATS_HTTP, 0, (size_t)n);
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

/* Copies the value of a header line (name includes the colon) into out. */
static bool headerValue(const char *headers, const char *name, char *out, size_t outSize)
{
    size_t nameLen = strlen(name);
    for (const char *p = headers; *p;) {
        size_t i = 0;
        while (i < nameLen && p[i] != '\0' && tolower((unsigned char)p[i]) == tolower((unsigned char)name[i])) {
            i++;
        }
        if (i == nameLen) {
            p += nameLen;
            while (*p == ' ' || *p == '\t') {
                p++;
            }
            size_t n = 0;
            while (p[n] != '\0' && p[n] != '\r' && p[n] != '\n') {
                n++;
            }
            if (n == 0 || n >= outSize) {
                return false;
            }
            memcpy(out, p, n);
            out[n] = '\0';
            return true;
        }
        while (*p != '\0' && *p != '\n') {
            p++;
        }
        if (*p != '\0') {
            p++;
        }
    }
    return false;
}

/* One GET hop. Returns 0 on a 2xx response with the body appended to w, 1 for a redirect
   (location filled in), -1 on failure. */
static int downloadHop(ucxapp_t *app, char *url, ucxapp_store_writer_t *w, char *location, size_t locationSize,
                       char *scheme, char *hostOut, size_t hostOutSize)
{
    static char headers[4096];
    bool https;
    const char *host;
    const char *path;
    int rc = -1;

    if (!splitUrl(url, &https, &host, &path)) {
        ucxapp_print("ERROR: invalid URL\n");
        return -1;
    }
    snprintf(scheme, 8, "%s", https ? "https" : "http");
    snprintf(hostOut, hostOutSize, "%s", host);

    int32_t err = configureSession(app, https, host, path);
    if (err >= 0) {
        err = uCxHttpAddHeaderField(app->ucx, HTTP_SESSION, "User-Agent", "u-connectClientApp/1.0");
    }
    if (err < 0) {
        ucxapp_print("ERROR: cannot configure HTTP session (%d%s)\n", (int)err, ucxapp_errname(err));
        return -1;
    }

    ucxapp_events_clear(URC_FLAG_HTTP_RESPONSE_READY);
    err = uCxHttpGetRequest(app->ucx, HTTP_SESSION);
    if (err < 0) {
        ucxapp_print("ERROR: GET request failed (%d%s)\n", (int)err, ucxapp_errname(err));
        return -1;
    }
    if (!ucxapp_events_wait(URC_FLAG_HTTP_RESPONSE_READY, 20)) {
        ucxapp_print("ERROR: timeout waiting for the HTTP response\n");
        goto done;
    }

    size_t hlen = 0;
    bool more = true;
    while (more) {
        uCxHttpGetHeader_t header;
        memset(&header, 0, sizeof(header));
        if (!uCxHttpGetHeader2Begin(app->ucx, HTTP_SESSION, HTTP_HDR_CHUNK, &header)) {
            ucxapp_print("ERROR: cannot read HTTP headers\n");
            goto done;
        }
        more = header.more_to_read != 0;
        if (header.byte_array_data.pData != NULL &&
            hlen + header.byte_array_data.length < sizeof(headers)) {
            memcpy(headers + hlen, header.byte_array_data.pData, header.byte_array_data.length);
            hlen += header.byte_array_data.length;
        }
        uCxEnd(app->ucx);
    }
    headers[hlen] = '\0';

    int status = 0;
    if (strncmp(headers, "HTTP/", 5) == 0) {
        const char *sp = strchr(headers, ' ');
        status = sp != NULL ? atoi(sp + 1) : 0;
    }

    if (headerValue(headers, "Location:", location, locationSize) &&
        (status == 0 || (status >= 300 && status < 400))) {
        ucxapp_print("redirect (HTTP %d)\n", status);
        rc = 1;
        goto done;
    }
    if (status != 0 && (status < 200 || status >= 300)) {
        ucxapp_print("ERROR: HTTP status %d\n", status);
        goto done;
    }

    char lenText[24];
    int32_t expected = -1;
    if (headerValue(headers, "Content-Length:", lenText, sizeof(lenText))) {
        expected = atoi(lenText);
    }

    uint8_t buf[HTTP_CHUNK];
    int32_t total = 0;
    int32_t moreToRead = 1;
    while (moreToRead) {
        int32_t n = uCxHttpGetBody(app->ucx, HTTP_SESSION, HTTP_CHUNK, buf, &moreToRead);
        if (n < 0) {
            ucxapp_print("\nERROR: reading body failed (%d%s)\n", (int)n, ucxapp_errname(n));
            goto done;
        }
        if (!ucxapp_store_append(w, buf, (size_t)n)) {
            ucxapp_print("\nERROR: cannot write file\n");
            goto done;
        }
        total += n;
        ucxapp_stats_add(UCXAPP_STATS_HTTP, 0, (size_t)n);
        if (expected > 0) {
            ucxapp_print("\r%d/%d bytes (%d%%)", (int)total, (int)expected, (int)((int64_t)total * 100 / expected));
        }
    }
    ucxapp_print("\n");
    if (expected >= 0 && total != expected) {
        ucxapp_print("ERROR: incomplete download (%d of %d bytes)\n", (int)total, (int)expected);
        goto done;
    }
    ucxapp_print("%d bytes saved\n", (int)total);
    rc = 0;

done:
    uCxHttpDisconnect(app->ucx, HTTP_SESSION);
    return rc;
}

static int cmdHttpDownload(ucxapp_t *app, int argc, char *argv[])
{
    enum { URL_SIZE = 2048, MAX_REDIRECTS = 5 };
    static char url[URL_SIZE];
    static char location[URL_SIZE];
    char scheme[8];
    char host[256];

    if (argc != 3 || strlen(argv[1]) >= URL_SIZE / 2) {
        return UCXAPP_ERR_USAGE;
    }
    ucxapp_store_writer_t *w = ucxapp_store_begin(argv[2]);
    if (w == NULL) {
        ucxapp_print("ERROR: cannot create %s\n", argv[2]);
        return UCXAPP_ERR_FAILED;
    }
    strcpy(url, argv[1]);

    int rc = UCXAPP_ERR_FAILED;
    for (int hop = 0; hop <= MAX_REDIRECTS; hop++) {
        int r = downloadHop(app, url, w, location, sizeof(location), scheme, host, sizeof(host));
        if (r == 0) {
            rc = UCXAPP_OK;
            break;
        }
        if (r < 0) {
            break;
        }
        int n = location[0] == '/' ? snprintf(url, URL_SIZE, "%s://%s%s", scheme, host, location)
                                   : snprintf(url, URL_SIZE, "%s", location);
        if (n < 0 || n >= URL_SIZE) {
            ucxapp_print("ERROR: redirect URL too long\n");
            break;
        }
        if (hop == MAX_REDIRECTS) {
            ucxapp_print("ERROR: too many redirects\n");
        }
    }
    if (!ucxapp_store_end(w, rc == UCXAPP_OK)) {
        if (rc == UCXAPP_OK) {
            ucxapp_print("ERROR: cannot save %s\n", argv[2]);
        }
        rc = UCXAPP_ERR_FAILED;
    }
    return rc;
}

static void iperfOutput(struct uCxHandle *h, const char *text)
{
    (void)h;
    ucxapp_print("iperf: %s\n", text != NULL ? text : "");
}

static int cmdNetIperf(ucxapp_t *app, int argc, char *argv[])
{
    uDiagProtocolType_t proto = U_DIAG_PROTOCOL_TYPE_TCP;
    uSockIpAddress_t ip;
    int32_t seconds = 10, port = 5001, r;
    bool client = argc > 1 && strcmp(argv[1], "client") == 0;
    int next = client ? 3 : 2;

    if (argc >= 2 && strcmp(argv[1], "stop") == 0 && argc == 2) {
        r = uCxDiagnosticsIperf2(app->ucx, U_DIAG_IPERF_ACTION_STOP, proto);
        ucxapp_print(r == 0 ? "iperf stopped\n" : "ERROR: nothing to stop\n");
        return r == 0 ? UCXAPP_OK : UCXAPP_ERR_FAILED;
    }
    if (argc < 2 || (!client && strcmp(argv[1], "server") != 0) || (client && argc < 3)) {
        return UCXAPP_ERR_USAGE;
    }
    if (argc > next && (strcmp(argv[next], "tcp") == 0 || strcmp(argv[next], "udp") == 0)) {
        proto = argv[next][0] == 't' ? U_DIAG_PROTOCOL_TYPE_TCP : U_DIAG_PROTOCOL_TYPE_UDP;
        next++;
    }
    if (client && argc > next) {
        if (!ucxapp_parse_int(argv[next++], &seconds) || seconds < 1 || seconds > 300) {
            return UCXAPP_ERR_USAGE;
        }
    }
    if (argc > next && (!ucxapp_parse_int(argv[next], &port) || port < 1 || port > 65535)) {
        return UCXAPP_ERR_USAGE;
    }
    if (client && uCxStringToIpAddress(argv[2], &ip) != 0) {
        ucxapp_print("ERROR: give the server as an IP address (net.dns resolves a name)\n");
        return UCXAPP_ERR_FAILED;
    }
    uCxDiagnosticsRegisterIperfOutput(app->ucx, iperfOutput);
    if (client) {
        r = uCxDiagnosticsIperf7(app->ucx, U_DIAG_IPERF_ACTION_START, proto, U_DIAG_ROLE_CLIENT, port, 1, seconds, &ip);
    } else {
        memset(&ip, 0, sizeof(ip));
        r = uCxDiagnosticsIperf5(app->ucx, U_DIAG_IPERF_ACTION_START, proto, U_DIAG_ROLE_SERVER, port, 1);
    }
    if (r != 0) {
        ucxapp_print("ERROR: iperf failed to start (%d%s); the Wi-Fi station must be connected\n", (int)r, ucxapp_errname(r));
        return UCXAPP_ERR_FAILED;
    }
    if (!client) {
        ucxapp_print("iperf server on port %d; results appear here, stop with net.iperf stop\n", (int)port);
        return UCXAPP_OK;
    }
    U_CX_PORT_SLEEP_MS((seconds + 3) * 1000);
    uCxDiagnosticsIperf2(app->ucx, U_DIAG_IPERF_ACTION_STOP, proto);
    return UCXAPP_OK;
}

static const ucxapp_cmd_t gNetCmds[] = {
    { "net.ping", "<host> [count]",    "ICMP ping (count 1-10, default 4)",     UCXAPP_CMD_NEEDS_DEVICE, cmdNetPing },
    { "net.iperf", "<server|client|stop> [args...]", "iperf throughput: 'server [tcp|udp] [port]', 'client <ip> [tcp|udp] [seconds] [port]', 'stop'", UCXAPP_CMD_NEEDS_DEVICE, cmdNetIperf },
    { "net.dns",  "<host>",            "Resolve a host name to an IP address",  UCXAPP_CMD_NEEDS_DEVICE, cmdNetDns },
    { "http.get", "<url> [show_bytes]", "HTTP/HTTPS GET, prints status and body", UCXAPP_CMD_NEEDS_DEVICE, cmdHttpGet },
    { "http.download", "<url> <file>", "Download to a file, following redirects", UCXAPP_CMD_NEEDS_DEVICE, cmdHttpDownload },
};

void ucxapp_register_net_cmds(void)
{
    ucxapp_cmd_register(gNetCmds, sizeof(gNetCmds) / sizeof(gNetCmds[0]));
}
