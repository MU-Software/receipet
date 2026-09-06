/*
 * ESC/P dot matrix column graphics.
 *
 *   ESC * m nL nH d1..dk             (1B 2A m ...)
 *
 * Column oriented, unlike everything else here: one byte (8-pin) or three
 * (24-pin) per vertical slice, MSB = top pin.  So the image is transposed
 * band by band, each band followed by a feed of exactly the band height.
 *
 * ESC 3 n is n/216" on 9-pin and n/180" on ESC/P2; n = 24 is right for both
 * (8 dots at 72 dpi, 24 at 180).
 */
#include "rcpt_internal.h"

#define ESC 0x1B

static void bands(rcpt_buf *out, const rcpt_image *img, uint8_t mode, int invert) {
    uint32_t dots   = (mode >= 32) ? 24u : 8u; /* rows per band */
    uint32_t nbytes = dots / 8u;               /* bytes per column */
    uint32_t y0;

    for (y0 = 0; y0 < img->height; y0 += dots) {
        uint8_t hdr[5];
        uint32_t x;

        hdr[0] = ESC;
        hdr[1] = '*';
        hdr[2] = mode;
        hdr[3] = (uint8_t)(img->width & 0xFFu); /* nL: column count */
        hdr[4] = (uint8_t)(img->width >> 8);    /* nH */
        rcpt_w_bytes(out, hdr, sizeof(hdr));

        for (x = 0; x < img->width; x++) {
            uint8_t col[3];
            uint32_t k;
            for (k = 0; k < nbytes; k++) {
                uint8_t b = 0, j;
                for (j = 0; j < 8; j++) {
                    uint32_t y = y0 + k * 8u + j;
                    /* Inversion applies only to image pixels.  Rows needed to
                     * fill the last printer-head band stay blank. */
                    if (y < img->height && (rcpt_image_get_pixel(img, (int32_t)x, (int32_t)y) != invert)) b |= (uint8_t)(0x80u >> j);
                }
                col[k] = b;
            }
            rcpt_w_bytes(out, col, nbytes);
        }
        rcpt_w_str(out, "\r\n");
    }
}

static void emit(rcpt_buf *out, const rcpt_image *img, uint8_t mode, uint32_t flags) {
    int raw = (flags & RCPT_F_RAW) != 0;

    if (!raw) {
        uint8_t init[2]    = { ESC, '@' };
        uint8_t spacing[3] = { ESC, '3', 24 }; /* one band's worth of feed */
        rcpt_w_bytes(out, init, sizeof(init));
        rcpt_w_bytes(out, spacing, sizeof(spacing));
    }

    bands(out, img, mode, RCPT_INVERT(flags, 0));

    if (!raw) {
        uint8_t restore[2] = { ESC, '2' }; /* back to 1/6" line spacing */
        rcpt_w_bytes(out, restore, sizeof(restore));
        rcpt_w_byte(out, 0x0C); /* form feed */
    }
}

rcpt_result rcpt_enc_escp(rcpt_buf *out, const rcpt_image *img, uint8_t mode, uint32_t flags) {
    rcpt_result checked = rcpt_enc_start(out, img);
    if (checked != RCPT_OK) return checked;
    if (mode != 0 && mode != 1 && mode != 2 && mode != 3 && mode != 32 && mode != 33 && mode != 38 && mode != 39 && mode != 40) return RCPT_ERR_ARG;

    RCPT_ENCODE(img, mode, flags);
}
