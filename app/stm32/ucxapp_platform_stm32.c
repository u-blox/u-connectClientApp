/* Platform backend for bare-metal STM32: no OS shell, no network of its own. */

#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "ucxapp_platform.h"

#include <sys/time.h>

#include "ucxapp_cmd.h"

extern int32_t exampleConsoleUartRead(uint8_t *pByte, int32_t timeoutMs);

static int gPendingKey = -1;

/* Features not built for STM32. */
void ucxapp_web_on_incoming(int32_t sock, int32_t listenSock) { (void)sock; (void)listenSock; }
void ucxapp_web_on_data(int32_t sock, int32_t bytes) { (void)sock; (void)bytes; }
void ucxapp_web_on_closed(int32_t sock) { (void)sock; }
bool ucxapp_hid_server_write(int32_t conn, int32_t handle, const uint8_t *data, size_t len)
{
    (void)conn; (void)handle; (void)data; (void)len;
    return false;
}
bool ucxapp_qr_print_wifi(const char *ssid, const char *password, const char *security, bool hidden)
{
    (void)ssid; (void)password; (void)security; (void)hidden;
    return false;
}

/* newlib syscall used by time(); the board has no wall clock. */
int _gettimeofday(struct timeval *tv, void *tz)
{
    (void)tz;
    if (tv != NULL) {
        uint32_t ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
        tv->tv_sec = ms / 1000;
        tv->tv_usec = (ms % 1000) * 1000;
    }
    return 0;
}

void ucxapp_platform_init(void)
{
}

uint32_t ucxapp_platform_tick_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

bool ucxapp_platform_key_pressed(void)
{
    uint8_t c;

    if (gPendingKey < 0 && exampleConsoleUartRead(&c, 0) == 1) {
        gPendingKey = c;
    }
    return gPendingKey >= 0;
}

int ucxapp_platform_get_key(void)
{
    int key;

    while (!ucxapp_platform_key_pressed()) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    key = gPendingKey;
    gPendingKey = -1;
    return key;
}

void ucxapp_platform_exe_dir(char *buffer, size_t bufferSize)
{
    if (bufferSize > 0) {
        buffer[0] = '\0';
    }
}

int ucxapp_platform_list_serial_ports(ucxapp_serial_port_t *ports, int maxPorts)
{
    (void)ports;
    (void)maxPorts;
    return 0;
}

char *ucxapp_platform_https_get_text(const char *host, const char *path)
{
    (void)host;
    (void)path;
    return NULL;
}

char *ucxapp_platform_https_get_binary(const char *host, const char *path, size_t *outSize)
{
    (void)host;
    (void)path;
    (void)outSize;
    return NULL;
}

bool ucxapp_platform_extract_zip(const char *zipPath, const char *destDir)
{
    (void)zipPath;
    (void)destDir;
    return false;
}

bool ucxapp_platform_find_file(const char *dir, const char *ext, char *out, size_t outSize)
{
    (void)dir;
    (void)ext;
    (void)out;
    (void)outSize;
    return false;
}
