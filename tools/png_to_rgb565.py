#!/usr/bin/env python3
"""
png_to_rgb565.py - turn sprite artwork into RGB565 C arrays for the LCD.

Steps:
  1. load the PNG, or one cell of a sprite sheet
  2. remove the background by flood filling inwards from the border, not by
     matching that colour everywhere - a global match would also punch out the
     white eye highlights
  3. crop to the bounding box of what's left
  4. scale with nearest neighbour to fit the target size, keeping the aspect ratio
  5. quantize to a small palette, which flattens any soft gradients the image
     generator left in
  6. composite onto the stage colour so the sprite is opaque, and place it on a
     fixed size canvas resting on a fixed baseline
  7. write out a const uint16_t RGB565 array and its dimensions

Run it with the venv python:
    ~/.venvs/img/bin/python png_to_rgb565.py <in.png> --name slime_idle \
        --grid 2x2 --cell 0,0 --size 64 --colors 14 --out-dir sprites/
"""
import argparse
import os
import re
import sys
from collections import deque

import numpy as np
from PIL import Image, ImageFilter

# Stage background the sprites get composited onto. Has to match the firmware's
# stage fill, so the blit can be one opaque transfer.
DEFAULT_STAGE = (30, 42, 50)      # #1E2A32


def parse_hex(value, default=None):
    if value is None:
        return default
    v = value.strip().lstrip('#')
    if len(v) != 6:
        sys.exit(f"colour must be RRGGBB, got {value!r}")
    return tuple(int(v[i:i + 2], 16) for i in (0, 2, 4))


def rgb565(r, g, b):
    # The int() calls matter. Callers pass numpy uint8 values and shifting a
    # uint8 left by 11 wraps at 8 bits, so 3 << 11 comes out as 0. That zeroes
    # the red field of every pixel and wrecks green too.
    r, g, b = int(r), int(g), int(b)
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def cell_of(img, grid, cell):
    """Cut one cell out of a rows x cols sprite sheet."""
    if grid is None:
        return img
    rows, cols = grid
    r, c = cell
    if not (0 <= r < rows and 0 <= c < cols):
        sys.exit(f"cell {r},{c} outside grid {rows}x{cols}")
    w, h = img.size
    cw, ch = w // cols, h // rows
    return img.crop((c * cw, r * ch, (c + 1) * cw, (r + 1) * ch))


def background_mask(rgb, tolerance):
    """
    Flood fill from every border pixel, marking anything within `tolerance` of
    the corner colour that you can reach from the edge.

    Returns a bool array, True meaning background.

    Flood filling rather than testing the colour everywhere is what lets a
    sprite keep the background colour inside it - white eye highlights on white
    paper, for instance. Only the outer connected region gets removed.
    """
    h, w, _ = rgb.shape
    corner = rgb[0, 0].astype(np.int16)
    near = (np.abs(rgb.astype(np.int16) - corner).max(axis=2) <= tolerance)

    mask = np.zeros((h, w), dtype=bool)
    q = deque()

    for x in range(w):
        for y in (0, h - 1):
            if near[y, x] and not mask[y, x]:
                mask[y, x] = True
                q.append((x, y))
    for y in range(h):
        for x in (0, w - 1):
            if near[y, x] and not mask[y, x]:
                mask[y, x] = True
                q.append((x, y))

    while q:
        x, y = q.popleft()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, ny = x + dx, y + dy
            if 0 <= nx < w and 0 <= ny < h and near[ny, nx] and not mask[ny, nx]:
                mask[ny, nx] = True
                q.append((nx, ny))
    return mask


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("src")
    ap.add_argument("--name", required=True, help="C identifier for the array")
    ap.add_argument("--grid", help="RxC, e.g. 2x2, to cut one cell from a sheet")
    ap.add_argument("--cell", help="row,col (0-based) when --grid is used")
    ap.add_argument("--size", type=int, default=64, help="target sprite size (square)")
    ap.add_argument("--colors", type=int, default=14, help="palette size")
    ap.add_argument("--stage", default=None, help="stage background RRGGBB")
    ap.add_argument("--tolerance", type=int, default=40, help="background colour tolerance")
    ap.add_argument("--baseline-margin", type=int, default=2,
                    help="blank rows between the feet and the bottom of the canvas")
    ap.add_argument("--fit", type=float, default=0.92,
                    help="fraction of the canvas the sprite fills, leaving room to "
                         "squash and stretch during animation")
    ap.add_argument("--defringe", type=int, default=1,
                    help="pixels to erode off the sprite edge, to remove the "
                         "key-coloured ring that the flood fill can't. Don't just "
                         "raise this: eroding 2 px eats through a thin dark outline "
                         "and leaves the body colour touching the background, which "
                         "is worse than the fringe it removed. 1 suits this art.")
    ap.add_argument("--out-dir", default=".")
    ap.add_argument("--emit-header", action="store_true")
    args = ap.parse_args()

    stage = parse_hex(args.stage, DEFAULT_STAGE)

    img = Image.open(args.src).convert("RGB")
    grid = None
    cell = (0, 0)
    if args.grid:
        m = re.fullmatch(r"(\d+)x(\d+)", args.grid)
        if not m:
            sys.exit("--grid must look like 2x2")
        grid = (int(m.group(1)), int(m.group(2)))
        cell = tuple(int(v) for v in (args.cell or "0,0").split(","))
    img = cell_of(img, grid, cell)

    rgb = np.array(img)
    h, w, _ = rgb.shape
    total = h * w

    bg = background_mask(rgb, args.tolerance)
    kept = ~bg
    if not kept.any():
        sys.exit("every pixel was classified as background - tolerance too high?")

    ys, xs = np.where(kept)
    y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1

    # Crop to the sprite, keeping an alpha mask of which pixels survived keying.
    sprite = rgb[y0:y1, x0:x1].astype(np.uint8)
    sprite_bg = bg[y0:y1, x0:x1]
    rgba = np.dstack([sprite, np.where(sprite_bg, 0, 255).astype(np.uint8)])
    src = Image.fromarray(rgba, "RGBA")

    # Fit into a fraction of the canvas so there's room for the sprite to grow
    # and squash during animation.
    sw, sh = x1 - x0, y1 - y0
    box = max(1, int(round(args.size * args.fit)))
    scale = box / max(sw, sh)
    tw, th = max(1, int(round(sw * scale))), max(1, int(round(sh * scale)))
    fit = src.resize((tw, th), Image.Resampling.NEAREST)

    # Downscaling a soft edge leaves a ring of pixels blended between the artwork
    # and the key colour. Those aren't within tolerance of the key colour, so the
    # flood fill can't remove them and they quantize into a pink fringe. Eroding
    # the alpha takes the ring off.
    alpha = fit.split()[3]
    if args.defringe > 0:
        alpha = alpha.filter(ImageFilter.MinFilter(2 * args.defringe + 1))

    # Composite onto the stage colour first and quantize the whole canvas after,
    # otherwise palette entries get spent on background that was already removed.
    canvas = Image.new("RGB", (args.size, args.size), stage)
    px = (args.size - tw) // 2
    py = max(0, args.size - args.baseline_margin - th)
    canvas.paste(fit.convert("RGB"), (px, py), alpha)

    # Which canvas pixels aren't part of the sprite?
    sprite_mask = Image.new("L", (args.size, args.size), 0)
    sprite_mask.paste(alpha, (px, py))
    outside = np.array(sprite_mask) == 0

    quant = canvas.quantize(colors=args.colors,
                            method=Image.Quantize.MEDIANCUT).convert("RGB")
    arr = np.array(quant).copy()

    # The firmware fills the stage with one exact colour, so every pixel the
    # sprite doesn't cover has to come back as exactly that value. Quantization
    # can shift it, and on hardware that shows as a faint rectangular seam round
    # the pet. Snap the outside, plus anything still background-coloured just
    # inside the edge.
    stage_px = np.array(stage, dtype=np.uint8)
    near_stage = (np.abs(arr.astype(np.int16) - stage_px.astype(np.int16))
                  .max(axis=2) <= 10)
    arr[outside | near_stage] = stage_px
    pal = {tuple(v) for v in arr.reshape(-1, 3).tolist()}
    vals = [rgb565(r, g, b) for r, g, b in (tuple(v) for v in arr.reshape(-1, 3))]

    stem = args.name.lower()
    os.makedirs(args.out_dir, exist_ok=True)
    c_path = os.path.join(args.out_dir, f"{stem}.c")
    preview_path = os.path.join(args.out_dir, f"{stem}_preview.png")
    # Build the preview by decoding the RGB565 we just emitted, not from the
    # canvas. A preview of the canvas hides quantization and colour conversion
    # problems, which is how a zeroed red channel got past every look at these
    # sprites before.
    decoded = [(((v >> 11) & 0x1F) * 255 // 31,
                ((v >> 5) & 0x3F) * 255 // 63,
                (v & 0x1F) * 255 // 31) for v in vals]
    prev_px = np.array(decoded, dtype=np.uint8).reshape(args.size, args.size, 3)
    Image.fromarray(prev_px).resize(
        (args.size * 4, args.size * 4), Image.Resampling.NEAREST).save(preview_path)

    with open(c_path, "w") as f:
        f.write("/* Generated by tools/png_to_rgb565.py - do not edit by hand.\n")
        f.write(f" * source: {os.path.basename(args.src)}")
        if args.grid:
            f.write(f" cell {cell[0]},{cell[1]} of {args.grid}")
        f.write(f"\n * {args.size}x{args.size}, {len(pal)} colours, RGB565, "
                f"opaque (composited on the stage colour)\n */\n")
        f.write(f'#include "{stem}.h"\n\n')
        f.write(f"const uint16_t {args.name}[{len(vals)}] = {{\n")
        for i in range(0, len(vals), 12):
            f.write("    " + ", ".join(f"0x{v:04X}" for v in vals[i:i + 12]) + ",\n")
        f.write("};\n")

    h_path = os.path.join(args.out_dir, f"{stem}.h")
    with open(h_path, "w") as f:
        f.write("/* Generated by tools/png_to_rgb565.py - do not edit by hand. */\n")
        f.write("#ifndef %s_H\n#define %s_H\n\n#include <stdint.h>\n\n"
                % (stem.upper(), stem.upper()))
        f.write(f"#define {args.name.upper()}_WIDTH   {args.size}u\n")
        f.write(f"#define {args.name.upper()}_HEIGHT  {args.size}u\n\n")
        f.write(f"extern const uint16_t {args.name}[{len(vals)}];\n\n#endif\n")

    print(f"source      : {os.path.basename(args.src)}"
          + (f"  cell {cell[0]},{cell[1]} of {args.grid}" if args.grid else ""))
    print(f"cell size   : {w}x{h}  ->  sprite bbox {sw}x{sh}")
    print(f"scaled to   : {tw}x{th}  placed at ({px},{py}) on {args.size}x{args.size}")
    print(f"background  : keyed {bg.sum():,} px of {total:,} ({bg.sum()/total*100:.1f}%) "
          f"via border flood fill")
    print(f"palette     : {len(pal)} distinct colours after quantization")
    print(f"output      : {len(vals):,} pixels = {len(vals)*2:,} bytes "
          f"({len(vals)*2/1048576*100:.2f}% of 1 MB flash)")
    print(f"wrote       : {c_path}")
    print(f"              {h_path}")
    print(f"preview     : {preview_path}  (4x nearest-neighbour, for eyeballing)")


if __name__ == "__main__":
    main()
