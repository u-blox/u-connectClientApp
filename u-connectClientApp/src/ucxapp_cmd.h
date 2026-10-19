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
void ucxapp_register_wifi_cmds(void);
void ucxapp_register_sock_cmds(void);
void ucxapp_register_net_cmds(void);
void ucxapp_register_sys_cmds(void);
void ucxapp_register_mqtt_cmds(void);
void ucxapp_register_bt_cmds(void);

#endif /* UCXAPP_CMD_H */
