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
 */
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

// The published package, imported exactly as a consumer would import it - so
// what this checks against the native build is the artefact that ships.
import createReceipet from '../packages/js/emscripten-port/dist/receipet.mjs';

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

const ERRORS = { '-1': 'invalid argument', '-2': 'out of memory', '-3': 'internal error' };

// Every encoder takes a rcpt_buf* to fill and returns a result code, so the
// 12-byte block below is an ordinary first argument - {data, cap, len} - and
// the same pointer goes straight to rcpt_buf_free.
function encode(M, call) {
  const buf = M._malloc(12);
  try {
    const rc = call(buf);
    try {
      if (rc !== 0) throw new Error(`encode failed: ${ERRORS[rc] || rc}`);
      const data = M.getValue(buf, 'i32');
      const cap = M.getValue(buf + 4, 'i32');
      const len = M.getValue(buf + 8, 'i32');
      // len === cap on success; clamping costs nothing and cannot be wrong
      return M.HEAPU8.slice(data, data + Math.min(len, cap)); // copy: the heap moves
    } finally {
      M._rcpt_buf_free(buf);
    }
  } finally {
    M._free(buf);
  }
}

const M = await createReceipet();

// rcpt_image_new writes the image pointer through an out-parameter too
const slot = M._malloc(4);
if (M._rcpt_image_new(slot, W, H) !== 0) throw new Error('rcpt_image_new failed');
const img = M.getValue(slot, 'i32');
M._free(slot);

try {
  draw(M, img);
  if (M._rcpt_image_get_pixel(img, 0, 0) !== 1) throw new Error('pixel readback failed');

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
