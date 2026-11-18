#!/bin/sh
# Build the pinned SDL2 as a static, universal (arm64 + x86_64) library for
# macOS 11 and install it under PREFIX. Vector War links it into
# VectorWar.app, so the app needs no separate SDL2 and CI tests exactly what
# ships. (Homebrew's SDL2 is single-architecture and targets the build host's
# macOS, so it cannot be used.)
#
# Usage: build_sdl2.sh PREFIX [DEPLOYMENT_TARGET]
set -eu
prefix=$1
target=${2:-11.0}
version=2.30.9
sha256=24b574f71c87a763f50704bbb630cbe38298d544a1f890f099a4696b1d6beba4
work=${TMPDIR:-/tmp}/ggpo-sdl2-$version

mkdir -p "$work"
archive="$work/SDL2-$version.tar.gz"
curl -sSfL -o "$archive" \
  "https://github.com/libsdl-org/SDL/releases/download/release-$version/SDL2-$version.tar.gz"
echo "$sha256  $archive" | shasum -a 256 -c -
tar -xzf "$archive" -C "$work"

cmake -S "$work/SDL2-$version" -B "$work/build" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="$target" \
  -DCMAKE_INSTALL_PREFIX="$prefix" \
  -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST=OFF -DSDL_TESTS=OFF
cmake --build "$work/build" --parallel 3
cmake --install "$work/build"
