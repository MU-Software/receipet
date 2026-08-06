/*
 * TSPL / TSPL2 (TSC label printers).
 *
 *   BITMAP x,y,width_in_bytes,height,mode,<raw binary>
 *
 * TSPL inherits the Windows DIB convention: 0 prints black, the opposite of
 * every other format here, so the payload is inverted by default.
 *
 * SIZE / GAP / DENSITY depend on the media and are not emitted; send them
 * once at job start.
 */
#include "rcpt_internal.h"

static void emit(rcpt_buf *out, const rcpt_image *img, int32_t x, int32_t y, uint8_t mode, uint32_t copies, uint32_t flags) {
    int raw    = (flags & RCPT_F_RAW) != 0;
    int invert = RCPT_INVERT(flags, 1);

    if (!raw) rcpt_w_str(out, "CLS\r\n");

    rcpt_w_str(out, "BITMAP ");
    rcpt_w_int(out, x);
    rcpt_w_byte(out, ',');
    rcpt_w_int(out, y);
    rcpt_w_byte(out, ',');
    rcpt_w_uint(out, img->stride);
    rcpt_w_byte(out, ',');
    rcpt_w_uint(out, img->height);
    rcpt_w_byte(out, ',');
    rcpt_w_uint(out, mode);
    rcpt_w_byte(out, ',');
    rcpt_w_bits(out, img->data, (size_t)img->stride * img->height, invert);
    rcpt_w_str(out, "\r\n");

    if (!raw) {
        rcpt_w_str(out, "PRINT 1,");
        rcpt_w_uint(out, copies ? copies : 1u);
        rcpt_w_str(out, "\r\n");
    }
}

rcpt_result rcpt_enc_tspl(rcpt_buf *out, const rcpt_image *img, int32_t x, int32_t y, uint8_t mode, uint32_t copies, uint32_t flags) {
    RCPT_ENCODE(img, x, y, mode, copies, flags);
}
