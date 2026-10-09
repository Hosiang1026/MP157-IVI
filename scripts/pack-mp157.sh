#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
BUILD=${IVI_BUILD_DIR:-"$ROOT/build-mp157"}
STAGE=${IVI_STAGE_DIR:-"$ROOT/dist/mp157-ivi"}
QT6=${IVI_QT6_DIR:-}

if [ ! -x "$BUILD/src/shell/ivi-shell" ] && [ ! -x "$BUILD/ivi-shell" ]; then
  echo "error: ivi-shell not found, run scripts/build-mp157.sh first" >&2
  exit 1
fi

SHELL_BIN=$BUILD/src/shell/ivi-shell
[ -x "$SHELL_BIN" ] || SHELL_BIN=$BUILD/ivi-shell
PROBE_BIN=$BUILD/tools/board-probe/board-probe

rm -rf "$STAGE"
mkdir -p "$STAGE"

cp -a "$SHELL_BIN" "$STAGE/ivi-shell"
[ -x "$PROBE_BIN" ] && cp -a "$PROBE_BIN" "$STAGE/board-probe"
cp -a "$ROOT/version.json" "$STAGE/version.json"
cp -a "$ROOT/apps" "$STAGE/apps"
cp -a "$ROOT/feed" "$STAGE/feed"
mkdir -p "$STAGE/wallpapers"
cp -a "$ROOT/assets/wallpapers" "$STAGE/wallpapers/builtin"
cp -a "$ROOT/assets/media" "$STAGE/media"
if [ -d "$ROOT/assets/carplay/offline-mfi" ]; then
  cp -a "$ROOT/assets/carplay/offline-mfi" "$STAGE/offline-mfi"
fi

install -m 0755 "$ROOT/scripts/run-ivi-shell.sh" "$STAGE/run-ivi-shell.sh"
install -m 0755 "$ROOT/scripts/mp157-board-check.sh" "$STAGE/mp157-board-check.sh"
install -m 0644 "$ROOT/deploy/ivi-shell.service" "$STAGE/ivi-shell.service"

if [ -n "$QT6" ] && [ -d "$QT6" ]; then
  mkdir -p "$STAGE/qt"
  for d in lib plugins qml; do
    [ -d "$QT6/$d" ] && cp -a "$QT6/$d" "$STAGE/qt/"
  done
fi

if command -v "${STRIP:-strip}" >/dev/null 2>&1; then
  "${STRIP:-strip}" "$STAGE/ivi-shell" 2>/dev/null || true
  [ -x "$STAGE/board-probe" ] && "${STRIP:-strip}" "$STAGE/board-probe" 2>/dev/null || true
fi

OUT=${IVI_BUNDLE:-"$ROOT/dist/mp157-ivi.tar.gz"}
mkdir -p "$(dirname "$OUT")"
tar czf "$OUT" -C "$STAGE" .

VERSION=$(sed -n 's/.*"version"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' "$ROOT/version.json" | head -1)
VERSION=${VERSION:-0.0.0}
if command -v sha256sum >/dev/null 2>&1; then
  SHA=$(sha256sum "$OUT" | awk '{print $1}')
elif command -v shasum >/dev/null 2>&1; then
  SHA=$(shasum -a 256 "$OUT" | awk '{print $1}')
else
  SHA=
fi
SIZE=$(wc -c < "$OUT" | tr -d ' ')
MANIFEST=${IVI_UPDATE_MANIFEST_OUT:-"${OUT%.tar.gz}.json"}
BASE_URL=${IVI_UPDATE_BASE_URL:-}
if [ -n "$BASE_URL" ]; then
  PKG_URL="${BASE_URL%/}/$(basename "$OUT")"
else
  PKG_URL="$(basename "$OUT")"
fi
cat > "$MANIFEST" <<EOF
{
  "version": "$VERSION",
  "url": "$PKG_URL",
  "sha256": "$SHA",
  "size": $SIZE,
  "notes": ""
}
EOF

echo "stage: $STAGE"
echo "bundle: $OUT"
echo "manifest: $MANIFEST"
