# ESC/POS — GOOJPRT PT-390

Verified 2026-08-06.  USB `0483:5720`, which enumerates as manufacturer
`GEZHI`, product `micro-printer`.  Bought from AliExpress; the protocol notes
were assembled during that session from live USB traffic and the printer's
companion app.

The least troublesome of the four.

| | |
| --- | --- |
| encoder | `rcpt_enc_escpos` |
| width | 576 dots, 8 dots/mm |
| flags | `RCPT_F_RAW` — it has no cutter, so add your own feed |
| `chunk_rows` | 64 worked; the full-label Hangul card was 576 × 1040 |
| ASB | `14 00 00 0F` |

## It must be in ESC/POS mode

The PT-390 has two command modes.  `cmdMode` must be `0x01`; read it with
`ESC##JXIG`.  In TSPL mode it caps a label at 16 kB, which is a different
and worse problem — and, as it happens, the same figure that bit the
4B-3044TA in [tspl.md](tspl.md).

## No cutter

`rcpt_enc_escpos` with default flags ends the job with `ESC d 3` and
`GS V 66 0`, a partial cut.  This printer has no cutter, so build with
`RCPT_F_RAW` and append your own feed:

```python
job = b"\x1b\x40" + img.escpos(chunk_rows=64, flags=RAW) + b"\x1b\x64\x08"
```

A consequence worth recording: the `GS V` cut path in the encoder has
therefore **never run on hardware**.

## Label size cannot be read back

Unlike the Brother, this printer will not tell you what media is loaded.  The
130 × 76 mm stock used for testing had to be measured by hand.

## Minor

An early test showed the printed length running very slightly over 10 mm where
10 mm was expected — small enough that the user judged it negligible, and it
was not pursued.
