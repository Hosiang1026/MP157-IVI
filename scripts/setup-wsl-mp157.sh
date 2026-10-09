#!/bin/sh
# WSL2: install host tools + Bootlin ARM toolchain + Qt 6.8.3 (host + cross) + build ivi-shell
set -eu

ROOT=${IVI_WSL_ROOT:-$HOME/mp157-dev}
PROJ=${IVI_PROJ:-/mnt/d/Github/MP157-IVI}
QT_VER=${IVI_QT_VER:-6.8.3}
JOBS=${IVI_JOBS:-$(nproc 2>/dev/null || echo 4)}
BOOTLIN_URL=${BOOTLIN_URL:-https://toolchains.bootlin.com/downloads/releases/toolchains/armv7-eabihf/tarballs/armv7-eabihf--glibc--stable-2024.05-1.tar.xz}
CMAKE_URL=${CMAKE_URL:-https://github.com/Kitware/CMake/releases/download/v3.30.5/cmake-3.30.5-linux-x86_64.tar.gz}
NINJA_URL=${NINJA_URL:-https://github.com/ninja-build/ninja/releases/download/v1.12.1/ninja-linux.zip}
QT_SRC_URL=${QT_SRC_URL:-https://download.qt.io/official_releases/qt/6.8/${QT_VER}/single/qt-everywhere-src-${QT_VER}.tar.xz}

TOOLS=$ROOT/tools
DL=$ROOT/downloads
SDK=$ROOT/sdk
QT_HOST=$ROOT/qt-host/${QT_VER}/gcc_64
QT_ARM=$ROOT/qt-arm/${QT_VER}
ARM_BUILD=$ROOT/build/qt-arm
TC_DIR=$SDK/bootlin

mkdir -p "$TOOLS" "$DL" "$SDK" "$ROOT/build" "$ROOT/bin"
export PATH="$HOME/.local/bin:$TOOLS/cmake/bin:$TOOLS:$PATH"

log() { printf '[setup] %s\n' "$*"; }

need_cmd() {
  command -v "$1" >/dev/null 2>&1
}

fetch() {
  url=$1
  out=$2
  if [ -f "$out" ] && [ -s "$out" ]; then
    log "exists: $out"
    return 0
  fi
  log "download: $url"
  curl -L --fail --retry 5 --retry-delay 2 -o "$out.partial" "$url"
  mv "$out.partial" "$out"
}

install_host_tools() {
  if ! need_cmd cmake || ! need_cmd ninja || ! need_cmd aqt; then
    log "ensure pip cmake/ninja/aqt"
    python3 -m pip install --user --break-system-packages -q cmake ninja aqtinstall
  fi
  export PATH="$HOME/.local/bin:$PATH"
  log "cmake=$(cmake --version | head -1)"
  log "ninja=$(ninja --version)"
}

install_python_aqt() {
  install_host_tools
}

host_qt_ready() {
  [ -x "$1/libexec/moc" ] || [ -x "$1/bin/moc" ]
}

install_qt_host() {
  if host_qt_ready "$QT_HOST"; then
    log "host Qt already at $QT_HOST"
    return 0
  fi
  install_python_aqt
  log "aqt install host Qt $QT_VER"
  python3 -m aqt install-qt linux desktop "$QT_VER" linux_gcc_64 \
    -O "$ROOT/qt-host" \
    -m qtshadertools
  if ! host_qt_ready "$QT_HOST"; then
    ALT=$(find "$ROOT/qt-host" -type f \( -path '*/libexec/moc' -o -path '*/bin/moc' \) | head -1)
    if [ -n "$ALT" ]; then
      QT_HOST=$(CDPATH= cd -- "$(dirname "$ALT")/.." && pwd)
      log "QT_HOST resolved to $QT_HOST"
    fi
  fi
  host_qt_ready "$QT_HOST" || { log "host Qt install failed"; return 1; }
}

install_bootlin() {
  if [ -d "$TC_DIR" ] && ls "$TC_DIR"/*/bin/*gcc >/dev/null 2>&1; then
    log "Bootlin toolchain present"
  else
    fetch "$BOOTLIN_URL" "$DL/bootlin-armv7.tar.xz"
    mkdir -p "$TC_DIR"
    tar -xJf "$DL/bootlin-armv7.tar.xz" -C "$TC_DIR" --strip-components=0
  fi
  TC_PREFIX=$(echo "$TC_DIR"/*/bin/*-gcc | head -1)
  TC_PREFIX=${TC_PREFIX%-gcc}
  CROSS_GCC=${TC_PREFIX}-gcc
  SYSROOT=$("$CROSS_GCC" -print-sysroot)
  [ -n "$SYSROOT" ] && [ -d "$SYSROOT" ] || SYSROOT=$(dirname "$(dirname "$TC_PREFIX")")/armv7-eabihf/sysroot
  if [ ! -d "$SYSROOT" ]; then
    # Bootlin layout: <top>/<tuple>/sysroot
    SYSROOT=$(find "$TC_DIR" -type d -name sysroot | head -1)
  fi
  echo "$TC_PREFIX" >"$SDK/cross-prefix.txt"
  echo "$SYSROOT" >"$SDK/sysroot.txt"
  log "CROSS=$TC_PREFIX"
  log "SYSROOT=$SYSROOT"
  "$CROSS_GCC" --version | head -1
}

write_toolchain_file() {
  PREFIX=$(cat "$SDK/cross-prefix.txt")
  SYSROOT=$(cat "$SDK/sysroot.txt")
  cat >"$SDK/toolchain-armv7.cmake" <<EOF
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_SYSROOT "${SYSROOT}")
set(CMAKE_C_COMPILER "${PREFIX}-gcc")
set(CMAKE_CXX_COMPILER "${PREFIX}-g++")
set(CMAKE_FIND_ROOT_PATH "${SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
EOF
  log "wrote $SDK/toolchain-armv7.cmake"
}

fetch_qt_src() {
  SRC_TAR="$DL/qt-everywhere-src-${QT_VER}.tar.xz"
  SRC_DIR="$ROOT/src/qt-everywhere-src-${QT_VER}"
  if [ ! -d "$SRC_DIR" ]; then
    fetch "$QT_SRC_URL" "$SRC_TAR"
    mkdir -p "$ROOT/src"
    log "extract Qt source (slow)"
    tar -xJf "$SRC_TAR" -C "$ROOT/src"
  fi
  echo "$SRC_DIR"
}

build_qt_arm() {
  if [ -f "$QT_ARM/lib/cmake/Qt6/Qt6Config.cmake" ]; then
    log "ARM Qt already at $QT_ARM"
    return 0
  fi
  install_host_tools
  install_bootlin
  write_toolchain_file
  SRC=$(fetch_qt_src)
  mkdir -p "$ARM_BUILD"
  cd "$ARM_BUILD"
  if [ ! -f CMakeCache.txt ]; then
    log "configure ARM Qt (eglfs/linuxfb)"
    "$SRC/configure" \
      -prefix "$QT_ARM" \
      -release \
      -nomake examples \
      -nomake tests \
      -qt-host-path "$QT_HOST" \
      -skip qtwebengine -skip qtwebview -skip qt3d -skip qtquick3d \
      -skip qtmultimedia -skip qtlocation -skip qtpositioning \
      -skip qtsensors -skip qtconnectivity -skip qtserialbus \
      -skip qtserialport -skip qtcharts -skip qtdatavis3d \
      -skip qtscxml -skip qtremoteobjects -skip qthttpserver \
      -skip qtgrpc -skip qtprotobuf -skip qtopcua \
      -skip qtvirtualkeyboard -skip qtwebsockets -skip qtwebchannel \
      -skip qtspeech -skip qtnetworkauth -skip qt5compat \
      -- \
      -DCMAKE_TOOLCHAIN_FILE="$SDK/toolchain-armv7.cmake" \
      -DQT_FEATURE_eglfs=ON \
      -DQT_FEATURE_linuxfb=ON \
      -DQT_FEATURE_xcb=OFF \
      -DQT_FEATURE_wayland=OFF
  fi
  log "build ARM Qt -j$JOBS (may take 1-3h)"
  cmake --build . -j"$JOBS"
  cmake --install .
  log "ARM Qt installed: $QT_ARM"
}

write_env() {
  PREFIX=$(cat "$SDK/cross-prefix.txt")
  cat >"$ROOT/env-mp157.sh" <<EOF
# source $ROOT/env-mp157.sh
export PATH="$TOOLS/cmake/bin:$TOOLS:\$PATH"
export IVI_SDK_ENV="$ROOT/env-mp157.sh"
export CC=${PREFIX}-gcc
export CXX=${PREFIX}-g++
export AR=${PREFIX}-ar
export STRIP=${PREFIX}-strip
export OECORE_TARGET_SYSROOT="$(cat "$SDK/sysroot.txt")"
export IVI_QT6_DIR="$QT_ARM"
export CMAKE_TOOLCHAIN_FILE="$SDK/toolchain-armv7.cmake"
export PATH="$(dirname "$PREFIX"):\$PATH"
EOF
  log "env file: $ROOT/env-mp157.sh"
}

build_ivi() {
  [ -d "$PROJ" ] || { log "project missing: $PROJ"; return 1; }
  # shellcheck disable=SC1090
  . "$ROOT/env-mp157.sh"
  export IVI_BUILD_DIR="$ROOT/build/ivi"
  export IVI_STAGE_DIR="$ROOT/dist/mp157-ivi"
  export IVI_BUNDLE="$ROOT/dist/mp157-ivi.tar.gz"
  sh "$PROJ/scripts/build-mp157.sh"
  sh "$PROJ/scripts/pack-mp157.sh"
  log "bundle: $IVI_BUNDLE"
}

install_st_sdk_if_present() {
  # If user dropped official ST SDK tarball/script, prefer it
  CAND=$(ls "$DL"/SDK-x86_64-stm32mp1*.tar.gz "$DL"/en.SDK-x86_64-stm32mp1*.tar.gz 2>/dev/null | head -1 || true)
  if [ -z "${CAND:-}" ]; then
    log "no ST SDK in $DL (optional). Put SDK-x86_64-stm32mp1-*.tar.gz there for official toolchain."
    return 0
  fi
  log "found ST SDK: $CAND"
  mkdir -p "$SDK/st-extract"
  tar -xzf "$CAND" -C "$SDK/st-extract"
  SH=$(find "$SDK/st-extract" -name 'st-image-*-toolchain-*.sh' | head -1)
  [ -n "$SH" ] || { log "toolchain .sh not found in ST package"; return 1; }
  chmod +x "$SH"
  "$SH" -d "$SDK/st" -y
  ENVF=$(find "$SDK/st" -name 'environment-setup-cortexa7*' | head -1)
  log "ST env: $ENVF"
  ln -sfn "$ENVF" "$ROOT/env-st.sh"
}

case "${1:-all}" in
  tools) install_host_tools ;;
  host-qt) install_host_tools; install_qt_host ;;
  bootlin) install_host_tools; install_bootlin; write_toolchain_file; write_env ;;
  qt-arm) install_qt_host; build_qt_arm; write_env ;;
  ivi) write_env; build_ivi ;;
  st) install_st_sdk_if_present ;;
  all)
    install_host_tools
    install_qt_host
    install_st_sdk_if_present
    install_bootlin
    write_toolchain_file
    build_qt_arm
    write_env
    build_ivi
    ;;
  *)
    echo "usage: $0 {all|tools|host-qt|bootlin|qt-arm|ivi|st}"
    exit 1
    ;;
esac
