#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${ROOT}/build-c64"
PKG_ROOT="${ROOT}/pkgbuild"
GAME_DIR="${PKG_ROOT}/PS3_GAME"
USR_DIR="${GAME_DIR}/USRDIR"
OUT_DIR="${ROOT}/dist"
TITLE_ID="VICEP3001"
CONTENT_ID="UP0001-${TITLE_ID}_00-0000000000000000"
PKG_NAME="VICE4PS3.pkg"
ELF="${BUILD_DIR}/vice_c64.elf"

export PS3DEV="${PS3DEV:-/mnt/d/ps3dev-sdk}"
export PSL1GHT="${PSL1GHT:-$PS3DEV}"
export PATH="${PS3DEV}/bin:${PS3DEV}/ppu/bin:${PATH}"
export PYTHONPATH="${PS3DEV}/bin:${PYTHONPATH:-}"

if [ ! -f "${ELF}" ]; then
  echo "Missing ELF: ${ELF}. Run build.sh first" >&2
  exit 1
fi

rm -rf "${GAME_DIR}"
mkdir -p "${USR_DIR}/data/games" \
         "${USR_DIR}/data/saves" \
         "${USR_DIR}/data/vice" \
         "${USR_DIR}/data/tmp" \
         "${USR_DIR}/data/covers" \
         "${USR_DIR}/resources"

cp -r "${ROOT}/vice/resources/C64" "${USR_DIR}/resources/"
cp -r "${ROOT}/vice/resources/DRIVES" "${USR_DIR}/resources/"
cp -r "${ROOT}/vice/resources/PRINTER" "${USR_DIR}/resources/"
cp -r "${ROOT}/assets/data" "${USR_DIR}/"
cp -r "${ROOT}/assets/data" "${USR_DIR}/resources/"

cp "${ROOT}/assets/ICON0.PNG" "${GAME_DIR}/ICON0.PNG"
cp "${ROOT}/assets/PIC1.PNG" "${GAME_DIR}/PIC1.PNG"

cp "${ELF}" "${PKG_ROOT}/vice_c64.elf"
powerpc64-ps3-elf-strip "${PKG_ROOT}/vice_c64.elf"
sprxlinker "${PKG_ROOT}/vice_c64.elf"
(cd "${USR_DIR}" && make_self_npdrm "${PKG_ROOT}/vice_c64.elf" EBOOT.BIN "${CONTENT_ID}")

if [ -x "${PS3DEV}/bin/sfo" ]; then
  "${PS3DEV}/bin/sfo" --title="VICE4PS3" --appid="${TITLE_ID}" -f "${PKG_ROOT}/sfo.xml" "${GAME_DIR}/PARAM.SFO"
else
  python3 "${ROOT}/tools/sfo.py" -f "${PKG_ROOT}/sfo.xml" "${GAME_DIR}/PARAM.SFO"
fi

mkdir -p "${OUT_DIR}"
python3 "${ROOT}/tools/pkg_ps3.py" --contentid="${CONTENT_ID}" "${GAME_DIR}" "${OUT_DIR}/${PKG_NAME}"

GN_PKG="${OUT_DIR}/${PKG_NAME%.pkg}.gnpdrm.pkg"
cp -f "${OUT_DIR}/${PKG_NAME}" "${GN_PKG}"
if [ -x "${PS3DEV}/bin/package_finalize" ]; then
  "${PS3DEV}/bin/package_finalize" "${GN_PKG}"
fi

echo "PKG: ${OUT_DIR}/${PKG_NAME}"
echo "Signed PKG: ${GN_PKG}"
