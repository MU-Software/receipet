#include <stdlib.h>
#include <string.h>

#include "rcpt_internal.h"

rcpt_result rcpt_image_new(rcpt_image **out, uint32_t width, uint32_t height) {
    uint32_t stride;
    size_t need;
    rcpt_image *img;

    if (!out) return RCPT_ERR_ARG;
    *out = NULL;

    if (width == 0 || height == 0) return RCPT_ERR_ARG;
    if (width > RCPT_MAX_DIM || height > RCPT_MAX_DIM) return RCPT_ERR_ARG;

    stride = (width + 7u) / 8u;
    /* stride <= 8192 and height <= 65535, so this cannot overflow 32 bits. */
    need = (size_t)stride * (size_t)height;

    /* One allocation: header followed by the pixel data. */
    img = (rcpt_image *)malloc(sizeof(rcpt_image) + need);
    if (!img) return RCPT_ERR_NOMEM;

    img->width  = width;
    img->height = height;
    img->stride = stride;
    img->data   = (uint8_t *)img + sizeof(rcpt_image);
    memset(img->data, 0, need);

    *out = img;
    return RCPT_OK;
}

void rcpt_image_free(rcpt_image *img) {
    free(img);
}

void rcpt_image_clear(rcpt_image *img) {
    if (!img || !img->data) return;
    memset(img->data, 0, (size_t)img->stride * img->height);
}

void rcpt_image_set_pixel(rcpt_image *img, int32_t x, int32_t y, int value) {
    size_t off;
    uint8_t bit;

    if (!img || !img->data) return;
    if (x < 0 || y < 0 || (uint32_t)x >= img->width || (uint32_t)y >= img->height) return;

    off = (size_t)y * img->stride + ((size_t)x >> 3);
    bit = (uint8_t)(0x80u >> ((uint32_t)x & 7u));

    if (value)
        img->data[off] |= bit;
    else
        img->data[off] &= (uint8_t)~bit;
}

int rcpt_image_get_pixel(const rcpt_image *img, int32_t x, int32_t y) {
    size_t off;
    if (!img || !img->data) return 0;
    if (x < 0 || y < 0 || (uint32_t)x >= img->width || (uint32_t)y >= img->height) return 0;
    off = (size_t)y * img->stride + ((size_t)x >> 3);
    return (img->data[off] >> (7u - ((uint32_t)x & 7u))) & 1u;
}
