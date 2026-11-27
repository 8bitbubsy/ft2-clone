#!/bin/sh
set -eu

if ! command -v emcmake >/dev/null 2>&1 || ! command -v emcc >/dev/null 2>&1; then
	echo "Emscripten is required. Install it with: brew install emscripten" >&2
	exit 1
fi

if ! command -v cmake >/dev/null 2>&1 || ! command -v ninja >/dev/null 2>&1; then
	echo "CMake and Ninja are required. Install them with: brew install cmake ninja" >&2
	exit 1
fi

emcmake cmake -S . -B build-web -G Ninja
cmake --build build-web --parallel
case "${MODS_SOURCE:-local}" in
	local) python3 tools/generate-mods-index.py --copy mods build-web/web/mods ;;
	remote) python3 tools/download-mods.py build-web/web/mods ;;
	*) echo "Unknown MODS_SOURCE: $MODS_SOURCE (expected local or remote)" >&2; exit 1 ;;
esac

echo "Web build ready: build-web/web/index.html (module library: build-web/web/mods)"
