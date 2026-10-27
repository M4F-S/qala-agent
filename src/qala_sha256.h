/* src/qala_sha256.h - Self-Contained ISO C99 SHA-256 Implementation
 * Dubai Create AI Agents Championship 2026-2027 | Best Business AI Agent
 * Conforms strictly to ISO C99 (-std=c99 -Wall -Wextra -Werror -O2)
 * Zero external cryptographic dependencies
 */

#ifndef QALA_SHA256_H
#define QALA_SHA256_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t state[8];
    uint64_t count;
    uint8_t  buffer[64];
} qala_sha256_t;

static inline uint32_t qala_sha256_ror32(uint32_t val, uint32_t bits) {
    return (val >> bits) | (val << (32 - bits));
}

static inline void qala_sha256_init(qala_sha256_t *ctx) {
    ctx->state[0] = 0x6a09e667UL;
    ctx->state[1] = 0xbb67ae85UL;
    ctx->state[2] = 0x3c6ef372UL;
    ctx->state[3] = 0xa54ff53aUL;
    ctx->state[4] = 0x510e527fUL;
    ctx->state[5] = 0x9b05688cUL;
    ctx->state[6] = 0x1f83d9abUL;
    ctx->state[7] = 0x5be0cd19UL;
    ctx->count = 0;
}

static const uint32_t QALA_K256[64] = {
    0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL,
    0x3956c25bUL, 0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL,
    0xd807aa98UL, 0x12835b01UL, 0x243185beUL, 0x550c7dc3UL,
    0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL, 0xc19bf174UL,
    0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL,
    0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL,
    0x983e5152UL, 0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL,
    0xc6e00bf3UL, 0xd5a79147UL, 0x06ca6351UL, 0x14292967UL,
    0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL, 0x53380d13UL,
    0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
    0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL,
    0xd192e819UL, 0xd6990624UL, 0xf40e3585UL, 0x106aa070UL,
    0x19a4c116UL, 0x1e376c08UL, 0x2748774cUL, 0x34b0bcb5UL,
    0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL, 0x682e6ff3UL,
    0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL,
    0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL
};

static inline void qala_sha256_transform(uint32_t state[8], const uint8_t data[64]) {
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
    uint32_t w[64];

    for (int i = 0; i < 16; ++i) {
        w[i] = ((uint32_t)data[i * 4] << 24) |
               ((uint32_t)data[i * 4 + 1] << 16) |
               ((uint32_t)data[i * 4 + 2] << 8) |
               ((uint32_t)data[i * 4 + 3]);
    }
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = qala_sha256_ror32(w[i - 15], 7) ^ qala_sha256_ror32(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = qala_sha256_ror32(w[i - 2], 17) ^ qala_sha256_ror32(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    for (int i = 0; i < 64; ++i) {
        uint32_t s1 = qala_sha256_ror32(e, 6) ^ qala_sha256_ror32(e, 11) ^ qala_sha256_ror32(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + s1 + ch + QALA_K256[i] + w[i];
        uint32_t s0 = qala_sha256_ror32(a, 2) ^ qala_sha256_ror32(a, 13) ^ qala_sha256_ror32(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

static inline void qala_sha256_update(qala_sha256_t *ctx, const uint8_t *data, size_t len) {
    size_t buffer_index = (size_t)(ctx->count & 63);
    ctx->count += len;

    if (buffer_index > 0) {
        size_t space = 64 - buffer_index;
        if (len < space) {
            memcpy(ctx->buffer + buffer_index, data, len);
            return;
        }
        memcpy(ctx->buffer + buffer_index, data, space);
        qala_sha256_transform(ctx->state, ctx->buffer);
        data += space;
        len -= space;
    }

    while (len >= 64) {
        qala_sha256_transform(ctx->state, data);
        data += 64;
        len -= 64;
    }

    if (len > 0) {
        memcpy(ctx->buffer, data, len);
    }
}

static inline void qala_sha256_final(qala_sha256_t *ctx, uint8_t digest[32]) {
    uint8_t bits[8];
    uint64_t total_bits = ctx->count * 8;
    for (int i = 0; i < 8; ++i) {
        bits[7 - i] = (uint8_t)((total_bits >> (i * 8)) & 0xff);
    }

    size_t rem = (size_t)(ctx->count & 63);
    size_t pad_len = (rem < 56) ? (56 - rem) : (120 - rem);
    static const uint8_t PADDING[1] = { 0x80 };
    static const uint8_t ZEROES[64] = { 0 };

    qala_sha256_update(ctx, PADDING, 1);
    if (pad_len > 1) {
        size_t p_rem = pad_len - 1;
        while (p_rem > 0) {
            size_t chunk = p_rem > 64 ? 64 : p_rem;
            qala_sha256_update(ctx, ZEROES, chunk);
            p_rem -= chunk;
        }
    }
    qala_sha256_update(ctx, bits, 8);

    for (int i = 0; i < 8; ++i) {
        digest[i * 4]     = (uint8_t)((ctx->state[i] >> 24) & 0xff);
        digest[i * 4 + 1] = (uint8_t)((ctx->state[i] >> 16) & 0xff);
        digest[i * 4 + 2] = (uint8_t)((ctx->state[i] >> 8) & 0xff);
        digest[i * 4 + 3] = (uint8_t)(ctx->state[i] & 0xff);
    }
}

static inline void qala_compute_sha256_hex(const char *input, char output_hex[65]) {
    qala_sha256_t ctx;
    uint8_t digest[32];
    qala_sha256_init(&ctx);
    if (input) {
        qala_sha256_update(&ctx, (const uint8_t *)input, strlen(input));
    }
    qala_sha256_final(&ctx, digest);
    for (int i = 0; i < 32; ++i) {
        snprintf(output_hex + (i * 2), 3, "%02x", digest[i]);
    }
    output_hex[64] = '\0';
}

static inline bool qala_compute_file_sha256_hex(const char *file_path, char output_hex[65]) {
    FILE *fp = fopen(file_path, "rb");
    if (!fp) {
        output_hex[0] = '\0';
        return false;
    }

    qala_sha256_t ctx;
    qala_sha256_init(&ctx);

    uint8_t buf[4096];
    size_t nread = 0;
    while ((nread = fread(buf, 1, sizeof(buf), fp)) > 0) {
        qala_sha256_update(&ctx, buf, nread);
    }
    fclose(fp);

    uint8_t digest[32];
    qala_sha256_final(&ctx, digest);
    for (int i = 0; i < 32; ++i) {
        snprintf(output_hex + (i * 2), 3, "%02x", digest[i]);
    }
    output_hex[64] = '\0';
    return true;
}

#ifdef __cplusplus
}
#endif

#endif /* QALA_SHA256_H */
