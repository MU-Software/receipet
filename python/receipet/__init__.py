"""Monochrome image buffer with printer-language output.

    img = Image(384, 120)
    img.set_pixel(10, 10, 1)
    data = img.escpos()       # bytes, ready for the device

One method per printer language, taking that language's own options.
fill_rect() and from_pil() show what a drawing layer looks like: per-pixel
calls, never reaching into the packed buffer.
"""

from ._receipet import ffi, lib

__all__ = ["INVERT", "RAW", "Image"]

RAW = 1 << 0  # emit only the graphics command, no job wrapper
INVERT = 1 << 1  # flip bit polarity relative to the format default

_ERRORS = {
    lib.RCPT_ERR_ARG: (ValueError, "invalid argument"),
    lib.RCPT_ERR_NOMEM: (MemoryError, "out of memory"),
    lib.RCPT_ERR_INTERNAL: (RuntimeError, "internal error - please report this"),
}


def _check(rc, what):
    """Turn a rcpt_result into the matching Python exception."""
    if rc == lib.RCPT_OK:
        return
    cls, msg = _ERRORS.get(rc, (RuntimeError, f"unknown error {rc}"))
    raise cls(f"{what}: {msg}")


class Image:
    """A 1-bit-per-pixel buffer; 1 means black (ink)."""

    def __init__(self, width, height):
        out = ffi.new("rcpt_image **")
        _check(lib.rcpt_image_new(out, width, height), f"image size {width!r}x{height!r}")
        # ffi.gc ties rcpt_image_free to this object's lifetime.
        self._img = ffi.gc(out[0], lib.rcpt_image_free)

    @property
    def width(self):
        return self._img.width

    @property
    def height(self):
        return self._img.height

    def clear(self):
        lib.rcpt_image_clear(self._img)

    def set_pixel(self, x, y, value=1):
        lib.rcpt_image_set_pixel(self._img, x, y, 1 if value else 0)

    def get_pixel(self, x, y):
        return lib.rcpt_image_get_pixel(self._img, x, y)

    def fill_rect(self, x, y, w, h, value=1):
        """Clipped rectangle, one pixel at a time."""
        for yy in range(max(y, 0), min(y + h, self.height)):
            for xx in range(max(x, 0), min(x + w, self.width)):
                lib.rcpt_image_set_pixel(self._img, xx, yy, 1 if value else 0)

    @classmethod
    def from_pil(cls, im):
        """Build from a PIL image; anything not 1-bit is dithered."""
        if im.mode != "1":
            im = im.convert("1")
        img = cls(im.width, im.height)
        # PIL's "1" mode uses 255 for white, the opposite of ours.
        for i, px in enumerate(im.getdata()):
            if not px:
                img.set_pixel(i % im.width, i // im.width, 1)
        return img

    # -- output -------------------------------------------------------------
    def _take(self, rc, buf, what):
        """Check the code, copy the bytes out, release the buffer."""
        try:
            _check(rc, what)
            # len == cap on success; clamping costs nothing
            return bytes(ffi.buffer(buf.data, min(buf.len, buf.cap)))
        finally:
            lib.rcpt_buf_free(buf)

    def escpos(self, chunk_rows=0, flags=0):
        buf = ffi.new("rcpt_buf *")
        return self._take(lib.rcpt_enc_escpos(buf, self._img, chunk_rows, flags), buf, "ESC/POS")

    def escp(self, mode=0, flags=0):
        buf = ffi.new("rcpt_buf *")
        return self._take(lib.rcpt_enc_escp(buf, self._img, mode, flags), buf, "ESC/P")

    def tspl(self, x=0, y=0, mode=0, copies=1, flags=0):
        buf = ffi.new("rcpt_buf *")
        return self._take(lib.rcpt_enc_tspl(buf, self._img, x, y, mode, copies, flags), buf, "TSPL")

    def cpcl(self, x=0, y=0, dpi=203, copies=1, flags=0):
        buf = ffi.new("rcpt_buf *")
        return self._take(lib.rcpt_enc_cpcl(buf, self._img, x, y, dpi, copies, flags), buf, "CPCL")

    def zpl(self, x=0, y=0, copies=1, flags=0):
        buf = ffi.new("rcpt_buf *")
        return self._take(lib.rcpt_enc_zpl(buf, self._img, x, y, copies, flags), buf, "ZPL")

    def __repr__(self):
        return f"<receipet.Image {self.width}x{self.height}>"
