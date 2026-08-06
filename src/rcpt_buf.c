#include <stdlib.h>
#include <string.h>

#include "rcpt_internal.h"

static const char HEX_DIGITS[] = "0123456789ABCDEF";

/* Also clears *out, so a caller that ignores the result still holds a buffer
 * that is safe to read and to free.  The image tests guard against a
 * hand-built rcpt_image; past here the back-ends assume a good one. */
rcpt_result rcpt_enc_start(rcpt_buf *out, const rcpt_image *img) {
    rcpt_buf empty = RCPT_BUF_EMPTY;

    if (!out) return RCPT_ERR_ARG;
    *out = empty;

    if (!img || !img->data) return RCPT_ERR_ARG;
    if (img->width == 0 || img->height == 0) return RCPT_ERR_ARG;
    if (img->width > RCPT_MAX_DIM || img->height > RCPT_MAX_DIM) return RCPT_ERR_ARG;
    if (img->stride != (img->width + 7u) / 8u) return RCPT_ERR_ARG;
    return RCPT_OK;
}

void rcpt_buf_free(rcpt_buf *b) {
    if (!b) return;
    free(b->data);
    b->data = NULL;
    b->cap  = 0;
    b->len  = 0;
}

rcpt_result rcpt_buf_alloc(rcpt_buf *b, size_t cap) {
    void *p;

    if (cap == 0) { /* the encoder wrote nothing */
        rcpt_buf_free(b);
        return RCPT_ERR_INTERNAL;
    }

    p = malloc(cap);
    if (!p) {
        rcpt_buf_free(b);
        return RCPT_ERR_NOMEM;
    }

    b->data = (uint8_t *)p;
    b->cap  = cap;
    b->len  = 0;
    return RCPT_OK;
}

rcpt_result rcpt_buf_done(rcpt_buf *b) {
    /* A different length means the two runs disagree: a bug, not a runtime
     * condition.  Drop the buffer rather than hand back a half-built one. */
    if (b->data && b->len == b->cap) return RCPT_OK;
    rcpt_buf_free(b);
    return RCPT_ERR_INTERNAL;
}

void rcpt_w_bytes(rcpt_buf *b, const void *p, size_t n) {
    if (b->len < b->cap) {
        size_t room = b->cap - b->len;
        memcpy(b->data + b->len, p, n < room ? n : room);
    }
    b->len += n; /* counted even when dropped - that is how measuring works */
}

void rcpt_w_byte(rcpt_buf *b, uint8_t v) {
    rcpt_w_bytes(b, &v, 1);
}

void rcpt_w_str(rcpt_buf *b, const char *s) {
    rcpt_w_bytes(b, s, strlen(s));
}

void rcpt_w_uint(rcpt_buf *b, uint32_t v) {
    char tmp[10];
    int i = (int)sizeof(tmp);

    if (v == 0) {
        rcpt_w_byte(b, '0');
        return;
    }
    while (v) {
        tmp[--i] = (char)('0' + (v % 10u));
        v /= 10u;
    }
    rcpt_w_bytes(b, tmp + i, sizeof(tmp) - (size_t)i);
}

void rcpt_w_int(rcpt_buf *b, int32_t v) {
    if (v < 0) {
        rcpt_w_byte(b, '-');
        rcpt_w_uint(b, (uint32_t)(-(int64_t)v));
    } else {
        rcpt_w_uint(b, (uint32_t)v);
    }
}

void rcpt_w_bits(rcpt_buf *b, const uint8_t *p, size_t n, int invert) {
    if (!invert) {
        rcpt_w_bytes(b, p, n);
        return;
    }
    while (n) {
        uint8_t tmp[128];
        size_t i, chunk = n < sizeof(tmp) ? n : sizeof(tmp);
        for (i = 0; i < chunk; i++)
            tmp[i] = (uint8_t)~p[i];
        rcpt_w_bytes(b, tmp, chunk);
        p += chunk;
        n -= chunk;
    }
}

void rcpt_w_hex(rcpt_buf *b, const uint8_t *p, size_t n, int invert) {
    while (n) {
        char tmp[256];
        size_t i, chunk = n < sizeof(tmp) / 2 ? n : sizeof(tmp) / 2;
        for (i = 0; i < chunk; i++) {
            uint8_t v      = invert ? (uint8_t)~p[i] : p[i];
            tmp[i * 2]     = HEX_DIGITS[v >> 4];
            tmp[i * 2 + 1] = HEX_DIGITS[v & 0x0Fu];
        }
        rcpt_w_bytes(b, tmp, chunk * 2);
        p += chunk;
        n -= chunk;
    }
}
