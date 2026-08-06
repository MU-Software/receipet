/*
 * TSPL / TSPL2 (TSC label printers).
 *
 *   BITMAP x,y,width_in_bytes,height,mode,<raw binary>
 *
 * TSPL inherits the Windows DIB convention: 0 prints black, the opposite of
 * every other format here, so the payload is inverted by default.
 *
 * chunk_rows emits one BITMAP per band, each at its own y.  Firmware that
 * caps a single BITMAP is common and it fails silently - a 4B-3044TA renders
 * 11.8 kB and drops 22.5 kB with no error - so bands of a few thousand bytes
 * are the safe way to fill a label.
 *
 * SIZE / GAP / DENSITY depend on the media and are not emitted; send them
 * once at job start.
 */
#include "rcpt_internal.h"

static void band(rcpt_buf *out, const rcpt_image *img, int32_t x, int32_t y, uint32_t y0, uint32_t rows, uint8_t mode, int invert) {
    rcpt_w_str(out, "BITMAP ");
    rcpt_w_int(out, x);
    rcpt_w_byte(out, ',');
    rcpt_w_int(out, y + (int32_t)y0);
    rcpt_w_byte(out, ',');
    rcpt_w_uint(out, img->stride);
    rcpt_w_byte(out, ',');
    rcpt_w_uint(out, rows);
    rcpt_w_byte(out, ',');
    rcpt_w_uint(out, mode);
    rcpt_w_byte(out, ',');
    rcpt_w_bits(out, img->data + (size_t)y0 * img->stride, (size_t)rows * img->stride, invert);
    rcpt_w_str(out, "\r\n");
}

static void emit(rcpt_buf *out, const rcpt_image *img, int32_t x, int32_t y, uint8_t mode, uint32_t chunk_rows, uint32_t copies, uint32_t flags) {
    int raw        = (flags & RCPT_F_RAW) != 0;
    int invert     = RCPT_INVERT(flags, 1);
    uint32_t chunk = chunk_rows ? chunk_rows : img->height;
    uint32_t y0;

    if (!raw) rcpt_w_str(out, "CLS\r\n");

    for (y0 = 0; y0 < img->height; y0 += chunk) {
        uint32_t rows = img->height - y0;
        if (rows > chunk) rows = chunk;
        band(out, img, x, y, y0, rows, mode, invert);
    }

    if (!raw) {
        rcpt_w_str(out, "PRINT 1,");
        rcpt_w_uint(out, copies ? copies : 1u);
        rcpt_w_str(out, "\r\n");
    }
}

rcpt_result rcpt_enc_tspl(rcpt_buf *out, const rcpt_image *img, int32_t x, int32_t y, uint8_t mode, uint32_t chunk_rows, uint32_t copies,
                          uint32_t flags) {
    RCPT_ENCODE(img, x, y, mode, chunk_rows, copies, flags);
}
