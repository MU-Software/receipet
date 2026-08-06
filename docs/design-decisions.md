# How the API got to thirteen functions

The first draft had a writer object, an options struct, a format registry and
a dispatch function.  Everything below was removed, and the reason each one
went is worth keeping — most of them are the sort of thing that grows back.

## Removed, and why

| Removed | Reason |
| --- | --- |
| `rcpt_image_fill_rect` | Drawing belongs above the buffer. It survives as a Python example, to show what that layer looks like |
| `rcpt_format_name` | A name lookup nobody needed |
| `rcpt_image_bufsize` | The caller never had to size the buffer once the image owned its allocation |
| `rcpt_image_init(mem, mem_size)` | Caller-supplied memory, dropped once it was clear `malloc` is available on ESP32 and RP2040 too. Allocate once at startup, `rcpt_image_clear()` between jobs |
| `rcpt_image_set_pixel` (the old one) | Merged with `rcpt_image_op_pixel`, which then took its name |
| `rcpt_writer_init`, `rcpt_writer_init_mem`, `rcpt_opts_init` | Unnecessary once encoders return heap buffers |
| `rcpt_opts` | Each encoder takes its own language's options as ordinary typed arguments. No struct to keep in sync, no fields that mean nothing for four of the six formats |
| `rcpt_encode(format, ...)` | Replaced by six named functions. A dispatch table is the caller's business if they want one; the test suite builds one to prove it stays possible |
| `rcpt_build` | Its only useful member was the pass counter, so the function returns that instead |
| `rcpt_writer` / `rcpt_job` | Merged into one `rcpt_buf`. It is the write cursor during encoding and the result afterwards |

`rcpt_buf` exposing `cap` as well as `len` is deliberate: a binding can read
`min(len, cap)` and be safe even if a future bug lets `len` run past the
allocation.

## Result codes

Settled as a policy rather than case by case:

- Anything that can fail returns `rcpt_result` and delivers its value through
  an out-parameter — a plain pointer for a value, a double pointer for
  something allocated.
- Anything that cannot fail returns nothing, or the value asked for.
- **Out-of-range pixel coordinates are not a failure.** They are clipped.

The last one was argued explicitly.  `rcpt_image_set_pixel(img, -1, 0, 1)`
*could* return an error, but no caller can do anything useful with it, and an
error nobody acts on trains people to ignore the ones that matter.  Clipping
is also what makes the padding-bit invariant hold: bits past `width` are
always zero, because the buffer starts blank and nothing can write them.

The enum is named, and every function that returns one says `rcpt_result` in
its signature rather than `int`, so the contract is visible at the call site.

`rcpt_buf_pass` used to return 0 or 1, which read as a boolean and was not
one.  Splitting it into `rcpt_buf_alloc` and `rcpt_buf_done` removed the
question.

## The two-run contract

Each encoder runs its `emit()` twice: once against an empty buffer to
*measure*, then once more to *fill* an allocation of exactly that size.  Write
primitives count bytes even when there is nowhere to put them, which is what
makes the measuring run work.

This was questioned — is running twice really necessary?  It is not the
fastest option, but the alternatives all reintroduce either a growable buffer
or a caller-supplied one, and the library's stated priority is simplicity over
speed.  384 × 200 per-pixel fills take about 9 ms, which is fine.

`rcpt_buf_done()` checks that the two runs agreed.  An encoder that is not
deterministic comes back as `RCPT_ERR_INTERNAL` with the buffer dropped,
rather than as a corrupt job.  This was verified by deliberately sabotaging
`zpl.c` to emit different bytes on the second pass, and confirming the failure
path fires.

## `RCPT_ENCODE`

The six wrappers were nearly identical, so the whole body is a macro:

```c
rcpt_result rcpt_enc_zpl(rcpt_buf *out, const rcpt_image *img,
                         int32_t x, int32_t y, uint32_t copies, uint32_t flags)
{
    RCPT_ENCODE(img, x, y, copies, flags);
}
```

It expects, and cannot check, three things: parameters named `out` and `img`,
a static `emit()` in the same file, and side-effect-free arguments — they are
expanded twice.  Accepted as a reasonable trade for how small it keeps each
language file.  Adding a language means adding one file to `src/lang/`; the
build globs the directory, so nothing else changes.

## Signature changes made later

`chunk_rows` was added to `rcpt_enc_tspl` after the 4B-3044TA was found to
discard oversized `BITMAP` commands — see [tspl.md](tspl.md).  It mirrors
`rcpt_enc_escpos` exactly, in name and behaviour, and 0 keeps the old
single-command output.

## Style

Attached braces (1TBS), 4-space indent, 150-column ceiling for code, prose
wrapped near 80 by hand.  Comments minimal, short, and only where the code
cannot say it — keep the "why" (TSPL's inverted polarity, the two-run
contract), drop anything that restates the line below it.

`.clang-format` and `ruff` enforce what they can, run from `make fmt` /
`make lint` and from `.pre-commit-config.yaml`.

## Bugs the tests caught

Worth listing, because each one is a category:

- A test helper called `take(rcpt_enc_x(&job, ...), job, ...)`, relying on
  **unspecified argument evaluation order** in C. Split into two statements.
- The header promised `*out` is always written, but the early
  `rcpt_image_ok` failure path did not write it. Folded into
  `rcpt_enc_start(out, img)`, with a poisoned-out-parameter test to keep it
  honest.
- The ESC/P test expected a 4-byte header; `ESC * m nL nH` is five.
- A Brother test buffer of 256 bytes silently truncated a 267-byte job.
- A rename script produced `rcpt_w_bytes(b, &b, 1)` — the buffer where the
  value belonged — and a shadowed `b` in `rcpt_w_hex`. Both caught by the
  build and the suite.
