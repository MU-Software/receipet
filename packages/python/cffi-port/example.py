"""Small usage example / smoke test for the Python binding.

make python-test          (or: uv run python python/example.py)
"""

from receipet import BROTHER_STATUS_REQUEST, RAW, BrotherStatus, Image

W, H = 64, 32

img = Image(W, H)
img.fill_rect(0, 0, W, 2)  # top rule
for i in range(H):
    img.set_pixel(i * 2, i, 1)  # diagonal
for y in range(8, 24):  # invert a square, the caller's way
    for x in range(8, 24):
        img.set_pixel(x, y, not img.get_pixel(x, y))

for y in range(0, H, 2):
    print("".join("#" if img.get_pixel(x, y) else "." for x in range(W)))

# One method per language, each taking that language's own options.  The C
# result codes surface as ValueError / MemoryError / RuntimeError.
for name, data in [
    ("ESCPOS", img.escpos(chunk_rows=16)),
    ("ESCP", img.escp(mode=0)),
    ("TSPL", img.tspl(x=0, y=0)),
    ("CPCL", img.cpcl(dpi=203)),
    ("ZPL", img.zpl(copies=1)),
    ("BROTHER", img.brother(width_mm=38, length_mm=90, offset=295)),
]:
    print(f"{name:<7} {len(data):>6} bytes  {data[:16].hex()}...")

# RAW drops the job wrapper, leaving just the graphics command.
print(img.zpl(x=40, y=100, flags=RAW)[:32].decode("ascii"))

# A Brother QL answers BROTHER_STATUS_REQUEST with exactly 32 bytes.  This is
# one such reply, built by hand: a QL-820NWB with 38x90 die-cut and the cover
# open.  On a real device: write BROTHER_STATUS_REQUEST, read exactly 32 back.
assert BROTHER_STATUS_REQUEST == b"\x1b\x69\x53"
reply = bytearray(32)
reply[0:5] = b"\x80\x20\x42\x34\x41"  # fixed header, then model code
reply[9] = 0x10  # error information 2, bit 4
reply[10] = 38  # media width, mm
reply[11] = 0x0B  # die-cut label
reply[17] = 90  # media length, mm
reply[18] = 0x02  # status type: error occurred

st = BrotherStatus(bytes(reply))
print(f"status  {st['media_width_mm']}x{st['media_length_mm']}mm  errors={st['error_names']}")
