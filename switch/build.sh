#!/usr/bin/env bash
# Build ra2engine.nro inside Docker.
#   switch/build.sh [Release|RelWithDebInfo|Debug]
# RA2_DKP_IMAGE picks the toolchain image:
#   ra2-switch-dkp (default)   devkitA64 + unified Mesa 26 (see switch/Dockerfile)
#   devkitpro/devkita64        stock switch-mesa 20.1 (fallback)
# Each image builds into its own folder (build-switch / build-switch-stock).
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
type="${1:-Release}"
image="${RA2_DKP_IMAGE:-ra2-switch-dkp}"
out=build-switch
[ "$image" = ra2-switch-dkp ] || out=build-switch-stock
docker run --rm -u "$(id -u):$(id -g)" -v "$root:/src" -w /src "$image" bash -c "
  export PATH=/opt/devkitpro/devkitA64/bin:/opt/devkitpro/portlibs/switch/bin:/opt/devkitpro/tools/bin:\$PATH
  cmake -S . -B $out -G Ninja -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake \
        -DCMAKE_BUILD_TYPE=$type &&
  cmake --build $out"
