/*
 * Generated from include/receipet.h - do not edit, run `make js-constants`.
 * The RCPT_ prefix is dropped; import the module as a namespace.
 * Offsets and sizes are the wasm32 layout.
 */

/* encoder flags */
export const F_RAW = 1;
export const F_INVERT = 2;
export const F_BROTHER_NO_CUT = 4;

/* rcpt_result */
export const OK = 0;
export const ERR_ARG = -1;
export const ERR_NOMEM = -2;
export const ERR_INTERNAL = -3;

/* limits */
export const MAX_DIM = 65535;

/* API version - the C API, not the release number */
export const VERSION_MAJOR = 0;
export const VERSION_MINOR = 2;
export const VERSION_PATCH = 0;

/* Brother QL status */
export const BROTHER_STATUS_REQUEST = /* @__PURE__ */ Uint8Array.of(0x1b, 0x69, 0x53);
export const BROTHER_STATUS_LEN = 32;
export const BROTHER_STATUS_FIELDS = 9;

/* Brother QL media_type */
export const BROTHER_MEDIA_NONE = 0;
export const BROTHER_MEDIA_CONTINUOUS = 10;
export const BROTHER_MEDIA_DIE_CUT = 11;

/* Brother QL error bits */
export const BROTHER_ERR_NO_MEDIA = 1;
export const BROTHER_ERR_END_OF_MEDIA = 2;
export const BROTHER_ERR_CUTTER_JAM = 4;
export const BROTHER_ERR_IN_USE = 16;
export const BROTHER_ERR_POWERED_OFF = 32;
export const BROTHER_ERR_REPLACE_MEDIA = 256;
export const BROTHER_ERR_BUFFER_FULL = 512;
export const BROTHER_ERR_COMMS = 1024;
export const BROTHER_ERR_COVER_OPEN = 4096;
export const BROTHER_ERR_CANNOT_FEED = 16384;
export const BROTHER_ERR_SYSTEM = 32768;

/* rcpt_image layout */
export const IMAGE_WIDTH_OFFSET = 0;
export const IMAGE_HEIGHT_OFFSET = 4;
export const IMAGE_STRIDE_OFFSET = 8;
export const IMAGE_DATA_OFFSET = 12;
export const IMAGE_SIZEOF = 16;

/* rcpt_buf layout */
export const BUF_DATA_OFFSET = 0;
export const BUF_CAP_OFFSET = 4;
export const BUF_LEN_OFFSET = 8;
export const BUF_SIZEOF = 12;

/* rcpt_brother_status layout */
export const BROTHER_STATUS_MODEL_OFFSET = 0;
export const BROTHER_STATUS_ERRORS_OFFSET = 2;
export const BROTHER_STATUS_MEDIA_WIDTH_MM_OFFSET = 4;
export const BROTHER_STATUS_MEDIA_LENGTH_MM_OFFSET = 5;
export const BROTHER_STATUS_MEDIA_TYPE_OFFSET = 6;
export const BROTHER_STATUS_STATUS_TYPE_OFFSET = 7;
export const BROTHER_STATUS_PHASE_TYPE_OFFSET = 8;
export const BROTHER_STATUS_PHASE_NUMBER_OFFSET = 10;
export const BROTHER_STATUS_NOTIFICATION_OFFSET = 12;
export const BROTHER_STATUS_SIZEOF = 14;
