/*
 * receipet - a monochrome image buffer and printer-language encoders.
 *
 * C99, no dependencies beyond stdint/stddef/string/stdlib.  Two allocations
 * in the whole library: one per image, one per encoded job.
 *
 * Image layout: 1 bit per pixel, row-major, MSB first, 1 = black (ink).
 * Row n starts at data[n * stride], stride = (width + 7) / 8.  Padding bits
 * past `width` are always zero - pixel writes clip and the buffer starts
 * blank, so nothing can set them.
 */
#ifndef RECEIPET_H
#define RECEIPET_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef RCPT_API
#    if defined(__wasm__)
#        define RCPT_API __attribute__((used, visibility("default")))
#    else
#        define RCPT_API
#    endif
#endif

/* ------------------------------------------------------------------ */
/* constants                                                          */
/* ------------------------------------------------------------------ */
#define RCPT_VERSION_MAJOR 0
#define RCPT_VERSION_MINOR 2
#define RCPT_VERSION_PATCH 0

/* Keeps stride * height inside 32 bits. */
#define RCPT_MAX_DIM 65535u

/* RCPT_F_RAW emits the graphics command alone, without the job wrapper
 * (init / CLS / ^XA..^XZ / feed / cut / PRINT), for embedding in a larger
 * job.  RCPT_F_INVERT flips bit polarity: every format prints on 1 except
 * TSPL, whose BITMAP prints on 0. */
#define RCPT_F_RAW    (1u << 0)
#define RCPT_F_INVERT (1u << 1)
/* Brother only: leave the label uncut. */
#define RCPT_F_BROTHER_NO_CUT (1u << 2)

/* ------------------------------------------------------------------ */
/* types                                                              */
/* ------------------------------------------------------------------ */
/* Whatever can fail returns one of these and delivers its result through an
 * out-parameter; whatever cannot returns nothing, or the value asked for.
 * Out-of-range pixel coordinates are not a failure - they are clipped. */
typedef enum rcpt_result {
    RCPT_OK           = 0,
    RCPT_ERR_ARG      = -1,
    RCPT_ERR_NOMEM    = -2,
    RCPT_ERR_INTERNAL = -3 /* a bug in this library; please report it */
} rcpt_result;

/* Private except for `width` and `height`.  Writing `data` directly bypasses
 * the clipping and the padding-bit invariant the encoders rely on; use
 * rcpt_image_set_pixel(), however many calls that takes. */
typedef struct rcpt_image {
    uint32_t width;
    uint32_t height;
    uint32_t stride; /* bytes per row = (width + 7) / 8 */
    uint8_t *data;
} rcpt_image;

/* An encoded job.  `data` is NULL if the encode failed, otherwise `len`
 * bytes are yours to send.  `cap` equals `len` - it is exposed so a binding
 * can read min(len, cap) and be safe regardless.  Binary, not a string. */
typedef struct rcpt_buf {
    uint8_t *data;
    size_t cap;
    size_t len;
} rcpt_buf;

/* ------------------------------------------------------------------ */
/* image buffer                                                       */
/* ------------------------------------------------------------------ */
/* Header and pixels in one malloc(), starting blank.  *out is set either
 * way, NULL on failure.  Allocate once at startup and rcpt_image_clear()
 * between jobs; per-receipt allocation only fragments the heap. */
RCPT_API rcpt_result rcpt_image_new(rcpt_image **out, uint32_t width, uint32_t height);
RCPT_API void rcpt_image_free(rcpt_image *img);
RCPT_API void rcpt_image_clear(rcpt_image *img);

/* Out-of-bounds coordinates are clipped; get returns 0.
 *
 * Pixels are all the drawing there is.  Shapes, text and blitting belong in
 * a layer above - including logic ops, which on one pixel are a read and a
 * write:  set_pixel(img, x, y, !get_pixel(img, x, y)). */
RCPT_API void rcpt_image_set_pixel(rcpt_image *img, int32_t x, int32_t y, int value);
RCPT_API int rcpt_image_get_pixel(const rcpt_image *img, int32_t x, int32_t y);

/* Frees the block and zeroes the struct, so a double free is a no-op.  Safe
 * on a failed buffer and on NULL. */
RCPT_API void rcpt_buf_free(rcpt_buf *buf);

/* ------------------------------------------------------------------ */
/* encoders                                                           */
/* ------------------------------------------------------------------ */
/* One per printer language, taking exactly that language's knobs.  Each
 * returns RCPT_OK, RCPT_ERR_ARG for an unusable image, or RCPT_ERR_NOMEM;
 * *out is always written, zeroed on failure. */

/* Thermal receipt printers.  chunk_rows splits the raster into bands of N
 * rows, 0 for one block - cheap printers drop blocks bigger than their
 * image buffer. */
RCPT_API rcpt_result rcpt_enc_escpos(rcpt_buf *out, const rcpt_image *img, uint32_t chunk_rows, uint32_t flags);

/* Dot matrix printers.  mode is the `m` of ESC * m: 0/1/2/3 for an 8-pin
 * head at 60/120/120/240 dpi, 32/33/38/39/40 for 24-pin at 60/120/90/180/360. */
RCPT_API rcpt_result rcpt_enc_escp(rcpt_buf *out, const rcpt_image *img, uint8_t mode, uint32_t flags);

/* TSC label printers.  x, y are in dots; mode is the BITMAP combine mode,
 * 0 OVERWRITE / 1 OR / 2 XOR; copies of 0 means one label.  chunk_rows is as
 * for ESC/POS - firmware that silently drops an oversized BITMAP is common,
 * and a banded image still prints as one label. */
RCPT_API rcpt_result rcpt_enc_tspl(rcpt_buf *out, const rcpt_image *img, int32_t x, int32_t y, uint8_t mode, uint32_t chunk_rows, uint32_t copies,
                                   uint32_t flags);

/* Zebra/Comtec mobile printers.  dpi goes into the `!` header; 0 means 203. */
RCPT_API rcpt_result rcpt_enc_cpcl(rcpt_buf *out, const rcpt_image *img, int32_t x, int32_t y, uint32_t dpi, uint32_t copies, uint32_t flags);

/* Zebra label printers. */
RCPT_API rcpt_result rcpt_enc_zpl(rcpt_buf *out, const rcpt_image *img, int32_t x, int32_t y, uint32_t copies, uint32_t flags);

/* Brother QL label printers, raster mode - the only one that gets the scale
 * right; their ESC/P mode is not worth the trouble.
 *
 * media_type is RCPT_BROTHER_MEDIA_*, width_mm and length_mm the media (0 for
 * continuous tape) - all from rcpt_parse_brother_status() - and `offset` where
 * the image starts across the 720-dot head, counted from the left.  The raster
 * line itself runs right to left, so for die-cut media
 * offset = 720 - image width - the label's right margin.  The grid is 300 dpi,
 * so a millimetre is 11.81 dots, and the image must be the label's printable
 * size, not the media size. */
RCPT_API rcpt_result rcpt_enc_brother(rcpt_buf *out, const rcpt_image *img, uint8_t media_type, uint8_t width_mm, uint8_t length_mm, uint32_t offset,
                                      uint32_t flags);

/* ------------------------------------------------------------------ */
/* Brother QL status                                                  */
/* ------------------------------------------------------------------ */
/* Send RCPT_BROTHER_STATUS_REQUEST and read back exactly
 * RCPT_BROTHER_STATUS_LEN bytes - the reply carries no framing, so the length
 * is how you know it is complete.  This is the one printer here that reports
 * the media it has actually sensed. */
#define RCPT_BROTHER_STATUS_REQUEST "\x1B\x69\x53" /* ESC i S */
#define RCPT_BROTHER_STATUS_LEN     32

/* media_type */
#define RCPT_BROTHER_MEDIA_NONE       0x00
#define RCPT_BROTHER_MEDIA_CONTINUOUS 0x0A
#define RCPT_BROTHER_MEDIA_DIE_CUT    0x0B

/* errors: information 1 in the low byte, 2 in the high one */
#define RCPT_BROTHER_ERR_NO_MEDIA      (1u << 0)
#define RCPT_BROTHER_ERR_END_OF_MEDIA  (1u << 1)
#define RCPT_BROTHER_ERR_CUTTER_JAM    (1u << 2)
#define RCPT_BROTHER_ERR_IN_USE        (1u << 4)
#define RCPT_BROTHER_ERR_POWERED_OFF   (1u << 5)
#define RCPT_BROTHER_ERR_REPLACE_MEDIA (1u << 8)
#define RCPT_BROTHER_ERR_BUFFER_FULL   (1u << 9)
#define RCPT_BROTHER_ERR_COMMS         (1u << 10)
#define RCPT_BROTHER_ERR_COVER_OPEN    (1u << 12)
#define RCPT_BROTHER_ERR_CANNOT_FEED   (1u << 14)
#define RCPT_BROTHER_ERR_SYSTEM        (1u << 15)

typedef struct rcpt_brother_status {
    uint8_t model;           /* raw model code; 0x37 is a QL-720NW */
    uint16_t errors;         /* RCPT_BROTHER_ERR_* */
    uint8_t media_width_mm;  /* what the printer sensed */
    uint8_t media_length_mm; /* 0 on continuous tape */
    uint8_t media_type;      /* RCPT_BROTHER_MEDIA_* */
    uint8_t status_type;     /* 0 reply, 1 printed, 2 error, 5 notify, 6 phase */
    uint8_t phase_type;      /* 0 receiving, 1 printing */
    uint16_t phase_number;
    uint8_t notification; /* 3 cooling started, 4 cooling finished */
} rcpt_brother_status;

/* RCPT_ERR_ARG unless `len` is exactly RCPT_BROTHER_STATUS_LEN. */
RCPT_API rcpt_result rcpt_parse_brother_status(rcpt_brother_status *out, const uint8_t *data, size_t len);

/* The same parse as RCPT_BROTHER_STATUS_FIELDS uint32 values, for a binding
 * that cannot see the struct.  `out` takes them in declaration order:
 *
 *   model, errors, media_width_mm, media_length_mm, media_type,
 *   status_type, phase_type, phase_number, notification */
#define RCPT_BROTHER_STATUS_FIELDS 9
RCPT_API rcpt_result rcpt_parse_brother_status_u32(uint32_t *out, const uint8_t *data, size_t len);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* RECEIPET_H */
