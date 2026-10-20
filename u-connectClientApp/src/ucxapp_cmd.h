#ifndef UCXAPP_CMD_H
#define UCXAPP_CMD_H

#include "ucxapp.h"

#define UCXAPP_OK                  0
#define UCXAPP_ERR_UNKNOWN_CMD   (-100)
#define UCXAPP_ERR_USAGE         (-101)
#define UCXAPP_ERR_NOT_CONNECTED (-102)
#define UCXAPP_ERR_FAILED        (-103)

#define UCXAPP_MAX_ARGS    8
#define UCXAPP_MAX_PACKS   16

/* Command needs a connected module (app->ucx != NULL). */
#define UCXAPP_CMD_NEEDS_DEVICE  0x01

/* argv[0] is the command name. Return UCXAPP_OK, UCXAPP_ERR_USAGE or another negative code. */
typedef int (*ucxapp_cmd_fn_t)(ucxapp_t *app, int argc, char *argv[]);

typedef struct {
    const char *name;     /* "wifi.connect" */
    const char *usage;    /* "<ssid> [password]" */
    const char *help;     /* one line */
    uint8_t flags;
    ucxapp_cmd_fn_t run;
} ucxapp_cmd_t;

/* Register a command table (a "pack"). The table must stay valid for the program lifetime. */
bool ucxapp_cmd_register(const ucxapp_cmd_t *table, size_t count);

/* Tokenize a line in place (whitespace separated, "double quotes" group) and run it. */
int ucxapp_cmd_exec(ucxapp_t *app, char *line);

void ucxapp_cmd_print_help(const char *prefix);

/* Read-eval loop on the IO layer. Returns on "exit"/"quit" or end of input. */
void ucxapp_shell_run(ucxapp_t *app);

/* Parse a whole decimal integer; false if s is empty or has trailing characters. */
bool ucxapp_parse_int(const char *s, int32_t *out);

/* Command packs */
void ucxapp_register_device_cmds(void);

/** Free NORA EVK AT ports found on this host; returns the count written to names. */
int ucxapp_find_modules(char names[][32], int maxNames);
void ucxapp_register_wifi_cmds(void);
void ucxapp_register_sock_cmds(void);
void ucxapp_register_net_cmds(void);
void ucxapp_register_sys_cmds(void);
void ucxapp_register_mqtt_cmds(void);
void ucxapp_register_bt_cmds(void);
void ucxapp_register_cert_cmds(void);
void ucxapp_register_fw_cmds(void);
void ucxapp_register_gatt_cmds(void);
void ucxapp_register_qr_cmds(void);
void ucxapp_register_hid_cmds(void);
void ucxapp_register_web_cmds(void);

/* Socket URCs for the web server (ucxapp_cmds_web.c); queued only, handled on its own thread. */
void ucxapp_web_on_incoming(int32_t sock, int32_t listenSock);
void ucxapp_web_on_data(int32_t sock, int32_t bytes);
void ucxapp_web_on_closed(int32_t sock);

/* Wi-Fi station state for other packs; false when not connected (ssid and ip are then empty). */
bool ucxapp_wifi_get_status(uCxHandle_t *ucx, char *ssid, size_t ssidSize, char *ip, size_t ipSize);

/* GATT server helpers shared with the HID pack (ucxapp_cmds_gatt.c). */
int ucxapp_gatt_server_ensure(ucxapp_t *app, const char *key);   /* defines a predefined profile once, e.g. "bas" */
void ucxapp_gatt_ensure_advertising(ucxapp_t *app);
bool ucxapp_hid_server_write(int32_t conn, int32_t handle, const uint8_t *data, size_t len);   /* true if consumed */
void ucxapp_gatt_server_read(int32_t conn, int32_t handle);

/* QR rendering (ucxapp_cmds_qr.c): any text, or a Wi-Fi join payload (security WPA, WEP or nopass). */
bool ucxapp_qr_print(const char *text);
bool ucxapp_qr_print_wifi(const char *ssid, const char *password, const char *security, bool hidden);

/* Bluetooth security (ucxapp_cmds_bt.c): URC sinks that reply themselves, and the Just Works defaults. */
void ucxapp_bt_bond_status(const uBtLeAddress_t *addr, int32_t status);
void ucxapp_bt_user_confirmation(uCxHandle_t *ucx, const uBtLeAddress_t *addr, int32_t value);
void ucxapp_bt_passkey_display(const uBtLeAddress_t *addr, int32_t passkey);
void ucxapp_bt_passkey_request(const uBtLeAddress_t *addr);
void ucxapp_bt_security_defaults(ucxapp_t *app);   /* once per run */

/* GATT URC sinks (ucxapp_cmds_gatt.c); the host's URC handlers forward to these. */
void ucxapp_gatt_notification(int32_t conn, int32_t handle, const uint8_t *data, size_t len);
void ucxapp_gatt_server_write(int32_t conn, int32_t handle, const uint8_t *data, size_t len);

#endif /* UCXAPP_CMD_H */
