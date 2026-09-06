/*
 * Generated from include/receipet.h - do not edit, run `make js-constants`.
 * The RCPT_ prefix is dropped; import the module as a namespace.
 * Offsets and sizes are the wasm32 layout.
 */

/* encoder flags */
export const F_RAW: number;
export const F_INVERT: number;
export const F_BROTHER_NO_CUT: number;

/* rcpt_result */
export const OK: number;
export const ERR_ARG: number;
export const ERR_NOMEM: number;
export const ERR_INTERNAL: number;

/* limits */
export const MAX_DIM: number;

/* API version - the C API, not the release number */
export const VERSION_MAJOR: number;
export const VERSION_MINOR: number;
export const VERSION_PATCH: number;

/* Brother QL status */
export const BROTHER_STATUS_REQUEST: Uint8Array;
export const BROTHER_STATUS_LEN: number;
export const BROTHER_STATUS_FIELDS: number;

/* Brother QL media_type */
export const BROTHER_MEDIA_NONE: number;
export const BROTHER_MEDIA_CONTINUOUS: number;
export const BROTHER_MEDIA_DIE_CUT: number;

/* Brother QL error bits */
export const BROTHER_ERR_NO_MEDIA: number;
export const BROTHER_ERR_END_OF_MEDIA: number;
export const BROTHER_ERR_CUTTER_JAM: number;
export const BROTHER_ERR_WEAK_BATTERIES: number;
export const BROTHER_ERR_IN_USE: number;
export const BROTHER_ERR_POWERED_OFF: number;
export const BROTHER_ERR_HIGH_VOLTAGE_ADAPTER: number;
export const BROTHER_ERR_FAN_MOTOR: number;
export const BROTHER_ERR_REPLACE_MEDIA: number;
export const BROTHER_ERR_BUFFER_FULL: number;
export const BROTHER_ERR_COMMS: number;
export const BROTHER_ERR_COMM_BUFFER_FULL: number;
export const BROTHER_ERR_COVER_OPEN: number;
export const BROTHER_ERR_CANCEL_KEY: number;
export const BROTHER_ERR_CANNOT_FEED: number;
export const BROTHER_ERR_SYSTEM: number;

/* rcpt_image layout */
export const IMAGE_WIDTH_OFFSET: number;
export const IMAGE_HEIGHT_OFFSET: number;
export const IMAGE_STRIDE_OFFSET: number;
export const IMAGE_DATA_OFFSET: number;
export const IMAGE_SIZEOF: number;

/* rcpt_buf layout */
export const BUF_DATA_OFFSET: number;
export const BUF_CAP_OFFSET: number;
export const BUF_LEN_OFFSET: number;
export const BUF_SIZEOF: number;

/* rcpt_brother_status layout */
export const BROTHER_STATUS_MODEL_OFFSET: number;
export const BROTHER_STATUS_ERRORS_OFFSET: number;
export const BROTHER_STATUS_MEDIA_WIDTH_MM_OFFSET: number;
export const BROTHER_STATUS_MEDIA_LENGTH_MM_OFFSET: number;
export const BROTHER_STATUS_MEDIA_TYPE_OFFSET: number;
export const BROTHER_STATUS_STATUS_TYPE_OFFSET: number;
export const BROTHER_STATUS_PHASE_TYPE_OFFSET: number;
export const BROTHER_STATUS_PHASE_NUMBER_OFFSET: number;
export const BROTHER_STATUS_NOTIFICATION_OFFSET: number;
export const BROTHER_STATUS_SIZEOF: number;
