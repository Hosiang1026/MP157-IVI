$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Split-Path -Parent $PSScriptRoot)).Path
$mbtiles = "osm-2020-02-10-v3.11_china_hangzhou.mbtiles"
if (-not (Test-Path (Join-Path $root $mbtiles))) {
    Write-Error "missing $mbtiles in repo root"
}

$drive = $root.Substring(0, 1).ToLowerInvariant()
$wslRoot = "/mnt/$drive" + ($root.Substring(2) -replace "\\", "/")

wsl -e bash -lc @"
set -e
docker rm -f ivi-tileserver >/dev/null 2>&1 || true
docker run -d --name ivi-tileserver --restart unless-stopped \
  -v '${wslRoot}:/data' \
  -p 1999:8080 \
  maptiler/tileserver-gl:latest \
  --file /data/$mbtiles
sleep 2
docker logs ivi-tileserver 2>&1 | tail -20
curl -sI 'http://127.0.0.1:1999/styles/basic-preview/14/13659/6745.png' | head -5
"@

Write-Host "TileServer: http://127.0.0.1:1999"
Write-Host "XYZ: http://127.0.0.1:1999/styles/basic-preview/{z}/{x}/{y}.png"
