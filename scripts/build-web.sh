#!/usr/bin/env bash
# Builds the browser version into build/site (the page GitHub Pages serves).
# Needs Emscripten (em++ on the PATH, e.g. `source emsdk/emsdk_env.sh`).
# Extra CMake arguments are passed through, e.g. -DNOVA_WEB_SDL_PORT=OFF -DCMAKE_PREFIX_PATH=/path/to/sdl3-wasm
set -euo pipefail
cd "$(dirname "$0")/.."
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release "$@"
cmake --build build-web -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"
mkdir -p build/site
cp web/index.html build/site/
cp platform/icon.png build/site/icon.png
cp build-web/nova.js build-web/nova.wasm build-web/nova.data build/site/
echo "web build: build/site (serve it with: python3 -m http.server -d build/site)"
