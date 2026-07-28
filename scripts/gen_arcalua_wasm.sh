#!/bin/sh
# Builds the WASM-compiled arcalua (minilua) language driver for the browser
# runtime. Requires the Emscripten SDK; sources it from the well-known
# location on this machine if emcc isn't already on PATH.
set -e
BASEDIR=$(dirname "$0")

if ! command -v emcc >/dev/null 2>&1; then
	export EMSDK=/home/gf/emsdk
	export PATH="$EMSDK:$EMSDK/upstream/emscripten:$PATH"
fi

emcc "$BASEDIR/bindings_arcalua_wasm.c" \
	-I"$BASEDIR/../external" \
	-s WASM=1 -s MODULARIZE=1 -s EXPORT_NAME=createArcaluaModule \
	-s EXPORTED_FUNCTIONS=_initVM,_shutdownVM,_dispatchLifecycleEvent,_dispatchLifecycleEventArgv,_dispatchUpdateEvent,_dispatchDrawEvent,_dispatchAxisEvent,_dispatchButtonEvent,_malloc,_free \
	-s EXPORTED_RUNTIME_METHODS=ccall,cwrap,UTF8ToString,stringToUTF8,allocateUTF8,setValue,getValue \
	-s ALLOW_MEMORY_GROWTH=1 \
	-O2 \
	-o "$BASEDIR/../browser_runtime/arcalua.js"
