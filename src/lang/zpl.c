/*
 * ZPL II (Zebra label printers).
 *
 *   ^XA
 *   ^FO<x>,<y>^GFA,<bytes>,<bytes>,<bytes per row>,<ASCII hex>^FS
 *   ^PQ<copies>
 *   ^XZ
 *
 * ^GFA hex is 1 = black, our layout as-is.  The two byte counts differ only
 * when the data is compressed, which it is not here.
 *
 * ZPL also accepts run-length (ACS) compression in the hex stream - roughly
 * a tenfold saving on label artwork, and a pure addition to this file.
 */
#include "rcpt_internal.h"

static void emit(rcpt_buf *out, const rcpt_image *img, int32_t x, int32_t y, uint32_t copies, uint32_t flags) {
    int raw        = (flags & RCPT_F_RAW) != 0;
    int invert     = RCPT_INVERT(flags, 0);
    uint32_t total = img->stride * img->height;

    if (!raw) rcpt_w_str(out, "^XA\r\n");

    rcpt_w_str(out, "^FO");
    rcpt_w_int(out, x);
    rcpt_w_byte(out, ',');
    rcpt_w_int(out, y);
    rcpt_w_str(out, "^GFA,");
    rcpt_w_uint(out, total);
    rcpt_w_byte(out, ',');
    rcpt_w_uint(out, total);
    rcpt_w_byte(out, ',');
    rcpt_w_uint(out, img->stride);
    rcpt_w_byte(out, ',');
    rcpt_w_hex(out, img->data, (size_t)total, invert);
    rcpt_w_str(out, "^FS\r\n");

    if (!raw) {
        rcpt_w_str(out, "^PQ");
        rcpt_w_uint(out, copies ? copies : 1u);
        rcpt_w_str(out, "\r\n^XZ\r\n");
    }
}

rcpt_result rcpt_enc_zpl(rcpt_buf *out, const rcpt_image *img, int32_t x, int32_t y, uint32_t copies, uint32_t flags) {
    RCPT_ENCODE(img, x, y, copies, flags);
}
