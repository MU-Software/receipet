/*
 * Emscripten smoke test / usage example.
 *
 *   make wasm-emcc && ./build/demo build/native/
 *   node examples/node_demo.mjs
 *
 * Everything is plain pointer arithmetic against the wasm heap: allocate an
 * image, poke pixels, ask for the encoded size, then encode into a scratch
 * buffer and copy it out.
 *
 * It draws the same card as examples/demo.c and compares every encoder's
 * output against what the native build wrote to build/native/, byte for byte.
 * Printing the sizes would not have caught an argument list going stale.
 *
 * It also checks the generated constants: a wrong offset reads a neighbouring
 * field rather than failing, so nothing else in the build would notice.
 */
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

// The published package, imported exactly as a consumer would import it - so
// what this checks against the native build is the artefact that ships.
import createReceipet from '../packages/js/emscripten-port/dist/receipet.mjs';
import * as RCPT from '../packages/js/emscripten-port/constants.mjs';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const NATIVE = path.join(HERE, '..', 'build', 'native');
const W = 128, H = 64;

// The C side only writes pixels; rectangles and the like live out here.
function fillRect(M, img, x, y, w, h, value) {
  for (let yy = y; yy < y + h; yy++)
    for (let xx = x; xx < x + w; xx++) M._rcpt_image_set_pixel(img, xx, yy, value);
}

// Must match draw() in examples/demo.c exactly, or the comparison is noise.
function draw(M, img) {
  M._rcpt_image_clear(img);
  fillRect(M, img, 0, 0, W, 2, 1);
  fillRect(M, img, 0, H - 2, W, 2, 1);
  fillRect(M, img, 0, 0, 2, H, 1);
  fillRect(M, img, W - 2, 0, 2, H, 1);
  for (let i = 0; i < H; i++) {
    M._rcpt_image_set_pixel(img, Math.floor((i * W) / H), i, 1);
    M._rcpt_image_set_pixel(img, W - 1 - Math.floor((i * W) / H), i, 1);
  }
  fillRect(M, img, 16, 24, 24, 16, 1);
  for (let i = 0; i < 16 * 24; i++) {
    const x = 72 + (i % 24), y = 24 + Math.floor(i / 24);
    M._rcpt_image_set_pixel(img, x, y, (x + y) & 1);
  }
}

const ERRORS = {
  [RCPT.ERR_ARG]: 'invalid argument',
  [RCPT.ERR_NOMEM]: 'out of memory',
  [RCPT.ERR_INTERNAL]: 'internal error',
};

// Every encoder takes a rcpt_buf* to fill and returns a result code, so the
// block below is an ordinary first argument - {data, cap, len} - and the same
// pointer goes straight to rcpt_buf_free.
function encode(M, call) {
  const buf = M._malloc(RCPT.BUF_SIZEOF);
  try {
    const rc = call(buf);
    try {
      if (rc !== RCPT.OK) throw new Error(`encode failed: ${ERRORS[rc] || rc}`);
      const data = M.getValue(buf + RCPT.BUF_DATA_OFFSET, 'i32');
      const cap = M.getValue(buf + RCPT.BUF_CAP_OFFSET, 'i32');
      const len = M.getValue(buf + RCPT.BUF_LEN_OFFSET, 'i32');
      // len === cap on success; clamping costs nothing and cannot be wrong
      return M.HEAPU8.slice(data, data + Math.min(len, cap)); // copy: the heap moves
    } finally {
      M._rcpt_buf_free(buf);
    }
  } finally {
    M._free(buf);
  }
}

// A reply whose every field differs, so a wrong offset cannot land on a
// matching value.  Same bytes as test_brother_status() in tests/.
function checkBrotherStatus(M) {
  const reply = new Uint8Array(RCPT.BROTHER_STATUS_LEN);
  reply[4] = 0x41;  // QL-820NWB
  reply[8] = 0x01;  // error information 1: no media
  reply[9] = 0x10;  // error information 2: cover open
  reply[10] = 62;   // media width, mm
  reply[11] = RCPT.BROTHER_MEDIA_CONTINUOUS;
  reply[17] = 0;    // continuous tape has no length
  reply[18] = 0x00; // reply to status request
  reply[19] = 0x01; // printing
  reply[21] = 0x0a; // phase number, big endian
  reply[22] = 0x03; // cooling started

  const raw = M._malloc(reply.length);
  const st = M._malloc(RCPT.BROTHER_STATUS_SIZEOF);
  try {
    M.HEAPU8.set(reply, raw);
    if (M._rcpt_parse_brother_status(st, raw, reply.length) !== RCPT.OK)
      throw new Error('rcpt_parse_brother_status rejected a well-formed reply');

    const u8 = (off) => M.HEAPU8[st + off];
    const u16 = (off) => M.getValue(st + off, 'i16') & 0xffff; // getValue sign-extends
    const errors = u16(RCPT.BROTHER_STATUS_ERRORS_OFFSET);

    const expected = {
      model: [u8(RCPT.BROTHER_STATUS_MODEL_OFFSET), 0x41],
      media_width_mm: [u8(RCPT.BROTHER_STATUS_MEDIA_WIDTH_MM_OFFSET), 62],
      media_length_mm: [u8(RCPT.BROTHER_STATUS_MEDIA_LENGTH_MM_OFFSET), 0],
      media_type: [u8(RCPT.BROTHER_STATUS_MEDIA_TYPE_OFFSET), RCPT.BROTHER_MEDIA_CONTINUOUS],
      status_type: [u8(RCPT.BROTHER_STATUS_STATUS_TYPE_OFFSET), 0x00],
      phase_type: [u8(RCPT.BROTHER_STATUS_PHASE_TYPE_OFFSET), 0x01],
      phase_number: [u16(RCPT.BROTHER_STATUS_PHASE_NUMBER_OFFSET), 10],
      notification: [u8(RCPT.BROTHER_STATUS_NOTIFICATION_OFFSET), 0x03],
      // the two error bytes must not be swapped
      no_media_bit: [errors & RCPT.BROTHER_ERR_NO_MEDIA ? 1 : 0, 1],
      cover_open_bit: [errors & RCPT.BROTHER_ERR_COVER_OPEN ? 1 : 0, 1],
      replace_media_bit: [errors & RCPT.BROTHER_ERR_REPLACE_MEDIA ? 1 : 0, 0],
    };

    for (const [field, [got, want]] of Object.entries(expected))
      if (got !== want) throw new Error(`brother status ${field}: got ${got}, expected ${want}`);

    // A short reply is not a status, and the length constant is what says so.
    if (M._rcpt_parse_brother_status(st, raw, reply.length - 1) !== RCPT.ERR_ARG)
      throw new Error('a short reply was accepted as a status');

    // The flat form, which is what a JS caller should use, against the struct.
    const vals = M._malloc(RCPT.BROTHER_STATUS_FIELDS * 4);
    try {
      if (M._rcpt_parse_brother_status_u32(vals, raw, reply.length) !== RCPT.OK)
        throw new Error('rcpt_parse_brother_status_u32 rejected a well-formed reply');

      const base = vals >>> 2; // HEAPU32 is indexed in words
      const flat = M.HEAPU32.subarray(base, base + RCPT.BROTHER_STATUS_FIELDS);
      const [model, err, widthMm, lengthMm, mediaType,
             statusType, phaseType, phaseNumber, notification] = flat;

      const sameAsStruct = {
        model: [model, expected.model[1]],
        errors: [err, errors],
        media_width_mm: [widthMm, expected.media_width_mm[1]],
        media_length_mm: [lengthMm, expected.media_length_mm[1]],
        media_type: [mediaType, expected.media_type[1]],
        status_type: [statusType, expected.status_type[1]],
        phase_type: [phaseType, expected.phase_type[1]],
        phase_number: [phaseNumber, expected.phase_number[1]],
        notification: [notification, expected.notification[1]],
      };
      for (const [field, [got, want]] of Object.entries(sameAsStruct))
        if (got !== want) throw new Error(`u32 status ${field}: got ${got}, expected ${want}`);
    } finally {
      M._free(vals);
    }
  } finally {
    M._free(st);
    M._free(raw);
  }
  console.log('brother  status matches the generated offsets, both forms agree');
}

const M = await createReceipet();

// rcpt_image_new writes the image pointer through an out-parameter too
const slot = M._malloc(4);
if (M._rcpt_image_new(slot, W, H) !== RCPT.OK) throw new Error('rcpt_image_new failed');
const img = M.getValue(slot, 'i32');
M._free(slot);

try {
  // The header's public fields, read through the generated offsets.
  const header = {
    width: [M.getValue(img + RCPT.IMAGE_WIDTH_OFFSET, 'i32'), W],
    height: [M.getValue(img + RCPT.IMAGE_HEIGHT_OFFSET, 'i32'), H],
    stride: [M.getValue(img + RCPT.IMAGE_STRIDE_OFFSET, 'i32'), (W + 7) >> 3],
  };
  for (const [field, [got, want]] of Object.entries(header))
    if (got !== want) throw new Error(`rcpt_image ${field}: got ${got}, expected ${want}`);

  draw(M, img);
  if (M._rcpt_image_get_pixel(img, 0, 0) !== 1) throw new Error('pixel readback failed');

  checkBrotherStatus(M);

  // The same options examples/demo.c passes, in the same order.
  const formats = {
    escpos:  (job) => M._rcpt_enc_escpos(job, img, 0, 0),
    escp:    (job) => M._rcpt_enc_escp(job, img, 0, 0),
    tspl:    (job) => M._rcpt_enc_tspl(job, img, 0, 0, 0, 0, 1, 0),
    cpcl:    (job) => M._rcpt_enc_cpcl(job, img, 0, 0, 203, 1, 0),
    zpl:     (job) => M._rcpt_enc_zpl(job, img, 0, 0, 1, 0),
    brother: (job) => M._rcpt_enc_brother(job, img, 0x0B, 38, 90, 0, 0),
  };

  let failures = 0;
  for (const [name, call] of Object.entries(formats)) {
    const bytes = Buffer.from(encode(M, call));
    const ref = fs.readFileSync(path.join(NATIVE, `demo.${name}`));
    const ok = bytes.equals(ref);
    if (!ok) failures++;
    console.log(`${name.padEnd(8)} ${String(bytes.length).padStart(6)} bytes  ` +
                (ok ? 'matches native' : `DIFFERS from native (${ref.length} bytes)`));
  }
  if (failures) throw new Error(`${failures} format(s) differ from the native build`);
  console.log('emcc: all formats byte-identical to the native build');
} finally {
  M._rcpt_image_free(img);
}
