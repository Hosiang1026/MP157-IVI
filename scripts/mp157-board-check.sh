#!/bin/sh
set -eu

ok=0
fail=0

check() {
  name=$1
  shift
  if "$@"; then
    echo "[OK] $name"
    ok=$((ok + 1))
  else
    echo "[FAIL] $name"
    fail=$((fail + 1))
  fi
}

echo "== MP157 board preflight =="

check "framebuffer node" test -e /dev/fb0
check "DRM device" sh -c 'ls /dev/dri/card* >/dev/null 2>&1'
check "input events" sh -c 'ls /dev/input/event* >/dev/null 2>&1'
check "network" sh -c 'ip -4 route show default >/dev/null 2>&1 || ping -c1 -W2 8.8.8.8 >/dev/null 2>&1'

PROBE=""
if command -v board-probe >/dev/null 2>&1; then
  PROBE=board-probe
elif [ -x ./board-probe ]; then
  PROBE=./board-probe
elif [ -x /opt/ivi/board-probe ]; then
  PROBE=/opt/ivi/board-probe
elif [ -x /opt/ivi/bin/board-probe ]; then
  PROBE=/opt/ivi/bin/board-probe
fi

if [ -n "$PROBE" ]; then
  check "board-probe fb" "$PROBE" --no-draw
else
  echo "[SKIP] board-probe not installed"
fi

if [ -x /opt/ivi/ivi-shell ] || [ -x /opt/ivi/bin/ivi-shell ] || [ -x ./ivi-shell ]; then
  echo "[OK] ivi-shell present"
  ok=$((ok + 1))
else
  echo "[FAIL] ivi-shell missing"
  fail=$((fail + 1))
fi

echo "-- Qt platform hints --"
echo "QT_QPA_PLATFORM=${QT_QPA_PLATFORM:-<unset>}"
if [ -d ./qt/plugins/platforms ]; then
  ls ./qt/plugins/platforms
elif [ -d /opt/ivi/qt/plugins/platforms ]; then
  ls /opt/ivi/qt/plugins/platforms
else
  ls /usr/lib/*/qt6/plugins/platforms 2>/dev/null || \
    ls /usr/lib/qt6/plugins/platforms 2>/dev/null || \
    echo "qt platforms dir not found"
fi

echo "== result: ok=$ok fail=$fail =="
[ "$fail" -eq 0 ]
