/* tls.* - local CA and certificates for the app's own TLS servers (stream bridge, web server). */

#include "ucxapp_cmd.h"
#include "ucxapp_store.h"
#include "ucxapp_tls.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int cmdCaCreate(ucxapp_t *app, int argc, char *argv[])
{
    (void)app;
    char err[160] = "";
    bool force = argc > 1 && strcmp(argv[argc - 1], "force") == 0;
    int named = argc - 1 - (force ? 1 : 0);
    if (named > 1) {
        return UCXAPP_ERR_USAGE;
    }
    if (!ucxapp_tls_create_ca(named == 1 ? argv[1] : NULL, force, err, sizeof(err))) {
        ucxapp_print("ERROR: %s\n", err);
        return UCXAPP_ERR_FAILED;
    }
    ucxapp_print("created local CA tls-ca.crt / tls-ca.key\n");
    ucxapp_print("install tls-ca.crt as a trusted root on clients (tls.pem ca prints it)\n");
    return UCXAPP_OK;
}

static int createCert(int argc, char *argv[], bool client)
{
    char err[160] = "";
    if (argc < 2 || (client && argc != 2)) {
        return UCXAPP_ERR_USAGE;
    }
    if (!ucxapp_tls_create_cert(argv[1], client, (const char *const *)&argv[2], argc - 2, err, sizeof(err))) {
        ucxapp_print("ERROR: %s\n", err);
        return UCXAPP_ERR_FAILED;
    }
    ucxapp_print("created tls-%s.crt / tls-%s.key (signed by the local CA)\n", argv[1], argv[1]);
    if (!client && argc == 2) {
        ucxapp_print("warning: no host names or IP addresses, browsers will reject the name\n");
    }
    return UCXAPP_OK;
}

static int cmdServerCreate(ucxapp_t *app, int argc, char *argv[])
{
    (void)app;
    return createCert(argc, argv, false);
}

static int cmdClientCreate(ucxapp_t *app, int argc, char *argv[])
{
    (void)app;
    return createCert(argc, argv, true);
}

static void listOne(const char *name, uint64_t size, void *ctx)
{
    (void)size;
    int *count = ctx;
    size_t n = strlen(name);
    if (strncmp(name, "tls-", 4) != 0 || n < 9 || strcmp(name + n - 4, ".crt") != 0) {
        return;
    }
    char cert[64], info[400];
    snprintf(cert, sizeof(cert), "%.*s", (int)(n - 8), name + 4);
    if (ucxapp_tls_describe(cert, info, sizeof(info))) {
        ucxapp_print("%-12s %s\n", cert, info);
    } else {
        ucxapp_print("%-12s (unreadable)\n", cert);
    }
    (*count)++;
}

static int cmdList(ucxapp_t *app, int argc, char *argv[])
{
    (void)app;
    (void)argv;
    int count = 0;
    if (argc != 1) {
        return UCXAPP_ERR_USAGE;
    }
    if (ucxapp_store_list(listOne, &count) < 0) {
        ucxapp_print("ERROR: cannot list the store\n");
        return UCXAPP_ERR_FAILED;
    }
    if (count == 0) {
        ucxapp_print("no certificates, start with tls.ca.create\n");
    }
    return UCXAPP_OK;
}

static int cmdPem(ucxapp_t *app, int argc, char *argv[])
{
    (void)app;
    char file[96];
    size_t len = 0;
    bool key = argc == 3 && strcmp(argv[2], "key") == 0;
    if (argc != 2 && !key) {
        return UCXAPP_ERR_USAGE;
    }
    if (key) {
        ucxapp_tls_key_file(argv[1], file, sizeof(file));
    } else {
        ucxapp_tls_cert_file(argv[1], file, sizeof(file));
    }
    uint8_t *pem = ucxapp_store_load(file, &len);
    if (!pem) {
        ucxapp_print("ERROR: %s not found\n", file);
        return UCXAPP_ERR_FAILED;
    }
    ucxapp_print("%s", (const char *)pem);
    free(pem);
    return UCXAPP_OK;
}

static int cmdRemove(ucxapp_t *app, int argc, char *argv[])
{
    (void)app;
    char crt[96], key[96];
    if (argc != 2) {
        return UCXAPP_ERR_USAGE;
    }
    ucxapp_tls_cert_file(argv[1], crt, sizeof(crt));
    ucxapp_tls_key_file(argv[1], key, sizeof(key));
    bool a = ucxapp_store_remove(crt);
    bool b = ucxapp_store_remove(key);
    if (!a && !b) {
        ucxapp_print("ERROR: %s not found\n", argv[1]);
        return UCXAPP_ERR_FAILED;
    }
    ucxapp_print("removed %s\n", argv[1]);
    return UCXAPP_OK;
}

static const ucxapp_cmd_t gTlsCmds[] = {
    { "tls.ca.create",     "[cn] [force]",            "Create the local CA (ECDSA P-256) that signs the app certificates", 0, cmdCaCreate },
    { "tls.server.create", "<name> <host|ip> [...]",  "Create a server certificate for the given host names / IPv4 addresses", 0, cmdServerCreate },
    { "tls.client.create", "<name>",                  "Create a client certificate (for tls clientauth)", 0, cmdClientCreate },
    { "tls.list",          "",                        "List the app certificates", 0, cmdList },
    { "tls.pem",           "<name> [key]",            "Print a certificate (or its key) in PEM", 0, cmdPem },
    { "tls.remove",        "<name>",                  "Remove a certificate and its key", 0, cmdRemove },
};

void ucxapp_register_tls_cmds(void)
{
    ucxapp_cmd_register(gTlsCmds, sizeof(gTlsCmds) / sizeof(gTlsCmds[0]));
}
