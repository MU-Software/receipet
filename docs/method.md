# Working out an unfamiliar printer

Every real bug in this library was found on paper, and none of them would have
been found by reading a specification.  The list below is what that cost, in
the order the lessons were learned.

## Read the working implementation, including its call sites

The Brother work started from the user's own ROSA and ROSA_V2 code.  Skimming
the module and taking its defaults produced two rounds of wrong output.  Both
answers were in the code:

- the caller passed `{ mode: 39 }`, not the module's default
- both implementations ended each band with `LF` alone, where the spec reading
  said `CR LF`

A protocol spec describes a family of printers.  Working code describes *the*
printer.  Grep for the call site, not just the function.

## Diff against a reference implementation before sending anything

`brother_ql` is a widely used Python implementation of the same protocol.
Generating the same image through both and comparing bytes took a few minutes,
cost nothing, and immediately surfaced two facts no amount of squinting at the
spec had:

- the raster line runs right to left
- the image must be the label's *printable* size, not the media size

Once fixed, all 92,164 raster bytes matched.  The remaining differences were
two deliberate choices, which is exactly the outcome you want from a diff: a
short list you can explain.

Do this before the first print, not after the third wasted label.

## The printer's own answer about itself may be wrong

`tools/hw/query.py` asks a printer what it is.  The 4B-3044TA answers
generously — TSPL `~!I`/`~!A`/`~!F` and ZPL `~HS`/`^HH` — and its `^HH`
configuration dump states:

    1296 11.8/MM FULL   RESOLUTION

The head is 203 dpi.  The figure describes its ZPL emulation profile, not the
hardware.  Taking it at face value and printing immediately produced several
useless labels.

Query first — it is free and it narrows the search.  Then verify on paper.

## When more than one thing is unknown, print a ruler

The 4B had three unknowns at once: resolution, which edge of the label faces
the head, and how much of the label is actually printable.  Guessing one at a
time, with a label burned per guess, went nowhere for several rounds.

What worked in a single label: a ruler that **numbers every tick with its own
dot number** and deliberately overshoots the label.  Whatever the truth is,
the last number still legible on each axis *is* the printable size.  No
assumption about dpi, orientation or margins survives contact with it.

`tools/hw/ruler.py`.  Reach for it early — it is one label instead of five.

## Failures are often silent

Three separate faults here produced no error at all:

- the 4B drops a `BITMAP` over roughly 16 kB and prints a blank label; no
  error code, no status change
- the Brother accepted an over-long raster job and simply overran the label,
  cutting into the next one
- the ZPL probe left the printer in a mode where a TSPL job was parsed as
  garbage, printed nothing, and hung the firmware off the USB bus

So "nothing came out" is not one symptom, it is a class.  Distinguish the
cases by construction: put numbered markers *around* the suspect command in a
single job, and which markers appear tells you whether the parser died, the
command was ignored, or the whole job was rejected.  That test settled the
`BITMAP` question in one label after five had been wasted guessing.

## Change one variable, and make the test discriminate

The `BITMAP` limit could have been a height cap, a width cap or a byte cap,
and the three need different fixes.  One label carrying a 64 × 288 bitmap and
a 600 × 32 bitmap — nearly equal in bytes, opposite in shape — ruled out two
of them at once.

Similarly, banding the image was both the candidate fix *and* the test of
where the cap sits: keeping the total job size the same as the one that had
failed meant that if it printed, the cap was per command rather than per job.
Design the experiment so that either outcome is informative.

## A query is not always free

Two are known to change what the printer does afterwards:

- `~!D` puts TSPL firmware into hex dump mode, after which it prints every
  byte it receives instead of obeying it.  Never send it.
- the ZPL query set can leave a dual-language printer in ZPL mode.  On the
  4B-3044TA, the TSPL job that followed hung the firmware badly enough that
  the device dropped off the USB bus and needed a power cycle *and* a replug.

`query.py` keeps the ZPL set behind `--zpl` for that reason.  If you use it,
power cycle before printing.

## Make the test pattern honest

An early test card drew its diagonals by stepping along the *short* axis, so
on a tall label the line came out as scattered dots and read as faint output.
The printer was fine; the pattern was wrong.  A test that can fail for its own
reasons will waste your time twice — once when you chase the phantom, and again
when you stop trusting the pattern that was telling the truth.

Step the long axis, give lines a couple of pixels of thickness, and put
something on the card that makes clipping obvious — corner marks, a frame,
ruler ticks with numbers.

## What "passes the tests" is worth

`rcpt_enc_cpcl`, `rcpt_enc_zpl` and `rcpt_enc_escp` pass the suite and match
the spec as read.  So did the Brother encoder, which was mirrored left to
right; the symmetric test patterns never showed it.  So did the TSPL encoder,
which was fine — but the printer silently discarded its output above a size
nothing documents.

Unit tests hold an encoder to what its author believed.  Only paper holds it
to the printer.
