# @mu-software/receipet

The [receipet](https://github.com/MU-Software/receipet) C library compiled to
WebAssembly: a monochrome (1 bit per pixel) image buffer that emits **ESC/POS**,
**ESC/P**, **TSPL**, **CPCL**, **ZPL** or **Brother QL**.

One file. The `.wasm` is embedded in the `.mjs` as a data URI, so there is no
second request, no `locateFile`, and nothing for a bundler to resolve.

## Install

The package is private on GitHub Packages, which has no anonymous access — even
reading it needs a token. Put this in the consuming project's `.npmrc`:

```
@mu-software:registry=https://npm.pkg.github.com
//npm.pkg.github.com/:_authToken=${GITHUB_TOKEN}
```

`GITHUB_TOKEN` is a personal access token with the `read:packages` scope (in
GitHub Actions, the workflow's own `secrets.GITHUB_TOKEN` works if the job has
`permissions: packages: read`). Then:

```sh
npm install @mu-software/receipet
```

## Use

Everything is pointer arithmetic against the wasm heap. `rcpt_enc_*` fills a
12-byte `rcpt_buf` — `data` at offset 0, `cap` at 4, `len` at 8 — and returns a
result code; that same pointer goes to `_rcpt_buf_free`. `rcpt_image_new` works
the same way with a 4-byte slot.

```js
import createReceipet from '@mu-software/receipet';

const M = await createReceipet();

// rcpt_image_new writes the image pointer through an out-parameter
const slot = M._malloc(4);
if (M._rcpt_image_new(slot, 384, 200) !== 0) throw new Error('rcpt_image_new failed');
const img = M.getValue(slot, 'i32');
M._free(slot);

M._rcpt_image_set_pixel(img, 10, 10, 1);

const job = M._malloc(12);
try {
  if (M._rcpt_enc_escpos(job, img, 0, 0) !== 0) throw new Error('encode failed');
  const data = M.getValue(job, 'i32');
  const len = M.getValue(job + 8, 'i32');
  const bytes = M.HEAPU8.slice(data, data + len); // copy: the heap moves
  // ... write `bytes` to the device
  M._rcpt_buf_free(job);
} finally {
  M._free(job);
}

M._rcpt_image_free(img);
```

`M.HEAPU8` is replaced whenever memory grows, so read the property each time
rather than holding a reference, and copy bytes out instead of keeping a view.

Pixels are all the drawing there is — shapes, text and blitting belong in a
layer above this.

Constants (`RCPT_F_RAW`, `RCPT_BROTHER_MEDIA_*`, the error bits) are not
exported: the module exports functions, not values. Their numbers are in
[include/receipet.h](https://github.com/MU-Software/receipet/blob/main/include/receipet.h).

## Buffer layout

1 bit per pixel, row-major, MSB first, **1 = black**, row `n` at
`data[n * stride]` with `stride = (width + 7) / 8`. This is exactly what ESC/POS
`GS v 0`, ZPL `^GFA` and CPCL `EG` expect. TSPL prints on `0`, so its bits are
inverted on the way out; ESC/P is column oriented and transposed band by band.

Full documentation, including what each real printer gets wrong, is in the
[repository](https://github.com/MU-Software/receipet).
