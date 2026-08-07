"""Builds the CFFI extension.  API mode, so the C sources are compiled in and
there is no shared library to find at runtime.

Not run directly; the build backend calls it.  `uv sync` or `pip install -e .`
from the repo root.
"""

import re
from pathlib import Path

from cffi import FFI

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]  # packages/python/cffi-port -> repo root
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
    rcpt_result rcpt_enc_tspl(rcpt_buf *out, const rcpt_image *img, int32_t x, int32_t y, uint8_t mode,
                         uint32_t chunk_rows, uint32_t copies, uint32_t flags);
    rcpt_result rcpt_enc_cpcl(rcpt_buf *out, const rcpt_image *img, int32_t x, int32_t y, uint32_t dpi, uint32_t copies, uint32_t flags);
    rcpt_result rcpt_enc_zpl(rcpt_buf *out, const rcpt_image *img, int32_t x, int32_t y, uint32_t copies, uint32_t flags);
    rcpt_result rcpt_enc_brother(rcpt_buf *out, const rcpt_image *img, uint8_t media_type,
                         uint8_t width_mm, uint8_t length_mm, uint32_t offset, uint32_t flags);

    typedef struct rcpt_brother_status {
        uint8_t model;
        uint16_t errors;
        uint8_t media_width_mm;
        uint8_t media_length_mm;
        uint8_t media_type;
        uint8_t status_type;
        uint8_t phase_type;
        uint16_t phase_number;
        uint8_t notification;
    } rcpt_brother_status;

    rcpt_result rcpt_parse_brother_status(rcpt_brother_status *out, const uint8_t *data, size_t len);

    /* `...` means "take the value from the real header at compile time". */
    #define RCPT_VERSION_MAJOR ...
    #define RCPT_VERSION_MINOR ...
    #define RCPT_VERSION_PATCH ...
    #define RCPT_MAX_DIM ...

    #define RCPT_F_RAW ...
    #define RCPT_F_INVERT ...
    #define RCPT_F_BROTHER_NO_CUT ...

    #define RCPT_BROTHER_STATUS_LEN ...
    #define RCPT_BROTHER_MEDIA_NONE ...
    #define RCPT_BROTHER_MEDIA_CONTINUOUS ...
    #define RCPT_BROTHER_MEDIA_DIE_CUT ...

    #define RCPT_BROTHER_ERR_NO_MEDIA ...
    #define RCPT_BROTHER_ERR_END_OF_MEDIA ...
    #define RCPT_BROTHER_ERR_CUTTER_JAM ...
    #define RCPT_BROTHER_ERR_IN_USE ...
    #define RCPT_BROTHER_ERR_POWERED_OFF ...
    #define RCPT_BROTHER_ERR_REPLACE_MEDIA ...
    #define RCPT_BROTHER_ERR_BUFFER_FULL ...
    #define RCPT_BROTHER_ERR_COMMS ...
    #define RCPT_BROTHER_ERR_COVER_OPEN ...
    #define RCPT_BROTHER_ERR_CANNOT_FEED ...
    #define RCPT_BROTHER_ERR_SYSTEM ...
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


# RCPT_API is a visibility attribute, and cdef() has no way to take a string
# macro's value from the header, so neither can be checked.
NOT_CONSTANTS = {"RCPT_API", "RCPT_BROTHER_STATUS_REQUEST"}


def constants(text):
    """Every integer RCPT_* macro in `text`."""
    found = {m.group(1) for m in re.finditer(r"(?m)^\s*#\s*define\s+(RCPT_[A-Z0-9_]+)\s", text)}
    return found - NOT_CONSTANTS


def check_cdef_matches_header():
    """The compiler catches a wrong argument count but not a wrong argument
    type, and it says nothing at all about a constant nobody declared, so
    compare both sets against the header before building."""
    text = (INCLUDE / "receipet.h").read_text()
    header = declarations(text)
    cdef = declarations(CDEF)

    for name, want in header.items():
        got = cdef.get(name)
        if got is None:
            raise SystemExit(f"build_ffi.py: {name}() is missing from CDEF")
        if got != want:
            raise SystemExit(f"build_ffi.py: {name}() disagrees with receipet.h\n  header: {want}\n  cdef:   {got}")

    missing = constants(text) - constants(CDEF)
    if missing:
        raise SystemExit(f"build_ffi.py: missing from CDEF: {', '.join(sorted(missing))}")


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
