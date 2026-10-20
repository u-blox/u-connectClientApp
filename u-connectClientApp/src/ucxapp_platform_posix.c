/* POSIX backend for ucxapp_platform.h */

#define _POSIX_C_SOURCE 200809L

#include "ucxapp_platform.h"

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

void ucxapp_platform_init(void)
{
}

uint32_t ucxapp_platform_tick_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u);
}

bool ucxapp_platform_key_pressed(void)
{
    fd_set fds;
    struct timeval tv = { 0, 0 };
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

int ucxapp_platform_get_key(void)
{
    return getchar();
}

void ucxapp_platform_exe_dir(char *buffer, size_t bufferSize)
{
    ssize_t n = readlink("/proc/self/exe", buffer, bufferSize - 1);
    if (n <= 0) {
        snprintf(buffer, bufferSize, "./");
        return;
    }
    buffer[n] = '\0';
    char *lastSlash = strrchr(buffer, '/');
    if (lastSlash) {
        *(lastSlash + 1) = '\0';
    }
}

static void readSysfsText(const char *tty, const char *file, char *out, size_t outSize)
{
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "/sys/class/tty/%s/device/../%s", tty, file);
    FILE *f = fopen(path, "r");
    out[0] = '\0';
    if (f) {
        if (fgets(out, (int)outSize, f)) {
            out[strcspn(out, "\r\n")] = '\0';
        }
        fclose(f);
    }
}

int ucxapp_platform_list_serial_ports(ucxapp_serial_port_t *ports, int maxPorts)
{
    int count = 0;
    DIR *dir = opendir("/dev");
    if (!dir) {
        return 0;
    }
    struct dirent *entry;
    while (count < maxPorts && (entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "ttyUSB", 6) != 0 && strncmp(entry->d_name, "ttyACM", 6) != 0) {
            continue;
        }
        ucxapp_serial_port_t *port = &ports[count++];
        memset(port, 0, sizeof(*port));
        snprintf(port->name, sizeof(port->name), "/dev/%s", entry->d_name);
        readSysfsText(entry->d_name, "product", port->desc, sizeof(port->desc));
        port->available = access(port->name, R_OK | W_OK) == 0;
    }
    closedir(dir);
    return count;
}

bool ucxapp_platform_extract_zip(const char *zipPath, const char *destDir)
{
    if (strpbrk(zipPath, "\"'`$\\") != NULL || strpbrk(destDir, "\"'`$\\") != NULL) {
        return false;
    }
    char cmd[2 * PATH_MAX];
    snprintf(cmd, sizeof(cmd), "unzip -o -q \"%s\" -d \"%s\"", zipPath, destDir);
    return system(cmd) == 0;
}

bool ucxapp_platform_find_file(const char *dir, const char *ext, char *out, size_t outSize)
{
    DIR *d = opendir(dir);
    if (d == NULL) {
        return false;
    }
    bool found = false;
    size_t extLen = strlen(ext);
    struct dirent *entry;
    while (!found && (entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);
        size_t n = strlen(entry->d_name);
        if (n >= extLen && strcasecmp(entry->d_name + n - extLen, ext) == 0) {
            snprintf(out, outSize, "%s", path);
            found = true;
        } else if (ucxapp_platform_find_file(path, ext, out, outSize)) {
            found = true;
        }
    }
    closedir(d);
    return found;
}

/* No HTTPS client on POSIX; only the Windows menu's GitHub firmware download uses these. */
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
    if (outSize) {
        *outSize = 0;
    }
    return NULL;
}
