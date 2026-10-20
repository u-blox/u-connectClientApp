#include "ucxapp_cmd.h"
#include "ucxapp_hash.h"
#include "ucxapp_store.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AT_MAX_LEN 256

/* "ati9" -> "ATI9", "+GMI" -> "AT+GMI": the AT prefix is optional. */
static int cmdAt(ucxapp_t *app, int argc, char *argv[])
{
    char cmd[AT_MAX_LEN];
    size_t len = 0;

    if (argc < 2) {
        return UCXAPP_ERR_USAGE;
    }
    for (int i = 1; i < argc; i++) {
        size_t n = strlen(argv[i]);
        if (len + n + 3 >= sizeof(cmd)) {
            ucxapp_print("ERROR: command too long\n");
            return UCXAPP_ERR_FAILED;
        }
        if (i > 1) {
            cmd[len++] = ' ';
        }
        memcpy(&cmd[len], argv[i], n);
        len += n;
    }
    cmd[len] = '\0';

    char full[AT_MAX_LEN + 3];
    bool hasPrefix = len >= 2 && toupper((unsigned char)cmd[0]) == 'A' &&
                     toupper((unsigned char)cmd[1]) == 'T';
    snprintf(full, sizeof(full), "%s%s", hasPrefix ? "" : "AT", cmd);

    int32_t result = uCxAtClientExecSimpleCmd(app->ucx->pAtClient, full);
    if (result != 0) {
        ucxapp_print("ERROR: %s failed (%d)\n", full, (int)result);
        return UCXAPP_ERR_FAILED;
    }
    ucxapp_print("OK\n");
    return UCXAPP_OK;
}

static int cmdFileHash(ucxapp_t *app, int argc, char *argv[])
{
    (void)app;
    char hex[UCXAPP_SHA256_HEX_LEN];
    size_t len;

    if (argc != 3 || (strcmp(argv[1], "md5") != 0 && strcmp(argv[1], "sha256") != 0)) {
        return UCXAPP_ERR_USAGE;
    }
    uint8_t *data = ucxapp_store_load(argv[2], &len);
    if (data == NULL) {
        ucxapp_print("ERROR: cannot read %s\n", argv[2]);
        return UCXAPP_ERR_FAILED;
    }
    if (argv[1][0] == 'm') {
        ucxapp_md5_hex(data, len, hex);
    } else {
        ucxapp_sha256_hex(data, len, hex);
    }
    free(data);
    ucxapp_print("%s  %s (%u bytes)\n", hex, argv[2], (unsigned)len);
    return UCXAPP_OK;
}

static const ucxapp_cmd_t gSysCmds[] = {
    { "at", "<command>", "Send a raw AT command (AT prefix optional)", UCXAPP_CMD_NEEDS_DEVICE, cmdAt },
    { "file.hash", "<md5|sha256> <file>", "Print the hash of a file", 0, cmdFileHash },
};

void ucxapp_register_sys_cmds(void)
{
    ucxapp_cmd_register(gSysCmds, sizeof(gSysCmds) / sizeof(gSysCmds[0]));
}
