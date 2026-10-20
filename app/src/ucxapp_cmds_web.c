/* web.start / web.stop / web.status: HTTP + SSE server hosted on the module (NORA-W36 TCP listener).
 * The HTTP engine is u_webserver.c from u-connectMatter; this file adds routes, the output stream and the
 * threads that move socket work out of the URC context. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ucxapp_cmd.h"
#include "ucxapp_thread.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4200 4201)
#endif
#include "u_cx_socket.h"
#include "u_port.h"
#include "u_webserver.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#define DEFAULT_PORT   80
#define TOKEN_LEN      12
#define MAX_EVENTS     64
#define RING_SIZE      8192
#define OUT_CHUNK_ESC  380      /* escaped bytes per SSE event; the engine caps an event at 512 */
#define KEEPALIVE_MS   15000
#define MAX_JOB        256

enum { EVT_INCOMING, EVT_DATA, EVT_CLOSED };

typedef struct {
    int type;
    int32_t sock;
    int32_t n;
} webEvent_t;

static const char kPage[] =
    "<!doctype html><html><head><meta charset=\"utf-8\"><title>u-connectClientApp</title>"
    "<style>body{font-family:sans-serif;margin:1rem;background:#f4f4f4}"
    "#out{background:#111;color:#ddd;padding:.5rem;height:60vh;overflow:auto;white-space:pre-wrap;font:13px monospace}"
    "input{width:60%;padding:.3rem;font:14px monospace}button{padding:.3rem .8rem;margin-right:.3rem}"
    "#st{margin-left:1rem;color:#555}</style></head><body>"
    "<h3>u-connectClientApp <span id=\"st\">connecting...</span></h3>"
    "<div id=\"out\"></div><p><input id=\"cmd\" placeholder=\"command, e.g. device.info\" autofocus> "
    "<button id=\"run\">Run</button> <button data-c=\"help\">help</button>"
    "<button data-c=\"device.info\">device.info</button><button data-c=\"wifi.status\">wifi.status</button>"
    "</p><script>"
    "const out=document.getElementById('out'),st=document.getElementById('st'),cmd=document.getElementById('cmd');"
    "const T=new URLSearchParams(location.search).get('t')||'';"
    "function add(t){out.textContent+=t.replace(/\\x1b\\[[0-9;]*m/g,'');out.scrollTop=out.scrollHeight}"
    "const es=new EventSource('/events?t='+T);"
    "es.onopen=()=>st.textContent='live';es.onerror=()=>st.textContent='disconnected';"
    "es.addEventListener('out',e=>add(JSON.parse(e.data)));"
    "es.addEventListener('done',e=>add('[exit '+e.data+']\\n'));"
    "async function run(c){if(!c)return;add('> '+c+'\\n');"
    "const r=await fetch('/api/exec?t='+T+'&cmd='+encodeURIComponent(c),{method:'POST'});"
    "if(r.status==409)add('busy: a command is still running\\n');else if(!r.ok)add('error '+r.status+'\\n')}"
    "document.getElementById('run').onclick=()=>{run(cmd.value);cmd.value=''};"
    "cmd.onkeydown=e=>{if(e.key==='Enter'){run(cmd.value);cmd.value=''}};"
    "document.querySelectorAll('button[data-c]').forEach(b=>b.onclick=()=>run(b.dataset.c));"
    "</script></body></html>";

static ucxapp_t *gApp;
static uWebServer_t *gServer;
static uint16_t gPort;
static char gToken[TOKEN_LEN + 1];
static volatile bool gRunning;
static volatile bool gStop;
static U_CX_MUTEX_HANDLE gLock;
static ucxapp_thread_t *gPumpThread;
static ucxapp_thread_t *gWorkerThread;

static webEvent_t gEvents[MAX_EVENTS];
static int gEvHead, gEvCount;

static char gRing[RING_SIZE];
static int gRingHead, gRingCount;

static char gJob[MAX_JOB];
static bool gJobPending, gJobRunning, gDonePending;
static int gDoneRc;

/* ---- queues shared with the URC and print contexts ------------------------ */

static void pushEvent(int type, int32_t sock, int32_t n)
{
    if (!gRunning) {
        return;
    }
    U_CX_MUTEX_LOCK(gLock);
    if (gEvCount < MAX_EVENTS) {
        webEvent_t *e = &gEvents[(gEvHead + gEvCount) % MAX_EVENTS];
        e->type = type;
        e->sock = sock;
        e->n = n;
        gEvCount++;
    }
    U_CX_MUTEX_UNLOCK(gLock);
}

void ucxapp_web_on_incoming(int32_t sock, int32_t listenSock)
{
    pushEvent(EVT_INCOMING, sock, listenSock);
}

void ucxapp_web_on_data(int32_t sock, int32_t bytes)
{
    pushEvent(EVT_DATA, sock, bytes);
}

void ucxapp_web_on_closed(int32_t sock)
{
    pushEvent(EVT_CLOSED, sock, 0);
}

static bool popEvent(webEvent_t *e)
{
    bool have = false;

    U_CX_MUTEX_LOCK(gLock);
    if (gEvCount > 0) {
        *e = gEvents[gEvHead];
        gEvHead = (gEvHead + 1) % MAX_EVENTS;
        gEvCount--;
        have = true;
    }
    U_CX_MUTEX_UNLOCK(gLock);
    return have;
}

/* Keeps the newest RING_SIZE bytes so a browser that connects late still sees recent output. */
static void outputSink(const char *text, size_t len)
{
    if (!gRunning) {
        return;
    }
    U_CX_MUTEX_LOCK(gLock);
    for (size_t i = 0; i < len; i++) {
        if (gRingCount == RING_SIZE) {
            gRingHead = (gRingHead + 1) % RING_SIZE;
            gRingCount--;
        }
        gRing[(gRingHead + gRingCount) % RING_SIZE] = text[i];
        gRingCount++;
    }
    U_CX_MUTEX_UNLOCK(gLock);
}

/* ---- pump thread: the only thread that touches the HTTP engine ------------ */

/* Builds a JSON string of the next ring bytes into out; returns raw bytes used (0 = ring empty). */
static int peekOutput(char *out, size_t outSize)
{
    int used = 0;
    size_t n = 0;

    U_CX_MUTEX_LOCK(gLock);
    out[n++] = '"';
    while (used < gRingCount && n + 8 < outSize && n < OUT_CHUNK_ESC) {
        unsigned char c = (unsigned char)gRing[(gRingHead + used) % RING_SIZE];
        if (c == '"' || c == '\\') {
            out[n++] = '\\';
            out[n++] = (char)c;
        } else if (c == '\n') {
            out[n++] = '\\';
            out[n++] = 'n';
        } else if (c == '\t') {
            out[n++] = '\\';
            out[n++] = 't';
        } else if (c == '\r') {
            /* dropped */
        } else if (c < 0x20) {
            n += (size_t)snprintf(out + n, outSize - n, "\\u%04x", c);
        } else {
            out[n++] = (char)c;
        }
        used++;
    }
    /* do not split a UTF-8 sequence */
    while (used < gRingCount && ((unsigned char)gRing[(gRingHead + used) % RING_SIZE] & 0xC0) == 0x80 &&
           n + 8 < outSize) {
        out[n++] = gRing[(gRingHead + used) % RING_SIZE];
        used++;
    }
    out[n++] = '"';
    out[n] = '\0';
    U_CX_MUTEX_UNLOCK(gLock);
    return used;
}

static void consumeOutput(int n)
{
    U_CX_MUTEX_LOCK(gLock);
    gRingHead = (gRingHead + n) % RING_SIZE;
    gRingCount -= n;
    U_CX_MUTEX_UNLOCK(gLock);
}

static void flushOutput(void)
{
    char json[600];

    if (uWebServerGetSSEClientCount(gServer) <= 0) {
        return;
    }
    for (int round = 0; round < 4; round++) {
        int used = peekOutput(json, sizeof(json));
        if (used == 0) {
            break;
        }
        if (uWebServerBroadcastSSEEvent(gServer, "out", json) <= 0) {
            return;   /* previous event still draining; retry next pass */
        }
        consumeOutput(used);
    }
}

static void flushDone(void)
{
    int rc = 0;
    bool send = false;
    bool ringEmpty;

    U_CX_MUTEX_LOCK(gLock);
    ringEmpty = gRingCount == 0;
    if (gDonePending && ringEmpty) {
        rc = gDoneRc;
        send = true;
    }
    U_CX_MUTEX_UNLOCK(gLock);
    if (send && uWebServerGetSSEClientCount(gServer) > 0) {
        char text[16];
        snprintf(text, sizeof(text), "%d", rc);
        if (uWebServerBroadcastSSEEvent(gServer, "done", text) <= 0) {
            return;
        }
    }
    if (send) {
        U_CX_MUTEX_LOCK(gLock);
        gDonePending = false;
        U_CX_MUTEX_UNLOCK(gLock);
    }
}

static void pumpLoop(void *arg)
{
    int32_t lastKeepAlive = U_CX_PORT_GET_TIME_MS();

    (void)arg;
    while (!gStop) {
        webEvent_t e;
        while (!gStop && popEvent(&e)) {
            switch (e.type) {
            case EVT_INCOMING: {
                /* u-connectXpress 3.5.0 reports the listening socket first in +UESOIC. */
                int32_t listen = uWebServerGetListenSocket(gServer);
                int32_t client = (e.sock == listen) ? e.n : e.sock;
                uWebServerHandleIncomingConnection(gServer, gApp->ucx, client, NULL, listen);
                break;
            }
            case EVT_DATA:
                uWebServerHandleDataAvailable(gServer, gApp->ucx, e.sock, e.n);
                break;
            default:
                uWebServerHandleSocketClosed(gServer, gApp->ucx, e.sock);
                break;
            }
        }
        flushOutput();
        flushDone();
        if (U_CX_PORT_GET_TIME_MS() - lastKeepAlive > KEEPALIVE_MS) {
            uWebServerBroadcastSSEEvent(gServer, "ka", "");
            lastKeepAlive = U_CX_PORT_GET_TIME_MS();
        }
        uWebServerProcess(gServer);
        U_CX_PORT_SLEEP_MS(20);
    }
}

/* ---- worker thread: runs commands posted from the browser ----------------- */

static void workerLoop(void *arg)
{
    char line[MAX_JOB];

    (void)arg;
    while (!gStop) {
        bool have = false;

        U_CX_MUTEX_LOCK(gLock);
        if (gJobPending) {
            memcpy(line, gJob, sizeof(line));
            gJobPending = false;
            gJobRunning = true;
            have = true;
        }
        U_CX_MUTEX_UNLOCK(gLock);
        if (!have) {
            U_CX_PORT_SLEEP_MS(20);
            continue;
        }
        int rc = ucxapp_cmd_exec(gApp, line);
        U_CX_MUTEX_LOCK(gLock);
        gJobRunning = false;
        gDoneRc = rc;
        gDonePending = true;
        U_CX_MUTEX_UNLOCK(gLock);
    }
}

/* ---- routes ---------------------------------------------------------------- */

static bool tokenOk(const uHttpRequest_t *req)
{
    char t[TOKEN_LEN + 8];

    return uWebServerGetQueryParam(req, "t", t, sizeof(t)) == 0 && strcmp(t, gToken) == 0;
}

static int hexVal(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static void urlDecode(char *s)
{
    char *w = s;

    for (const char *r = s; *r != '\0'; r++) {
        if (*r == '+') {
            *w++ = ' ';
        } else if (*r == '%' && hexVal(r[1]) >= 0 && hexVal(r[2]) >= 0) {
            *w++ = (char)(hexVal(r[1]) * 16 + hexVal(r[2]));
            r += 2;
        } else {
            *w++ = *r;
        }
    }
    *w = '\0';
}

static void routePage(const uHttpRequest_t *req, uHttpResponse_t *resp, void *user)
{
    (void)user;
    if (!tokenOk(req)) {
        uWebServerSendError(resp, 403, "token required");
        return;
    }
    uWebServerSendHTML(resp, kPage);
}

static void routeEvents(const uHttpRequest_t *req, uHttpResponse_t *resp, void *user)
{
    (void)user;
    if (!tokenOk(req)) {
        uWebServerSendError(resp, 403, "token required");
        return;
    }
    if (uWebServerRegisterSSE(gServer, req, resp) < 0) {
        uWebServerSendError(resp, 500, "no event slot");
    }
}

static void routeExec(const uHttpRequest_t *req, uHttpResponse_t *resp, void *user)
{
    char cmd[MAX_JOB];
    bool busy;

    (void)user;
    if (!tokenOk(req)) {
        uWebServerSendError(resp, 403, "token required");
        return;
    }
    if (uWebServerGetQueryParam(req, "cmd", cmd, sizeof(cmd)) != 0) {
        uWebServerSendError(resp, 400, "missing cmd");
        return;
    }
    urlDecode(cmd);
    if (cmd[0] == '\0' || strncmp(cmd, "web.", 4) == 0) {
        uWebServerSendError(resp, 400, "command not allowed");
        return;
    }
    U_CX_MUTEX_LOCK(gLock);
    busy = gJobPending || gJobRunning;
    if (!busy) {
        memcpy(gJob, cmd, sizeof(gJob));
        gJobPending = true;
    }
    U_CX_MUTEX_UNLOCK(gLock);
    if (busy) {
        uWebServerSendError(resp, 409, "busy");
    } else {
        uWebServerSendText(resp, "accepted");
    }
}

/* ---- commands --------------------------------------------------------------- */

static void makeToken(char *out)
{
    srand((unsigned)time(NULL) ^ (unsigned)U_CX_PORT_GET_TIME_MS());
    for (int i = 0; i < TOKEN_LEN; i += 2) {
        snprintf(out + i, 3, "%02x", (unsigned)(rand() & 0xff));
    }
    out[TOKEN_LEN] = '\0';
}

static void webStop(void)
{
    gRunning = false;
    gStop = true;
    ucxapp_thread_join(gPumpThread);
    ucxapp_thread_join(gWorkerThread);
    gPumpThread = NULL;
    gWorkerThread = NULL;
    ucxapp_io_remove_sink(outputSink);
    uWebServerFree(gServer);
    gServer = NULL;
    U_CX_MUTEX_DELETE(gLock);
}

static int cmdWebStart(ucxapp_t *app, int argc, char *argv[])
{
    int32_t port = DEFAULT_PORT;
    char ssid[64], ip[48];

    if (argc > 3 || (argc > 1 && (!ucxapp_parse_int(argv[1], &port) || port < 1 || port > 65535))) {
        return UCXAPP_ERR_USAGE;
    }
    if (gServer != NULL) {
        ucxapp_print("already running; web.stop first\n");
        return UCXAPP_ERR_FAILED;
    }
    if (!ucxapp_wifi_get_status(app->ucx, ssid, sizeof(ssid), ip, sizeof(ip)) || ip[0] == '\0') {
        ucxapp_print("ERROR: connect Wi-Fi first (wifi.connect)\n");
        return UCXAPP_ERR_FAILED;
    }
    if (argc == 3) {
        snprintf(gToken, sizeof(gToken), "%s", argv[2]);
    } else {
        makeToken(gToken);
    }

    gApp = app;
    gPort = (uint16_t)port;
    gStop = false;
    gEvHead = gEvCount = gRingHead = gRingCount = 0;
    gJobPending = gJobRunning = gDonePending = false;
    U_CX_MUTEX_CREATE(gLock);

    (void)uCxSocketSetReadMode(app->ucx, U_READ_MODE_BUFFERED);
    gServer = uWebServerInit(app->ucx, gPort);
    if (gServer == NULL ||
        uWebServerAddRoute(gServer, U_HTTP_METHOD_GET, "/", routePage, NULL) != 0 ||
        uWebServerAddRoute(gServer, U_HTTP_METHOD_GET, "/events", routeEvents, NULL) != 0 ||
        uWebServerAddRoute(gServer, U_HTTP_METHOD_POST, "/api/exec", routeExec, NULL) != 0) {
        ucxapp_print("ERROR: cannot create the web server\n");
        uWebServerFree(gServer);
        gServer = NULL;
        U_CX_MUTEX_DELETE(gLock);
        return UCXAPP_ERR_FAILED;
    }
    int32_t result = uWebServerStart(gServer);
    if (result < 0) {
        ucxapp_print("ERROR: listen on port %d failed (%d)\n", (int)port, (int)result);
        uWebServerFree(gServer);
        gServer = NULL;
        U_CX_MUTEX_DELETE(gLock);
        return UCXAPP_ERR_FAILED;
    }

    gRunning = true;
    ucxapp_io_add_sink(outputSink);
    gPumpThread = ucxapp_thread_start(pumpLoop, NULL);
    gWorkerThread = ucxapp_thread_start(workerLoop, NULL);
    if (gPumpThread == NULL || gWorkerThread == NULL) {
        ucxapp_print("ERROR: cannot start the web threads\n");
        webStop();
        return UCXAPP_ERR_FAILED;
    }
    ucxapp_print("web server on %s: http://%s:%d/?t=%s\n", ssid, ip, (int)port, gToken);
    return UCXAPP_OK;
}

static int cmdWebStop(ucxapp_t *app, int argc, char *argv[])
{
    (void)app;
    (void)argc;
    (void)argv;
    if (gServer == NULL) {
        ucxapp_print("not running\n");
        return UCXAPP_OK;
    }
    webStop();
    ucxapp_print("web server stopped\n");
    return UCXAPP_OK;
}

static int cmdWebStatus(ucxapp_t *app, int argc, char *argv[])
{
    (void)app;
    (void)argc;
    (void)argv;
    if (gServer == NULL) {
        ucxapp_print("web server: stopped\n");
    } else {
        ucxapp_print("web server: port %u, %d event client(s)\n", (unsigned)gPort,
                     (int)uWebServerGetSSEClientCount(gServer));
    }
    return UCXAPP_OK;
}

static const ucxapp_cmd_t gWebCmds[] = {
    { "web.start",  "[port] [token]", "Serve a web UI from the module (needs Wi-Fi); default port 80", UCXAPP_CMD_NEEDS_DEVICE, cmdWebStart },
    { "web.stop",   "",               "Stop the web server",                                          0, cmdWebStop },
    { "web.status", "",               "Show web server state",                                        0, cmdWebStatus },
};

void ucxapp_register_web_cmds(void)
{
    ucxapp_cmd_register(gWebCmds, sizeof(gWebCmds) / sizeof(gWebCmds[0]));
}
