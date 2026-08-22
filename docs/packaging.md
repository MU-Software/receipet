# Packaging and builds

Mostly a record of what was tried and rejected, so it does not get tried again.

## Where the ports live

`packages/<language>/<port>/` — `packages/js/emscripten-port/`,
`packages/python/cffi-port/`.  The directory is named for how the binding is
made rather than for the language again, because the language is already the
directory above it; a second binding for the same language then lands beside
the first instead of forcing a rename.

`package.json` sits inside its package.  **`pyproject.toml` does not** — it
stays at the repository root, and that asymmetry is deliberate.  The cffi build
compiles `src/` and `include/` into the extension, and `Extension()` rejects
absolute paths (see `rel()` below), so the build has to run from a directory
that contains both the C sources and the package.  Only the root does.  Moving
it down would mean either a uv workspace or `../../../src` throughout, for no
gain.

So `python/` moved but the packaging metadata did not, and `MANIFEST.in`,
`setup.py`'s `cffi_modules` and `[tool.setuptools.package-dir]` all name the
new path.  After changing them, the sdist was rebuilt and installed into a
clean environment — see below for why that is not optional.

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

## The npm package

`@mu-software/receipet`, in `packages/js/emscripten-port/`, on GitHub Packages.
The scope has to be the repository owner — GitHub Packages rejects any other,
so the name is not a choice.

The directory is named for how the port is made, not for the language again;
the language is already the directory above it.  That leaves room for a second
JavaScript package built a different way without either of them having to be
renamed.

Four things that are easy to get wrong here:

- **`"private": true` blocks publishing entirely.**  It is npm's "never publish
  this", not "publish this privately".  What marks a proprietary package is
  `"license": "UNLICENSED"`, and the visibility comes from the repository.
- **The `repository` field is load-bearing.**  It is what links the package to
  this repo; without it the publish fails with a 404 that says nothing about
  the cause.
- **The package's `.npmignore` exists to stop npm reading the root
  `.gitignore`.**  That file has `dist/` in it, for good reasons, and `dist/`
  is the entire package.  Packing an empty package is not an error — it
  publishes, and then fails at import.  `make emscripten-port-pack` prints the
  file list before a tag so this is visible rather than discovered later.
- **No anonymous install.**  Even a public package on GitHub Packages needs a
  token to `npm install`, which is the real cost of choosing it over npmjs.

`-sSINGLE_FILE=1` embeds the wasm in the `.mjs` as a data URI.  That is about a
third more bytes — 25 kB in total, which is nothing — and it removes every
question a bundler can ask about where the `.wasm` is, which is not nothing.
`-sEXPORT_ES6=1` on top of `MODULARIZE` makes the factory the default export;
Emscripten requires the output be named `.mjs` for it.

The output goes into the package's own `dist/` rather than `build/`, and
`examples/node_demo.mjs` imports the package entry point rather than a build
directory, so the thing compared against the native build byte for byte is the
thing that ships.  `make wasm-test-emcc` is that check alone, so a release does
not pull the wasi-sdk image for a build it does not publish.

### Constants are generated, not copied

The wasm module exports functions, not values: a `#define` leaves nothing
behind to put in an export table, and a struct offset has no spelling in
TypeScript.  So `RCPT_F_*`, `RCPT_OK`, `RCPT_BROTHER_*` and the layout of
`rcpt_image` / `rcpt_buf` / `rcpt_brother_status` were at first simply absent
from the JS package, and every consumer copied them out of the header by hand
— about forty lines of it in the first real one.  Nothing checked the copy.
Changing the header broke nobody's build and nobody's types; it just read the
wrong bytes at runtime.  That is the drift `check_cdef_matches_header()` exists
to catch on the Python side, where CFFI parses the real header and the question
never comes up.

The fix is the same shape as CFFI's: let a compiler read the header.
`tools/gen_js_constants.c` includes `receipet.h`, prints the values and
`offsetof`s to stdout, and `make js-constants` runs it twice — once for
`constants.mjs`, once for `constants.d.ts` — so each constant is named once, in
C.  The package exposes them on the `./constants` subpath.

**It is compiled by `emcc`, not `$(CC)`.**  Half of what it reports depends on
the target:

| | native 64-bit | wasm32 |
| --- | --- | --- |
| `offsetof(rcpt_image, data)` | 16 | **12** |
| `sizeof(rcpt_image)` | 24 | **16** |
| `sizeof(rcpt_buf)` | 24 | **12** |
| `sizeof(rcpt_brother_status)` | 14 | 14 |

Pointers and `size_t` are 4 bytes under wasm32 and 8 natively, which moves the
first three.  `rcpt_brother_status` is `uint8`/`uint16` throughout and is the
same either way — which is worth knowing, because it is the one that looks
target-dependent and is not, so a comment claiming otherwise is easy to write.

Generation alone is not enough: a wrong offset does not fail, it reads the
neighbouring field and returns a plausible number.  So `examples/node_demo.mjs`
uses the generated constants for all of its own arithmetic and parses a Brother
status reply whose every field differs, checking each one lands where the
constants say.  That runs in `make wasm-test-emcc`, which is what the release
workflow runs.

One struct did not have to be described at all in the end.
`rcpt_parse_brother_status_u32()` writes the nine status fields as `uint32`s in
declaration order, so the JS caller reads one `HEAPU32` window — no offsets,
and no masking off the sign extension `getValue(_, 'i8')` brings with it.  The
struct form stays for the CFFI port, which sees the layout for free.  Adding a
function to a C API to suit one binding is worth being suspicious of; what made
it defensible is that it is a wrapper over the same parse rather than a second
one, and that the ordering it introduces is checked in both test suites.

Both generated files are committed.  `npm install` then needs no emsdk, and a
`git diff` shows what a header change did to the JS surface.  They are
deliberately left out of `make clean`, which would otherwise delete tracked
files.

The `.d.ts` is hand-written for the same reason `cdef()` is: `emcc --emit-tsd`
describes embind, and this is plain C exports driven by pointer arithmetic, so
it has nothing to say.  Unlike the cffi case the compiler *does* help here — a
wrong argument count is a `tsc` error at the call site, which was checked by
introducing one.

Its constants are declared `number`, not literal types.  `export const F_RAW: 1`
is not a *fresh* literal type, so `let flags = F_RAW; flags |= F_INVERT;` would
not compile — which is the one thing flags are for.

## Versions

There are two, and they were deliberately split.

The **API version** (`RCPT_VERSION_*`, repeated in `pyproject.toml`) describes
the C API and only a human changes it.  The **release version** is dated,
`YYYY.M.N`, generated per publish.

They were one value at first, with a script comparing four copies of it.  That
fell apart when releasing moved from tags to every push to `main`: a release
number now has to be generated, and generating it into `RCPT_VERSION_MAJOR`
would have made the C API claim to be version 2026.

The two now have a script each, named and placed for who calls them:
`tools/api_version.py`, which `make version-check` runs before a push, and
`.github/scripts/release_version.py`, which only the workflow runs.

Two things about the numbering are load-bearing, both because **npm accepts a
version exactly once** — publishing over it, or deleting and retrying, are not
options:

- `N` is the highest existing `N` plus one, not the count of tags.  A deleted
  tag would otherwise reissue a number that the registry already has, and that
  release could never be published.
- The tag is pushed *before* `npm publish`, not after.  A burned number costs
  nothing; a number issued twice cannot be published at all.

`2026.08.61` is **not valid semver** — numeric identifiers may not carry a
leading zero — so the month goes in bare.  `npm pack` builds it without
complaint and only the registry refuses it, so this does not show up locally.

`package.json` therefore holds `0.0.0` on `main`.  It is a placeholder the
workflow overwrites with `npm version --no-git-tag-version`, and nothing is
committed back; the tag is the record of what shipped.

Both scripts are stdlib-only, so CI runs them before installing anything.  That
is also why `setuptools-scm` was rejected above — the same problem, silently
wrong versions, solved by checking rather than by deriving.

## compile_flags.txt

Three characters of content, one line per flag, and it cannot be merged into
`.clang-format` or `MANIFEST.in` — it is clangd's, and clangd reads only that
filename.  It is what makes editor diagnostics find `include/` and `src/`.
