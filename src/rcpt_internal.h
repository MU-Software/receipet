/* Shared plumbing for the encoders; not part of the public API. */
#ifndef RCPT_INTERNAL_H
#define RCPT_INTERNAL_H

#include "receipet.h"

#define RCPT_INVERT(flags, dflt) (((flags) & RCPT_F_INVERT) ? !(dflt) : (dflt))
#define RCPT_BUF_EMPTY           { NULL, 0, 0 }

/* The entire body of a rcpt_enc_* function:
 *
 *     rcpt_result rcpt_enc_zpl(rcpt_buf *out, const rcpt_image *img, ...) {
 *         RCPT_ENCODE(img, x, y, copies, flags);
 *     }
 *
 * An empty buffer counts what it discards, so the first run measures and the
 * second fills.  Arguments are emit()'s minus the buffer; writing them once
 * is the point, since the runs must agree.
 *
 * Expects, and cannot check: parameters named `out` and `img`, a static
 * emit() in the same file, and arguments free of side effects - they are
 * expanded twice. */
#define RCPT_ENCODE(...)                                            \
    do {                                                            \
        rcpt_buf b = RCPT_BUF_EMPTY;                                \
        rcpt_result rc;                                             \
                                                                    \
        if ((rc = rcpt_enc_start(out, img)) != RCPT_OK) return rc;  \
        emit(&b, __VA_ARGS__); /* measure */                        \
        if ((rc = rcpt_buf_alloc(&b, b.len)) != RCPT_OK) return rc; \
        emit(&b, __VA_ARGS__); /* fill */                           \
        rc = rcpt_buf_done(&b);                                     \
                                                                    \
        *out = b;                                                   \
        return rc;                                                  \
    } while (0)

/* While an encoder runs, rcpt_buf is its write cursor: `data`/`cap` are the
 * destination, `len` how far along.  Nothing here can fail, so an encoder
 * writes its twenty-odd calls in a row and `len` is checked once at the end. */
void rcpt_w_bytes(rcpt_buf *b, const void *p, size_t n);
void rcpt_w_byte(rcpt_buf *b, uint8_t v);
void rcpt_w_str(rcpt_buf *b, const char *s);
void rcpt_w_uint(rcpt_buf *b, uint32_t v);
void rcpt_w_int(rcpt_buf *b, int32_t v);

/* `invert` flips image bits on the way out.  Padding past the image width is
 * always kept at the format's non-printing polarity (`pad_one`). */
void rcpt_w_image_bits(rcpt_buf *b, const rcpt_image *img, uint32_t y, uint32_t rows, int invert, int pad_one);
void rcpt_w_image_hex(rcpt_buf *b, const rcpt_image *img, int invert, int pad_one);

rcpt_result rcpt_enc_start(rcpt_buf *out, const rcpt_image *img);

/* `cap` must be what the measuring run counted; anything else fails in
 * rcpt_buf_done().  Both leave the buffer empty rather than half-built. */
rcpt_result rcpt_buf_alloc(rcpt_buf *b, size_t cap);
rcpt_result rcpt_buf_done(rcpt_buf *b);

#endif /* RCPT_INTERNAL_H */
