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
`rcpt_buf` and returns a result code; that same pointer goes to
`_rcpt_buf_free`. `rcpt_image_new` works the same way with a 4-byte slot.

The sizes and offsets that arithmetic needs come from
`@mu-software/receipet/constants` — never write them out by hand.

```js
import createReceipet from '@mu-software/receipet';
import * as RCPT from '@mu-software/receipet/constants';

const M = await createReceipet();

// rcpt_image_new writes the image pointer through an out-parameter
const slot = M._malloc(4);
if (M._rcpt_image_new(slot, 384, 200) !== RCPT.OK) throw new Error('rcpt_image_new failed');
const img = M.getValue(slot, 'i32');
M._free(slot);

M._rcpt_image_set_pixel(img, 10, 10, 1);

const job = M._malloc(RCPT.BUF_SIZEOF);
try {
  if (M._rcpt_enc_escpos(job, img, 0, 0) !== RCPT.OK) throw new Error('encode failed');
  const data = M.getValue(job + RCPT.BUF_DATA_OFFSET, 'i32');
  const len = M.getValue(job + RCPT.BUF_LEN_OFFSET, 'i32');
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

## Constants

The wasm module exports functions, not values — a `#define` leaves nothing
behind to export, and a struct offset has no spelling in TypeScript. So they
ship as a separate module, generated at build time by compiling
[include/receipet.h](https://github.com/MU-Software/receipet/blob/main/include/receipet.h)
**for wasm32** and printing what the compiler found. Copying them out of the
header by hand is the one thing that will silently read the wrong bytes; the
header can change and nothing would tell you.

```js
import * as RCPT from '@mu-software/receipet/constants';
```

The `RCPT_` prefix is dropped, so `RCPT_F_RAW` is `RCPT.F_RAW`. What is there:

| | |
| --- | --- |
| flags | `F_RAW` `F_INVERT` `F_BROTHER_NO_CUT` |
| results | `OK` `ERR_ARG` `ERR_NOMEM` `ERR_INTERNAL` |
| limits, version | `MAX_DIM` `VERSION_MAJOR` `VERSION_MINOR` `VERSION_PATCH` |
| Brother status | `BROTHER_STATUS_REQUEST` (a `Uint8Array`) `BROTHER_STATUS_LEN` `BROTHER_STATUS_FIELDS` |
| Brother media | `BROTHER_MEDIA_NONE` `_CONTINUOUS` `_DIE_CUT` |
| Brother errors | `BROTHER_ERR_*`, eleven bits |
| layout | `IMAGE_*_OFFSET` `IMAGE_SIZEOF` `BUF_*_OFFSET` `BUF_SIZEOF` `BROTHER_STATUS_*_OFFSET` `BROTHER_STATUS_SIZEOF` |

## Reading a Brother QL's status

Send `BROTHER_STATUS_REQUEST`, read back exactly `BROTHER_STATUS_LEN` bytes —
the reply carries no framing, so the length is how you know it is complete —
and hand them to `_rcpt_parse_brother_status_u32`. It writes
`BROTHER_STATUS_FIELDS` 32-bit values, so the result is one `HEAPU32` window
and no offsets at all.

```js
const reply = await readExactly(RCPT.BROTHER_STATUS_LEN); // Uint8Array, from the device

const raw = M._malloc(reply.length);
const out = M._malloc(RCPT.BROTHER_STATUS_FIELDS * 4);
try {
  M.HEAPU8.set(reply, raw);
  if (M._rcpt_parse_brother_status_u32(out, raw, reply.length) !== RCPT.OK)
    throw new Error('not a 32-byte status reply');

  // Declaration order.
  const base = out >>> 2; // HEAPU32 is indexed in words
  const [model, errors, widthMm, lengthMm, mediaType,
         statusType, phaseType, phaseNumber, notification] =
    M.HEAPU32.subarray(base, base + RCPT.BROTHER_STATUS_FIELDS);

  if (errors & RCPT.BROTHER_ERR_COVER_OPEN) throw new Error('cover open');
  if (mediaType === RCPT.BROTHER_MEDIA_NONE) throw new Error('no media loaded');
  // widthMm and lengthMm are what the printer sensed - pass them to _rcpt_enc_brother
} finally {
  M._free(out);
  M._free(raw);
}
```

`_rcpt_parse_brother_status` fills the C struct instead, which is what the
`BROTHER_STATUS_*_OFFSET` constants are for. It is the same parse; use it only
if you need the struct itself.

## Buffer layout

1 bit per pixel, row-major, MSB first, **1 = black**, row `n` at
`data[n * stride]` with `stride = (width + 7) / 8`. This is exactly what ESC/POS
`GS v 0`, ZPL `^GFA` and CPCL `EG` expect. TSPL prints on `0`, so its bits are
inverted on the way out; ESC/P is column oriented and transposed band by band.

Full documentation, including what each real printer gets wrong, is in the
[repository](https://github.com/MU-Software/receipet).
