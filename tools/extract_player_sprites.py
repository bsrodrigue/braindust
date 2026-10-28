#!/usr/bin/env python3
"""Cut the player sprite sheet (docs/, JPEG on a flat grey background) into a transparent atlas for the game.

    python3 tools/extract_player_sprites.py [sheet.jpeg]

Writes assets/player.png (one row per animation, fixed cells, feet on a common baseline) and
assets/player.json (cell size, anchor = feet centre, frame counts, head position per frame).
"""
import json
import os
import sys
from collections import deque

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHEET = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "docs", "Gemini_Generated_Image_l6z59hl6z59hl6z5.jpeg")
BG = (199, 199, 199)
WIDE = [25, 246, 467, 689, 912, 1133, 1352]
ROWS = {  # cell borders measured on the sheet: (top, bottom, column edges)
    "idle": (36, 250, [25, 194, 361, 529, 689]),
    "run": (284, 482, WIDE),
    "attack": (517, 720, WIDE),
}


def dist(c, ref=BG):
    return sum((a - b) ** 2 for a, b in zip(c, ref)) ** .5


def chroma(c):
    return max(c) - min(c)


def cut(img):
    """Return an RGBA copy with the grey background removed."""
    w, h = img.size
    src = img.load()
    out = img.convert("RGBA")
    px = out.load()
    for y in range(min(30, h)):  # the frame number sits in the top-left corner
        for x in range(min(40, w)):
            px[x, y] = BG + (255,)
            src[x, y] = BG
    clear = [[False] * w for _ in range(h)]
    is_bg = lambda c: dist(c) < 24 and chroma(c) < 12  # cream cloth is warm (chroma > 20), so it survives
    q = deque((x, y) for x in range(w) for y in (0, h - 1)) + deque((x, y) for y in range(h) for x in (0, w - 1))
    while q:  # flood the background in from the cell edges
        x, y = q.popleft()
        if not (0 <= x < w and 0 <= y < h) or clear[y][x] or not is_bg(src[x, y]):
            continue
        clear[y][x] = True
        q.extend(((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)))
    for y in range(h):  # enclosed gaps (between arm and body) that are exactly background grey
        for x in range(w):
            if not clear[y][x] and dist(src[x, y]) < 12 and chroma(src[x, y]) < 6:
                clear[y][x] = True
    for _ in range(2):  # eat the JPEG halo: greyish pixels touching transparency
        edge = [(x, y) for y in range(1, h - 1) for x in range(1, w - 1)
                if not clear[y][x] and dist(src[x, y]) < 45 and chroma(src[x, y]) < 16
                and (clear[y][x - 1] or clear[y][x + 1] or clear[y - 1][x] or clear[y + 1][x])]
        for x, y in edge:
            clear[y][x] = True
    seen = [[False] * w for _ in range(h)]
    comps = []
    for y in range(h):
        for x in range(w):
            if clear[y][x] or seen[y][x]:
                continue
            comp, q = [], deque([(x, y)])
            seen[y][x] = True
            while q:
                cx, cy = q.popleft()
                comp.append((cx, cy))
                for nx, ny in ((cx + 1, cy), (cx - 1, cy), (cx, cy + 1), (cx, cy - 1)):
                    if 0 <= nx < w and 0 <= ny < h and not clear[ny][nx] and not seen[ny][nx]:
                        seen[ny][nx] = True
                        q.append((nx, ny))
            comps.append(comp)
    body = max(comps, key=len)
    for comp in comps:
        if comp is body:
            continue
        speck = len(comp) < 40 and max(chroma(src[p]) for p in comp) < 30  # JPEG noise; small purple aura bits stay
        spill = min(p[0] for p in comp) <= 2  # a neighbouring frame's spear or aura crossing the cell border
        if speck or spill:
            for p in comp:
                clear[p[1]][p[0]] = True
    for y in range(h):
        for x in range(w):
            if clear[y][x]:
                px[x, y] = (0, 0, 0, 0)
    return out


def anchor_and_head(frame):
    """Feet centre (x of the lower body's mass, lowest opaque row) and head top centre."""
    a = frame.getchannel("A").load()
    w, h = frame.size
    ys = [y for y in range(h) if any(a[x, y] for x in range(w))]
    top, bottom = ys[0], ys[-1]
    band = [(x, y) for y in range(top + int((bottom - top) * .55), bottom + 1) for x in range(w) if a[x, y]]
    fx = sum(p[0] for p in band) / len(band)
    c = frame.load()
    body = lambda x, y: (a[x, y] and abs(x - fx) < 45  # near the body's centre line, not the purple aura
                         and not (c[x, y][2] > c[x, y][0] + 25 and c[x, y][2] > c[x, y][1] + 25))
    htop = next(y for y in range(h) if any(body(x, y) for x in range(w)))
    head = [x for y in range(htop, htop + 14) for x in range(w) if body(x, y)]
    return fx, bottom, sum(head) / len(head), htop


def main():
    sheet = Image.open(SHEET).convert("RGB")
    frames = {}
    for name, (y0, y1, xs) in ROWS.items():
        frames[name] = []
        for x0, x1 in zip(xs, xs[1:]):
            cell = sheet.crop((x0 + 4, y0 + 4, x1 - 3, y1 - 3))
            frames[name].append(cut(cell))
    # one cell size for all frames, anchored at the feet
    info = {n: [anchor_and_head(f) for f in fs] for n, fs in frames.items()}
    left = max(fx for n in info for (fx, _, _, _) in info[n])
    right = max(f.size[0] - fx for n in frames for f, (fx, _, _, _) in zip(frames[n], info[n]))
    up = max(by for n in info for (_, by, _, _) in info[n])
    half = int(max(left, right)) + 2
    cw, ch = half * 2, int(up) + 6
    cols = max(len(f) for f in frames.values())
    atlas = Image.new("RGBA", (cw * cols, ch * len(frames)), (0, 0, 0, 0))
    meta = {"cell": [cw, ch], "anchor": [half, ch - 4], "anims": {}}
    for row, (name, fs) in enumerate(frames.items()):
        heads = []
        for col, (f, (fx, by, hx, hy)) in enumerate(zip(fs, info[name])):
            ox, oy = col * cw + half - round(fx), row * ch + (ch - 4) - by
            atlas.alpha_composite(f, (ox, oy))
            heads.append([round(hx - fx), round(hy - by)])  # relative to the feet anchor
        meta["anims"][name] = {"row": row, "frames": len(fs), "head": heads}
    out = os.path.join(ROOT, "assets")
    atlas.save(os.path.join(out, "player.png"))
    with open(os.path.join(out, "player.json"), "w") as fh:
        json.dump(meta, fh, indent=1)
    print("atlas", atlas.size, "cell", (cw, ch), {n: len(f) for n, f in frames.items()})


if __name__ == "__main__":
    main()
