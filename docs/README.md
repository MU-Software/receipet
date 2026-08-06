# Working notes

What went wrong while building receipet, and what it turned out to be.  The
main [README](../README.md) says how the library works today; these files say
why, and record the dead ends so nobody walks back into them.

Written after the hardware sessions of 2026-08-06.  Everything attributed to a
printer here was measured on that printer, not read out of a manual — several
of the entries below exist precisely because the manual and the hardware
disagreed.

| | |
| --- | --- |
| [method.md](method.md) | How to work out an unfamiliar printer without burning a roll of labels. The most transferable file here |
| [brother-ql.md](brother-ql.md) | QL-820NWB: why ESC/P was abandoned, and the three things raster mode gets wrong if you trust the spec |
| [tspl.md](tspl.md) | SL43 and 4B-3044TA: a printer that lies about its resolution, and a silent 16 kB limit |
| [escpos.md](escpos.md) | PT-390: the one that mostly just worked |
| [design-decisions.md](design-decisions.md) | How the API shrank to twelve functions, and what each removal cost |
| [packaging.md](packaging.md) | Python packaging and WASM build dead ends |

## Status

Proven on paper, on real hardware:

- `rcpt_enc_escpos` — GOOJPRT PT-390
- `rcpt_enc_tspl` — HPRT SL43, 4BARCODE 4B-3044TA
- `rcpt_enc_brother` — Brother QL-820NWB, plus `rcpt_parse_brother_status`

Never sent to a printer, because no device here speaks them: `rcpt_enc_cpcl`,
`rcpt_enc_zpl`, `rcpt_enc_escp`.  They pass the test suite and match the spec
as read.  That is a weaker claim than it sounds — see
[method.md](method.md) — so treat them as unproven.  The three verified
encoders are the ones in use for now.
