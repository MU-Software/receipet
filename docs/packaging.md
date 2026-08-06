# Packaging and builds

Mostly a record of what was tried and rejected, so it does not get tried again.

## setup.py stays

`hatchling` was evaluated as a way to delete `setup.py`.  It cannot:

- cffi's API mode needs setuptools at build time on Python 3.12+ anyway, so
  the dependency does not actually go away
- editable installs break
- it leaves stray build output behind

So the project keeps a minimal `setup.py` and drives cffi through setuptools.

An earlier attempt called `ffibuilder.compile()` directly.  It `chdir`s and
scatters object files through the tree; routing the build through setuptools
instead fixed both.

`setuptools-scm` was also tried for versioning.  It works only inside a git
checkout and is **silently wrong** outside one, which is the worst failure
mode available.  The version is written out by hand.

`Extension()` rejects absolute paths, hence `rel()` in `build_ffi.py` using
`Path.relative_to(Path.cwd(), walk_up=True)`.

## MANIFEST.in is load-bearing

Without it the sdist contains no headers and will not install.  This was
confirmed by building an sdist and installing it in a clean environment, not
assumed.

If an install fails in a way that looks impossible, check for a stale pip
wheel cache before believing anything else — one round of debugging here was
spent on a cached wheel from a previous broken build.

## The cdef/header sync check

`cdef()` has no preprocessor, so it cannot digest `receipet.h` directly and
the declarations are repeated in `build_ffi.py`.  Repetition drifts, so
`check_cdef_matches_header()` compares the two before every build.

It exists because the compiler does *not* catch this.  A wrong argument
**count** is a compile error; a wrong argument **type** that happens to be
convertible — `uint8_t` where the header says `uint32_t` — passes silently and
misbehaves at runtime.  The check caught two real omissions the first time it
ran.

It was later extended to compare **constant names** too, after
`lib.RCPT_BROTHER_ERR_COVER_OPEN` turned out not to exist: the header defined
all eleven Brother error bits, and none of them had ever been declared to
cffi, so `BrotherStatus()` was returning `errors` as an uninterpretable
integer.  Adding the names check immediately found `RCPT_MAX_DIM` and the
three `RCPT_VERSION_*` macros missing as well.

Constants are declared as `#define RCPT_F_RAW ...` — the literal `...` tells
cffi to take the value from the real header at compile time, so there is no
second copy to drift.  That also removed the hardcoded `RAW = 1 << 0` and
`INVERT = 1 << 1` from the Python module.

Two macros cannot be checked and are listed as exceptions: `RCPT_API` is a
visibility attribute, and `RCPT_BROTHER_STATUS_REQUEST` is a string, which
`cdef()` has no way to import.

## WASM

Built through Docker rather than local toolchains — `emscripten/emsdk` and
`ghcr.io/webassembly/wasi-sdk` — so there is nothing to install and the build
is reproducible.

The WASI build is a reactor module; the Emscripten build uses `MODULARIZE`.
No struct is returned by value across the boundary in either.  Both produce
byte-identical output to the native build, which `make wasm-test` checks.

## compile_flags.txt

Three characters of content, one line per flag, and it cannot be merged into
`.clang-format` or `MANIFEST.in` — it is clangd's, and clangd reads only that
filename.  It is what makes editor diagnostics find `include/` and `src/`.
