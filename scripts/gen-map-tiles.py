#!/usr/bin/env python3
import math
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1] / "assets" / "maps" / "tiles"
CENTER_LAT = 31.2304
CENTER_LON = 121.4737
ZOOMS = (12, 13, 14)
RADIUS = {12: 2, 13: 3, 14: 3}
SIZE = 256


def latlon_to_tile(lat, lon, z):
    n = 2**z
    x = int((lon + 180.0) / 360.0 * n)
    lat_rad = math.radians(lat)
    y = int((1.0 - math.log(math.tan(lat_rad) + 1.0 / math.cos(lat_rad)) / math.pi) / 2.0 * n)
    return x, y


def draw_tile(z: int, x: int, y: int) -> Image.Image:
    img = Image.new("RGB", (SIZE, SIZE), (232, 236, 230))
    draw = ImageDraw.Draw(img)
    seed = (z * 73856093) ^ (x * 19349663) ^ (y * 83492791)

    for i in range(4):
        gy = ((seed >> (i * 3)) & 0xFF) % SIZE
        draw.line([(0, gy), (SIZE, gy)], fill=(214, 220, 212), width=1)
        gx = ((seed >> (i * 5)) & 0xFF) % SIZE
        draw.line([(gx, 0), (gx, SIZE)], fill=(214, 220, 212), width=1)

    for i in range(3):
        px = ((seed >> (i * 7)) & 0xFF) % (SIZE - 40)
        py = ((seed >> (i * 11)) & 0xFF) % (SIZE - 40)
        w = 28 + ((seed >> (i * 2)) & 0x1F)
        h = 20 + ((seed >> (i * 4)) & 0x1F)
        draw.rectangle([px, py, px + w, py + h], fill=(198, 214, 186))

    if (x + y) % 5 == 0:
        draw.ellipse([40, 40, 210, 210], fill=(186, 214, 230))

    road = (245, 245, 240)
    edge = (180, 180, 170)
    if (x + y) % 2 == 0:
        draw.line([(0, SIZE // 2), (SIZE, SIZE // 2)], fill=edge, width=14)
        draw.line([(0, SIZE // 2), (SIZE, SIZE // 2)], fill=road, width=8)
    else:
        draw.line([(SIZE // 2, 0), (SIZE // 2, SIZE)], fill=edge, width=14)
        draw.line([(SIZE // 2, 0), (SIZE // 2, SIZE)], fill=road, width=8)

    if (x * 3 + y) % 4 == 0:
        draw.line([(0, 0), (SIZE, SIZE)], fill=edge, width=10)
        draw.line([(0, 0), (SIZE, SIZE)], fill=road, width=5)

    draw.rectangle([0, 0, SIZE - 1, SIZE - 1], outline=(210, 214, 208))
    return img


def main() -> None:
    cx, cy = latlon_to_tile(CENTER_LAT, CENTER_LON, 13)
    print(f"center tile z13 = {cx}/{cy}")
    count = 0
    for z in ZOOMS:
        tx, ty = latlon_to_tile(CENTER_LAT, CENTER_LON, z)
        r = RADIUS[z]
        for x in range(tx - r, tx + r + 1):
            for y in range(ty - r, ty + r + 1):
                out = ROOT / str(z) / str(x) / f"{y}.png"
                out.parent.mkdir(parents=True, exist_ok=True)
                draw_tile(z, x, y).save(out, optimize=True)
                count += 1
    print(f"wrote {count} tiles -> {ROOT}")


if __name__ == "__main__":
    main()
