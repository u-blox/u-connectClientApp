#include "ucxapp_cmd.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const ucxapp_cmd_t *table;
    size_t count;
} pack_t;

static pack_t gPacks[UCXAPP_MAX_PACKS];
static size_t gPackCount = 0;

bool ucxapp_cmd_register(const ucxapp_cmd_t *table, size_t count)
{
    if (table == NULL || gPackCount >= UCXAPP_MAX_PACKS) {
        return false;
    }
    gPacks[gPackCount].table = table;
    gPacks[gPackCount].count = count;
    gPackCount++;
    return true;
}

static const ucxapp_cmd_t *findCmd(const char *name)
{
    for (size_t p = 0; p < gPackCount; p++) {
        for (size_t i = 0; i < gPacks[p].count; i++) {
            if (strcmp(gPacks[p].table[i].name, name) == 0) {
                return &gPacks[p].table[i];
            }
        }
    }
    return NULL;
}

bool ucxapp_parse_int(const char *s, int32_t *out)
{
    char *end;
    long v;

    if (s == NULL || *s == '\0') {
        return false;
    }
    v = strtol(s, &end, 0);
    if (*end != '\0') {
        return false;
    }
    *out = (int32_t)v;
    return true;
}

static int tokenize(char *line, char *argv[], int maxArgs)
{
    int argc = 0;
    char *r = line;
    char *w = line;

    for (;;) {
        while (*r == ' ' || *r == '\t') {
            r++;
        }
        if (*r == '\0') {
            break;
        }
        if (argc >= maxArgs) {
            return -1;
        }
        argv[argc++] = w;
        if (*r == '"') {
            r++;
            while (*r != '\0' && *r != '"') {
                if (*r == '\\' && (r[1] == '"' || r[1] == '\\')) {
                    r++;
                }
                *w++ = *r++;
            }
            if (*r == '"') {
                r++;
            }
        } else {
            while (*r != '\0' && *r != ' ' && *r != '\t') {
                *w++ = *r++;
            }
        }
        if (*r != '\0') {
            r++;
        }
        *w++ = '\0';
    }
    return argc;
}

void ucxapp_cmd_print_help(const char *prefix)
{
    size_t prefixLen = prefix != NULL ? strlen(prefix) : 0;
    bool any = false;

    for (size_t p = 0; p < gPackCount; p++) {
        for (size_t i = 0; i < gPacks[p].count; i++) {
            const ucxapp_cmd_t *c = &gPacks[p].table[i];
            if (strncmp(c->name, prefix != NULL ? prefix : "", prefixLen) != 0) {
                continue;
            }
            ucxapp_print("  %-16s %-24s %s\n", c->name, c->usage, c->help);
            any = true;
        }
    }
    if (!any) {
        ucxapp_print("No commands match '%s'\n", prefix);
    }
}

int ucxapp_cmd_exec(ucxapp_t *app, char *line)
{
    char *argv[UCXAPP_MAX_ARGS];
    int argc;

    while (*line == ' ' || *line == '\t') {
        line++;
    }
    if (*line == '\0' || *line == '#') {
        return UCXAPP_OK;
    }
    argc = tokenize(line, argv, UCXAPP_MAX_ARGS);
    if (argc < 0) {
        ucxapp_print("ERROR: too many arguments (max %d)\n", UCXAPP_MAX_ARGS);
        return UCXAPP_ERR_USAGE;
    }
    if (argc == 0) {
        return UCXAPP_OK;
    }

    if (strcmp(argv[0], "help") == 0) {
        ucxapp_cmd_print_help(argc > 1 ? argv[1] : NULL);
        return UCXAPP_OK;
    }

    const ucxapp_cmd_t *cmd = findCmd(argv[0]);
    if (cmd == NULL) {
        ucxapp_print("ERROR: unknown command '%s' (try 'help')\n", argv[0]);
        return UCXAPP_ERR_UNKNOWN_CMD;
    }
    if ((cmd->flags & UCXAPP_CMD_NEEDS_DEVICE) && app->ucx == NULL) {
        ucxapp_print("ERROR: not connected to a module\n");
        return UCXAPP_ERR_NOT_CONNECTED;
    }

    int result = cmd->run(app, argc, argv);
    if (result == UCXAPP_ERR_USAGE) {
        ucxapp_print("usage: %s %s\n", cmd->name, cmd->usage);
    }
    return result;
}

void ucxapp_shell_run(ucxapp_t *app)
{
    char line[256];

    ucxapp_print("Command shell. Type 'help' for commands, 'exit' to return.\n");
    for (;;) {
        if (ucxapp_readline("ucxapp> ", line, sizeof(line)) < 0) {
            break;
        }
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
            break;
        }
        ucxapp_cmd_exec(app, line);
    }
}
