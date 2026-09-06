/*
 * Emits the npm package's constants.mjs and constants.d.ts; argv[1] picks
 * which.  `make js-constants`.
 *
 * Compile with emcc, not $(CC): pointer and size_t widths move
 * rcpt_image.data and shrink rcpt_buf.
 */
#include <stdio.h>
#include <stddef.h>
#include <string.h>

#include "receipet.h"

static int decls;

/* `number`, not the literal type - see docs/packaging.md. */
static void num(const char *name, long long value) {
    if (decls) {
        printf("export const %s: number;\n", name);
        return;
    }
    printf("export const %s = %lld;\n", name, value);
}

static void bytes(const char *name, const char *s, size_t n) {
    if (decls) {
        printf("export const %s: Uint8Array;\n", name);
        return;
    }
    printf("export const %s = /* @__PURE__ */ Uint8Array.of(", name);
    for (size_t i = 0; i < n; i++) {
        printf("%s0x%02x", i ? ", " : "", (unsigned char)s[i]);
    }
    printf(");\n");
}

static void section(const char *title) {
    printf("\n/* %s */\n", title);
}

int main(int argc, char **argv) {
    decls = argc > 1 && strcmp(argv[1], "dts") == 0;

    printf("/*\n"
           " * Generated from include/receipet.h - do not edit, run `make js-constants`.\n"
           " * The RCPT_ prefix is dropped; import the module as a namespace.\n"
           " * Offsets and sizes are the wasm32 layout.\n"
           " */\n");

    section("encoder flags");
    num("F_RAW", RCPT_F_RAW);
    num("F_INVERT", RCPT_F_INVERT);
    num("F_BROTHER_NO_CUT", RCPT_F_BROTHER_NO_CUT);

    section("rcpt_result");
    num("OK", RCPT_OK);
    num("ERR_ARG", RCPT_ERR_ARG);
    num("ERR_NOMEM", RCPT_ERR_NOMEM);
    num("ERR_INTERNAL", RCPT_ERR_INTERNAL);

    section("limits");
    num("MAX_DIM", RCPT_MAX_DIM);

    section("API version - the C API, not the release number");
    num("VERSION_MAJOR", RCPT_VERSION_MAJOR);
    num("VERSION_MINOR", RCPT_VERSION_MINOR);
    num("VERSION_PATCH", RCPT_VERSION_PATCH);

    section("Brother QL status");
    /* A string literal: the trailing NUL is not part of the request. */
    bytes("BROTHER_STATUS_REQUEST", RCPT_BROTHER_STATUS_REQUEST, sizeof(RCPT_BROTHER_STATUS_REQUEST) - 1);
    num("BROTHER_STATUS_LEN", RCPT_BROTHER_STATUS_LEN);
    num("BROTHER_STATUS_FIELDS", RCPT_BROTHER_STATUS_FIELDS);

    section("Brother QL media_type");
    num("BROTHER_MEDIA_NONE", RCPT_BROTHER_MEDIA_NONE);
    num("BROTHER_MEDIA_CONTINUOUS", RCPT_BROTHER_MEDIA_CONTINUOUS);
    num("BROTHER_MEDIA_DIE_CUT", RCPT_BROTHER_MEDIA_DIE_CUT);

    section("Brother QL error bits");
    num("BROTHER_ERR_NO_MEDIA", RCPT_BROTHER_ERR_NO_MEDIA);
    num("BROTHER_ERR_END_OF_MEDIA", RCPT_BROTHER_ERR_END_OF_MEDIA);
    num("BROTHER_ERR_CUTTER_JAM", RCPT_BROTHER_ERR_CUTTER_JAM);
    num("BROTHER_ERR_WEAK_BATTERIES", RCPT_BROTHER_ERR_WEAK_BATTERIES);
    num("BROTHER_ERR_IN_USE", RCPT_BROTHER_ERR_IN_USE);
    num("BROTHER_ERR_POWERED_OFF", RCPT_BROTHER_ERR_POWERED_OFF);
    num("BROTHER_ERR_HIGH_VOLTAGE_ADAPTER", RCPT_BROTHER_ERR_HIGH_VOLTAGE_ADAPTER);
    num("BROTHER_ERR_FAN_MOTOR", RCPT_BROTHER_ERR_FAN_MOTOR);
    num("BROTHER_ERR_REPLACE_MEDIA", RCPT_BROTHER_ERR_REPLACE_MEDIA);
    num("BROTHER_ERR_BUFFER_FULL", RCPT_BROTHER_ERR_BUFFER_FULL);
    num("BROTHER_ERR_COMMS", RCPT_BROTHER_ERR_COMMS);
    num("BROTHER_ERR_COMM_BUFFER_FULL", RCPT_BROTHER_ERR_COMM_BUFFER_FULL);
    num("BROTHER_ERR_COVER_OPEN", RCPT_BROTHER_ERR_COVER_OPEN);
    num("BROTHER_ERR_CANCEL_KEY", RCPT_BROTHER_ERR_CANCEL_KEY);
    num("BROTHER_ERR_CANNOT_FEED", RCPT_BROTHER_ERR_CANNOT_FEED);
    num("BROTHER_ERR_SYSTEM", RCPT_BROTHER_ERR_SYSTEM);

    section("rcpt_image layout");
    num("IMAGE_WIDTH_OFFSET", (long long)offsetof(rcpt_image, width));
    num("IMAGE_HEIGHT_OFFSET", (long long)offsetof(rcpt_image, height));
    num("IMAGE_STRIDE_OFFSET", (long long)offsetof(rcpt_image, stride));
    num("IMAGE_DATA_OFFSET", (long long)offsetof(rcpt_image, data));
    num("IMAGE_SIZEOF", (long long)sizeof(rcpt_image));

    section("rcpt_buf layout");
    num("BUF_DATA_OFFSET", (long long)offsetof(rcpt_buf, data));
    num("BUF_CAP_OFFSET", (long long)offsetof(rcpt_buf, cap));
    num("BUF_LEN_OFFSET", (long long)offsetof(rcpt_buf, len));
    num("BUF_SIZEOF", (long long)sizeof(rcpt_buf));

    section("rcpt_brother_status layout");
    num("BROTHER_STATUS_MODEL_OFFSET", (long long)offsetof(rcpt_brother_status, model));
    num("BROTHER_STATUS_ERRORS_OFFSET", (long long)offsetof(rcpt_brother_status, errors));
    num("BROTHER_STATUS_MEDIA_WIDTH_MM_OFFSET", (long long)offsetof(rcpt_brother_status, media_width_mm));
    num("BROTHER_STATUS_MEDIA_LENGTH_MM_OFFSET", (long long)offsetof(rcpt_brother_status, media_length_mm));
    num("BROTHER_STATUS_MEDIA_TYPE_OFFSET", (long long)offsetof(rcpt_brother_status, media_type));
    num("BROTHER_STATUS_STATUS_TYPE_OFFSET", (long long)offsetof(rcpt_brother_status, status_type));
    num("BROTHER_STATUS_PHASE_TYPE_OFFSET", (long long)offsetof(rcpt_brother_status, phase_type));
    num("BROTHER_STATUS_PHASE_NUMBER_OFFSET", (long long)offsetof(rcpt_brother_status, phase_number));
    num("BROTHER_STATUS_NOTIFICATION_OFFSET", (long long)offsetof(rcpt_brother_status, notification));
    num("BROTHER_STATUS_SIZEOF", (long long)sizeof(rcpt_brother_status));

    return 0;
}
