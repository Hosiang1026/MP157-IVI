#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
BUILD=${IVI_BUILD_DIR:-"$ROOT/build-mp157"}
JOBS=${IVI_JOBS:-$(nproc 2>/dev/null || echo 2)}

if [ -n "${IVI_SDK_ENV:-}" ] && [ -f "${IVI_SDK_ENV}" ]; then
  # shellcheck disable=SC1090
  . "$IVI_SDK_ENV"
fi

if [ -z "${CC:-}" ] || ! echo "$CC" | grep -Eqi 'arm|aarch64'; then
  echo "error: source SDK first, or set IVI_SDK_ENV to env-mp157.sh / environment-setup-*" >&2
  exit 1
fi

if [ -z "${IVI_QT6_DIR:-}" ]; then
  echo "error: set IVI_QT6_DIR to ARM Qt6 prefix (contains lib/cmake/Qt6)" >&2
  exit 1
fi

TOOLCHAIN=${CMAKE_TOOLCHAIN_FILE:-$ROOT/cmake/stm32mp157-toolchain.cmake}

cmake -S "$ROOT" -B "$BUILD" \
  --toolchain "$TOOLCHAIN" \
  -DCMAKE_BUILD_TYPE="${IVI_BUILD_TYPE:-Release}" \
  -DCMAKE_INSTALL_PREFIX="${IVI_INSTALL_PREFIX:-/opt/ivi}" \
  -DCMAKE_PREFIX_PATH="$IVI_QT6_DIR" \
  -DIVI_BUILD_BOARD_PROBE=ON

cmake --build "$BUILD" --target ivi-shell board-probe -j"$JOBS"
echo "build ok: $BUILD"
