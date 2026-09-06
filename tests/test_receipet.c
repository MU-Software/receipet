#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "receipet.h"

static int failures = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                            \
        }                                                          \
    } while (0)

/* The shape an application needs to pick a language at runtime; the thunks
 * that fill the table live next to the table itself. */
typedef rcpt_result (*encode_fn)(rcpt_buf *out, const rcpt_image *img);

static void test_alloc(void) {
    rcpt_image *img = (rcpt_image *)1; /* must be overwritten even on failure */

    CHECK(rcpt_image_new(&img, 0, 4) == RCPT_ERR_ARG);
    CHECK(img == NULL);
    CHECK(rcpt_image_new(&img, 4, 0) == RCPT_ERR_ARG);
    CHECK(rcpt_image_new(&img, 70000, 1) == RCPT_ERR_ARG);
    CHECK(rcpt_image_new(NULL, 8, 8) == RCPT_ERR_ARG);

    CHECK(rcpt_image_new(&img, 9, 4) == RCPT_OK);
    CHECK(img != NULL);
    if (img) {
        CHECK(img->width == 9 && img->height == 4);
        CHECK(img->stride == 2);     /* rounds up to whole bytes */
        CHECK(img->data[0] == 0x00); /* starts blank */
        rcpt_image_free(img);
    }

    /* the reuse pattern: draw, blank, draw again, never reallocating */
    CHECK(rcpt_image_new(&img, 200, 100) == RCPT_OK);
    CHECK(img != NULL);
    if (img) {
        CHECK(img->stride == 25);
        rcpt_image_set_pixel(img, 199, 99, 1);
        CHECK(rcpt_image_get_pixel(img, 199, 99) == 1);
        rcpt_image_clear(img);
        CHECK(rcpt_image_get_pixel(img, 199, 99) == 0);
        rcpt_image_set_pixel(img, 0, 0, 1);
        CHECK(rcpt_image_get_pixel(img, 0, 0) == 1);
        rcpt_image_free(img);
    }
}

static void test_pixels(void) {
    rcpt_image *img;
    int32_t x, y;

    if (rcpt_image_new(&img, 12, 4) != RCPT_OK) {
        CHECK(0);
        return;
    }

    rcpt_image_set_pixel(img, 0, 0, 1);
    rcpt_image_set_pixel(img, 11, 3, 1);
    CHECK(img->data[0] == 0x80);
    CHECK(img->data[7] == 0x10); /* bit 11 of the last row */
    CHECK(rcpt_image_get_pixel(img, 0, 0) == 1);
    CHECK(rcpt_image_get_pixel(img, 11, 3) == 1);
    CHECK(rcpt_image_get_pixel(img, 1, 0) == 0);

    rcpt_image_set_pixel(img, 0, 0, 0);
    CHECK(rcpt_image_get_pixel(img, 0, 0) == 0);

    /* out of range writes are dropped, not wrapped */
    rcpt_image_set_pixel(img, -1, 0, 1);
    rcpt_image_set_pixel(img, 12, 0, 1);
    rcpt_image_set_pixel(img, 0, 4, 1);
    CHECK(rcpt_image_get_pixel(img, -1, 0) == 0);
    CHECK(img->data[0] == 0x00);

    /* every pixel of every row set: the padding bits past width stay zero */
    for (y = 0; y < 4; y++)
        for (x = 0; x < 12; x++)
            rcpt_image_set_pixel(img, x, y, 1);
    for (y = 0; y < 4; y++) {
        CHECK(img->data[y * 2] == 0xFF);
        CHECK(img->data[y * 2 + 1] == 0xF0);
    }

    rcpt_image_clear(img);
    CHECK(img->data[0] == 0x00 && img->data[7] == 0x00);

    /* the caller's version of a logic op: read, invert, write back */
    rcpt_image_set_pixel(img, 2, 2, !rcpt_image_get_pixel(img, 2, 2));
    CHECK(rcpt_image_get_pixel(img, 2, 2) == 1);
    rcpt_image_set_pixel(img, 2, 2, !rcpt_image_get_pixel(img, 2, 2));
    CHECK(rcpt_image_get_pixel(img, 2, 2) == 0);

    rcpt_image_free(img);
}

/* A 16x2 image: row 0 = 0xFF 0xAA, row 1 blank.  NULL only if malloc failed. */
static rcpt_image *make_sample(void) {
    rcpt_image *img;
    int32_t x;

    if (rcpt_image_new(&img, 16, 2) != RCPT_OK) return NULL;
    for (x = 0; x < 8; x++)
        rcpt_image_set_pixel(img, x, 0, 1); /* 0xFF */
    for (x = 8; x < 16; x += 2)
        rcpt_image_set_pixel(img, x, 0, 1); /* 0xAA */
    return img;
}

/* A non-byte-aligned image exposes accidental inversion of row padding. */
static rcpt_image *make_unaligned_sample(void) {
    rcpt_image *img;

    if (rcpt_image_new(&img, 12, 2) != RCPT_OK) return NULL;
    rcpt_image_set_pixel(img, 0, 0, 1);
    return img;
}

/* Copy an encoded job into a caller-owned scratch buffer so the assertions
 * below can stay simple; every call still goes through the real API. */
static size_t take(rcpt_buf *job, uint8_t *out, size_t cap) {
    size_t len = job->len;

    if (!job->data) {
        CHECK(0);
        return 0;
    }
    memcpy(out, job->data, len < cap ? len : cap);
    rcpt_buf_free(job);
    return len;
}

static void test_escpos(void) {
    uint8_t out[64];
    rcpt_buf job;
    rcpt_image *img;
    size_t n;
    static const uint8_t expect[] = { 0x1D, 'v',  '0',  0x00, 0x02, 0x00, 0x02, 0x00, /* GS v 0, 2 bytes wide, 2 rows */
                                      0xFF, 0xAA, 0x00, 0x00 };

    img = make_sample();
    if (!img) {
        CHECK(0);
        return;
    }

    CHECK(rcpt_enc_escpos(&job, img, 0, RCPT_F_RAW) == RCPT_OK);
    n = take(&job, out, sizeof(out));
    CHECK(n == sizeof(expect));
    CHECK(memcmp(out, expect, sizeof(expect)) == 0);

    /* chunking splits into two commands but keeps the pixels identical */
    CHECK(rcpt_enc_escpos(&job, img, 1, RCPT_F_RAW) == RCPT_OK);
    n = take(&job, out, sizeof(out));
    CHECK(n == 8 + 2 + 8 + 2);
    CHECK(out[6] == 1 && out[16] == 1);
    CHECK(out[8] == 0xFF && out[9] == 0xAA);
    CHECK(out[18] == 0x00 && out[19] == 0x00);

    /* the wrapper adds ESC @ up front and feed + cut at the end */
    CHECK(rcpt_enc_escpos(&job, img, 0, 0) == RCPT_OK);
    n = take(&job, out, sizeof(out));
    CHECK(n == sizeof(expect) + 2 + 3 + 4);
    CHECK(out[0] == 0x1B && out[1] == '@');
    CHECK(out[n - 4] == 0x1D && out[n - 3] == 'V' && out[n - 2] == 66);

    rcpt_image_free(img);

    img = make_unaligned_sample();
    if (!img) {
        CHECK(0);
        return;
    }
    CHECK(rcpt_enc_escpos(&job, img, 0, RCPT_F_RAW | RCPT_F_INVERT) == RCPT_OK);
    n = take(&job, out, sizeof(out));
    CHECK(n == 12);
    CHECK(out[8] == 0x7F && out[9] == 0xF0);
    CHECK(out[10] == 0xFF && out[11] == 0xF0);
    rcpt_image_free(img);
}

static void test_escp(void) {
    uint8_t out[128];
    rcpt_buf job;
    rcpt_image *img;
    size_t n;

    img = make_sample();
    if (!img) {
        CHECK(0);
        return;
    }

    CHECK(rcpt_enc_escp(&job, img, 0, RCPT_F_RAW) == RCPT_OK);
    n = take(&job, out, sizeof(out));
    /* one 8-row band: ESC * m nL nH + 16 column bytes + CRLF */
    CHECK(n == 5 + 16 + 2);
    CHECK(out[0] == 0x1B && out[1] == '*' && out[2] == 0);
    CHECK(out[3] == 16 && out[4] == 0);
    /* column 0: row 0 set -> top pin only */
    CHECK(out[5] == 0x80);
    /* column 8: 0xAA bit 7 is set -> top pin; column 9 is clear */
    CHECK(out[5 + 8] == 0x80);
    CHECK(out[5 + 9] == 0x00);
    CHECK(out[n - 2] == '\r' && out[n - 1] == '\n');

    /* 24-pin mode packs three bytes per column and still covers 2 rows */
    CHECK(rcpt_enc_escp(&job, img, 39, RCPT_F_RAW) == RCPT_OK);
    n = take(&job, out, sizeof(out));
    CHECK(n == 5 + 16 * 3 + 2);
    CHECK(out[2] == 39);
    CHECK(out[5] == 0x80 && out[6] == 0x00 && out[7] == 0x00);

    rcpt_image_free(img);

    CHECK(rcpt_image_new(&img, 2, 3) == RCPT_OK);
    if (!img) return;
    rcpt_image_set_pixel(img, 0, 0, 1);
    CHECK(rcpt_enc_escp(&job, img, 0, RCPT_F_RAW | RCPT_F_INVERT) == RCPT_OK);
    n = take(&job, out, sizeof(out));
    CHECK(n == 9);
    CHECK(out[5] == 0x60 && out[6] == 0xE0); /* only the three image rows invert */

    job.data = (uint8_t *)1;
    job.cap = job.len = 12345;
    CHECK(rcpt_enc_escp(&job, img, 200, RCPT_F_RAW) == RCPT_ERR_ARG);
    CHECK(job.data == NULL && job.cap == 0 && job.len == 0);
    rcpt_image_free(img);
}

static void test_tspl(void) {
    uint8_t out[128];
    rcpt_buf job;
    rcpt_image *img;
    size_t n;
    static const char prefix[] = "BITMAP 0,0,2,2,0,";

    img = make_sample();
    if (!img) {
        CHECK(0);
        return;
    }

    CHECK(rcpt_enc_tspl(&job, img, 0, 0, 0, 0, 1, RCPT_F_RAW) == RCPT_OK);
    n = take(&job, out, sizeof(out));
    CHECK(n == sizeof(prefix) - 1 + 4 + 2);
    CHECK(memcmp(out, prefix, sizeof(prefix) - 1) == 0);
    /* TSPL prints on 0, so the payload is inverted */
    CHECK(out[sizeof(prefix) - 1] == 0x00);
    CHECK(out[sizeof(prefix)] == 0x55);
    CHECK(out[sizeof(prefix) + 1] == 0xFF);

    /* RCPT_F_INVERT undoes the format default */
    CHECK(rcpt_enc_tspl(&job, img, 0, 0, 0, 0, 1, RCPT_F_RAW | RCPT_F_INVERT) == RCPT_OK);
    take(&job, out, sizeof(out));
    CHECK(out[sizeof(prefix) - 1] == 0xFF);

    /* chunking emits one BITMAP per band, each carrying its own y */
    CHECK(rcpt_enc_tspl(&job, img, 0, 5, 0, 1, 1, RCPT_F_RAW) == RCPT_OK);
    n = take(&job, out, sizeof(out));
    CHECK(n == 2 * (sizeof("BITMAP 0,5,2,1,0,") - 1 + 2 + 2));
    CHECK(memcmp(out, "BITMAP 0,5,2,1,0,", 17) == 0);
    CHECK(out[17] == 0x00 && out[18] == 0x55); /* row 0, inverted */
    CHECK(memcmp(out + 21, "BITMAP 0,6,2,1,0,", 17) == 0);
    CHECK(out[38] == 0xFF && out[39] == 0xFF); /* row 1 is blank -> all ink-off */

    /* Every band's y coordinate must remain representable as int32_t. */
    CHECK(rcpt_enc_tspl(&job, img, 0, INT32_MAX - 1, 0, 1, 1, RCPT_F_RAW) == RCPT_OK);
    rcpt_buf_free(&job);

    job.data = (uint8_t *)1;
    job.cap = job.len = 12345;
    CHECK(rcpt_enc_tspl(&job, img, 0, INT32_MAX, 0, 1, 1, RCPT_F_RAW) == RCPT_ERR_ARG);
    CHECK(job.data == NULL && job.cap == 0 && job.len == 0);

    rcpt_image_free(img);

    img = make_unaligned_sample();
    if (!img) {
        CHECK(0);
        return;
    }
    CHECK(rcpt_enc_tspl(&job, img, 0, 0, 0, 0, 1, RCPT_F_RAW | RCPT_F_INVERT) == RCPT_OK);
    n = take(&job, out, sizeof(out));
    CHECK(n == sizeof(prefix) - 1 + 4 + 2);
    CHECK(out[sizeof(prefix) - 1] == 0x80 && out[sizeof(prefix)] == 0x0F);
    CHECK(out[sizeof(prefix) + 1] == 0x00 && out[sizeof(prefix) + 2] == 0x0F);

    job.data = (uint8_t *)1;
    job.cap = job.len = 12345;
    CHECK(rcpt_enc_tspl(&job, img, 0, 0, 99, 0, 1, RCPT_F_RAW) == RCPT_ERR_ARG);
    CHECK(job.data == NULL && job.cap == 0 && job.len == 0);
    rcpt_image_free(img);
}

static void test_brother(void) {
    uint8_t out[1024];
    rcpt_buf job;
    rcpt_image *img;
    size_t n;
    int i;

    img = make_sample(); /* 16x2 */
    if (!img) {
        CHECK(0);
        return;
    }

    /* raw is just the raster lines: g 00 5A then 90 bytes, once per row */
    CHECK(rcpt_enc_brother(&job, img, RCPT_BROTHER_MEDIA_DIE_CUT, 38, 90, 0, RCPT_F_RAW) == RCPT_OK);
    n = take(&job, out, sizeof(out));
    CHECK(n == 2 * (3 + 90));
    CHECK(out[0] == 'g' && out[1] == 0x00 && out[2] == 90);
    /* The line runs right to left, so offset 0 puts x=0 in the last bit of the
     * last byte and 0xFF 0xAA arrives bit-reversed as 0x55 0xFF. */
    CHECK(out[3 + 89] == 0xFF && out[3 + 88] == 0x55);
    CHECK(out[3 + 87] == 0x00); /* the rest of the line is blank */
    CHECK(out[3 + 90] == 'g');  /* second line follows */

    CHECK(rcpt_enc_brother(&job, img, RCPT_BROTHER_MEDIA_DIE_CUT, 38, 90, 0, RCPT_F_RAW | RCPT_F_INVERT) == RCPT_OK);
    n = take(&job, out, sizeof(out));
    CHECK(out[3 + 89] == 0x00 && out[3 + 88] == 0xAA);
    for (i = 0; i < 88; i++)
        CHECK(out[3 + i] == 0x00); /* inversion cannot ink outside the image */

    /* offset shifts the image across the head, a whole byte at a time here */
    CHECK(rcpt_enc_brother(&job, img, RCPT_BROTHER_MEDIA_DIE_CUT, 38, 90, 8, RCPT_F_RAW) == RCPT_OK);
    n = take(&job, out, sizeof(out));
    CHECK(out[3 + 89] == 0x00 && out[3 + 88] == 0xFF && out[3 + 87] == 0x55);

    /* the wrapper carries the media in the print information command */
    CHECK(rcpt_enc_brother(&job, img, RCPT_BROTHER_MEDIA_DIE_CUT, 38, 90, 0, 0) == RCPT_OK);
    n = take(&job, out, sizeof(out));
    for (i = 0; i < 200; i++)
        CHECK(out[i] == 0x00);
    CHECK(out[200] == 0x1B && out[201] == '@');
    CHECK(out[202] == 0x1B && out[203] == 'i' && out[204] == 'a' && out[205] == 1);
    CHECK(out[206] == 0x1B && out[207] == 'i' && out[208] == 'z');
    CHECK(out[210] == RCPT_BROTHER_MEDIA_DIE_CUT);
    CHECK(out[211] == 38 && out[212] == 90);
    CHECK(out[213] == 2 && out[214] == 0 && out[215] == 0 && out[216] == 0); /* two lines */
    CHECK(out[222] == 0x40);                                                 /* auto cut */
    CHECK(out[230] == 0x08);                                                 /* cut at end */
    CHECK(out[234] == 0 && out[235] == 0);                                   /* die-cut feed margin */
    CHECK(out[n - 1] == 0x1A);                                               /* print and feed */

    CHECK(rcpt_enc_brother(&job, img, RCPT_BROTHER_MEDIA_CONTINUOUS, 62, 0, 0, RCPT_F_BROTHER_NO_CUT) == RCPT_OK);
    n = take(&job, out, sizeof(out));
    CHECK(out[222] == 0x00);                /* auto cut disabled */
    CHECK(out[230] == 0x00);                /* cut at end disabled too */
    CHECK(out[234] == 35 && out[235] == 0); /* continuous-tape minimum */
    CHECK(out[n - 1] == 0x1A);

    rcpt_image_free(img);

    /* The fixed-width head must not silently discard pixels. */
    CHECK(rcpt_image_new(&img, 720, 1) == RCPT_OK);
    if (img) {
        CHECK(rcpt_enc_brother(&job, img, RCPT_BROTHER_MEDIA_DIE_CUT, 62, 29, 0, RCPT_F_RAW) == RCPT_OK);
        rcpt_buf_free(&job);
        rcpt_image_free(img);
    }

    CHECK(rcpt_image_new(&img, 721, 1) == RCPT_OK);
    if (img) {
        job.data = (uint8_t *)1;
        job.cap = job.len = 12345;
        CHECK(rcpt_enc_brother(&job, img, RCPT_BROTHER_MEDIA_DIE_CUT, 62, 29, 0, RCPT_F_RAW) == RCPT_ERR_ARG);
        CHECK(job.data == NULL && job.cap == 0 && job.len == 0);
        rcpt_image_free(img);
    }

    CHECK(rcpt_image_new(&img, 1, 1) == RCPT_OK);
    if (img) {
        CHECK(rcpt_enc_brother(&job, img, RCPT_BROTHER_MEDIA_DIE_CUT, 62, 29, 720, RCPT_F_RAW) == RCPT_ERR_ARG);
        CHECK(job.data == NULL && job.cap == 0 && job.len == 0);
        rcpt_image_free(img);
    }
}

/* A 32-byte reply as a QL-820NWB sends it. */
static void test_brother_status(void) {
    uint8_t reply[RCPT_BROTHER_STATUS_LEN] = { 0 };
    rcpt_brother_status st;

    reply[0]  = 0x80;
    reply[1]  = 0x20;
    reply[2]  = 0x42;
    reply[3]  = 0x34;
    reply[4]  = 0x41; /* QL-820NWB */
    reply[8]  = 0x01; /* error information 1: no media */
    reply[9]  = 0x10; /* error information 2: cover open */
    reply[10] = 62;   /* media width, mm */
    reply[11] = RCPT_BROTHER_MEDIA_CONTINUOUS;
    reply[17] = 0;    /* continuous tape has no length */
    reply[18] = 0x00; /* reply to status request */
    reply[19] = 0x01; /* printing */
    reply[20] = 0x00;
    reply[21] = 0x0A;
    reply[22] = 0x03; /* cooling started */

    CHECK(rcpt_parse_brother_status(&st, reply, sizeof(reply)) == RCPT_OK);
    CHECK(st.model == 0x41);
    CHECK(st.media_width_mm == 62);
    CHECK(st.media_length_mm == 0);
    CHECK(st.media_type == RCPT_BROTHER_MEDIA_CONTINUOUS);
    CHECK(st.status_type == 0x00);
    CHECK(st.phase_type == 0x01);
    CHECK(st.phase_number == 10);
    CHECK(st.notification == 0x03);

    /* the two error bytes must not be swapped */
    CHECK((st.errors & RCPT_BROTHER_ERR_NO_MEDIA) != 0);
    CHECK((st.errors & RCPT_BROTHER_ERR_COVER_OPEN) != 0);
    CHECK((st.errors & RCPT_BROTHER_ERR_REPLACE_MEDIA) == 0);

    reply[8] = 0xC8; /* all three previously unnamed error-information-1 bits */
    reply[9] = 0x28; /* both previously unnamed error-information-2 bits */
    CHECK(rcpt_parse_brother_status(&st, reply, sizeof(reply)) == RCPT_OK);
    CHECK(st.errors == (RCPT_BROTHER_ERR_WEAK_BATTERIES | RCPT_BROTHER_ERR_HIGH_VOLTAGE_ADAPTER | RCPT_BROTHER_ERR_FAN_MOTOR |
                        RCPT_BROTHER_ERR_COMM_BUFFER_FULL | RCPT_BROTHER_ERR_CANCEL_KEY));

    reply[8] = 0x01;
    reply[9] = 0x10;
    CHECK(rcpt_parse_brother_status(&st, reply, sizeof(reply)) == RCPT_OK);

    /* Field for field against the struct: a swapped pair shows up here rather
     * than at a caller. */
    {
        uint32_t vals[RCPT_BROTHER_STATUS_FIELDS];
        uint32_t i;

        CHECK(rcpt_parse_brother_status_u32(vals, reply, sizeof(reply)) == RCPT_OK);
        CHECK(vals[0] == st.model);
        CHECK(vals[1] == st.errors);
        CHECK(vals[2] == st.media_width_mm);
        CHECK(vals[3] == st.media_length_mm);
        CHECK(vals[4] == st.media_type);
        CHECK(vals[5] == st.status_type);
        CHECK(vals[6] == st.phase_type);
        CHECK(vals[7] == st.phase_number);
        CHECK(vals[8] == st.notification);

        /* uint32 out: nothing truncated or sign extended */
        CHECK(vals[1] == (uint32_t)(RCPT_BROTHER_ERR_NO_MEDIA | RCPT_BROTHER_ERR_COVER_OPEN));

        for (i = 0; i < RCPT_BROTHER_STATUS_FIELDS; i++)
            vals[i] = UINT32_MAX;
        CHECK(rcpt_parse_brother_status_u32(vals, reply, sizeof(reply) - 1) == RCPT_ERR_ARG);
        for (i = 0; i < RCPT_BROTHER_STATUS_FIELDS; i++)
            CHECK(vals[i] == 0);

        vals[0] = UINT32_MAX;
        CHECK(rcpt_parse_brother_status_u32(vals, NULL, sizeof(reply)) == RCPT_ERR_ARG);
        CHECK(vals[0] == 0);
        CHECK(rcpt_parse_brother_status_u32(NULL, reply, sizeof(reply)) == RCPT_ERR_ARG);
    }

    /* A failed parse leaves no stale status behind. */
    memset(&st, 0xFF, sizeof(st));
    CHECK(rcpt_parse_brother_status(&st, reply, sizeof(reply) - 1) == RCPT_ERR_ARG);
    CHECK(st.model == 0 && st.errors == 0 && st.media_width_mm == 0 && st.media_length_mm == 0 && st.media_type == 0);
    CHECK(st.status_type == 0 && st.phase_type == 0 && st.phase_number == 0 && st.notification == 0);

    st.model = 1;
    CHECK(rcpt_parse_brother_status(&st, NULL, sizeof(reply)) == RCPT_ERR_ARG);
    CHECK(st.model == 0);
    CHECK(rcpt_parse_brother_status(NULL, reply, sizeof(reply)) == RCPT_ERR_ARG);
}

static void test_cpcl_zpl(void) {
    char out[256];
    rcpt_buf job;
    rcpt_image *img;
    size_t n;

    img = make_sample();
    if (!img) {
        CHECK(0);
        return;
    }

    CHECK(rcpt_enc_cpcl(&job, img, 0, 0, 0, 1, RCPT_F_RAW) == RCPT_OK);
    n      = take(&job, (uint8_t *)out, sizeof(out) - 1);
    out[n] = '\0';
    CHECK(strcmp(out, "EG 2 2 0 0 FFAA0000\r\n") == 0);

    /* Label height is computed without overflowing an int32_t y coordinate. */
    CHECK(rcpt_enc_cpcl(&job, img, 0, INT32_MAX, 203, 1, 0) == RCPT_OK);
    n      = take(&job, (uint8_t *)out, sizeof(out) - 1);
    out[n] = '\0';
    CHECK(strcmp(out, "! 0 203 203 2147483649 1\r\nEG 2 2 0 2147483647 FFAA0000\r\nFORM\r\nPRINT\r\n") == 0);

    CHECK(rcpt_enc_zpl(&job, img, 30, 40, 1, RCPT_F_RAW) == RCPT_OK);
    n      = take(&job, (uint8_t *)out, sizeof(out) - 1);
    out[n] = '\0';
    CHECK(strcmp(out, "^FO30,40^GFA,4,4,2,FFAA0000^FS\r\n") == 0);

    CHECK(rcpt_enc_zpl(&job, img, 30, 40, 3, 0) == RCPT_OK);
    n      = take(&job, (uint8_t *)out, sizeof(out) - 1);
    out[n] = '\0';
    CHECK(strcmp(out, "^XA\r\n^FO30,40^GFA,4,4,2,FFAA0000^FS\r\n^PQ3\r\n^XZ\r\n") == 0);

    rcpt_image_free(img);

    img = make_unaligned_sample();
    if (!img) {
        CHECK(0);
        return;
    }

    CHECK(rcpt_enc_cpcl(&job, img, 0, 0, 0, 1, RCPT_F_RAW | RCPT_F_INVERT) == RCPT_OK);
    n      = take(&job, (uint8_t *)out, sizeof(out) - 1);
    out[n] = '\0';
    CHECK(strcmp(out, "EG 2 2 0 0 7FF0FFF0\r\n") == 0);

    CHECK(rcpt_enc_zpl(&job, img, 0, 0, 1, RCPT_F_RAW | RCPT_F_INVERT) == RCPT_OK);
    n      = take(&job, (uint8_t *)out, sizeof(out) - 1);
    out[n] = '\0';
    CHECK(strcmp(out, "^FO0,0^GFA,4,4,2,7FF0FFF0^FS\r\n") == 0);

    job.data = (uint8_t *)1;
    job.cap = job.len = 12345;
    CHECK(rcpt_enc_cpcl(&job, img, -1, 0, 0, 1, RCPT_F_RAW) == RCPT_ERR_ARG);
    CHECK(job.data == NULL && job.cap == 0 && job.len == 0);

    job.data = (uint8_t *)1;
    job.cap = job.len = 12345;
    CHECK(rcpt_enc_zpl(&job, img, 0, -1, 1, RCPT_F_RAW) == RCPT_ERR_ARG);
    CHECK(job.data == NULL && job.cap == 0 && job.len == 0);

    CHECK(rcpt_enc_zpl(&job, img, 32000, 32000, 1, RCPT_F_RAW) == RCPT_OK);
    rcpt_buf_free(&job);
    CHECK(rcpt_enc_zpl(&job, img, 32001, 0, 1, RCPT_F_RAW) == RCPT_ERR_ARG);
    CHECK(job.data == NULL && job.cap == 0 && job.len == 0);

    rcpt_image_free(img);
}

/* Same table an application would write to pick a language at runtime. */
static rcpt_result as_escpos(rcpt_buf *o, const rcpt_image *i) {
    return rcpt_enc_escpos(o, i, 0, 0);
}
static rcpt_result as_escp(rcpt_buf *o, const rcpt_image *i) {
    return rcpt_enc_escp(o, i, 0, 0);
}
static rcpt_result as_tspl(rcpt_buf *o, const rcpt_image *i) {
    return rcpt_enc_tspl(o, i, 0, 0, 0, 0, 1, 0);
}
static rcpt_result as_cpcl(rcpt_buf *o, const rcpt_image *i) {
    return rcpt_enc_cpcl(o, i, 0, 0, 0, 1, 0);
}
static rcpt_result as_zpl(rcpt_buf *o, const rcpt_image *i) {
    return rcpt_enc_zpl(o, i, 0, 0, 1, 0);
}
static rcpt_result as_brother(rcpt_buf *o, const rcpt_image *i) {
    return rcpt_enc_brother(o, i, RCPT_BROTHER_MEDIA_DIE_CUT, 38, 90, 0, 0);
}

static const encode_fn ENCODERS[] = { as_escpos, as_escp, as_tspl, as_cpcl, as_zpl, as_brother };
#define ENCODER_COUNT ((int)(sizeof ENCODERS / sizeof ENCODERS[0]))

/* The measuring pass and the filling pass must agree exactly, for every
 * language: a mismatch makes the job come back NULL, and encoding the same
 * image twice must produce identical bytes. */
static void test_encode_all(void) {
    rcpt_image *img = make_sample();
    int f;

    if (!img) {
        CHECK(0);
        return;
    }

    for (f = 0; f < ENCODER_COUNT; f++) {
        rcpt_buf a, b;

        CHECK(ENCODERS[f](&a, img) == RCPT_OK);
        CHECK(ENCODERS[f](&b, img) == RCPT_OK);

        CHECK(a.data != NULL);
        CHECK(b.data != NULL);
        CHECK(a.len > 0);
        CHECK(a.len == a.cap); /* exactly sized */
        CHECK(a.len == b.len);
        if (a.data && b.data && a.len == b.len) CHECK(memcmp(a.data, b.data, a.len) == 0);

        rcpt_buf_free(&a);
        CHECK(a.data == NULL && a.len == 0); /* freeing zeroes the buffer */
        rcpt_buf_free(&a);                   /* so a second free is safe */
        rcpt_buf_free(&b);
    }

    rcpt_image_free(img);
}

static void test_errors(void) {
    rcpt_buf job;
    int f;

    /* every encoder must reject a NULL image the same way, and must still
     * leave behind a buffer that is safe to read and to free */
    for (f = 0; f < ENCODER_COUNT; f++) {
        job.data = (uint8_t *)1;
        job.cap = job.len = 12345;

        CHECK(ENCODERS[f](&job, NULL) == RCPT_ERR_ARG);
        CHECK(job.data == NULL);
        CHECK(job.cap == 0 && job.len == 0);
        rcpt_buf_free(&job);

        CHECK(ENCODERS[f](NULL, NULL) == RCPT_ERR_ARG); /* no out at all */
    }

    rcpt_buf_free(NULL); /* and on NULL itself */
}

int main(void) {
    test_alloc();
    test_pixels();
    test_escpos();
    test_escp();
    test_tspl();
    test_cpcl_zpl();
    test_brother();
    test_brother_status();
    test_encode_all();
    test_errors();

    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("all checks passed\n");
    return 0;
}
