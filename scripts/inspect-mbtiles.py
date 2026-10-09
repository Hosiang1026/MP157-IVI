#!/usr/bin/env python3
import sqlite3
import sys
from pathlib import Path

p = Path(sys.argv[1] if len(sys.argv) > 1 else "osm-2020-02-10-v3.11_china_hangzhou.mbtiles")
print("size_mb", round(p.stat().st_size / 1024 / 1024, 2))
con = sqlite3.connect(str(p))
cur = con.cursor()
print("tables", [r[0] for r in cur.execute("SELECT name FROM sqlite_master WHERE type='table'").fetchall()])
print("metadata:")
for k, v in cur.execute("SELECT name,value FROM metadata").fetchall():
    s = str(v)
    print(" ", k, ":", (s[:240] + "...") if len(s) > 240 else s)
print("tile_count", cur.execute("SELECT COUNT(*) FROM tiles").fetchone()[0])
print("zoom", cur.execute("SELECT MIN(zoom_level), MAX(zoom_level) FROM tiles").fetchone())
for r in cur.execute(
    "SELECT zoom_level,tile_column,tile_row,length(tile_data), quote(substr(tile_data,1,4)) FROM tiles LIMIT 5"
):
    print("sample", r)
bounds = cur.execute(
    "SELECT MIN(tile_column), MAX(tile_column), MIN(tile_row), MAX(tile_row) FROM tiles WHERE zoom_level=14"
).fetchone()
print("z14 bounds", bounds)
