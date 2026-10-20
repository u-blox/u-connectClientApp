#include "ucxapp_hash.h"

#include <ctype.h>
#include <string.h>

#define ROTL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void toHex(const uint8_t *digest, size_t len, char *hex)
{
    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        hex[i * 2] = digits[digest[i] >> 4];
        hex[i * 2 + 1] = digits[digest[i] & 0x0f];
    }
    hex[len * 2] = '\0';
}

int ucxapp_hex_equal(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (tolower((unsigned char)*a++) != tolower((unsigned char)*b++)) {
            return 0;
        }
    }
    return *a == *b;
}

/* ---- MD5 (RFC 1321) ---- */

static const uint32_t kMd5K[64] = {
    0xd76aa478U, 0xe8c7b756U, 0x242070dbU, 0xc1bdceeeU, 0xf57c0fafU, 0x4787c62aU, 0xa8304613U, 0xfd469501U,
    0x698098d8U, 0x8b44f7afU, 0xffff5bb1U, 0x895cd7beU, 0x6b901122U, 0xfd987193U, 0xa679438eU, 0x49b40821U,
    0xf61e2562U, 0xc040b340U, 0x265e5a51U, 0xe9b6c7aaU, 0xd62f105dU, 0x02441453U, 0xd8a1e681U, 0xe7d3fbc8U,
    0x21e1cde6U, 0xc33707d6U, 0xf4d50d87U, 0x455a14edU, 0xa9e3e905U, 0xfcefa3f8U, 0x676f02d9U, 0x8d2a4c8aU,
    0xfffa3942U, 0x8771f681U, 0x6d9d6122U, 0xfde5380cU, 0xa4beea44U, 0x4bdecfa9U, 0xf6bb4b60U, 0xbebfbc70U,
    0x289b7ec6U, 0xeaa127faU, 0xd4ef3085U, 0x04881d05U, 0xd9d4d039U, 0xe6db99e5U, 0x1fa27cf8U, 0xc4ac5665U,
    0xf4292244U, 0x432aff97U, 0xab9423a7U, 0xfc93a039U, 0x655b59c3U, 0x8f0ccc92U, 0xffeff47dU, 0x85845dd1U,
    0x6fa87e4fU, 0xfe2ce6e0U, 0xa3014314U, 0x4e0811a1U, 0xf7537e82U, 0xbd3af235U, 0x2ad7d2bbU, 0xeb86d391U
};

static const uint8_t kMd5S[64] = {
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
    5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
};

static void md5Block(uint32_t state[4], const uint8_t *p)
{
    uint32_t m[16];
    for (int i = 0; i < 16; i++) {
        m[i] = (uint32_t)p[i * 4] | ((uint32_t)p[i * 4 + 1] << 8) |
               ((uint32_t)p[i * 4 + 2] << 16) | ((uint32_t)p[i * 4 + 3] << 24);
    }
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    for (int i = 0; i < 64; i++) {
        uint32_t f;
        int g;
        if (i < 16) {
            f = (b & c) | (~b & d);
            g = i;
        } else if (i < 32) {
            f = (d & b) | (~d & c);
            g = (5 * i + 1) & 15;
        } else if (i < 48) {
            f = b ^ c ^ d;
            g = (3 * i + 5) & 15;
        } else {
            f = c ^ (b | ~d);
            g = (7 * i) & 15;
        }
        uint32_t tmp = d;
        d = c;
        c = b;
        b = b + ROTL(a + f + kMd5K[i] + m[g], kMd5S[i]);
        a = tmp;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

void ucxapp_md5_init(ucxapp_md5_t *ctx)
{
    ctx->state[0] = 0x67452301U;
    ctx->state[1] = 0xefcdab89U;
    ctx->state[2] = 0x98badcfeU;
    ctx->state[3] = 0x10325476U;
    ctx->bits = 0;
}

void ucxapp_md5_update(ucxapp_md5_t *ctx, const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    size_t fill = (size_t)((ctx->bits >> 3) & 63);
    ctx->bits += (uint64_t)len << 3;
    while (len > 0) {
        size_t n = 64 - fill < len ? 64 - fill : len;
        memcpy(ctx->block + fill, p, n);
        fill += n;
        p += n;
        len -= n;
        if (fill == 64) {
            md5Block(ctx->state, ctx->block);
            fill = 0;
        }
    }
}

void ucxapp_md5_final(ucxapp_md5_t *ctx, uint8_t digest[UCXAPP_MD5_LEN])
{
    uint64_t bits = ctx->bits;
    uint8_t pad[72] = { 0x80 };
    size_t fill = (size_t)((bits >> 3) & 63);
    size_t padLen = (fill < 56 ? 56 : 120) - fill;
    for (int i = 0; i < 8; i++) {
        pad[padLen + i] = (uint8_t)(bits >> (8 * i));
    }
    ucxapp_md5_update(ctx, pad, padLen + 8);
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            digest[i * 4 + j] = (uint8_t)(ctx->state[i] >> (8 * j));
        }
    }
    memset(ctx, 0, sizeof(*ctx));
}

void ucxapp_md5_hex(const void *data, size_t len, char *hex)
{
    ucxapp_md5_t ctx;
    uint8_t digest[UCXAPP_MD5_LEN];
    ucxapp_md5_init(&ctx);
    ucxapp_md5_update(&ctx, data, len);
    ucxapp_md5_final(&ctx, digest);
    toHex(digest, sizeof(digest), hex);
}

/* ---- SHA-256 (FIPS 180-4) ---- */

static const uint32_t kSha256K[64] = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
};

static void sha256Block(uint32_t state[8], const uint8_t *p)
{
    uint32_t w[64];
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) |
               ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
    }
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ROTR(w[i - 15], 7) ^ ROTR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ROTR(w[i - 2], 17) ^ ROTR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t v[8];
    memcpy(v, state, sizeof(v));
    for (int i = 0; i < 64; i++) {
        uint32_t t1 = v[7] + (ROTR(v[4], 6) ^ ROTR(v[4], 11) ^ ROTR(v[4], 25)) +
                      ((v[4] & v[5]) ^ (~v[4] & v[6])) + kSha256K[i] + w[i];
        uint32_t t2 = (ROTR(v[0], 2) ^ ROTR(v[0], 13) ^ ROTR(v[0], 22)) +
                      ((v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]));
        v[7] = v[6];
        v[6] = v[5];
        v[5] = v[4];
        v[4] = v[3] + t1;
        v[3] = v[2];
        v[2] = v[1];
        v[1] = v[0];
        v[0] = t1 + t2;
    }
    for (int i = 0; i < 8; i++) {
        state[i] += v[i];
    }
}

void ucxapp_sha256_init(ucxapp_sha256_t *ctx)
{
    static const uint32_t init[8] = {
        0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
        0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U
    };
    memcpy(ctx->state, init, sizeof(init));
    ctx->bits = 0;
}

void ucxapp_sha256_update(ucxapp_sha256_t *ctx, const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    size_t fill = (size_t)((ctx->bits >> 3) & 63);
    ctx->bits += (uint64_t)len << 3;
    while (len > 0) {
        size_t n = 64 - fill < len ? 64 - fill : len;
        memcpy(ctx->block + fill, p, n);
        fill += n;
        p += n;
        len -= n;
        if (fill == 64) {
            sha256Block(ctx->state, ctx->block);
            fill = 0;
        }
    }
}

void ucxapp_sha256_final(ucxapp_sha256_t *ctx, uint8_t digest[UCXAPP_SHA256_LEN])
{
    uint64_t bits = ctx->bits;
    uint8_t pad[72] = { 0x80 };
    size_t fill = (size_t)((bits >> 3) & 63);
    size_t padLen = (fill < 56 ? 56 : 120) - fill;
    for (int i = 0; i < 8; i++) {
        pad[padLen + i] = (uint8_t)(bits >> (8 * (7 - i)));
    }
    ucxapp_sha256_update(ctx, pad, padLen + 8);
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 4; j++) {
            digest[i * 4 + j] = (uint8_t)(ctx->state[i] >> (8 * (3 - j)));
        }
    }
    memset(ctx, 0, sizeof(*ctx));
}

void ucxapp_sha256_hex(const void *data, size_t len, char *hex)
{
    ucxapp_sha256_t ctx;
    uint8_t digest[UCXAPP_SHA256_LEN];
    ucxapp_sha256_init(&ctx);
    ucxapp_sha256_update(&ctx, data, len);
    ucxapp_sha256_final(&ctx, digest);
    toHex(digest, sizeof(digest), hex);
}
