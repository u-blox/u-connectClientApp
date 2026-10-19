#include "ucxapp.h"

#include <stdio.h>
#include <string.h>

#define UCXAPP_PRINT_BUF_SIZE 512

static const ucxapp_io_t *gIo = NULL;

void ucxapp_io_set(const ucxapp_io_t *io)
{
    gIo = io;
}

void ucxapp_print(const char *fmt, ...)
{
    char buf[UCXAPP_PRINT_BUF_SIZE];
    va_list args;

    if (gIo == NULL || gIo->write == NULL) {
        return;
    }
    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (n < 0) {
        return;
    }
    if ((size_t)n >= sizeof(buf)) {
        n = (int)sizeof(buf) - 1;   /* truncated */
    }
    gIo->write(buf, (size_t)n);
}

int ucxapp_readline(const char *prompt, char *buf, size_t size)
{
    if (gIo == NULL || gIo->readline == NULL || size == 0) {
        return -1;
    }
    if (prompt != NULL) {
        ucxapp_print("%s", prompt);
    }
    return gIo->readline(buf, size);
}
