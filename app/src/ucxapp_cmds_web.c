/* web.start / web.stop / web.status: HTTP + SSE server hosted on the module (NORA-W36 TCP listener).
 * The HTTP engine is u_webserver.c from u-connectMatter; this file adds routes, the output stream and the
 * threads that move socket work out of the URC context. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ucxapp_cmd.h"
#include "ucxapp_thread.h"
#include "ucxapp_ui.h"

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
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<style>:root{--ac:#2f6fed;--bd:#2b3648;--bg2:#0a0d12}body{margin:0;font:14px system-ui,-apple-system,'Segoe UI',sans-serif;background:#0f1419;color:#d6dde6}"
    "header{display:flex;align-items:center;gap:1rem;padding:.55rem 1rem;background:linear-gradient(90deg,#1a2230,#162033);border-bottom:2px solid var(--ac)}"
    "h1{margin:0;font-size:1.05rem;font-weight:600;letter-spacing:.02em}.logo{height:1.9rem;width:auto;background:#fff;border-radius:6px;padding:.25rem .5rem}"
    "#st{font-size:.78rem;padding:.1rem .55rem;border-radius:1rem;background:#2a2410;color:#f2c94c}#st.ok{background:#12301f;color:#6fcf97}#st.bad{background:#3a1717;color:#eb5757}"
    "#busy{margin-left:auto;font-size:.85rem;color:#f2994a}#state{font-size:.85rem;color:#8fa3bf}"
    "main{display:grid;grid-template-columns:19rem 3fr 2fr;gap:.6rem;padding:.6rem;height:calc(100vh - 9rem)}"
    ".pane{display:flex;flex-direction:column;min-height:0}.pane h4{margin:0 0 .3rem;color:#8fa3bf;font-weight:600;font-size:.75rem;text-transform:uppercase;letter-spacing:.08em}"
    "#out,#ev{flex:1;overflow:auto;background:#0a0d12;border:1px solid #2b3648;border-radius:6px;padding:.5rem;"
    "font:13px/1.4 Consolas,monospace}#out{white-space:pre-wrap}.e{white-space:pre-wrap}"
    ".e.wifi,.e.net,.e.sock{color:#6fcf97}.e.mqtt{color:#bb86fc}.e.bt,.e.gatt,.e.sps{color:#f2c94c}.e.http,.e.ping{color:#56ccf2}"
    "footer{padding:0 .6rem .6rem;display:flex;gap:.4rem;flex-wrap:wrap}"
    "#cmd{flex:1;min-width:12rem;padding:.5rem;font:14px Consolas,monospace;background:#0a0d12;color:#d6dde6;"
    "border:1px solid #2b3648;border-radius:6px}"
    "button{padding:.4rem .8rem;background:#233047;color:#d6dde6;border:1px solid var(--bd);border-radius:6px;cursor:pointer;transition:background .15s}"
    "button:hover{background:#2d3f5e}#run{background:var(--ac);border-color:var(--ac)}#cmd:focus,.c input:focus{outline:none;border-color:var(--ac)}"
    "#side{flex:1;overflow:auto;background:#0a0d12;border:1px solid #2b3648;border-radius:6px;padding:.4rem}"
    "summary{cursor:pointer;padding:.25rem;font-weight:600;text-transform:uppercase;font-size:.8rem;color:#8fa3bf}"
    ".c{display:flex;gap:.3rem;flex-wrap:wrap;align-items:center;padding:.3rem 0;border-bottom:1px solid #1d2635}"
    ".c b{width:100%;font:12px Consolas,monospace;color:#8fb4ff;cursor:help}"
    ".c input,.c select{flex:1;min-width:4rem;padding:.25rem;background:#0f1419;color:#d6dde6;"
    "border:1px solid #2b3648;border-radius:4px}.c button{padding:.25rem .6rem}"
    "@media(max-width:800px){main{grid-template-columns:1fr;height:auto}#out,#ev,#side{height:35vh}}</style></head><body>"
    "<header><svg class=\"logo\" viewBox=\"0 0 1024 400\" role=\"img\" aria-label=\"u-blox\">"
    "<circle cx=\"200\" cy=\"200\" r=\"190\" fill=\"#e60000\"/><circle cx=\"165\" cy=\"130\" r=\"32\" fill=\"#fff\"/>"
    "<g font-family=\"Arial,Helvetica,sans-serif\" font-weight=\"700\" font-size=\"300\">"
    "<text x=\"205\" y=\"325\" fill=\"#fff\" textLength=\"190\">u</text>"
    "<text x=\"410\" y=\"325\" fill=\"#1b1b1b\" textLength=\"600\">blox</text></g></svg>"
    "<h1>u-connectClientApp</h1><span id=\"st\">connecting...</span><span id=\"state\"></span><span id=\"busy\"></span>"
    "<button id=\"clr\">Clear</button></header>"
    "<main><div class=\"pane\"><h4>Commands</h4><div id=\"side\"></div></div>"
    "<div class=\"pane\"><h4>Log</h4><div id=\"out\"></div></div>"
    "<div class=\"pane\"><h4>Events</h4><div id=\"ev\"></div></div></main>"
    "<footer><input id=\"cmd\" placeholder=\"command, e.g. wifi.scan (up/down for history)\" autofocus>"
    "<button id=\"run\">Run</button></footer><script>"
    "const $=i=>document.getElementById(i),out=$('out'),ev=$('ev'),st=$('st'),cmd=$('cmd'),"
    "T=new URLSearchParams(location.search).get('t')||'',H=[];let hi=0;"
    "const stick=el=>el.scrollTop+el.clientHeight>=el.scrollHeight-20;"
    "function add(t){const s=stick(out);out.textContent+=t.replace(/\\x1b\\[[0-9;]*m/g,'');"
    "if(out.textContent.length>60000)out.textContent=out.textContent.slice(-40000);if(s)out.scrollTop=out.scrollHeight}"
    "function addEv(t){const s=stick(ev),d=document.createElement('div');d.className='e '+t.split(':')[0];"
    "d.textContent=new Date().toLocaleTimeString()+'  '+t;ev.appendChild(d);"
    "while(ev.children.length>200)ev.removeChild(ev.firstChild);if(s)ev.scrollTop=ev.scrollHeight}"
    "const es=new EventSource('/events?t='+T);"
    "es.onopen=()=>{st.textContent='live';st.className='ok'};"
    "es.onerror=()=>{st.textContent='disconnected';st.className='bad'};"
    "es.addEventListener('out',e=>add(JSON.parse(e.data)));"
    "es.addEventListener('evt',e=>addEv(JSON.parse(e.data)));"
    "es.addEventListener('state',e=>{$('state').textContent=JSON.parse(e.data)});"
    "es.addEventListener('done',e=>{$('busy').textContent='';add('[exit '+e.data+']\\n')});"
    "async function run(c,a,raw){c=c.trim();if(!c)return;const sh=c+(a?' '+a.join(' '):'');"
    "if(H[H.length-1]!==sh)H.push(sh);hi=H.length;add('> '+sh+'\\n');"
    "try{let u='/api/exec?t='+T+'&cmd='+encodeURIComponent(c);"
    "if(a){while(a.length&&!a[a.length-1])a.pop();a.forEach((v,i)=>u+='&a'+i+'='+encodeURIComponent(v));if(raw)u+='&r=1'}"
    "const r=await fetch(u,{method:'POST'});"
    "if(r.status==409)add('busy: a command is still running\\n');"
    "else if(!r.ok)add('error '+r.status+'\\n');else $('busy').textContent='running '+c}"
    "catch(x){add('send failed\\n')}}"
    "$('run').onclick=()=>{run(cmd.value);cmd.value=''};"
    "$('clr').onclick=()=>{out.textContent='';ev.textContent=''};"
    "cmd.onkeydown=e=>{if(e.key==='Enter'){run(cmd.value);cmd.value=''}"
    "else if(e.key==='ArrowUp'&&hi>0){cmd.value=H[--hi];e.preventDefault()}"
    "else if(e.key==='ArrowDown'){hi=Math.min(hi+1,H.length);cmd.value=H[hi]||''}};"
    "const DF={url:'http://example.com',host:'example.com',port:'80',count:'4',topic:'test/ucx',text:'hello',"
    "lat:'59.3293',lon:'18.0686',file:'download.bin',key:'a',command:'AT',seconds:'10'};"
    "function dv(n,k){return n==='sock.bind'&&k==='port'?'5000':DF[k.replace(/[.]+$/,'')]||''}"
    "function mk(c){const d=document.createElement('div'),b=document.createElement('b'),"
    "raw=!c.f,f=c.f||(c.u?[['args',1,'']]:[]),ins=[];"
    "d.className='c';b.textContent=c.n;b.title=c.h+'  '+c.u;d.appendChild(b);"
    "f.forEach(x=>{let e;if(x[2]){e=document.createElement('select');"
    "if(x[1])e.add(new Option('',''));x[2].split('|').forEach(v=>e.add(new Option(v,v)))}"
    "else{e=document.createElement('input');e.placeholder=x[0];e.title=c.u;if(!x[1])e.value=dv(c.n,x[0])}"
    "ins.push(e);d.appendChild(e)});"
    "const g=document.createElement('button');g.textContent='Run';g.title=c.h;"
    "g.onclick=()=>run(c.n,ins.map(e=>e.value.trim()),raw);d.appendChild(g);return d}"
    "fetch('/api/groups?t='+T).then(r=>r.json()).then(gs=>{"
    "gs.forEach(g=>{const d=document.createElement('details'),s=document.createElement('summary');let l=0;"
    "s.textContent=g;d.appendChild(s);d.ontoggle=()=>{if(d.open&&!l){l=1;"
    "fetch('/api/commands?t='+T+'&g='+g).then(r=>r.json()).then(cs=>cs.forEach(c=>d.appendChild(mk(c))))}};"
    "$('side').appendChild(d)})});"
    "</script></body></html>";

static ucxapp_t *gApp;
static uWebServer_t *gServer;
static uint16_t gPort;
static char gToken[TOKEN_LEN + 1];
static volatile bool gRunning;
static volatile bool gStop;
static U_CX_MUTEX_HANDLE gLock;
static ucxapp_thread_t *gPumpThread;

static webEvent_t gEvents[MAX_EVENTS];
static int gEvHead, gEvCount;

static uint32_t gLogCur, gEvCur, gDoneSeen;   /* this front end's positions in the shared streams */

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

/* ---- pump thread: the only thread that touches the HTTP engine ------------ */

/* Builds a JSON string from the start of raw; returns the raw bytes used. */
static int escapeOutput(const char *raw, int rawLen, char *out, size_t outSize)
{
    int used = 0;
    size_t n = 0;

    out[n++] = '"';
    while (used < rawLen && n + 8 < outSize && n < OUT_CHUNK_ESC) {
        unsigned char c = (unsigned char)raw[used];
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
    while (used < rawLen && ((unsigned char)raw[used] & 0xC0) == 0x80 && n + 8 < outSize) {
        out[n++] = raw[used];
        used++;
    }
    out[n++] = '"';
    out[n] = '\0';
    return used;
}

static void flushOutput(void)
{
    char raw[256], json[600];

    if (uWebServerGetSSEClientCount(gServer) <= 0) {
        return;
    }
    for (int round = 0; round < 4; round++) {
        int avail = (int)ucxapp_ui_log_peek(&gLogCur, raw, sizeof(raw));
        if (avail == 0) {
            break;
        }
        int used = escapeOutput(raw, avail, json, sizeof(json));
        if (uWebServerBroadcastSSEEvent(gServer, "out", json) <= 0) {
            return;   /* previous event still draining; retry next pass */
        }
        gLogCur += (uint32_t)used;
    }
}

static void flushEvents(void)
{
    char line[96], json[2 * sizeof(line) + 4];

    if (uWebServerGetSSEClientCount(gServer) <= 0) {
        return;
    }
    for (int round = 0; round < 4 && ucxapp_ui_event_peek(&gEvCur, line, sizeof(line)); round++) {
        size_t n = 0;

        json[n++] = '"';
        for (const char *p = line; *p != '\0'; p++) {
            if (*p == '"' || *p == '\\') {
                json[n++] = '\\';
                json[n++] = *p;
            } else if ((unsigned char)*p >= 0x20) {
                json[n++] = *p;
            }
        }
        json[n++] = '"';
        json[n] = '\0';
        if (uWebServerBroadcastSSEEvent(gServer, "evt", json) <= 0) {
            return;   /* previous event still draining; retry next pass */
        }
        gEvCur++;
    }
}

/* Sent on change and every 5 s so a page that connects later gets the current state. */
static void flushState(void)
{
    static uint32_t seen = UINT32_MAX;
    static int32_t lastMs;
    char text[200], json[210];
    int32_t now = U_CX_PORT_GET_TIME_MS();
    uint32_t version = ucxapp_ui_state(text, sizeof(text));

    if ((version == seen && now - lastMs < 5000) || uWebServerGetSSEClientCount(gServer) <= 0) {
        return;
    }
    snprintf(json, sizeof(json), "\"%s\"", text);
    if (uWebServerBroadcastSSEEvent(gServer, "state", json) > 0) {
        seen = version;
        lastMs = now;
    }
}

static void flushDone(void)
{
    int rc;
    uint32_t seq = ucxapp_ui_done_seq(&rc);
    uint32_t cur = gLogCur;
    char probe[8];

    /* send "done" only after the command's output has been delivered */
    if (seq == gDoneSeen || ucxapp_ui_log_peek(&cur, probe, sizeof(probe)) > 0 ||
        uWebServerGetSSEClientCount(gServer) <= 0) {
        return;
    }
    char text[16];
    snprintf(text, sizeof(text), "%d", rc);
    if (uWebServerBroadcastSSEEvent(gServer, "done", text) > 0) {
        gDoneSeen = seq;
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
        flushEvents();
        flushState();
        flushDone();
        if (U_CX_PORT_GET_TIME_MS() - lastKeepAlive > KEEPALIVE_MS) {
            uWebServerBroadcastSSEEvent(gServer, "ka", "");
            lastKeepAlive = U_CX_PORT_GET_TIME_MS();
        }
        uWebServerProcess(gServer);
        U_CX_PORT_SLEEP_MS(20);
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
    char cmd[MAX_JOB], line[MAX_JOB], vals[UCXAPP_UI_MAX_FIELDS][96], tmp[8];
    const char *ptrs[UCXAPP_UI_MAX_FIELDS];
    int count = 0;
    const char *name = cmd;

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
    while (*name == ' ' || *name == '\t') {
        name++;
    }
    if (*name == '\0' || strncmp(name, "web.", 4) == 0) {
        uWebServerSendError(resp, 400, "command not allowed");
        return;
    }
    for (int i = 0; i < UCXAPP_UI_MAX_FIELDS; i++) {
        char key[8];

        snprintf(key, sizeof(key), "a%d", i);
        ptrs[i] = "";
        if (uWebServerGetQueryParam(req, key, vals[i], sizeof(vals[i])) == 0) {
            urlDecode(vals[i]);
            ptrs[i] = vals[i];
            count = i + 1;
        }
    }
    if (count > 0) {
        bool raw = uWebServerGetQueryParam(req, "r", tmp, sizeof(tmp)) == 0;
        ucxapp_ui_compose(name, ptrs, count, raw, line, sizeof(line));
    } else {
        snprintf(line, sizeof(line), "%s", name);
    }
    if (ucxapp_ui_submit(line)) {
        uWebServerSendText(resp, "accepted");
    } else {
        uWebServerSendError(resp, 409, "busy");
    }
}

/* ---- generated command list for the page ------------------------------------- */

#define API_BUF 16384

static void jsonStr(char *buf, size_t *n, const char *s)
{
    for (; *s != '\0' && *n + 8 < API_BUF; s++) {
        if (*s == '"' || *s == '\\') {
            buf[(*n)++] = '\\';
            buf[(*n)++] = *s;
        } else if ((unsigned char)*s >= 0x20) {
            buf[(*n)++] = *s;
        }
    }
}

/* The response body must outlive the handler, hence static storage; only the pump thread calls this. */
static void routeList(const uHttpRequest_t *req, uHttpResponse_t *resp, void *user)
{
    static char buf[API_BUF];
    static char groups[UCXAPP_UI_MAX_GROUPS][UCXAPP_UI_GROUP_LEN];
    static const ucxapp_cmd_t *cmds[UCXAPP_UI_MAX_GROUP_CMDS];
    char group[16];
    size_t n = 0;
    bool first = true;

    (void)user;
    if (!tokenOk(req)) {
        uWebServerSendError(resp, 403, "token required");
        return;
    }
    buf[n++] = '[';
    if (strcmp(req->path, "/api/groups") == 0) {
        int count = ucxapp_ui_groups(groups, UCXAPP_UI_MAX_GROUPS);

        for (int i = 0; i < count; i++) {
            if (strcmp(groups[i], "web") != 0) {
                n += (size_t)snprintf(buf + n, API_BUF - n, "%s\"%s\"", first ? "" : ",", groups[i]);
                first = false;
            }
        }
    } else {
        if (uWebServerGetQueryParam(req, "g", group, sizeof(group)) != 0) {
            uWebServerSendError(resp, 400, "missing g");
            return;
        }
        int count = ucxapp_ui_group_cmds(group, cmds, UCXAPP_UI_MAX_GROUP_CMDS);

        for (int i = 0; i < count && n + 700 < API_BUF; i++) {
            ucxapp_ui_field_t fields[UCXAPP_UI_MAX_FIELDS];
            int nf = ucxapp_ui_parse_usage(cmds[i]->usage, fields, UCXAPP_UI_MAX_FIELDS);

            if (strncmp(cmds[i]->name, "web.", 4) == 0) {
                continue;
            }
            buf[n++] = first ? ' ' : ',';
            first = false;
            n += (size_t)snprintf(buf + n, API_BUF - n, "{\"n\":\"");
            jsonStr(buf, &n, cmds[i]->name);
            n += (size_t)snprintf(buf + n, API_BUF - n, "\",\"h\":\"");
            jsonStr(buf, &n, cmds[i]->help);
            n += (size_t)snprintf(buf + n, API_BUF - n, "\",\"u\":\"");
            jsonStr(buf, &n, cmds[i]->usage);
            n += (size_t)snprintf(buf + n, API_BUF - n, "\",\"f\":");
            if (nf < 0) {
                n += (size_t)snprintf(buf + n, API_BUF - n, "null");
            } else {
                buf[n++] = '[';
                for (int k = 0; k < nf; k++) {
                    n += (size_t)snprintf(buf + n, API_BUF - n, "%s[\"", k > 0 ? "," : "");
                    jsonStr(buf, &n, fields[k].name);
                    n += (size_t)snprintf(buf + n, API_BUF - n, "\",%d,\"", fields[k].optional ? 1 : 0);
                    jsonStr(buf, &n, fields[k].choices);
                    n += (size_t)snprintf(buf + n, API_BUF - n, "\"]");
                }
                buf[n++] = ']';
            }
            buf[n++] = '}';
        }
    }
    buf[n++] = ']';
    buf[n] = '\0';
    uWebServerSendJSON(resp, buf);
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
    gPumpThread = NULL;
    ucxapp_ui_release();
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
    gEvHead = gEvCount = 0;
    U_CX_MUTEX_CREATE(gLock);

    if (!ucxapp_ui_acquire(app)) {
        ucxapp_print("ERROR: cannot start the command runner\n");
        U_CX_MUTEX_DELETE(gLock);
        return UCXAPP_ERR_FAILED;
    }
    gLogCur = ucxapp_ui_log_start();
    gEvCur = ucxapp_ui_event_start();
    gDoneSeen = ucxapp_ui_done_seq(&(int){ 0 });

    (void)uCxSocketSetReadMode(app->ucx, U_READ_MODE_BUFFERED);
    gServer = uWebServerInit(app->ucx, gPort);
    if (gServer == NULL ||
        uWebServerAddRoute(gServer, U_HTTP_METHOD_GET, "/", routePage, NULL) != 0 ||
        uWebServerAddRoute(gServer, U_HTTP_METHOD_GET, "/events", routeEvents, NULL) != 0 ||
        uWebServerAddRoute(gServer, U_HTTP_METHOD_POST, "/api/exec", routeExec, NULL) != 0 ||
        uWebServerAddRoute(gServer, U_HTTP_METHOD_GET, "/api/groups", routeList, NULL) != 0 ||
        uWebServerAddRoute(gServer, U_HTTP_METHOD_GET, "/api/commands", routeList, NULL) != 0) {
        ucxapp_print("ERROR: cannot create the web server\n");
        uWebServerFree(gServer);
        gServer = NULL;
        ucxapp_ui_release();
        U_CX_MUTEX_DELETE(gLock);
        return UCXAPP_ERR_FAILED;
    }
    int32_t result = uWebServerStart(gServer);
    if (result < 0) {
        ucxapp_print("ERROR: listen on port %d failed (%d%s)\n", (int)port, (int)result, ucxapp_errname(result));
        uWebServerFree(gServer);
        gServer = NULL;
        ucxapp_ui_release();
        U_CX_MUTEX_DELETE(gLock);
        return UCXAPP_ERR_FAILED;
    }

    gRunning = true;
    gPumpThread = ucxapp_thread_start(pumpLoop, NULL);
    if (gPumpThread == NULL) {
        ucxapp_print("ERROR: cannot start the web thread\n");
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
