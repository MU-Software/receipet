/*
 * Brother QL label printers, raster mode.
 *
 *   00 x200                          invalidate
 *   ESC @                            initialize
 *   ESC i a 1                        select raster mode
 *   ESC i z ...                      print information: media, size, line count
 *   ESC i M n                        auto cut
 *   ESC i A n                        cut every n labels
 *   ESC i K n                        cut at end
 *   ESC i d n1 n2                    feed margin, in dots
 *   M 0                              no compression
 *   g 00 5A <90 bytes>               one raster line, repeated
 *   SUB                              print and feed
 *
 * A raster line is the width of the print head, not of the media: 90 bytes,
 * 720 dots, on every QL from the 500 to the 820NWB.  The QL-1050 and QL-1060N
 * have a 162-byte head, which this encoder does not currently support.
 *
 * The line runs right to left: bit 7 of byte 0 is the *rightmost* dot.
 * `offset` is counted from the left anyway, as everywhere else here, so for
 * die-cut media
 *
 *     offset = 720 - image width - offset_r
 *
 * with offset_r the label's right margin (12 dots for 38x90, so offset 295).
 *
 * The image must be the label's *printable* size, not the media size: 38x90 mm
 * is 413x991 dots, while the media measures 449x1061.  Feeding the full media
 * length overruns the label and the cut then lands on the next one.  The grid
 * is 300 dpi both ways (measured: 100 lines = 8.5 mm, 40 dots = 3.4 mm).
 *
 * ESC i S (1B 69 53) asks for the 32-byte status block that
 * rcpt_parse_brother_status() decodes; it carries the media width and length
 * the printer has actually sensed.
 */
#include "rcpt_internal.h"

#define ESC 0x1B
#define SUB 0x1A

#define RASTER_BYTES 90 /* 720 dots: QL-500 through QL-820. The QL-1050/1060N head is wider */
#define RASTER_DOTS  (RASTER_BYTES * 8u)

static void emit(rcpt_buf *out, const rcpt_image *img, uint8_t media_type, uint8_t width_mm, uint8_t length_mm, uint32_t offset, uint32_t flags) {
    int raw    = (flags & RCPT_F_RAW) != 0;
    int invert = RCPT_INVERT(flags, 0);
    uint32_t n = img->height;
    uint32_t y;

    if (!raw) {
        uint8_t mode[4]   = { ESC, 'i', 'a', 1 };
        uint8_t every[4]  = { ESC, 'i', 'A', 1 };
        uint8_t end[4]    = { ESC, 'i', 'K', 0x08 };
        uint8_t margin[5] = { ESC, 'i', 'd', 0, 0 };
        uint8_t info[13];
        uint32_t i;

        for (i = 0; i < 200; i++)
            rcpt_w_byte(out, 0x00);
        rcpt_w_byte(out, ESC);
        rcpt_w_byte(out, '@');
        rcpt_w_bytes(out, mode, sizeof(mode));

        info[0]  = ESC;
        info[1]  = 'i';
        info[2]  = 'z';
        info[3]  = 0x02 | 0x04 | 0x08 | 0x40 | 0x80; /* which of the fields below count */
        info[4]  = media_type;
        info[5]  = width_mm;
        info[6]  = length_mm;
        info[7]  = (uint8_t)(n & 0xFFu);
        info[8]  = (uint8_t)((n >> 8) & 0xFFu);
        info[9]  = (uint8_t)((n >> 16) & 0xFFu);
        info[10] = (uint8_t)((n >> 24) & 0xFFu);
        info[11] = 0; /* first page */
        info[12] = 0;
        rcpt_w_bytes(out, info, sizeof(info));

        rcpt_w_byte(out, ESC);
        rcpt_w_byte(out, 'i');
        rcpt_w_byte(out, 'M');
        rcpt_w_byte(out, (flags & RCPT_F_BROTHER_NO_CUT) ? 0x00u : 0x40u);
        rcpt_w_bytes(out, every, sizeof(every));
        rcpt_w_bytes(out, end, sizeof(end));
        rcpt_w_bytes(out, margin, sizeof(margin));
        rcpt_w_byte(out, 'M');
        rcpt_w_byte(out, 0x00); /* no compression */
    }

    for (y = 0; y < img->height; y++) {
        uint8_t line[RASTER_BYTES];
        uint32_t x;

        for (x = 0; x < RASTER_BYTES; x++)
            line[x] = invert ? 0xFFu : 0x00u;

        for (x = 0; x < img->width; x++) {
            uint32_t d = offset + x;
            if (d >= RASTER_DOTS) break; /* past the head */
            d = RASTER_DOTS - 1u - d;    /* left-referenced dot -> bit, the line runs right to left */
            if (rcpt_image_get_pixel(img, (int32_t)x, (int32_t)y)) {
                if (invert)
                    line[d >> 3] &= (uint8_t)~(0x80u >> (d & 7u));
                else
                    line[d >> 3] |= (uint8_t)(0x80u >> (d & 7u));
            }
        }

        rcpt_w_byte(out, 'g');
        rcpt_w_byte(out, 0x00);
        rcpt_w_byte(out, RASTER_BYTES);
        rcpt_w_bytes(out, line, RASTER_BYTES);
    }

    if (!raw) rcpt_w_byte(out, SUB); /* print and feed */
}

rcpt_result rcpt_enc_brother(rcpt_buf *out, const rcpt_image *img, uint8_t media_type, uint8_t width_mm, uint8_t length_mm, uint32_t offset,
                             uint32_t flags) {
    rcpt_result checked = rcpt_enc_start(out, img);
    if (checked != RCPT_OK) return checked;
    if (offset > RASTER_DOTS || img->width > RASTER_DOTS - offset) return RCPT_ERR_ARG;

    RCPT_ENCODE(img, media_type, width_mm, length_mm, offset, flags);
}

rcpt_result rcpt_parse_brother_status(rcpt_brother_status *out, const uint8_t *data, size_t len) {
    if (!out || !data) return RCPT_ERR_ARG;
    if (len != RCPT_BROTHER_STATUS_LEN) return RCPT_ERR_ARG;

    out->model = data[4];
    /* Error information 1 is the low byte, 2 the high one, so the flag values
     * read in the order Brother documents them.  Verified on a QL-820NWB: an
     * open cover sets data[9] bit 4, which is RCPT_BROTHER_ERR_COVER_OPEN. */
    out->errors          = (uint16_t)(data[8] | ((uint16_t)data[9] << 8));
    out->media_width_mm  = data[10];
    out->media_type      = data[11];
    out->media_length_mm = data[17];
    out->status_type     = data[18];
    out->phase_type      = data[19];
    out->phase_number    = (uint16_t)((data[20] << 8) | data[21]);
    out->notification    = data[22];
    return RCPT_OK;
}

rcpt_result rcpt_parse_brother_status_u32(uint32_t *out, const uint8_t *data, size_t len) {
    rcpt_brother_status st;
    rcpt_result rc;

    if (!out) return RCPT_ERR_ARG;

    rc = rcpt_parse_brother_status(&st, data, len);
    if (rc != RCPT_OK) return rc;

    out[0] = st.model;
    out[1] = st.errors;
    out[2] = st.media_width_mm;
    out[3] = st.media_length_mm;
    out[4] = st.media_type;
    out[5] = st.status_type;
    out[6] = st.phase_type;
    out[7] = st.phase_number;
    out[8] = st.notification;
    return RCPT_OK;
}
