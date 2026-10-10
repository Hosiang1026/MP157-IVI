#!/bin/sh
set -eu

DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
cd "$DIR"

mkdir -p "$DIR/logs" || true
: "${IVI_LOG_DIR:=$DIR/logs}"
export IVI_LOG_DIR

if [ -d "$DIR/qt/lib" ]; then
  export LD_LIBRARY_PATH="$DIR/qt/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
if [ -d "$DIR/qt/plugins" ]; then
  export QT_PLUGIN_PATH="$DIR/qt/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
fi
if [ -d "$DIR/qt/qml" ]; then
  export QML2_IMPORT_PATH="$DIR/qt/qml${QML2_IMPORT_PATH:+:$QML2_IMPORT_PATH}"
fi

: "${QT_QPA_EGLFS_PHYSICAL_WIDTH:=154}"
: "${QT_QPA_EGLFS_PHYSICAL_HEIGHT:=87}"
export QT_QPA_EGLFS_PHYSICAL_WIDTH QT_QPA_EGLFS_PHYSICAL_HEIGHT

if [ -z "${QT_QPA_PLATFORM:-}" ]; then
  if [ -e /dev/dri/card0 ]; then
    export QT_QPA_PLATFORM=eglfs
  elif [ -e /dev/fb0 ]; then
    export QT_QPA_PLATFORM=linuxfb
  else
    export QT_QPA_PLATFORM=eglfs
  fi
fi

exec ./ivi-shell "$@"
