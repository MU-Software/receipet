/*
 * CPCL (Zebra/Comtec mobile printers).
 *
 *   ! 0 <xdpi> <ydpi> <label height> <copies>
 *   EG <width_in_bytes> <height> <x> <y> <ASCII hex>
 *   FORM
 *   PRINT
 *
 * EG hex is 1 = black, our layout verbatim. The header height must cover
 * the image plus its y offset or the bottom is clipped.
 */
#include "rcpt_internal.h"

static void emit(rcpt_buf *out, const rcpt_image *img, int32_t x, int32_t y, uint32_t dpi, uint32_t copies, uint32_t flags) {
    int raw      = (flags & RCPT_F_RAW) != 0;
    int invert   = RCPT_INVERT(flags, 0);
    uint32_t res = dpi ? dpi : 203u;

    if (!raw) {
        int32_t bottom = y + (int32_t)img->height;
        rcpt_w_str(out, "! 0 ");
        rcpt_w_uint(out, res);
        rcpt_w_byte(out, ' ');
        rcpt_w_uint(out, res);
        rcpt_w_byte(out, ' ');
        rcpt_w_uint(out, bottom > 0 ? (uint32_t)bottom : img->height);
        rcpt_w_byte(out, ' ');
        rcpt_w_uint(out, copies ? copies : 1u);
        rcpt_w_str(out, "\r\n");
    }

    rcpt_w_str(out, "EG ");
    rcpt_w_uint(out, img->stride);
    rcpt_w_byte(out, ' ');
    rcpt_w_uint(out, img->height);
    rcpt_w_byte(out, ' ');
    rcpt_w_int(out, x);
    rcpt_w_byte(out, ' ');
    rcpt_w_int(out, y);
    rcpt_w_byte(out, ' ');
    rcpt_w_hex(out, img->data, (size_t)img->stride * img->height, invert);
    rcpt_w_str(out, "\r\n");

    if (!raw) rcpt_w_str(out, "FORM\r\nPRINT\r\n");
}

rcpt_result rcpt_enc_cpcl(rcpt_buf *out, const rcpt_image *img, int32_t x, int32_t y, uint32_t dpi, uint32_t copies, uint32_t flags) {
    RCPT_ENCODE(img, x, y, dpi, copies, flags);
}
