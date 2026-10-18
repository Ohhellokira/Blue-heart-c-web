#!/usr/bin/env bash
# Builds main_sdl.cpp into a single self-contained index.html at the repo
# root (wasm binary + all assets embedded inline — no companion .js/.wasm/
# .data files). Requires the Emscripten SDK installed and sourced first:
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
  -sSINGLE_FILE=1 \
  --embed-file assets@/assets \
  --shell-file shell.html \
  -o ../index.html

# SINGLE_FILE + embed-file inline everything into index.html, but emcc
# still writes the pre-inlining intermediates (index.wasm, index.data) to
# disk as unused leftovers — remove them so the repo matches what's live.
rm -f ../index.wasm ../index.data

echo "Built a single self-contained index.html at the repo root"
