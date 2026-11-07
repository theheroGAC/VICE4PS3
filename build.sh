#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
CORE="${1:-c64}"
JOBS="${JOBS:-8}"

export PS3DEV="${PS3DEV:-/mnt/d/ps3dev-sdk}"
export PSL1GHT="${PSL1GHT:-$PS3DEV}"
export PATH="${PS3DEV}/bin:${PS3DEV}/ppu/bin:${PATH}"

case "$CORE" in
  c64)
    SOURCE_DIR="${ROOT}/vice"
    BUILD_DIR="${ROOT}/build-c64"
    ;;
  c128)
    SOURCE_DIR="${ROOT}/vice-c128"
    BUILD_DIR="${ROOT}/build-c128"
    ;;
  vic20)
    SOURCE_DIR="${ROOT}/vice-vic20"
    BUILD_DIR="${ROOT}/build-vic20"
    ;;
  plus4)
    SOURCE_DIR="${ROOT}/vice-plus4"
    BUILD_DIR="${ROOT}/build-plus4"
    ;;
  *)
    echo "Uso: $0 [c64|c128|vic20|plus4]" >&2
    exit 1
    ;;
esac

if [ ! -f "${SOURCE_DIR}/CMakeLists.txt" ]; then
  echo "Sorgente ${SOURCE_DIR} non disponibile" >&2
  exit 1
fi

cmake -S "${SOURCE_DIR}" -B "${BUILD_DIR}" -DPS3DEV="${PS3DEV}" -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}" -j "${JOBS}"
