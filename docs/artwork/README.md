# Original artwork — master sources

Keep the **master PNG** of every sprite and animation frame here, before
conversion. The project brief requires the original artwork to be kept in the
project documentation folder, and the pet's appearance must be original work
created or customised for this project.

## Files

| File | Contents |
|---|---|
| _TBD_ | _TBD_ |

Converted RGB565 output does **not** live here — generated `.c`/`.h` asset files
belong with the firmware so the build can find them.

## Conversion pipeline

1. Draw or generate the master PNG at a generous size on a **flat single-colour
   background** (magenta `#FF00FF` is the convention used so far) or transparent.
2. Reduce to a limited palette with hard, aliased edges. Avoid gradients, soft
   shadows and anti-aliasing — they survive downscaling badly and bloat the asset.
3. Downscale with **nearest-neighbour** to the target sprite size. Prefer an exact
   integer ratio (e.g. 512 → 64 is exactly 8×) so pixels sample evenly.
4. Composite the keyed background against the pet screen's real background colour
   so the sprite is **opaque** — this lets the firmware blit with a single SPI
   transfer and no per-pixel work.
5. Emit bytes already in **panel order** (high byte of each RGB565 word first) so
   the firmware needs no byte swapping.
6. Verify on hardware that **red and blue are not swapped** and the orientation is
   correct before converting the remaining frames.

## Target dimensions and budget

**Recommended sprite size: 64×64** (8,192 bytes each in RGB565).

| Sprite | Bytes |
|---|---|
| 240×320 (full frame) | 153,600 |
| 128×128 | 32,768 |
| 96×96 | 18,432 |
| **64×64** | **8,192** |
| 48×48 | 4,608 |

Roughly 904 KB of the 1 MB flash is available for assets after code and libraries.
A 64×64 sprite at 6 frames × 6 animations is 288 KB (~28%).

**Do not plan full-screen animated frames** — a single 4-frame full-screen cycle
would consume ~59% of the entire flash.

## Colour format

Panel is configured **BGR** (`MADCTL = 0x08`) with the high byte written first.
Reference values from `ili9341.h`:

| Colour | RGB565 |
|---|---|
| Red | `0xF800` |
| Green | `0x07E0` |
| Blue | `0x001F` |
| White | `0xFFFF` |
| Black | `0x0000` |
