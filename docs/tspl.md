# TSPL label printers

Two devices, verified 2026-08-06.  One was straightforward; the other cost
most of a session and produced the two nastiest findings in this repository.

| | HPRT SL43 | 4BARCODE 4B-3044TA |
| --- | --- | --- |
| USB | `2aaf:9006` | `2d84:637b` **or** `0416:5011` |
| media tested | 80 × 40 mm | 80 × 40 mm |
| dots | 640 × 320 | ~620 × 310 printable of 640 × 320 |
| resolution | 203 dpi | 203 dpi, whatever it claims |
| `chunk_rows` | not needed | **required** |
| queries | answers almost nothing | answers a great deal, some of it wrong |

Both need `SIZE` / `GAP` / `DIRECTION` sent by the caller; receipet
deliberately does not emit them, because they depend on the installed stock.

## HPRT SL43

Worked at `x = 0`, 640 × 320.  The one hiccup was a ~3 mm rightward offset on
the first print, which turned out to be the label sitting off-centre in the
printer rather than anything in the job — with the stock seated properly and
`x = 0`, it was correct.

Status queries mostly go unanswered (`~!@`, `~HS`).  That is normal for cheap
firmware and is not a fault.

## 4BARCODE 4B-3044TA

### It has two USB identities

Normally `2d84:637b` "4B-3044TA", manufacturer 4BARCODE, serial `TRH5BC1C0`.

After the firmware crash described below it came back as `0416:5011` "POS
Label Printer", manufacturer STMicroelectronics, serial `Printer` — which
answers ESC/POS `GS I 67` with:

    MODEL:GP-9025T

That is the Gainscha unit 4BARCODE rebrands.  It accepts TSPL under either
identity, so match on both ids.

### It lies about its resolution

`^XA^HH^XZ` returns a full Zebra-style configuration dump, including:

    1198 DOTS           PRINT WIDTH
    0749                LABEL LENGTH
    1296 11.8/MM FULL   RESOLUTION

11.8 dots/mm is 300 dpi.  **The head is 203 dpi.**  The figure belongs to its
ZPL emulation profile.

Believing it produced a run of bad labels, and the mistake was compounded by
guessing two other unknowns at the same time — which way round the label sits,
and how much of it is printable.  What finally settled all three in one label
was a ruler that numbers every tick with its own dot number and overshoots the
label deliberately, so the last legible number *is* the answer
(`tools/hw/ruler.py`):

    horizontal: cut at ~620 dots       80 mm -> 8 dots/mm
    vertical:   cut at ~310 dots       40 mm -> 8 dots/mm

Two independent axes, same answer.  The printable area is also slightly inside
the media — 620 of 640 across, 310 of 320 down — which explains why an earlier
attempt at exactly 640 × 320 lost its top border entirely, showed half the
bottom border, and dropped the right border.  Three symptoms, one cause.

### Orientation

TSPL `SIZE` is *width,length*: width across the head, length along the feed.
"40 × 80 mm" stock here is 80 mm across the head and 40 mm of feed, so
`SIZE 80 mm,40 mm`.  Getting it backwards printed rotated *and* fed two labels
per job, since the declared 80 mm length is two 40 mm labels.

### It silently drops a large BITMAP

The finding that took longest.  A `BITMAP` command whose payload exceeds
roughly **16 kB** is discarded: blank label, no error, no status change,
printer still healthy.

Measured:

| bitmap | payload | result |
| --- | --- | --- |
| 64 × 64 | 512 B | prints |
| 472 × 200 | 11.8 kB | prints |
| 600 × 300 | 22.5 kB | **blank** |
| 472 × 472 | 27.8 kB | **blank** |
| 472 × 945 | 55.8 kB | **blank** |

It is not a dimension limit.  A 64 × 288 bitmap (tall, narrow) and a 600 × 32
bitmap (short, wide) — 2.3 kB and 2.4 kB — both render, so neither height nor
width is capped.  Only bytes.

And the cap is **per `BITMAP` command**, not per job: the same 600 × 300 image
split into three 600 × 100 bands, in a job of 22,632 bytes — 37 bytes *larger*
than the single-bitmap job that failed — prints correctly.

That is what `chunk_rows` on `rcpt_enc_tspl` is for.  Each band carries its own
`y`, and a single `PRINT` follows, so the result is still one label.

### Never diagnose this the way it was diagnosed here

Five labels went to guessing before the useful test was built: numbered
markers *around* two small bitmaps in one job, so that which markers appeared
distinguished "the parser died" from "the command was ignored" from "the job
was rejected".  All four markers and both bitmaps printed, which cleared the
encoder and pointed straight at size.  See [method.md](method.md).

## Probing changed the printer's language mode

The sequence that broke it:

1. `query.py` sent the TSPL queries — all answered normally.
2. It then sent ZPL `~HS` and `^XA^HH^XZ` — also answered.
3. A 55 kB TSPL job followed immediately.
4. Nothing printed, and the device vanished from the USB bus.  A power cycle
   alone did not bring it back; it needed a replug too.

The same 55 kB job went through fine later, in a session where no ZPL had been
sent — so it was the language mode, not the size.  `query.py` now keeps the
ZPL queries behind `--zpl` and prints a warning to power cycle before printing.

Never send `~!D` to any of these: it enters hex dump mode, after which the
printer prints every byte it receives rather than obeying it.
