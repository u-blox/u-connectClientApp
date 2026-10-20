#ifndef UCXAPP_HASH_H
#define UCXAPP_HASH_H

/* Portable MD5 and SHA-256 (no OS crypto dependency, endian independent). */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UCXAPP_MD5_LEN        16
#define UCXAPP_SHA256_LEN     32
#define UCXAPP_MD5_HEX_LEN    (UCXAPP_MD5_LEN * 2 + 1)      /* including the NUL */
#define UCXAPP_SHA256_HEX_LEN (UCXAPP_SHA256_LEN * 2 + 1)

typedef struct {
    uint32_t state[4];
    uint64_t bits;
    uint8_t block[64];
} ucxapp_md5_t;

typedef struct {
    uint32_t state[8];
    uint64_t bits;
    uint8_t block[64];
} ucxapp_sha256_t;

void ucxapp_md5_init(ucxapp_md5_t *ctx);
void ucxapp_md5_update(ucxapp_md5_t *ctx, const void *data, size_t len);
void ucxapp_md5_final(ucxapp_md5_t *ctx, uint8_t digest[UCXAPP_MD5_LEN]);

void ucxapp_sha256_init(ucxapp_sha256_t *ctx);
void ucxapp_sha256_update(ucxapp_sha256_t *ctx, const void *data, size_t len);
void ucxapp_sha256_final(ucxapp_sha256_t *ctx, uint8_t digest[UCXAPP_SHA256_LEN]);

/* One-shot helpers that produce lowercase hex (buffers: UCXAPP_*_HEX_LEN bytes). */
void ucxapp_md5_hex(const void *data, size_t len, char *hex);
void ucxapp_sha256_hex(const void *data, size_t len, char *hex);

/* Case-insensitive compare of two hex strings. */
int ucxapp_hex_equal(const char *a, const char *b);

#ifdef __cplusplus
}
#endif

#endif /* UCXAPP_HASH_H */
