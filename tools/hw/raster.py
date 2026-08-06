"""Brother QL raster mode, prototyped here before it goes into the library."""

RASTER_BYTES = 90  # 720 dots, fixed for the QL series


def job(lines, media_type, width_mm, length_mm, autocut=True, margin=0):
    """lines: list of RASTER_BYTES-long bytes objects, 1 = black."""
    n = len(lines)
    out = bytearray(b"\x00" * 200)  # invalidate
    out += b"\x1b\x40"  # initialize
    out += b"\x1b\x69\x61\x01"  # raster mode
    out += b"\x1b\x69\x7a" + bytes(
        [
            0x02 | 0x04 | 0x08 | 0x40 | 0x80,  # media type / width / length / quality / recovery
            media_type,
            width_mm,
            length_mm,
            n & 0xFF,
            (n >> 8) & 0xFF,
            (n >> 16) & 0xFF,
            (n >> 24) & 0xFF,
            0,
            0,  # first page
        ]
    )
    out += b"\x1b\x69\x4d" + bytes([0x40 if autocut else 0x00])  # various mode
    out += b"\x1b\x69\x41" + bytes([1])  # cut every label
    out += b"\x1b\x69\x4b" + bytes([0x08])  # cut at end
    out += b"\x1b\x69\x64" + bytes([margin & 0xFF, margin >> 8])  # feed margin
    out += b"\x4d\x00"  # no compression
    for ln in lines:
        assert len(ln) == RASTER_BYTES
        out += b"\x67\x00" + bytes([RASTER_BYTES]) + ln
    out += b"\x1a"  # print and feed
    return bytes(out)


def comb(n_lines, tick=40, major=200, rule_every=100):
    """A comb across all 720 dots plus horizontal rules, to find the label."""
    lines = []
    for y in range(n_lines):
        row = bytearray(RASTER_BYTES)
        rule = (y % rule_every) < 3
        for d in range(720):
            on = rule or (d % tick) < 2 or (d % major) < 6
            if on:
                row[d >> 3] |= 0x80 >> (d & 7)
        lines.append(bytes(row))
    return lines
