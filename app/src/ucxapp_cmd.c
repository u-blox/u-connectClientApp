#include "ucxapp_cmd.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "u_cx_error_codes.h"

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

const ucxapp_cmd_t *ucxapp_cmd_find(const char *name)
{
    return findCmd(name);
}

const char *ucxapp_errname(int32_t code)
{
    static char text[64];
    const char *name = uCxGetErrorName(code);

    if (name == NULL) {
        return "";
    }
    snprintf(text, sizeof(text), ": %s", name);
    return text;
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

void ucxapp_cmd_foreach(void (*visit)(const ucxapp_cmd_t *cmd, void *user), void *user)
{
    for (size_t p = 0; p < gPackCount; p++) {
        for (size_t i = 0; i < gPacks[p].count; i++) {
            visit(&gPacks[p].table[i], user);
        }
    }
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

#define HANDLE_MAX 8

typedef struct {
    int32_t ids[HANDLE_MAX];
    int count;
} handleSet_t;

static handleSet_t gHandles[2];
static volatile uint32_t gHandleVersion;

uint32_t ucxapp_handle_version(void)
{
    return gHandleVersion;
}

int ucxapp_handle_list(ucxapp_handle_kind_t kind, int32_t *out, int max)
{
    handleSet_t *s = &gHandles[kind];
    int n;

    ucxapp_events_lock();
    n = s->count < max ? s->count : max;
    memcpy(out, s->ids, sizeof(out[0]) * (size_t)n);
    ucxapp_events_unlock();
    return n;
}

void ucxapp_handle_remove(ucxapp_handle_kind_t kind, int32_t id)
{
    handleSet_t *s = &gHandles[kind];

    ucxapp_events_lock();
    for (int i = 0; i < s->count; i++) {
        if (s->ids[i] == id) {
            memmove(&s->ids[i], &s->ids[i + 1], sizeof(s->ids[0]) * (size_t)(s->count - i - 1));
            s->count--;
            gHandleVersion++;
            break;
        }
    }
    ucxapp_events_unlock();
}

/* The newest handle becomes the current one; the oldest is dropped when the list is full. */
void ucxapp_handle_add(ucxapp_handle_kind_t kind, int32_t id)
{
    handleSet_t *s = &gHandles[kind];

    ucxapp_handle_remove(kind, id);
    ucxapp_events_lock();
    if (s->count == HANDLE_MAX) {
        memmove(&s->ids[0], &s->ids[1], sizeof(s->ids[0]) * (HANDLE_MAX - 1));
        s->count--;
    }
    s->ids[s->count++] = id;
    gHandleVersion++;
    ucxapp_events_unlock();
}

int32_t ucxapp_handle_current(ucxapp_handle_kind_t kind)
{
    handleSet_t *s = &gHandles[kind];

    ucxapp_events_lock();
    int32_t id = s->count > 0 ? s->ids[s->count - 1] : -1;
    ucxapp_events_unlock();
    return id;
}

/* A usage starting with "[handle]" (socket) or "[conn]" (BLE) takes the current one when it is left out. */
static void insertCurrentHandle(const ucxapp_cmd_t *cmd, int *argc, char *argv[])
{
    static char text[16];
    ucxapp_handle_kind_t kind;
    const char *u = cmd->usage;
    int required = 0;
    int depth = 0;
    int32_t id;

    if (strncmp(u, "[handle]", 8) == 0) {
        kind = UCXAPP_HANDLE_SOCK;
        u += 8;
    } else if (strncmp(u, "[conn]", 6) == 0) {
        kind = UCXAPP_HANDLE_CONN;
        u += 6;
    } else {
        return;
    }
    for (; *u != '\0'; u++) {
        if (*u == '<' || *u == '[') {
            required += (*u == '<' && depth == 0);
            depth++;
        } else if (*u == '>' || *u == ']') {
            depth--;
        }
    }
    if (*argc >= UCXAPP_MAX_ARGS || (*argc - 1 > required && ucxapp_parse_int(argv[1], &id))) {
        return;
    }
    id = ucxapp_handle_current(kind);
    if (id < 0) {
        return;
    }
    snprintf(text, sizeof(text), "%d", (int)id);
    memmove(&argv[2], &argv[1], (size_t)(*argc - 1) * sizeof(argv[0]));
    argv[1] = text;
    (*argc)++;
}

static bool gAtMode;

void ucxapp_at_mode(bool on)
{
    gAtMode = on;
}

bool ucxapp_at_mode_active(void)
{
    return gAtMode;
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
    if (gAtMode) {
        if (strcmp(line, "back") == 0) {
            gAtMode = false;
            ucxapp_print("command mode\n");
            return UCXAPP_OK;
        }
        return app->ucx != NULL ? ucxapp_at_send(app, line) : UCXAPP_ERR_NOT_CONNECTED;
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
    if (cmd == NULL && argc < UCXAPP_MAX_ARGS && strlen(argv[0]) > 2 && strchr(argv[0], '.') == NULL &&
        (argv[0][0] == 'a' || argv[0][0] == 'A') && (argv[0][1] == 't' || argv[0][1] == 'T')) {
        /* "ati9" or "AT+GMR" typed directly is sent as a raw AT command */
        static char atName[] = "at";

        memmove(&argv[1], &argv[0], (size_t)argc * sizeof(argv[0]));
        argv[0] = atName;
        argc++;
        cmd = findCmd("at");
    }
    if (cmd == NULL) {
        ucxapp_print("ERROR: unknown command '%s' (try 'help')\n", argv[0]);
        return UCXAPP_ERR_UNKNOWN_CMD;
    }
    if ((cmd->flags & UCXAPP_CMD_NEEDS_DEVICE) && app->ucx == NULL) {
        ucxapp_print("ERROR: not connected to a module\n");
        return UCXAPP_ERR_NOT_CONNECTED;
    }

    int result;

    insertCurrentHandle(cmd, &argc, argv);
    result = cmd->run(app, argc, argv);
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
