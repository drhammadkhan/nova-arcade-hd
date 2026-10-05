#!/usr/bin/env bash
# Builds "Nova Arcade HD.app" on a Mac (Apple Silicon and Intel in one binary) and zips it.
# Needs Xcode's command line tools and CMake (brew install cmake). SDL3 is downloaded and built
# automatically the first time, unless CMake can find an installed one.
set -euo pipefail
cd "$(dirname "$0")/.."
cmake -S . -B build-mac -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 "$@"
cmake --build build-mac -j"$(sysctl -n hw.ncpu)" --target nova
APP="build-mac/Nova Arcade HD.app"
codesign --force --deep --sign - "$APP"     # ad-hoc signature (not notarised)
mkdir -p build
rm -f "build/Nova-Arcade-HD-mac.zip"
ditto -c -k --keepParent "$APP" "build/Nova-Arcade-HD-mac.zip"
echo "built $APP and build/Nova-Arcade-HD-mac.zip"
echo "run it with: open \"$APP\""
