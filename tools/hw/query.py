"""Ask a label printer what it is, without printing.

Firmware varies wildly in how much it will answer - the HPRT SL43 ignores
nearly everything, while the 4BARCODE 4B-3044TA answers TSPL *and* ZPL
queries, including the full `^HH` configuration dump that carries its
resolution.

A query is not always free.  Two are known to change what the printer does
afterwards:

- `~!D` puts TSPL firmware into hex dump mode, after which it prints every
  byte it receives instead of obeying it.  It is not in either list below.
- The ZPL set can leave a dual-language printer in ZPL mode, so a TSPL job
  sent next is parsed as garbage.  On a 4B-3044TA that wedged the firmware
  badly enough to drop it off the USB bus until it was power cycled.

So the ZPL queries are opt-in, and after using them, power cycle before
sending a TSPL job.
"""

import time

import usb.core
import usbio

TSPL = [
    ("TSPL ~!I  code page/country", b"~!I\r\n"),
    ("TSPL ~!A  free memory", b"~!A\r\n"),
    ("TSPL ~!F  file list", b"~!F\r\n"),
    ("TSPL ESC!? status", b"\x1b!?"),
]
ZPL = [
    ("ZPL  ~HS  host status", b"~HS"),
    ("ZPL  ^HH  configuration", b"^XA^HH^XZ"),  # states the resolution
]


def ask(ep_in, ep_out, query, cap=8192, settle=0.4):
    """Write a query, then read until the printer stops talking."""
    usbio.write(ep_out, query)
    time.sleep(settle)
    buf = bytearray()
    while len(buf) < cap:
        try:
            chunk = ep_in.read(4096, timeout=1200)
        except usb.core.USBError:  # timeout, i.e. it has said all it will say
            break
        if not len(chunk):
            break
        buf += bytes(chunk)
    return bytes(buf)


def probe(vid, pid, zpl=False):
    _, ep_out, ep_in = usbio.open_dev(vid, pid)
    for name, q in TSPL + (ZPL if zpl else []):
        reply = ask(ep_in, ep_out, q)
        print(f"--- {name}: {len(reply)} bytes")
        if reply:
            print(reply.decode("latin-1"))
    if zpl:
        print("\n*** ZPL queries were sent - power cycle before printing TSPL ***")


if __name__ == "__main__":
    import sys

    probe(int(sys.argv[1], 16), int(sys.argv[2], 16), zpl="--zpl" in sys.argv)
