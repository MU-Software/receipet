# receipet - build for host, Emscripten and WASI.
#
#   make                       static lib + demo + tests (host)
#   make test                  build and run the test suite
#   make wasm                  both WASM targets
#   make wasm-emcc             the npm package's dist/  (docker: emscripten/emsdk)
#   make wasm-wasi             build/wasi/receipet.wasm (docker: wasi-sdk)
#   make wasm-test             run both WASM builds under Node
#   make emscripten-port-pack  what `npm publish` would send, without sending it
#   make version-check         receipet.h and pyproject.toml agree on the API
#   make python                build and install the CFFI extension (editable)
#   make python-test           run the Python example
#   make fmt                   format C and Python in place
#   make lint                  check formatting without touching anything
#   make clean
#
# The WASM targets shell out to docker so that nothing has to be installed
# locally; set DOCKER= to an empty value to use toolchains already on PATH.

CC      ?= cc
AR      ?= ar
CFLAGS  ?= -O2
CFLAGS  += -std=c99 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Iinclude -Isrc

# -MMD -MP records which headers each object used, so editing receipet.h or
# rcpt_internal.h rebuilds what depends on it instead of leaving a stale .o.
CFLAGS  += -MMD -MP

SRC     := $(wildcard src/*.c) $(wildcard src/lang/*.c)
OBJ     := $(SRC:src/%.c=build/obj/%.o)
LIB     := build/libreceipet.a

# The rcpt_* functions export themselves (RCPT_API carries the keepalive
# attribute under __wasm__); the allocator does not, so name it explicitly.
EMCC_EXPORTS := _malloc,_free

.PHONY: all test clean wasm wasm-emcc wasm-wasi \
        wasm-test wasm-test-emcc wasm-test-wasi \
        emscripten-port-pack version-check \
        python python-test fmt lint

all: $(LIB) build/demo build/test_receipet

build:
	@mkdir -p build

# The pattern spans src/ and src/lang/, so mirror the source tree under build/.
build/obj/%.o: src/%.c | build
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIB): $(OBJ)
	$(AR) rcs $@ $^

build/demo: examples/demo.c $(LIB)
	$(CC) $(CFLAGS) $< $(LIB) -o $@

build/test_receipet: tests/test_receipet.c $(LIB)
	$(CC) $(CFLAGS) $< $(LIB) -o $@

test: build/test_receipet
	./build/test_receipet

# ---- WebAssembly -----------------------------------------------------------
EMSCRIPTEN_PKG := packages/js/emscripten-port

DOCKER          ?= docker
EMSDK_IMAGE     ?= emscripten/emsdk:latest
WASI_SDK_IMAGE  ?= ghcr.io/webassembly/wasi-sdk:latest
WASI_SDK        ?= /opt/wasi-sdk

# Mount the checkout at /src and build as the invoking user, so the artefacts
# in build/ are not left root-owned.
DOCKER_RUN = $(DOCKER) run --rm -v "$(CURDIR)":/src -w /src \
             -u $(shell id -u):$(shell id -g)

# Emscripten does not need our -std=c99 pedantry; keep the flags it likes.
WASM_CFLAGS = -O2 -Wall -Wextra -Iinclude -Isrc

wasm: wasm-emcc wasm-wasi

build/wasi: | build
	@mkdir -p $@

$(EMSCRIPTEN_PKG)/dist:
	@mkdir -p $@

# MODULARIZE + EXPORT_ES6 gives a factory as the .mjs default export.
# SINGLE_FILE embeds the wasm as a data URI: a third more bytes, and nothing
# for a bundler to resolve.
wasm-emcc: | $(EMSCRIPTEN_PKG)/dist
	$(if $(DOCKER),$(DOCKER_RUN) $(EMSDK_IMAGE)) emcc $(WASM_CFLAGS) $(SRC) \
	    -o $(EMSCRIPTEN_PKG)/dist/receipet.mjs \
	    -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=createReceipet \
	    -sEXPORTED_FUNCTIONS=$(EMCC_EXPORTS) \
	    -sEXPORTED_RUNTIME_METHODS=HEAPU8,ccall,cwrap,getValue,setValue \
	    -sALLOW_MEMORY_GROWTH=1 -sFILESYSTEM=0 \
	    -sSINGLE_FILE=1 -sENVIRONMENT=web,node

# A reactor module (no main), so the host calls the exports directly.
wasm-wasi: | build/wasi
	$(if $(DOCKER),$(DOCKER_RUN) $(WASI_SDK_IMAGE)) \
	    $(WASI_SDK)/bin/clang --sysroot=$(WASI_SDK)/share/wasi-sysroot \
	    $(WASM_CFLAGS) $(SRC) -o build/wasi/receipet.wasm \
	    -mexec-model=reactor -Wl,--no-entry -Wl,--export-dynamic \
	    -Wl,--export=malloc,--export=free

# Run both WASM builds through Node.  The emsdk image already ships Node, so
# reuse it rather than pulling another image (or set NODE=node to use a local
# install).
NODE ?= $(if $(DOCKER),$(DOCKER_RUN) $(EMSDK_IMAGE) node,node)

# The demos compare their output against the native build's, byte for byte, so
# the native reference has to exist first.  Sizes alone would not have caught
# an argument list going stale across the FFI boundary; this does.
#
# One of the six files stands for all of them - ./build/demo writes them in a
# single run - so that the demos share one run rather than a phony target's.
NATIVE_REF := build/native/demo.escpos

$(NATIVE_REF): build/demo
	@mkdir -p build/native
	@./build/demo build/native/ >/dev/null

wasm-test-emcc: wasm-emcc $(NATIVE_REF)
	$(NODE) examples/node_demo.mjs

wasm-test-wasi: wasm-wasi $(NATIVE_REF)
	$(NODE) --experimental-wasi-unstable-preview1 examples/wasi_demo.mjs

wasm-test: wasm-test-emcc wasm-test-wasi

# ---- npm package -----------------------------------------------------------
# A package that ships without dist/ publishes cleanly and fails at import, so
# check the file list before tagging.  --dry-run does not contact the registry.
NPM ?= npm

emscripten-port-pack: wasm-emcc
	cd $(EMSCRIPTEN_PKG) && $(NPM) publish --dry-run

# ---- versions --------------------------------------------------------------
# Not through uv: the script is stdlib-only so CI can run it before installing.
PYTHON ?= python3

version-check:
	$(PYTHON) tools/api_version.py

# ---- Python ----------------------------------------------------------------
# uv sync installs the package editable, so the extension is rebuilt from the
# C sources and `import receipet` works with no PYTHONPATH.  Without uv:
# pip install -e .
UV ?= uv

python:
	$(UV) sync --reinstall-package receipet

python-test: python
	$(UV) run python python/example.py

# ---- formatting ------------------------------------------------------------
# C style lives in .clang-format, Python style in pyproject.toml.  The same
# two tools run from .pre-commit-config.yaml; `uvx pre-commit install` wires
# them to the commit hook.
C_SOURCES := $(wildcard include/*.h src/*.c src/*.h src/lang/*.c examples/*.c tests/*.c)

fmt:
	clang-format -i $(C_SOURCES)
	$(UV) run ruff check --fix python/ tools/ .github/scripts/
	$(UV) run ruff format python/ tools/ .github/scripts/

lint:
	clang-format --dry-run -Werror $(C_SOURCES)
	$(UV) run ruff check python/ tools/ .github/scripts/
	$(UV) run ruff format --check python/ tools/ .github/scripts/

-include $(OBJ:.o=.d) build/demo.d build/test_receipet.d

clean:
	rm -rf build $(EMSCRIPTEN_PKG)/dist python/build python/receipet/__pycache__ \
	       python/receipet/_receipet.c python/receipet/_receipet.o \
	       python/receipet/_receipet*.so receipet.egg-info
