"""A numbered ruler, for when you do not trust the printer's own geometry.

Every tick carries its own dot number and the ruler deliberately overshoots
the label, so the last number still legible on each axis *is* the printable
size - no assumption about dpi, orientation or margins.  Two readings settle
all three at once.

Built from TSPL's own BAR/TEXT, so it also tells you whether the printer is
obeying TSPL at all without involving any encoder.
"""

STEP = 40


def job(size_cmd=b"SIZE 80 mm,40 mm", gap_cmd=b"GAP 2 mm,0 mm", across=1040, down=600):
    cmds = [size_cmd, gap_cmd, b"DIRECTION 1", b"CLS"]

    for d in range(0, across + 1, STEP):  # along the head
        major = (d % (STEP * 5)) == 0
        cmds.append(b"BAR %d,0,3,%d" % (d, 40 if major else 20))
        if major:
            cmds.append(b'TEXT %d,44,"1",0,1,1,"%d"' % (d + 2, d))

    for d in range(0, down + 1, STEP):  # along the feed
        major = (d % (STEP * 5)) == 0
        cmds.append(b"BAR 0,%d,%d,3" % (d, 40 if major else 20))
        if major and d:
            cmds.append(b'TEXT 44,%d,"1",0,1,1,"%d"' % (d + 2, d))

    cmds += [b'TEXT 200,200,"3",0,1,1,"READ LAST NUMBER"', b"PRINT 1,1"]
    return b"\r\n".join(cmds) + b"\r\n"


if __name__ == "__main__":
    import sys

    import usbio

    _, ep_out, _ = usbio.open_dev(int(sys.argv[1], 16), int(sys.argv[2], 16))
    data = job()
    usbio.write(ep_out, data)
    print(f"sent {len(data)} bytes; read the last legible number on each axis")
