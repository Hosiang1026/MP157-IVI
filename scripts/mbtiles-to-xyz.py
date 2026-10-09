#!/usr/bin/env python3
import argparse
import gzip
import math
import shutil
import sqlite3
from pathlib import Path

import mapbox_vector_tile
from PIL import Image, ImageDraw, ImageFont

SIZE = 256
LAND = (236, 236, 230)
WATER = (170, 210, 230)
PARK = (196, 220, 180)
LANDUSE = (220, 220, 200)
BUILDING = (210, 205, 195)
ROAD = {
    "motorway": ((80, 80, 80), 5),
    "trunk": ((90, 90, 90), 4),
    "primary": ((100, 100, 100), 3),
    "secondary": ((120, 120, 120), 3),
    "tertiary": ((140, 140, 140), 2),
    "minor": ((160, 160, 160), 2),
    "service": ((175, 175, 175), 1),
    "path": ((190, 190, 180), 1),
    "track": ((180, 170, 150), 1),
    "rail": ((120, 120, 140), 1),
    "transit": ((120, 120, 140), 1),
}


def decompress(data: bytes) -> bytes:
    if data[:2] == b"\x1f\x8b":
        return gzip.decompress(data)
    return data


def tms_to_xyz(z: int, y: int) -> int:
    return (1 << z) - 1 - y


def scale_ring(ring, extent: int):
    s = SIZE / float(extent)
    return [(p[0] * s, p[1] * s) for p in ring]


def draw_polygon(draw, geom, fill, extent):
    coords = geom["coordinates"]
    if geom["type"] == "Polygon":
        polys = [coords]
    elif geom["type"] == "MultiPolygon":
        polys = coords
    else:
        return
    for poly in polys:
        if not poly:
            continue
        pts = scale_ring(poly[0], extent)
        if len(pts) >= 3:
            draw.polygon(pts, fill=fill)


def draw_line(draw, geom, fill, width, extent):
    coords = geom["coordinates"]
    if geom["type"] == "LineString":
        lines = [coords]
    elif geom["type"] == "MultiLineString":
        lines = coords
    else:
        return
    for line in lines:
        pts = scale_ring(line, extent)
        if len(pts) >= 2:
            draw.line(pts, fill=fill, width=max(1, width))


def render_tile(data: bytes) -> Image.Image:
    decoded = mapbox_vector_tile.decode(decompress(data))
    img = Image.new("RGB", (SIZE, SIZE), LAND)
    draw = ImageDraw.Draw(img)

    for layer_name, fill in (
        ("water", WATER),
        ("landcover", PARK),
        ("landuse", LANDUSE),
        ("park", PARK),
    ):
        layer = decoded.get(layer_name)
        if not layer:
            continue
        extent = layer.get("extent", 4096)
        for feat in layer["features"]:
            g = feat["geometry"]
            if g["type"] in ("Polygon", "MultiPolygon"):
                draw_polygon(draw, g, fill, extent)

    buildings = decoded.get("building")
    if buildings:
        extent = buildings.get("extent", 4096)
        for feat in buildings["features"]:
            g = feat["geometry"]
            if g["type"] in ("Polygon", "MultiPolygon"):
                draw_polygon(draw, g, BUILDING, extent)

    roads = decoded.get("transportation")
    if roads:
        extent = roads.get("extent", 4096)
        for feat in roads["features"]:
            props = feat.get("properties") or {}
            cls = props.get("class") or "minor"
            color, width = ROAD.get(cls, ((150, 150, 150), 2))
            g = feat["geometry"]
            if g["type"] in ("LineString", "MultiLineString"):
                draw_line(draw, g, color, width, extent)
            elif g["type"] in ("Polygon", "MultiPolygon"):
                draw_polygon(draw, g, color, extent)

    return img


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("mbtiles")
    ap.add_argument("-o", "--out", default="assets/maps/tiles")
    ap.add_argument("--min-zoom", type=int, default=12)
    ap.add_argument("--max-zoom", type=int, default=14)
    ap.add_argument("--clean", action="store_true")
    args = ap.parse_args()

    src = Path(args.mbtiles)
    out = Path(args.out)
    if args.clean and out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True, exist_ok=True)

    con = sqlite3.connect(str(src))
    meta = dict(con.execute("SELECT name, value FROM metadata"))
    print("name:", meta.get("name"))
    print("bounds:", meta.get("bounds"))
    print("center:", meta.get("center"))
    print("format:", meta.get("format"))

    rows = con.execute(
        "SELECT zoom_level, tile_column, tile_row, tile_data FROM tiles "
        "WHERE zoom_level BETWEEN ? AND ? ORDER BY zoom_level, tile_column, tile_row",
        (args.min_zoom, args.max_zoom),
    ).fetchall()
    print("tiles:", len(rows), "zoom", args.min_zoom, "-", args.max_zoom)

    count = 0
    for z, x, tms_y, data in rows:
        xyz_y = tms_to_xyz(z, tms_y)
        path = out / str(z) / str(x) / f"{xyz_y}.png"
        path.parent.mkdir(parents=True, exist_ok=True)
        render_tile(data).save(path, optimize=True)
        count += 1
        if count % 50 == 0:
            print("...", count)

    print("wrote", count, "png ->", out.resolve())
    if "center" in meta:
        print("MapTiles center hint:", meta["center"])


if __name__ == "__main__":
    main()
