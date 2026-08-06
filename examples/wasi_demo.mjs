/*
 * WASI smoke test / usage example (Node's built-in WASI host).
 *
 *   make wasm-wasi && ./build/demo build/native/
 *   node --experimental-wasi-unstable-preview1 examples/wasi_demo.mjs
 *
 * The module is a *reactor*: it has no main(), so the host calls
 * wasi.initialize() once and then invokes the exports directly.  Any other
 * WASI runtime (wasmtime, wasmer, wazero, a Go/Rust host, ...) works the same
 * way - the library itself makes no WASI calls at all.
 *
 * It draws the same card as examples/demo.c and compares every encoder's
 * output against what the native build wrote to build/native/, byte for byte.
 * Printing the sizes would not have caught an argument list going stale.
 */
import { readFile } from 'node:fs/promises';
import { WASI } from 'node:wasi';

const wasi = new WASI({ version: 'preview1', args: [], env: {} });
const bytes = await readFile(new URL('../build/wasi/receipet.wasm', import.meta.url));
const instance = await WebAssembly.instantiate(
  await WebAssembly.compile(bytes),
  wasi.getImportObject(),
);
wasi.initialize(instance);

const e = instance.exports;
// Re-read the view after every call that can grow memory.
const heap = () => new Uint8Array(e.memory.buffer);

// The C side only writes pixels; rectangles and the like live out here.
function fillRect(img, x, y, w, h, value) {
  for (let yy = y; yy < y + h; yy++)
    for (let xx = x; xx < x + w; xx++) e.rcpt_image_set_pixel(img, xx, yy, value);
}

const ERRORS = { '-1': 'invalid argument', '-2': 'out of memory', '-3': 'internal error' };

// Every encoder takes a rcpt_buf* to fill and returns a result code, so the
// 12-byte block below is an ordinary first argument - {data, cap, len} - and
// the same pointer goes straight to rcpt_buf_free.
function encode(call) {
  const buf = e.malloc(12);
  try {
    const rc = call(buf);
    try {
      if (rc !== 0) throw new Error(`encode failed: ${ERRORS[rc] || rc}`);
      const view = new DataView(e.memory.buffer);
      const data = view.getUint32(buf, true);
      const cap = view.getUint32(buf + 4, true);
      const len = view.getUint32(buf + 8, true);
      // len === cap on success; clamping costs nothing and cannot be wrong
      return heap().slice(data, data + Math.min(len, cap));
    } finally {
      e.rcpt_buf_free(buf);
    }
  } finally {
    e.free(buf);
  }
}

const W = 128, H = 64;

// Must match draw() in examples/demo.c exactly, or the comparison is noise.
function draw(img) {
  e.rcpt_image_clear(img);
  fillRect(img, 0, 0, W, 2, 1);
  fillRect(img, 0, H - 2, W, 2, 1);
  fillRect(img, 0, 0, 2, H, 1);
  fillRect(img, W - 2, 0, 2, H, 1);
  for (let i = 0; i < H; i++) {
    e.rcpt_image_set_pixel(img, Math.floor((i * W) / H), i, 1);
    e.rcpt_image_set_pixel(img, W - 1 - Math.floor((i * W) / H), i, 1);
  }
  fillRect(img, 16, 24, 24, 16, 1);
  for (let i = 0; i < 16 * 24; i++) {
    const x = 72 + (i % 24), y = 24 + Math.floor(i / 24);
    e.rcpt_image_set_pixel(img, x, y, (x + y) & 1);
  }
}

// rcpt_image_new writes the image pointer through an out-parameter too
const slot = e.malloc(4);
if (e.rcpt_image_new(slot, W, H) !== 0) throw new Error('rcpt_image_new failed');
const img = new DataView(e.memory.buffer).getUint32(slot, true);
e.free(slot);

try {
  draw(img);
  if (e.rcpt_image_get_pixel(img, 0, 0) !== 1) throw new Error('pixel readback failed');

  // The same options examples/demo.c passes, in the same order.
  const formats = {
    escpos:  (job) => e.rcpt_enc_escpos(job, img, 0, 0),
    escp:    (job) => e.rcpt_enc_escp(job, img, 0, 0),
    tspl:    (job) => e.rcpt_enc_tspl(job, img, 0, 0, 0, 0, 1, 0),
    cpcl:    (job) => e.rcpt_enc_cpcl(job, img, 0, 0, 203, 1, 0),
    zpl:     (job) => e.rcpt_enc_zpl(job, img, 0, 0, 1, 0),
    brother: (job) => e.rcpt_enc_brother(job, img, 0x0B, 38, 90, 0, 0),
  };

  let failures = 0;
  for (const [name, call] of Object.entries(formats)) {
    const out = Buffer.from(encode(call));
    const ref = await readFile(new URL(`../build/native/demo.${name}`, import.meta.url));
    const ok = out.equals(ref);
    if (!ok) failures++;
    console.log(`${name.padEnd(8)} ${String(out.length).padStart(6)} bytes  ` +
                (ok ? 'matches native' : `DIFFERS from native (${ref.length} bytes)`));
  }
  if (failures) throw new Error(`${failures} format(s) differ from the native build`);
  console.log('wasi: all formats byte-identical to the native build');
} finally {
  e.rcpt_image_free(img);
}
