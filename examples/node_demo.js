/*
 * Emscripten smoke test / usage example.
 *
 *   make wasm-emcc
 *   node examples/node_demo.js
 *
 * Everything is plain pointer arithmetic against the wasm heap: allocate an
 * image, poke pixels, ask for the encoded size, then encode into a scratch
 * buffer and copy it out.
 */
const path = require('path');
const createReceipet = require(path.join(__dirname, '..', 'build', 'emcc', 'receipet.js'));

// The C side only writes pixels; rectangles and the like live out here.
function fillRect(M, img, x, y, w, h, value) {
  for (let yy = y; yy < y + h; yy++)
    for (let xx = x; xx < x + w; xx++) M._rcpt_image_set_pixel(img, xx, yy, value);
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

(async () => {
  const M = await createReceipet();
  const W = 64, H = 32;

  // rcpt_image_new writes the image pointer through an out-parameter too
  const slot = M._malloc(4);
  if (M._rcpt_image_new(slot, W, H) !== 0) throw new Error('rcpt_image_new failed');
  const img = M.getValue(slot, 'i32');
  M._free(slot);

  try {
    fillRect(M, img, 0, 0, W, 2, 1);                     // top rule
    for (let i = 0; i < H; i++) M._rcpt_image_set_pixel(img, i * 2, i, 1);

    if (M._rcpt_image_get_pixel(img, 0, 0) !== 1) throw new Error('pixel readback failed');

    const formats = {
      ESCPOS: (job) => M._rcpt_enc_escpos(job, img, 0, 0),
      ESCP:   (job) => M._rcpt_enc_escp(job, img, 0, 0),
      TSPL:   (job) => M._rcpt_enc_tspl(job, img, 0, 0, 0, 1, 0),
      CPCL:   (job) => M._rcpt_enc_cpcl(job, img, 0, 0, 0, 1, 0),
      ZPL:    (job) => M._rcpt_enc_zpl(job, img, 0, 0, 1, 0),
    };

    for (const [name, call] of Object.entries(formats)) {
      const bytes = encode(M, call);
      const head = Buffer.from(bytes.slice(0, 16)).toString('hex');
      console.log(`${name.padEnd(7)} ${String(bytes.length).padStart(6)} bytes  ${head}...`);
    }
  } finally {
    M._rcpt_image_free(img);
  }
})();
