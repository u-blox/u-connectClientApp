#include "ucxapp_cmd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir(p, 0777)
#endif

#define MAX_FILE_SIZE  (256u * 1024u * 1024u)
#define ZIP_MAX_FILES  (UCXAPP_MAX_ARGS - 2)
#define ZIP_SIG_LOCAL  0x04034b50u
#define ZIP_SIG_CENTRAL 0x02014b50u
#define ZIP_SIG_END    0x06054b50u
#define ZIP_UTF8_FLAG  0x0800u

/* ---- CRC-32 ----------------------------------------------------------------- */

static uint32_t crc32Of(const uint8_t *data, size_t len)
{
    static uint32_t table[256];
    static bool ready = false;
    uint32_t crc = 0xFFFFFFFFu;

    if (!ready) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) {
                c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            }
            table[i] = c;
        }
        ready = true;
    }
    for (size_t i = 0; i < len; i++) {
        crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
    }
    return ~crc;
}

/* ---- Deflate: fixed-Huffman compressor ---------------------------------------- */

static const uint16_t kLenBase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                                      35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const uint8_t kLenExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
                                      3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const uint16_t kDistBase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
                                       257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
                                       8193, 12289, 16385, 24577};
static const uint8_t kDistExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
                                       7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

#define WINDOW 32768u
#define HASH_BITS 15

typedef struct {
    uint8_t *buf;
    size_t len;
    size_t cap;
    uint32_t acc;
    int nbits;
    bool over;
} bitWriter_t;

static void putBits(bitWriter_t *w, uint32_t value, int count)
{
    w->acc |= value << w->nbits;
    w->nbits += count;
    while (w->nbits >= 8) {
        if (w->len < w->cap) {
            w->buf[w->len++] = (uint8_t)w->acc;
        } else {
            w->over = true;
        }
        w->acc >>= 8;
        w->nbits -= 8;
    }
}

static uint32_t reverseBits(uint32_t code, int count)
{
    uint32_t r = 0;

    while (count-- > 0) {
        r = (r << 1) | (code & 1u);
        code >>= 1;
    }
    return r;
}

static void putSymbol(bitWriter_t *w, int sym)
{
    if (sym < 144) {
        putBits(w, reverseBits((uint32_t)(0x30 + sym), 8), 8);
    } else if (sym < 256) {
        putBits(w, reverseBits((uint32_t)(0x190 + sym - 144), 9), 9);
    } else if (sym < 280) {
        putBits(w, reverseBits((uint32_t)(sym - 256), 7), 7);
    } else {
        putBits(w, reverseBits((uint32_t)(0xC0 + sym - 280), 8), 8);
    }
}

static uint32_t hash3(const uint8_t *p)
{
    return (((uint32_t)p[0] << 10) ^ ((uint32_t)p[1] << 5) ^ p[2]) & ((1u << HASH_BITS) - 1u);
}

/* Returns the compressed size, or 0 if it does not fit in cap or memory is short. */
static size_t deflateFixed(const uint8_t *in, size_t n, uint8_t *out, size_t cap)
{
    int32_t *head = (int32_t *)malloc(sizeof(int32_t) << HASH_BITS);
    int32_t *prev = (int32_t *)malloc(sizeof(int32_t) * WINDOW);
    bitWriter_t w = { out, 0, cap, 0, 0, false };
    size_t i = 0;

    if (head == NULL || prev == NULL) {
        free(head);
        free(prev);
        return 0;
    }
    memset(head, 0xFF, sizeof(int32_t) << HASH_BITS);
    putBits(&w, 1, 1);   /* final block */
    putBits(&w, 1, 2);   /* fixed Huffman */

    while (i < n) {
        size_t bestLen = 0;
        size_t bestDist = 0;

        if (i + 3 <= n) {
            int32_t cand = head[hash3(in + i)];
            size_t maxLen = (n - i < 258) ? n - i : 258;

            for (int tries = 32; cand >= 0 && i - (size_t)cand <= WINDOW && tries > 0; tries--) {
                size_t l = 0;
                while (l < maxLen && in[(size_t)cand + l] == in[i + l]) {
                    l++;
                }
                if (l > bestLen) {
                    bestLen = l;
                    bestDist = i - (size_t)cand;
                    if (l == maxLen) {
                        break;
                    }
                }
                cand = prev[(size_t)cand & (WINDOW - 1u)];
            }
        }

        size_t step = 1;
        if (bestLen >= 3) {
            int ls = 28;
            int ds = 29;
            while (kLenBase[ls] > bestLen) {
                ls--;
            }
            while (kDistBase[ds] > bestDist) {
                ds--;
            }
            putSymbol(&w, 257 + ls);
            putBits(&w, (uint32_t)(bestLen - kLenBase[ls]), kLenExtra[ls]);
            putBits(&w, reverseBits((uint32_t)ds, 5), 5);
            putBits(&w, (uint32_t)(bestDist - kDistBase[ds]), kDistExtra[ds]);
            step = bestLen;
        } else {
            putSymbol(&w, in[i]);
        }
        for (size_t k = 0; k < step; k++) {
            size_t pos = i + k;
            if (pos + 3 <= n) {
                uint32_t h = hash3(in + pos);
                prev[pos & (WINDOW - 1u)] = head[h];
                head[h] = (int32_t)pos;
            }
        }
        i += step;
    }
    putSymbol(&w, 256);
    if (w.nbits > 0) {
        putBits(&w, 0, 8 - w.nbits);
    }
    free(head);
    free(prev);
    return w.over ? 0 : w.len;
}

/* ---- Inflate (decoder, after zlib's puff) --------------------------------------- */

typedef struct {
    const uint8_t *in;
    size_t inLen;
    size_t inPos;
    uint32_t bitBuf;
    int bitCnt;
    uint8_t *out;
    size_t outLen;
    size_t outPos;
} inflater_t;

typedef struct {
    short count[16];
    short symbol[288];
} huff_t;

static int getBits(inflater_t *s, int need)
{
    uint32_t val = s->bitBuf;

    while (s->bitCnt < need) {
        if (s->inPos == s->inLen) {
            return -1;
        }
        val |= (uint32_t)s->in[s->inPos++] << s->bitCnt;
        s->bitCnt += 8;
    }
    s->bitBuf = val >> need;
    s->bitCnt -= need;
    return (int)(val & ((1u << need) - 1u));
}

static int decodeSymbol(inflater_t *s, const huff_t *h)
{
    int code = 0;
    int first = 0;
    int index = 0;

    for (int len = 1; len <= 15; len++) {
        int b = getBits(s, 1);
        if (b < 0) {
            return -1;
        }
        code |= b;
        int count = h->count[len];
        if (code - count < first) {
            return h->symbol[index + (code - first)];
        }
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -2;
}

static int buildHuff(huff_t *h, const short *length, int n)
{
    short offs[16];
    int left = 1;

    for (int len = 0; len <= 15; len++) {
        h->count[len] = 0;
    }
    for (int sym = 0; sym < n; sym++) {
        h->count[length[sym]]++;
    }
    if (h->count[0] == n) {
        return 0;
    }
    for (int len = 1; len <= 15; len++) {
        left <<= 1;
        left -= h->count[len];
        if (left < 0) {
            return left;
        }
    }
    offs[1] = 0;
    for (int len = 1; len < 15; len++) {
        offs[len + 1] = (short)(offs[len] + h->count[len]);
    }
    for (int sym = 0; sym < n; sym++) {
        if (length[sym] != 0) {
            h->symbol[offs[length[sym]]++] = (short)sym;
        }
    }
    return left;
}

static int inflateCodes(inflater_t *s, const huff_t *lenCode, const huff_t *distCode)
{
    for (;;) {
        int symbol = decodeSymbol(s, lenCode);

        if (symbol < 0) {
            return symbol;
        }
        if (symbol < 256) {
            if (s->outPos == s->outLen) {
                return 1;
            }
            s->out[s->outPos++] = (uint8_t)symbol;
        } else if (symbol > 256) {
            symbol -= 257;
            if (symbol >= 29) {
                return -10;
            }
            int extra = getBits(s, kLenExtra[symbol]);
            if (extra < 0) {
                return -1;
            }
            size_t len = (size_t)kLenBase[symbol] + (size_t)extra;
            symbol = decodeSymbol(s, distCode);
            if (symbol < 0) {
                return symbol;
            }
            if (symbol >= 30) {
                return -10;
            }
            extra = getBits(s, kDistExtra[symbol]);
            if (extra < 0) {
                return -1;
            }
            size_t dist = (size_t)kDistBase[symbol] + (size_t)extra;
            if (dist > s->outPos) {
                return -11;
            }
            if (s->outPos + len > s->outLen) {
                return 1;
            }
            while (len-- > 0) {
                s->out[s->outPos] = s->out[s->outPos - dist];
                s->outPos++;
            }
        } else {
            return 0;
        }
    }
}

static int inflateStored(inflater_t *s)
{
    s->bitBuf = 0;
    s->bitCnt = 0;
    if (s->inPos + 4 > s->inLen) {
        return -1;
    }
    size_t len = (size_t)s->in[s->inPos] | ((size_t)s->in[s->inPos + 1] << 8);
    size_t nlen = (size_t)s->in[s->inPos + 2] | ((size_t)s->in[s->inPos + 3] << 8);
    s->inPos += 4;
    if (len != (~nlen & 0xFFFFu)) {
        return -2;
    }
    if (s->inPos + len > s->inLen || s->outPos + len > s->outLen) {
        return 1;
    }
    memcpy(s->out + s->outPos, s->in + s->inPos, len);
    s->inPos += len;
    s->outPos += len;
    return 0;
}

static int inflateFixed(inflater_t *s)
{
    static huff_t lenCode;
    static huff_t distCode;
    static bool ready = false;

    if (!ready) {
        short lengths[288];
        int sym = 0;
        for (; sym < 144; sym++) lengths[sym] = 8;
        for (; sym < 256; sym++) lengths[sym] = 9;
        for (; sym < 280; sym++) lengths[sym] = 7;
        for (; sym < 288; sym++) lengths[sym] = 8;
        buildHuff(&lenCode, lengths, 288);
        for (sym = 0; sym < 30; sym++) lengths[sym] = 5;
        buildHuff(&distCode, lengths, 30);
        ready = true;
    }
    return inflateCodes(s, &lenCode, &distCode);
}

static int inflateDynamic(inflater_t *s)
{
    static const short order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    short lengths[320];
    huff_t lenCode;
    huff_t distCode;
    int nlen = getBits(s, 5) + 257;
    int ndist = getBits(s, 5) + 1;
    int ncode = getBits(s, 4) + 4;
    int index;
    int err;

    if (nlen > 286 || ndist > 30 || ncode < 4) {
        return -3;
    }
    for (index = 0; index < ncode; index++) {
        int v = getBits(s, 3);
        if (v < 0) {
            return -1;
        }
        lengths[order[index]] = (short)v;
    }
    for (; index < 19; index++) {
        lengths[order[index]] = 0;
    }
    if (buildHuff(&lenCode, lengths, 19) != 0) {
        return -4;
    }
    index = 0;
    while (index < nlen + ndist) {
        int symbol = decodeSymbol(s, &lenCode);
        if (symbol < 0) {
            return symbol;
        }
        if (symbol < 16) {
            lengths[index++] = (short)symbol;
        } else {
            short fill = 0;
            int rep;
            if (symbol == 16) {
                if (index == 0) {
                    return -5;
                }
                fill = lengths[index - 1];
                rep = 3 + getBits(s, 2);
            } else if (symbol == 17) {
                rep = 3 + getBits(s, 3);
            } else {
                rep = 11 + getBits(s, 7);
            }
            if (index + rep > nlen + ndist) {
                return -6;
            }
            while (rep-- > 0) {
                lengths[index++] = fill;
            }
        }
    }
    if (lengths[256] == 0) {
        return -9;
    }
    err = buildHuff(&lenCode, lengths, nlen);
    if (err != 0 && (err < 0 || nlen != lenCode.count[0] + lenCode.count[1])) {
        return -7;
    }
    err = buildHuff(&distCode, lengths + nlen, ndist);
    if (err != 0 && (err < 0 || ndist != distCode.count[0] + distCode.count[1])) {
        return -8;
    }
    return inflateCodes(s, &lenCode, &distCode);
}

/* Inflates a raw deflate stream into out, which must be exactly outLen bytes. */
static bool inflateRaw(const uint8_t *in, size_t inLen, uint8_t *out, size_t outLen)
{
    inflater_t s = { in, inLen, 0, 0, 0, out, outLen, 0 };
    int last;

    do {
        last = getBits(&s, 1);
        int type = getBits(&s, 2);
        int err;

        if (last < 0 || type < 0) {
            return false;
        }
        if (type == 0) {
            err = inflateStored(&s);
        } else if (type == 1) {
            err = inflateFixed(&s);
        } else if (type == 2) {
            err = inflateDynamic(&s);
        } else {
            err = -1;
        }
        if (err != 0) {
            return false;
        }
    } while (!last);
    return s.outPos == outLen;
}

/* ---- Files and little-endian helpers ------------------------------------------- */

static void put16(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put32(uint8_t *p, uint32_t v)
{
    put16(p, v & 0xFFFFu);
    put16(p + 2, v >> 16);
}

static uint32_t get16(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8);
}

static uint32_t get32(const uint8_t *p)
{
    return get16(p) | (get16(p + 2) << 16);
}

static uint8_t *readWholeFile(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    uint8_t *data = NULL;
    long len;

    if (f == NULL) {
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) == 0 && (len = ftell(f)) >= 0 && (unsigned long)len <= MAX_FILE_SIZE &&
        fseek(f, 0, SEEK_SET) == 0) {
        data = (uint8_t *)malloc((size_t)len + 1);
        if (data != NULL && fread(data, 1, (size_t)len, f) != (size_t)len) {
            free(data);
            data = NULL;
        }
        *size = (size_t)len;
    }
    fclose(f);
    return data;
}

static const char *baseName(const char *path)
{
    const char *slash = strrchr(path, '/');
    const char *back = strrchr(path, '\\');

    if (back != NULL && (slash == NULL || back > slash)) {
        slash = back;
    }
    return slash != NULL ? slash + 1 : path;
}

/* ---- utils.zip ------------------------------------------------------------------- */

typedef struct {
    char name[256];
    uint32_t crc;
    uint32_t csize;
    uint32_t usize;
    uint16_t method;
    uint32_t offset;
} zipEntry_t;

static bool writeAll(FILE *f, const void *data, size_t len)
{
    return len == 0 || fwrite(data, 1, len, f) == len;
}

static int cmdZip(ucxapp_t *app, int argc, char *argv[])
{
    zipEntry_t entries[ZIP_MAX_FILES];
    int count = argc - 2;
    uint32_t offset = 0;
    uint32_t cdSize = 0;
    uint8_t hdr[46];
    FILE *zip;
    int rc = UCXAPP_ERR_FAILED;

    (void)app;
    if (argc < 3 || count > (int)ZIP_MAX_FILES) {
        return UCXAPP_ERR_USAGE;
    }
    zip = fopen(argv[1], "wb");
    if (zip == NULL) {
        ucxapp_print("ERROR: cannot create %s\n", argv[1]);
        return UCXAPP_ERR_FAILED;
    }

    for (int i = 0; i < count; i++) {
        zipEntry_t *e = &entries[i];
        const char *name = baseName(argv[2 + i]);
        size_t size = 0;
        size_t nameLen = strlen(name);
        uint8_t *data = readWholeFile(argv[2 + i], &size);
        uint8_t *packed = NULL;
        size_t packedLen = 0;
        const uint8_t *body;

        if (data == NULL) {
            ucxapp_print("ERROR: cannot read %s\n", argv[2 + i]);
            goto done;
        }
        if (nameLen == 0 || nameLen >= sizeof(e->name) || size >= MAX_FILE_SIZE) {
            ucxapp_print("ERROR: unsupported file %s\n", argv[2 + i]);
            free(data);
            goto done;
        }
        memcpy(e->name, name, nameLen + 1);
        e->crc = crc32Of(data, size);
        e->usize = (uint32_t)size;
        e->offset = offset;

        if (size > 0) {
            size_t cap = size + size / 8 + 16;
            packed = (uint8_t *)malloc(cap);
            if (packed != NULL) {
                packedLen = deflateFixed(data, size, packed, cap);
            }
        }
        if (packedLen > 0 && packedLen < size) {
            e->method = 8;
            body = packed;
            e->csize = (uint32_t)packedLen;
        } else {
            e->method = 0;
            body = data;
            e->csize = (uint32_t)size;
        }

        put32(hdr, ZIP_SIG_LOCAL);
        put16(hdr + 4, 20);
        put16(hdr + 6, ZIP_UTF8_FLAG);
        put16(hdr + 8, e->method);
        put16(hdr + 10, 0);        /* time */
        put16(hdr + 12, 0x21);     /* date: 1980-01-01 */
        put32(hdr + 14, e->crc);
        put32(hdr + 18, e->csize);
        put32(hdr + 22, e->usize);
        put16(hdr + 26, (uint32_t)nameLen);
        put16(hdr + 28, 0);
        if (!writeAll(zip, hdr, 30) || !writeAll(zip, name, nameLen) || !writeAll(zip, body, e->csize)) {
            ucxapp_print("ERROR: write failed\n");
            free(data);
            free(packed);
            goto done;
        }
        offset += 30 + (uint32_t)nameLen + e->csize;
        ucxapp_print("  %s: %u -> %u bytes\n", name, (unsigned)e->usize, (unsigned)e->csize);
        free(data);
        free(packed);
    }

    for (int i = 0; i < count; i++) {
        zipEntry_t *e = &entries[i];
        size_t nameLen = strlen(e->name);

        memset(hdr, 0, sizeof(hdr));
        put32(hdr, ZIP_SIG_CENTRAL);
        put16(hdr + 4, 20);
        put16(hdr + 6, 20);
        put16(hdr + 8, ZIP_UTF8_FLAG);
        put16(hdr + 10, e->method);
        put16(hdr + 14, 0x21);
        put32(hdr + 16, e->crc);
        put32(hdr + 20, e->csize);
        put32(hdr + 24, e->usize);
        put16(hdr + 28, (uint32_t)nameLen);
        put32(hdr + 42, e->offset);
        if (!writeAll(zip, hdr, 46) || !writeAll(zip, e->name, nameLen)) {
            ucxapp_print("ERROR: write failed\n");
            goto done;
        }
        cdSize += 46 + (uint32_t)nameLen;
    }

    memset(hdr, 0, 22);
    put32(hdr, ZIP_SIG_END);
    put16(hdr + 8, (uint32_t)count);
    put16(hdr + 10, (uint32_t)count);
    put32(hdr + 12, cdSize);
    put32(hdr + 16, offset);
    if (!writeAll(zip, hdr, 22)) {
        ucxapp_print("ERROR: write failed\n");
        goto done;
    }
    ucxapp_print("Wrote %s (%d file%s)\n", argv[1], count, count == 1 ? "" : "s");
    rc = UCXAPP_OK;

done:
    fclose(zip);
    return rc;
}

/* ---- utils.unzip ------------------------------------------------------------------ */

/* Reject absolute paths, drive letters, backslashes and ".." components. */
static bool safeName(const char *name)
{
    const char *p = name;

    if (name[0] == '\0' || name[0] == '/' || strchr(name, '\\') != NULL || strchr(name, ':') != NULL) {
        return false;
    }
    while (*p != '\0') {
        size_t len = strcspn(p, "/");
        if (len == 2 && p[0] == '.' && p[1] == '.') {
            return false;
        }
        p += len;
        if (*p == '/') {
            p++;
        }
    }
    return true;
}

static void makeDirs(char *path, size_t from)
{
    for (size_t i = from; path[i] != '\0'; i++) {
        if (path[i] == '/') {
            path[i] = '\0';
            (void)MKDIR(path);
            path[i] = '/';
        }
    }
}

static int cmdUnzip(ucxapp_t *app, int argc, char *argv[])
{
    const char *dir = (argc > 2) ? argv[2] : ".";
    size_t size = 0;
    uint8_t *zip;
    const uint8_t *end = NULL;
    const uint8_t *c;
    uint32_t count;
    int extracted = 0;
    int rc = UCXAPP_ERR_FAILED;

    (void)app;
    if (argc < 2 || argc > 3) {
        return UCXAPP_ERR_USAGE;
    }
    zip = readWholeFile(argv[1], &size);
    if (zip == NULL) {
        ucxapp_print("ERROR: cannot read %s\n", argv[1]);
        return UCXAPP_ERR_FAILED;
    }
    for (size_t i = size; i >= 22 && i + 65535u + 22u >= size; i--) {
        if (get32(zip + i - 22) == ZIP_SIG_END) {
            end = zip + i - 22;
            break;
        }
    }
    if (end == NULL || get32(end + 16) >= size) {
        ucxapp_print("ERROR: not a zip file\n");
        goto done;
    }
    count = get16(end + 10);
    c = zip + get32(end + 16);

    (void)MKDIR(dir);
    for (uint32_t i = 0; i < count; i++) {
        char name[256];
        char path[600];
        uint32_t method;
        uint32_t crc;
        uint32_t csize;
        uint32_t usize;
        uint32_t nameLen;
        uint32_t lho;
        uint32_t dataOff;
        const uint8_t *l;
        uint8_t *data;
        FILE *f;
        bool ok;

        if ((size_t)(c - zip) + 46 > size || get32(c) != ZIP_SIG_CENTRAL) {
            ucxapp_print("ERROR: corrupt central directory\n");
            goto done;
        }
        method = get16(c + 10);
        crc = get32(c + 16);
        csize = get32(c + 20);
        usize = get32(c + 24);
        nameLen = get16(c + 28);
        lho = get32(c + 42);
        if (nameLen >= sizeof(name) || (size_t)(c - zip) + 46 + nameLen > size) {
            ucxapp_print("ERROR: corrupt central directory\n");
            goto done;
        }
        memcpy(name, c + 46, nameLen);
        name[nameLen] = '\0';
        c += 46 + nameLen + get16(c + 30) + get16(c + 32);

        if (!safeName(name)) {
            ucxapp_print("  skipped unsafe name: %s\n", name);
            continue;
        }
        snprintf(path, sizeof(path), "%s/%s", dir, name);
        if (name[nameLen - 1] == '/') {
            makeDirs(path, 0);
            (void)MKDIR(path);
            continue;
        }
        if (usize > MAX_FILE_SIZE || (size_t)lho + 30 > size || get32(zip + lho) != ZIP_SIG_LOCAL) {
            ucxapp_print("ERROR: corrupt entry %s\n", name);
            goto done;
        }
        l = zip + lho;
        dataOff = lho + 30 + get16(l + 26) + get16(l + 28);
        if ((size_t)dataOff + csize > size) {
            ucxapp_print("ERROR: corrupt entry %s\n", name);
            goto done;
        }
        data = (uint8_t *)malloc((size_t)usize + 1);
        if (data == NULL) {
            ucxapp_print("ERROR: out of memory\n");
            goto done;
        }
        if (method == 0) {
            ok = (csize == usize);
            if (ok) {
                memcpy(data, zip + dataOff, usize);
            }
        } else if (method == 8) {
            ok = inflateRaw(zip + dataOff, csize, data, usize);
        } else {
            ok = false;
        }
        if (!ok || crc32Of(data, usize) != crc) {
            ucxapp_print("ERROR: %s: unsupported method or corrupt data\n", name);
            free(data);
            goto done;
        }
        makeDirs(path, strlen(dir) + 1);
        f = fopen(path, "wb");
        if (f == NULL || !writeAll(f, data, usize)) {
            ucxapp_print("ERROR: cannot write %s\n", path);
            if (f != NULL) {
                fclose(f);
            }
            free(data);
            goto done;
        }
        fclose(f);
        free(data);
        ucxapp_print("  %s (%u bytes)\n", path, (unsigned)usize);
        extracted++;
    }
    ucxapp_print("Extracted %d file%s to %s\n", extracted, extracted == 1 ? "" : "s", dir);
    rc = UCXAPP_OK;

done:
    free(zip);
    return rc;
}

static const ucxapp_cmd_t gUtilsCmds[] = {
    { "utils.zip",   "<archive.zip> <file> [file...]", "Create a zip archive from up to 6 files", 0, cmdZip },
    { "utils.unzip", "<archive.zip> [dir]",            "Extract a zip archive (default: current dir)", 0, cmdUnzip },
};

void ucxapp_register_utils_cmds(void)
{
    ucxapp_cmd_register(gUtilsCmds, sizeof(gUtilsCmds) / sizeof(gUtilsCmds[0]));
}
