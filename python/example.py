"""Small usage example / smoke test for the Python binding.

python python/build_ffi.py && cd python && python example.py
"""

from receipet import RAW, Image

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
]:
    print(f"{name:<7} {len(data):>6} bytes  {data[:16].hex()}...")

# RAW drops the job wrapper, leaving just the graphics command.
print(img.zpl(x=40, y=100, flags=RAW)[:32].decode("ascii"))
