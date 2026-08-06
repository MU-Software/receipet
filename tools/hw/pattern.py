"""Full-label test pattern built with the receipet Python binding."""

from receipet import Image


def build(w, h, dpmm=8):
    img = Image(w, h)
    b = 3
    img.fill_rect(0, 0, w, b)  # frame
    img.fill_rect(0, h - b, w, b)
    img.fill_rect(0, 0, b, h)
    img.fill_rect(w - b, 0, b, h)

    # diagonals: step along the longer axis, or the line comes out dotted
    n = max(w, h)
    for i in range(n):
        x = i * (w - 1) // (n - 1)
        y = i * (h - 1) // (n - 1)
        img.fill_rect(x, y, 2, 2)  # 2px, so it survives dithering
        img.fill_rect(w - 1 - x, y, 2, 2)

    # ruler ticks every 10 mm: long on the left edge, short on the top edge
    for mm in range(0, int(max(w, h) / dpmm) + 1, 10):
        d = int(mm * dpmm)
        if d < h:
            img.fill_rect(b, d, 40 if mm % 50 == 0 else 24, 3)
        if d < w:
            img.fill_rect(d, b, 3, 40 if mm % 50 == 0 else 24)

    # solid block and checkerboard, side by side in the middle
    bw, bh = w // 4, h // 6
    x0, y0 = w // 8, h // 2 - bh // 2
    img.fill_rect(x0, y0, bw, bh)
    for y in range(y0, y0 + bh):
        for x in range(w - x0 - bw, w - x0):
            img.set_pixel(x, y, (x + y) & 1)

    # corner L marks, so a clipped edge is obvious
    for cx, cy, sx, sy in ((0, 0, 1, 1), (w - 1, 0, -1, 1), (0, h - 1, 1, -1), (w - 1, h - 1, -1, -1)):
        for k in range(60):
            for t in range(6):
                img.set_pixel(cx + sx * k, cy + sy * t)
                img.set_pixel(cx + sx * t, cy + sy * k)
    return img
