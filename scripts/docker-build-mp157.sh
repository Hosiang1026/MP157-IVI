#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
IMG=${IVI_DOCKER_IMAGE:-mp157-ivi-builder:latest}
CACHE=${IVI_DOCKER_CACHE:-$HOME/mp157-dev}

mkdir -p "$CACHE"

echo "[docker] build image $IMG"
docker build -f "$ROOT/scripts/Dockerfile.mp157" -t "$IMG" "$ROOT/scripts"

echo "[docker] run setup (Qt ARM build may take 1-3h)"
docker run --rm \
  -v "$ROOT:/work/proj:ro" \
  -v "$CACHE:/opt/mp157-dev" \
  -e IVI_WSL_ROOT=/opt/mp157-dev \
  -e IVI_PROJ=/work/proj \
  -e IVI_JOBS="${IVI_JOBS:-$(nproc 2>/dev/null || echo 4)}" \
  "$IMG" \
  /usr/local/bin/setup-mp157.sh "${1:-all}"

echo "[docker] done. artifacts under $CACHE"
echo "  env:    $CACHE/env-mp157.sh"
echo "  qt-arm: $CACHE/qt-arm"
echo "  bundle: $CACHE/dist/mp157-ivi.tar.gz"
