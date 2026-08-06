/*
 * Draws a test card and dumps it in every supported printer language.
 *
 *   build/demo [dir/]     -> writes demo.<fmt>, into dir/ if given
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "receipet.h"

#define W 128
#define H 64

/* Choosing a language at runtime means a table of thunks; the library has no
 * format enum, since each language takes its own options. */
typedef rcpt_result (*encode_fn)(rcpt_buf *out, const rcpt_image *img);

typedef struct format {
    const char *name;
    encode_fn encode;
} format;

/* Shapes are the caller's business.  The naive version; a real drawing layer
 * would fill whole bytes of a row at a time. */
static void fill_rect(rcpt_image *img, int32_t x, int32_t y, int32_t w, int32_t h, int value) {
    int32_t ix, iy;
    for (iy = y; iy < y + h; iy++)
        for (ix = x; ix < x + w; ix++)
            rcpt_image_set_pixel(img, ix, iy, value); /* clips for us */
}

static void draw(rcpt_image *img) {
    int32_t i;

    rcpt_image_clear(img);

    /* frame */
    fill_rect(img, 0, 0, W, 2, 1);
    fill_rect(img, 0, H - 2, W, 2, 1);
    fill_rect(img, 0, 0, 2, H, 1);
    fill_rect(img, W - 2, 0, 2, H, 1);

    /* diagonals, one pixel at a time */
    for (i = 0; i < H; i++) {
        rcpt_image_set_pixel(img, i * W / H, i, 1);
        rcpt_image_set_pixel(img, W - 1 - i * W / H, i, 1);
    }

    /* a solid block and a checkerboard, to make polarity mistakes obvious */
    fill_rect(img, 16, 24, 24, 16, 1);
    for (i = 0; i < 16 * 24; i++) {
        int32_t x = 72 + i % 24, y = 24 + i / 24;
        rcpt_image_set_pixel(img, x, y, (x + y) & 1);
    }
}

static void ascii_preview(const rcpt_image *img) {
    int32_t x, y;
    for (y = 0; y < (int32_t)img->height; y += 2) {
        for (x = 0; x < (int32_t)img->width; x += 1)
            putchar(rcpt_image_get_pixel(img, x, y) ? '#' : '.');
        putchar('\n');
    }
}

static rcpt_result as_escpos(rcpt_buf *o, const rcpt_image *i) {
    return rcpt_enc_escpos(o, i, 0, 0);
}
static rcpt_result as_escp(rcpt_buf *o, const rcpt_image *i) {
    return rcpt_enc_escp(o, i, 0, 0);
}
static rcpt_result as_tspl(rcpt_buf *o, const rcpt_image *i) {
    return rcpt_enc_tspl(o, i, 0, 0, 0, 1, 0);
}
static rcpt_result as_cpcl(rcpt_buf *o, const rcpt_image *i) {
    return rcpt_enc_cpcl(o, i, 0, 0, 203, 1, 0);
}
static rcpt_result as_zpl(rcpt_buf *o, const rcpt_image *i) {
    return rcpt_enc_zpl(o, i, 0, 0, 1, 0);
}

static const format FORMATS[] = {
    { "escpos", as_escpos },
    { "escp",   as_escp   },
    { "tspl",   as_tspl   },
    { "cpcl",   as_cpcl   },
    { "zpl",    as_zpl    }
};
#define FORMAT_COUNT ((int)(sizeof FORMATS / sizeof FORMATS[0]))

static int dump(int f, const rcpt_image *img, const char *dir) {
    const char *name = FORMATS[f].name;
    char path[512];
    rcpt_buf job;
    FILE *fp;
    rcpt_result rc;

    rc = FORMATS[f].encode(&job, img);
    if (rc != RCPT_OK) {
        fprintf(stderr, "encode failed for %s: %d\n", name, (int)rc);
        return -1;
    }

    snprintf(path, sizeof(path), "%sdemo.%s", dir, name);
    fp = fopen(path, "wb");
    if (!fp) {
        perror(path);
        rcpt_buf_free(&job);
        return -1;
    }
    fwrite(job.data, 1, job.len, fp);
    fclose(fp);

    printf("%-8s %6zu bytes  -> %s\n", name, job.len, path);
    rcpt_buf_free(&job);
    return 0;
}

int main(int argc, char **argv) {
    rcpt_image *img;
    char dir[480];
    int f, rc = 0;

    if (rcpt_image_new(&img, W, H) != RCPT_OK) {
        fprintf(stderr, "image allocation failed\n");
        return 1;
    }
    draw(img);
    ascii_preview(img);

    dir[0] = '\0';
    if (argc > 1) {
        size_t n = strlen(argv[1]);
        if (n >= sizeof(dir) - 2) n = sizeof(dir) - 2;
        memcpy(dir, argv[1], n);
        dir[n] = '\0';
        if (n && dir[n - 1] != '/') {
            dir[n]     = '/';
            dir[n + 1] = '\0';
        }
    }

    for (f = 0; f < FORMAT_COUNT; f++)
        if (dump(f, img, dir) != 0) rc = 1;

    rcpt_image_free(img);
    return rc;
}
