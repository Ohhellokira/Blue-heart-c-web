#!/usr/bin/env bash
# Builds main_sdl.cpp to WebAssembly with Emscripten.
# Requires the Emscripten SDK to be installed and sourced first:
#   source /c/emsdk/emsdk_env.sh
set -euo pipefail
cd "$(dirname "$0")"

em++ main_sdl.cpp -O2 \
  --use-port=sdl2 \
  --use-port=sdl2_ttf \
  --use-port=sdl2_mixer:formats=mp3 \
  -sALLOW_MEMORY_GROWTH=1 \
  -sEXPORTED_FUNCTIONS=_main,_start_music \
  -sEXPORTED_RUNTIME_METHODS=ccall \
  --preload-file assets@/assets \
  --shell-file shell.html \
  -o ../index.html

echo "Built index.html, index.js, index.wasm, index.data at the repo root"
