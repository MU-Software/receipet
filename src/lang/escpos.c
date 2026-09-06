/*
 * ESC/POS raster bit image.
 *
 *   GS v 0 m xL xH yL yH d1..dk      (1D 76 30 m ...)
 *
 * x is the row width in *bytes*, y the row count, and the data layout is
 * ours exactly, so the payload goes out untouched.  Bands print
 * back-to-back with no visible seam.
 */
#include "rcpt_internal.h"

#define ESC 0x1B
#define GS  0x1D

static void band(rcpt_buf *out, const rcpt_image *img, uint32_t y0, uint32_t rows, int invert) {
    uint8_t hdr[8];
    hdr[0] = GS;
    hdr[1] = 'v';
    hdr[2] = '0';
    hdr[3] = 0;                              /* m: normal (no scaling) */
    hdr[4] = (uint8_t)(img->stride & 0xFFu); /* xL */
    hdr[5] = (uint8_t)(img->stride >> 8);    /* xH */
    hdr[6] = (uint8_t)(rows & 0xFFu);        /* yL */
    hdr[7] = (uint8_t)(rows >> 8);           /* yH */
    rcpt_w_bytes(out, hdr, sizeof(hdr));
    rcpt_w_image_bits(out, img, y0, rows, invert, 0);
}

static void emit(rcpt_buf *out, const rcpt_image *img, uint32_t chunk_rows, uint32_t flags) {
    int raw        = (flags & RCPT_F_RAW) != 0;
    int invert     = RCPT_INVERT(flags, 0);
    uint32_t chunk = chunk_rows ? chunk_rows : img->height;
    uint32_t y;

    if (chunk > 0xFFFFu) chunk = 0xFFFFu;

    if (!raw) {
        uint8_t init[2] = { ESC, '@' }; /* ESC @  - reset */
        rcpt_w_bytes(out, init, sizeof(init));
    }

    for (y = 0; y < img->height; y += chunk) {
        uint32_t rows = img->height - y;
        if (rows > chunk) rows = chunk;
        band(out, img, y, rows, invert);
    }

    if (!raw) {
        uint8_t feed[3] = { ESC, 'd', 3 };    /* feed 3 lines   */
        uint8_t cut[4]  = { GS, 'V', 66, 0 }; /* partial cut    */
        rcpt_w_bytes(out, feed, sizeof(feed));
        rcpt_w_bytes(out, cut, sizeof(cut));
    }
}

rcpt_result rcpt_enc_escpos(rcpt_buf *out, const rcpt_image *img, uint32_t chunk_rows, uint32_t flags) {
    RCPT_ENCODE(img, chunk_rows, flags);
}
