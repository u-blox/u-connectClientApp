#ifndef UCXAPP_PLATFORM_H
#define UCXAPP_PLATFORM_H

/* Host OS services used by the app. Each target provides one backend (ucxapp_platform_<os>.c). */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UCXAPP_PLATFORM_PORT_NAME_LEN   32
#define UCXAPP_PLATFORM_PORT_DESC_LEN   256
#define UCXAPP_PLATFORM_PORT_LABEL_LEN  64

typedef struct {
    char name[UCXAPP_PLATFORM_PORT_NAME_LEN];     /* e.g. "COM13" or "/dev/ttyUSB0" */
    char desc[UCXAPP_PLATFORM_PORT_DESC_LEN];     /* device description, empty if unknown */
    char label[UCXAPP_PLATFORM_PORT_LABEL_LEN];   /* port function such as "AT" or "LOG", may be empty */
    bool available;                               /* false when another process has the port open */
} ucxapp_serial_port_t;

/** One-time setup (UTF-8 console output). */
void ucxapp_platform_init(void);

/** Milliseconds since an arbitrary start point; wraps after 49 days. */
uint32_t ucxapp_platform_tick_ms(void);

/** True if a key press is waiting. */
bool ucxapp_platform_key_pressed(void);

/** Blocks for one key press and returns it. */
int ucxapp_platform_get_key(void);

/** Directory of the running executable, with a trailing path separator. */
void ucxapp_platform_exe_dir(char *buffer, size_t bufferSize);

/**
 * Lists serial ports, sorted as the OS reports them.
 * @return number of entries written (at most maxPorts).
 */
int ucxapp_platform_list_serial_ports(ucxapp_serial_port_t *ports, int maxPorts);

/**
 * HTTPS GET with redirects followed. The body is returned in a malloc'ed buffer (caller frees),
 * NULL on failure or empty body. The text variant is NUL terminated; the binary variant
 * reports the length in outSize and prints download progress.
 */
char *ucxapp_platform_https_get_text(const char *host, const char *path);
char *ucxapp_platform_https_get_binary(const char *host, const char *path, size_t *outSize);

#ifdef __cplusplus
}
#endif

#endif /* UCXAPP_PLATFORM_H */
