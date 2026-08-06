"""Builds the CFFI extension.  API mode, so the C sources are compiled in and
there is no shared library to find at runtime.

Not run directly; the build backend calls it.  `uv sync` or `pip install -e .`
from the repo root.
"""

import re
from pathlib import Path

from cffi import FFI

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
SRC = ROOT / "src"
INCLUDE = ROOT / "include"


def rel(path):
    """setuptools rejects absolute paths in Extension()."""
    return str(path.relative_to(Path.cwd(), walk_up=True))


ffibuilder = FFI()

# cdef() cannot digest the real header - it has no preprocessor - so the
# declarations are repeated here and checked against it below.
CDEF = """
    typedef enum rcpt_result {
        RCPT_OK = 0, RCPT_ERR_ARG = -1, RCPT_ERR_NOMEM = -2, RCPT_ERR_INTERNAL = -3
    } rcpt_result;

    typedef struct rcpt_image {
        uint32_t width;
        uint32_t height;
        uint32_t stride;
        uint8_t *data;
    } rcpt_image;

    typedef struct rcpt_buf {
        uint8_t *data;
        size_t   cap;
        size_t   len;
    } rcpt_buf;

    rcpt_result rcpt_image_new(rcpt_image **out, uint32_t width, uint32_t height);
    void   rcpt_image_free(rcpt_image *img);
    void   rcpt_image_clear(rcpt_image *img);
    void   rcpt_image_set_pixel(rcpt_image *img, int32_t x, int32_t y, int value);
    int    rcpt_image_get_pixel(const rcpt_image *img, int32_t x, int32_t y);

    void rcpt_buf_free(rcpt_buf *buf);

    rcpt_result rcpt_enc_escpos(rcpt_buf *out, const rcpt_image *img, uint32_t chunk_rows, uint32_t flags);
    rcpt_result rcpt_enc_escp(rcpt_buf *out, const rcpt_image *img, uint8_t mode, uint32_t flags);
    rcpt_result rcpt_enc_tspl(rcpt_buf *out, const rcpt_image *img, int32_t x, int32_t y, uint8_t mode, uint32_t copies, uint32_t flags);
    rcpt_result rcpt_enc_cpcl(rcpt_buf *out, const rcpt_image *img, int32_t x, int32_t y, uint32_t dpi, uint32_t copies, uint32_t flags);
    rcpt_result rcpt_enc_zpl(rcpt_buf *out, const rcpt_image *img, int32_t x, int32_t y, uint32_t copies, uint32_t flags);
"""


def declarations(text):
    """name -> normalised declaration, for every function in `text`."""
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)  # comments
    text = re.sub(r"(?m)^\s*#.*$", "", text)  # preprocessor
    text = re.sub(r"typedef\s+(struct|enum)\s+\w+\s*\{[^}]*\}\s*\w+\s*;", "", text)
    text = text.replace('extern "C" {', "").replace("RCPT_API", "")
    out = {}
    for decl in text.split(";"):
        decl = " ".join(decl.split())
        m = re.search(r"\b(\w+)\s*\(", decl)
        if m:
            out[m.group(1)] = decl
    return out


def check_cdef_matches_header():
    """The compiler catches a wrong argument count but not a wrong argument
    type, so compare the two sets of declarations before building."""
    header = declarations((INCLUDE / "receipet.h").read_text())
    cdef = declarations(CDEF)

    for name, want in header.items():
        got = cdef.get(name)
        if got is None:
            raise SystemExit(f"build_ffi.py: {name}() is missing from CDEF")
        if got != want:
            raise SystemExit(f"build_ffi.py: {name}() disagrees with receipet.h\n  header: {want}\n  cdef:   {got}")


check_cdef_matches_header()
ffibuilder.cdef(CDEF)

# Sorted for a reproducible build; rglob picks up src/lang/ too.
SOURCES = [rel(p) for p in sorted(SRC.rglob("*.c"))]

ffibuilder.set_source(
    "receipet._receipet",
    '#include "receipet.h"',
    sources=SOURCES,
    include_dirs=[rel(INCLUDE), rel(SRC)],  # src/ for rcpt_internal.h
    extra_compile_args=["-std=c99", "-O2"],
)
