# receipet

A small C99 library that does three things:

1. holds a monochrome (1 bit per pixel) image buffer of any size,
2. lets you write pixels into it,
3. emits that buffer as **ESC/POS**, **ESC/P**, **TSPL**, **CPCL** or **ZPL**.

No dependencies beyond `<stdint.h>`, `<stddef.h>`, `<string.h>` and `<stdlib.h>`.
Builds for the host, for Emscripten and for WASI, and ships a CFFI binding for
Python.

Two allocations in the whole library: one per image, one per encoded job. On a
microcontroller the pattern is to allocate the image once at startup and
`rcpt_image_clear()` between jobs, so only the job block comes and goes.

```c
#include "receipet.h"

rcpt_image *img;
rcpt_buf job;

if (rcpt_image_new(&img, 384, 200) != RCPT_OK) return;

rcpt_image_set_pixel(img, 10, 10, 1);

if (rcpt_enc_escpos(&job, img, 0, 0) == RCPT_OK) {
    /* write job.data, job.len bytes, to the device */
    rcpt_buf_free(&job);
}
rcpt_image_free(img);
```

## Buffer layout

1 bit per pixel, row-major, MSB first, **1 = black (ink)**, row `n` starting at
`data[n * stride]` with `stride = (width + 7) / 8`. The padding bits past
`width` are always zero — nothing in the library can set them, because pixel
writes clip at `width` and the buffer starts blank.

This is deliberately the exact byte layout that ESC/POS `GS v 0`, ZPL `^GFA` and
CPCL `EG` expect, so those encoders copy the buffer straight through. Only two
formats need work: TSPL prints on `0` (bits are inverted on the way out), and
ESC/P is column oriented (the buffer is transposed band by band).

The layout is documented so you can reason about the output, **not so you can
write into `data`**. Direct writes skip the clipping and the padding-bit
invariant the encoders depend on. Go through the pixel functions, even where
that means a lot of calls — this library trades speed for being small and
predictable.

## API

| Function | Purpose |
| --- | --- |
| `rcpt_image_new(&img, w, h)` / `rcpt_image_free` | header and pixels in one allocation |
| `rcpt_image_clear(img)` | blank it again for reuse |
| `rcpt_image_set_pixel` / `_get_pixel` | one pixel; out-of-range writes are dropped |
| `rcpt_enc_escpos(&buf, img, chunk_rows, flags)` | thermal receipt printers |
| `rcpt_enc_escp(&buf, img, mode, flags)` | dot matrix printers |
| `rcpt_enc_tspl(&buf, img, x, y, mode, copies, flags)` | TSC label printers |
| `rcpt_enc_cpcl(&buf, img, x, y, dpi, copies, flags)` | Zebra/Comtec mobile printers |
| `rcpt_enc_zpl(&buf, img, x, y, copies, flags)` | Zebra label printers |
| `rcpt_buf_free(&buf)` | release the bytes, and zero the struct |

That is the whole public surface: eleven functions and three types —
`rcpt_image`, `rcpt_buf` and the `rcpt_result` codes.

**Results.** Anything that can fail returns a `rcpt_result` — `RCPT_OK`,
`RCPT_ERR_ARG`, `RCPT_ERR_NOMEM` or `RCPT_ERR_INTERNAL` — and delivers its
result through an out-parameter. Anything that cannot fail returns nothing,
except a plain accessor like `rcpt_image_get_pixel()`, which returns the value
being asked for. So "did it work" and "what did I get" are never the same
expression, an out-of-memory is never mistaken for a bad argument, and the
signature says which kind of function you are looking at.

Out-of-range pixel coordinates are not a failure — they are clipped, which is
part of the contract. An error you are expected to ignore would be worse than
none: every shape routine draws off the edge on purpose, so a code returned
there would train callers to ignore the codes that do matter.

Failure paths always write the out-parameter too, zeroed, so a caller that
ignores the code still holds something safe to read and safe to free.

Each encoder fills a `rcpt_buf` — `{ uint8_t *data; size_t cap; size_t len; }`
— and takes exactly the options its language has. On success `len == cap`,
since the job is measured before it is built; `cap` is exposed so a binding can
read `min(len, cap)` and be safe whatever the library does. It is a byte block,
not a string: the payload is binary and full of zero bytes.
There is no shared options struct, so there is no table of which field applies
to which format, and no way to set `dpi` on a printer that has no such notion.
Picking a language at runtime is a small table of thunks in your code; see
[examples/demo.c](examples/demo.c).

Pixels are the only drawing the core does. Rectangles, lines and text belong to
a layer above, looping over `rcpt_image_set_pixel()`. The Python binding's
`Image.fill_rect()` and `Image.from_pil()`, and the `fill_rect()` in each
example, are what that looks like — all of them per-pixel, none of them
touching the packed buffer.

There is no combine mode (`OR`/`AND`/`XOR`) on pixel writes: on a single pixel
every one of them collapses to writing 0, writing 1, or toggling, and toggling
is `set_pixel(img, x, y, !get_pixel(img, x, y))`. A combine mode only earns its
place on a region-to-region blit, which would carry its own parameter.

Every encoder runs the same two passes: once with no output buffer at all,
which counts the bytes it would have written, then once into a block of exactly
that size. So the allocation is never over- or under-sized, and there is no
size estimator to keep in sync with the encoders. Both passes are a single
linear walk over the image; on a 384x800 receipt the measuring one costs
between 0.5 µs and 0.35 ms depending on the format, against the seconds it
takes to actually push the job down a serial line.

While an encoder runs, that same `rcpt_buf` is its write cursor: `data`/`cap`
are the destination and `len` is how far along it is, so the measuring pass is
just a buffer with nothing allocated. Nothing in that path can fail, so the
write primitives return nothing and an encoder emits its twenty-odd calls in a
row without a single error check.
Streaming to a socket or a UART would need a callback sink instead, which is
deliberately not there: one finished block is simpler for every caller this
library has.

### Options

| Parameter | Meaning |
| --- | --- |
| `x`, `y` | position on the label, in dots |
| `chunk_rows` | split the raster into bands of N rows (0 = one block). Some low-cost printers have a small image buffer and drop tall blocks |
| `copies` | number of labels; 0 means one |
| `dpi` | resolution written into the CPCL `!` header; 0 means 203 |
| `mode` (ESC/P) | the `m` of `ESC * m`; `>= 32` selects a 24-pin head |
| `mode` (TSPL) | `BITMAP` combine mode: 0 OVERWRITE, 1 OR, 2 XOR |
| `flags` | `RCPT_F_RAW`, `RCPT_F_INVERT` |

`RCPT_F_RAW` emits only the graphics command — no `ESC @`, `CLS`, `^XA…^XZ`,
feed, cut or `PRINT` — which is what you want when embedding the image in a
larger job you build yourself.

`RCPT_F_INVERT` flips bit polarity relative to the format default. Needed if a
particular firmware disagrees with the convention below.

### What each format emits

| Format | Graphics command | Polarity | Wrapper (unless `RCPT_F_RAW`) |
| --- | --- | --- | --- |
| ESC/POS | `GS v 0 m xL xH yL yH` | 1 = black | `ESC @` … feed 3 lines, partial cut |
| ESC/P | `ESC * m nL nH` per 8/24-row band | 1 = fires pin | `ESC @`, `ESC 3 24` … `ESC 2`, form feed |
| TSPL | `BITMAP x,y,w,h,mode,<binary>` | **0 = black** | `CLS` … `PRINT 1,<copies>` |
| CPCL | `EG w h x y <hex>` | 1 = black | `! 0 dpi dpi height copies` … `FORM`, `PRINT` |
| ZPL | `^FOx,y^GFA,n,n,stride,<hex>^FS` | 1 = black | `^XA` … `^PQ<copies>`, `^XZ` |

Media setup (TSPL `SIZE`/`GAP`/`DENSITY`, ZPL `^PW`/`^LL`, ESC/POS code pages)
is deliberately not emitted — it depends on the installed stock, and belongs in
the job you send around the image.

## Building

```sh
make            # static lib + demo + tests
make test       # run the test suite
./build/demo    # writes demo.<fmt> for every format, prints an ASCII preview
```

### Style

C is formatted by `clang-format` (`.clang-format`: attached braces, 4-space
indent, 150 columns) and Python by `ruff` (`pyproject.toml`). Both run from one
place:

```sh
make fmt        # format in place
make lint       # check without touching anything
```

The same two tools are wired into `.pre-commit-config.yaml`; `uvx pre-commit
install` runs them on every commit.

### WebAssembly

Both WASM toolchains run in Docker, so nothing has to be installed locally:

```sh
make wasm         # both targets
make wasm-emcc    # -> build/emcc/receipet.js + .wasm   (emscripten/emsdk)
make wasm-wasi    # -> build/wasi/receipet.wasm         (ghcr.io/webassembly/wasi-sdk)
make wasm-test    # runs examples/node_demo.js and examples/wasi_demo.mjs
```

Set `DOCKER=` (empty) to use `emcc` / a local wasi-sdk from `PATH` instead, or
override `EMSDK_IMAGE` / `WASI_SDK_IMAGE` / `WASI_SDK`.

The WASI build is a **reactor** module: no `main`, the host calls `_initialize`
once and then the exports directly. `malloc` and `free` are exported alongside
the `rcpt_*` functions so the host can hand buffers in and out. The library
itself makes no WASI calls at all, so it also runs under a bare
`WebAssembly.instantiate` with stub imports.

Nothing is returned by value across the boundary, so there are no hidden
arguments to know about: `rcpt_enc_*` takes a pointer to 12 bytes — `data` at
offset 0, `cap` at 4, `len` at 8 — and returns a result code, and that same
pointer goes straight to `rcpt_buf_free`. `rcpt_image_new` works the same way
with a 4-byte slot.

See [examples/node_demo.js](examples/node_demo.js) (Emscripten) and
[examples/wasi_demo.mjs](examples/wasi_demo.mjs) (WASI) for the calling
convention.

### Python

Python 3.12 or newer.

```sh
uv sync                        # builds the extension, installs it editable
uv run python -c "import receipet"

# or, without uv:
pip install -e .
```

```python
from receipet import Image

img = Image(384, 120)
img.set_pixel(10, 10)
img.fill_rect(20, 20, 100, 40)

data = img.escpos(chunk_rows=64)          # -> bytes
label = img.zpl(x=40, y=100, copies=2)
```

One method per language, mirroring the C API — keyword arguments make the
per-format options read better here than a shared options object would.

`Image.from_pil(pil_image)` converts a PIL image, dithering anything that is not
already 1-bit — the usual way to get a logo onto a receipt.

The binding is CFFI **API mode**: the C sources are compiled into the
extension, so there is no shared library to locate at runtime. The `cdef`
block in `python/build_ffi.py` repeats the declarations from `receipet.h`
because CFFI has no C preprocessor; the build compares the two and fails on a
mismatch, since the compiler catches a wrong argument count but not a wrong
argument type.

## Layout

```
include/receipet.h    public API - eleven functions and two types
src/rcpt_image.c      image buffer + pixel ops
src/rcpt_buf.c        the byte buffer: alloc, free, verify, write primitives
src/rcpt_encode.c     the entry check every encoder makes
src/rcpt_internal.h   the buffer's cursor role, the two-pass contract
src/lang/*.c          one file per printer language: options, encoder, wrapper
tests/                self-contained test suite (no framework)
examples/             C, Node/Emscripten and WASI demos
python/               CFFI builder + package
```

Adding a language means adding one file to `src/lang/`: an emit function that
writes through a `rcpt_buf`, and a public wrapper that runs it twice:

```c
rcpt_result rcpt_enc_zpl(rcpt_buf *out, const rcpt_image *img,
                         int32_t x, int32_t y,
                         uint32_t copies, uint32_t flags)
{
    RCPT_ENCODE(img, x, y, copies, flags);
}
```

`RCPT_ENCODE()` expands to the whole body: run `emit()` once against an empty
buffer to measure, allocate exactly that, run it again to fill, and check that
the two agreed. Its arguments are `emit()`'s minus the buffer, written once —
the two runs must produce the same bytes, and `rcpt_buf_done()` is the check
for that. An encoder that is not deterministic comes back as
`RCPT_ERR_INTERNAL` rather than as a bad job.

Driving the runs from the wrapper rather than through a callback is what lets
each language take its own options as ordinary typed arguments instead of
packing them into a struct. There is no dispatch table to update; the build
globs `src/lang/`, so nothing outside that one file changes.

## Not implemented (on purpose, for now)

- ZPL `^GFA` run-length (ACS) compression — typically a 10x smaller payload on
  label artwork, and a self-contained addition to `src/lang/zpl.c`.
- ESC/POS "download bit image" / `ESC *` legacy modes, for printers too old for
  `GS v 0`.
- Drawing beyond single pixels (lines, text, blitting). A blit is where a
  combine mode would finally earn its parameter.
- Streaming output, as described above.
