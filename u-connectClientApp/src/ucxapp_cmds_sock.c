#include "ucxapp_cmd.h"

#include <string.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4200 4201)
#endif
#include "u_cx_socket.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#define SOCK_MAX_READ 512

static bool parseHandle(const char *s, int32_t *handle)
{
    return ucxapp_parse_int(s, handle) && *handle >= 0;
}

static int cmdSockOpen(ucxapp_t *app, int argc, char *argv[])
{
    uSocketProtocol_t protocol;
    int32_t handle = -1;

    if (argc != 2) {
        return UCXAPP_ERR_USAGE;
    }
    if (strcmp(argv[1], "tcp") == 0) {
        protocol = U_SOCKET_PROTOCOL_TCP;
    } else if (strcmp(argv[1], "udp") == 0) {
        protocol = U_SOCKET_PROTOCOL_UDP;
    } else {
        return UCXAPP_ERR_USAGE;
    }
    if (uCxSocketCreate1(app->ucx, protocol, &handle) != 0) {
        ucxapp_print("ERROR: failed to create socket\n");
        return UCXAPP_ERR_FAILED;
    }
    ucxapp_print("socket %d\n", (int)handle);
    return UCXAPP_OK;
}

static int cmdSockConnect(ucxapp_t *app, int argc, char *argv[])
{
    int32_t handle;
    int32_t port;

    if (argc != 4 || !parseHandle(argv[1], &handle) ||
        !ucxapp_parse_int(argv[3], &port) || port < 1 || port > 65535) {
        return UCXAPP_ERR_USAGE;
    }
    ucxapp_events_clear(URC_FLAG_SOCK_CONNECTED);
    int32_t result = uCxSocketConnect(app->ucx, handle, argv[2], port);
    if (result != 0) {
        ucxapp_print("ERROR: connect failed (%d)\n", (int)result);
        return UCXAPP_ERR_FAILED;
    }
    if (!ucxapp_events_wait(URC_FLAG_SOCK_CONNECTED, 10)) {
        ucxapp_print("ERROR: timeout waiting for socket connection\n");
        return UCXAPP_ERR_FAILED;
    }
    ucxapp_print("socket %d connected to %s:%d\n", (int)handle, argv[2], (int)port);
    return UCXAPP_OK;
}

static int cmdSockSend(ucxapp_t *app, int argc, char *argv[])
{
    int32_t handle;
    char text[SOCK_MAX_READ];
    size_t len = 0;

    if (argc < 3 || !parseHandle(argv[1], &handle)) {
        return UCXAPP_ERR_USAGE;
    }
    for (int i = 2; i < argc; i++) {
        size_t n = strlen(argv[i]);
        if (len + n + 1 >= sizeof(text)) {
            ucxapp_print("ERROR: data too long (max %d bytes)\n", (int)sizeof(text) - 1);
            return UCXAPP_ERR_FAILED;
        }
        if (i > 2) {
            text[len++] = ' ';
        }
        memcpy(&text[len], argv[i], n);
        len += n;
    }
    int32_t result = uCxSocketWrite(app->ucx, handle, (const uint8_t *)text, (int32_t)len);
    if (result < 0) {
        ucxapp_print("ERROR: send failed (%d)\n", (int)result);
        return UCXAPP_ERR_FAILED;
    }
    ucxapp_print("sent %d bytes\n", (int)len);
    return UCXAPP_OK;
}

static int cmdSockRead(ucxapp_t *app, int argc, char *argv[])
{
    int32_t handle;
    int32_t maxLen = SOCK_MAX_READ;
    int32_t timeoutS = 5;
    uint8_t buf[SOCK_MAX_READ + 1];

    if (argc < 2 || argc > 4 || !parseHandle(argv[1], &handle) ||
        (argc > 2 && (!ucxapp_parse_int(argv[2], &maxLen) || maxLen < 1 || maxLen > SOCK_MAX_READ)) ||
        (argc > 3 && (!ucxapp_parse_int(argv[3], &timeoutS) || timeoutS < 0))) {
        return UCXAPP_ERR_USAGE;
    }
    ucxapp_events_wait(URC_FLAG_SOCK_DATA, (uint32_t)timeoutS);   /* a read below tells if data arrived */
    int32_t n = uCxSocketRead(app->ucx, handle, maxLen, buf);
    if (n < 0) {
        ucxapp_print("ERROR: read failed (%d)\n", (int)n);
        return UCXAPP_ERR_FAILED;
    }
    for (int32_t i = 0; i < n; i++) {
        if (buf[i] < 0x20 || buf[i] > 0x7e) {
            buf[i] = '.';
        }
    }
    buf[n] = '\0';
    ucxapp_print("received %d bytes: %s\n", (int)n, (const char *)buf);
    return UCXAPP_OK;
}

static int cmdSockClose(ucxapp_t *app, int argc, char *argv[])
{
    int32_t handle;

    if (argc != 2 || !parseHandle(argv[1], &handle)) {
        return UCXAPP_ERR_USAGE;
    }
    if (uCxSocketClose(app->ucx, handle) != 0) {
        ucxapp_print("ERROR: failed to close socket %d\n", (int)handle);
        return UCXAPP_ERR_FAILED;
    }
    ucxapp_print("socket %d closed\n", (int)handle);
    return UCXAPP_OK;
}

static int cmdSockList(ucxapp_t *app, int argc, char *argv[])
{
    uCxSocketListStatus_t status;
    int count = 0;
    (void)argc;
    (void)argv;

    uCxSocketListStatusBegin(app->ucx);
    while (uCxSocketListStatusGetNext(app->ucx, &status)) {
        count++;
        ucxapp_print("socket %d: %s, %s\n", (int)status.socket_handle,
                     status.protocol == U_SOCKET_PROTOCOL_TCP ? "TCP" : "UDP",
                     status.status == 0 ? "not connected" :
                     status.status == 1 ? "listening" : "connected");
    }
    uCxEnd(app->ucx);
    ucxapp_print("%d socket(s)\n", count);
    return UCXAPP_OK;
}

static const ucxapp_cmd_t gSockCmds[] = {
    { "sock.open",    "<tcp|udp>",                "Create a socket, prints its handle",   UCXAPP_CMD_NEEDS_DEVICE, cmdSockOpen },
    { "sock.connect", "<handle> <host> <port>",   "Connect a socket",                     UCXAPP_CMD_NEEDS_DEVICE, cmdSockConnect },
    { "sock.send",    "<handle> <text>",          "Send text on a connected socket",      UCXAPP_CMD_NEEDS_DEVICE, cmdSockSend },
    { "sock.read",    "<handle> [max] [wait_s]",  "Wait for data (default 5 s) and read", UCXAPP_CMD_NEEDS_DEVICE, cmdSockRead },
    { "sock.close",   "<handle>",                 "Close a socket",                       UCXAPP_CMD_NEEDS_DEVICE, cmdSockClose },
    { "sock.list",    "",                         "List open sockets",                    UCXAPP_CMD_NEEDS_DEVICE, cmdSockList },
};

void ucxapp_register_sock_cmds(void)
{
    ucxapp_cmd_register(gSockCmds, sizeof(gSockCmds) / sizeof(gSockCmds[0]));
}
