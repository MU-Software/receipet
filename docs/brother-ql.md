# Brother QL-820NWB

Verified 2026-08-06 on a QL-820NWB with 38 × 90 mm die-cut media, USB
`04f9:209d`, model code `0x41`.

Working settings, for the impatient:

| | |
| --- | --- |
| encoder | `rcpt_enc_brother`, raster mode |
| image | **413 × 991 dots** for 38 × 90 mm — the *printable* size |
| `offset` | **295** |
| media | `RCPT_BROTHER_MEDIA_DIE_CUT`, `width_mm` 38, `length_mm` 90 |
| grid | 300 dpi both axes; 100 raster lines = 8.5 mm, 40 dots = 3.4 mm |

## ESC/P mode was abandoned

The first implementation used the QL's ESC/P mode, following the user's
existing ROSA (Python) and ROSA_V2 (TypeScript) code.  It never worked:

1. `mode 33` printed the image across three labels at the wrong scale.
2. Guessing `mode=40` with `ESC 3 12` line spacing: three labels again.
3. After being asked whether the ROSA_V2 *call site* had been checked — it had
   not — it turned out the caller passed `{ mode: 39 }`, and that both ROSA
   implementations end a band with `LF` alone where this encoder emitted
   `CR LF`.  Fixing both made the output byte-identical to the known-good
   implementation, 593 → 590 bytes, the difference being exactly one `CR` per
   band.
4. Still three labels.

So the byte stream matched a working implementation and the printer still
misbehaved.  ESC/P mode on a QL-820NWB does not scale correctly however the
density and line spacing are set, and a stray `CR` before the band `LF` makes
it feed twice.  The path was removed.  Raster mode worked on the first
attempt and lands on a 300 dpi grid exactly.

`rcpt_enc_escp` still exists, for actual dot matrix printers.  It is not for
this device.

## Three things raster mode gets wrong if you trust the spec

### The image must be the printable size, not the media size

38 × 90 mm media measures 449 × 1061 dots.  The *printable* area is
**413 × 991**.  Die-cut labels lose about 3 mm across and 6 mm along the feed,
consistently — it holds for 17 × 54, 29 × 42, 29 × 90 and 62 × 100 as well.

Sending the media size overran the label: the print started ~2 mm in, the tail
was lost, and the cut landed on the next label.  72 lines of overrun is 6.1 mm.

The per-label numbers are in [`brother_ql`](https://github.com/pklaus/brother_ql)'s
label table, which is where these came from.

### The raster line runs right to left

A raster line is the width of the print head, not of the media: 90 bytes,
720 dots, on every QL from the 500 to the 820NWB — 162 bytes on the QL-1050
and QL-1060N, which this encoder does not support.  Bit 7 of byte 0 is the
**rightmost** dot of the head, not the leftmost.  Probed directly, one black
pixel at a time:

    image x=  0  ->  g-line dot 424
    image x=412  ->  g-line dot 12

Everything printed before this was discovered was mirrored.  Nobody noticed,
because every test pattern used until then was left-right symmetric.

`rcpt_enc_brother` still takes `offset` from the left, like every other
encoder here, and flips internally.  For die-cut media:

    offset = 720 - image width - offset_r

`offset_r` is the label's right margin: 12 dots for 38 × 90, giving 295.

### Dot 0 is not the left edge of the media

An earlier version of this documentation said it was.  It is not; see above.

## Verified against brother_ql

With both fixes in, receipet and `brother_ql` produce **identical raster
data** for the same image — 92,164 bytes, byte for byte.

The headers differ in two deliberate ways:

- `brother_ql` emits `ESC i S` inside the job.  receipet does not: it would
  make the printer send a 32-byte status reply mid-job that the caller must
  drain, which is a surprising thing for a function that just returns bytes.
- receipet emits `M 00` (no compression) explicitly; `brother_ql` omits it
  when not compressing.  Explicit is harmless and already proven on hardware.

## Status replies

`ESC i S` (`RCPT_BROTHER_STATUS_REQUEST`) returns exactly 32 bytes with no
framing, so the length is how you know the reply is complete.

`errors` packs error information 1 into the low byte and 2 into the high byte.
Confirmed on hardware: with the cover open,

    byte 8 (err 1): 0x00
    byte 9 (err 2): 0x10        -> errors = 0x1000 = RCPT_BROTHER_ERR_COVER_OPEN
    status_type   : 0x02        (error occurred)

and 0x0000 with it closed.

There are two ways to read the parse, and they are the same parse:
`rcpt_parse_brother_status()` fills the struct, and
`rcpt_parse_brother_status_u32()` writes the same fields as
`RCPT_BROTHER_STATUS_FIELDS` uint32 values in declaration order.  The second
exists for bindings that cannot see a struct layout — the JS port would
otherwise need nine hand-written offsets and a mask on every 8-bit read.  It is
a wrapper, so the byte offsets above are still written down once; the test
suite checks the two against each other field by field, since a swapped pair
would otherwise surface as a plausible wrong number at a caller.

**ROSA's `escp.py` parser is broken** and should not be copied.  It reads the
two bytes big-endian, so `data[8]` lands in the high byte, while its own flag
table assumes the opposite — an open cover reads as 0x0010, which its table
calls "Printer in use".  It also tests `int_data & (1 << bit)` where `bit` is
already the mask, so `1 << 4096` for cover open; every flag is effectively
always false.

## macOS: `ippusbd`

macOS grabs the QL as root the moment it enumerates, and libusb cannot claim
it.  Removing the CUPS queue is not enough.

    sudo killall ippusbd

then claim **without resetting** — a reset re-enumerates the device and
launchd starts the daemon again.  After claiming, force alt setting 0
(`tools/hw/usbio.py` does this).

## Known limitations

The head width is hardcoded at 720 dots (`RASTER_BYTES 90`), which covers the
QL-500 through the QL-820NWB.  The QL-1050 and QL-1060N have a 162-byte head
and would print wrongly.  Making it a parameter was considered and deferred:
there is no such printer here to verify against, and after this session the
bar for adding an untestable path is higher than it was.  `head_dots` as a
plain argument defaulting to 720 is the shape to reach for when one turns up —
`offset` then generalises to `head_dots - image width - offset_r`.

## Still unverified

Continuous tape (only die-cut was tested), `RCPT_F_BROTHER_NO_CUT`, and every
error bit except cover-open.
