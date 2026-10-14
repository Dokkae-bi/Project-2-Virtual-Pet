# Virtual pet design — the slime

> **Status:** design locked; the logic, artwork and tests are all done. Only the
> hardware integration is outstanding — see [`../TODO.md`](../TODO.md).
> Console name is still a working name (`Bloop`, easily changed).

---

## 1. Why a slime

The creature was chosen for engineering reasons, not just taste. Four properties
make it the lowest-risk design for this display and this memory budget:

| Property | Consequence |
|---|---|
| **One geometric shape, no limbs to draw** | Survives 64×64 RGB565 downscaling without turning to mush — unlike fluffy or heavily-detailed designs whose sub-pixel features vanish |
| **Squash and stretch is the whole animation** | The idle animation and most action animations are shape transforms of a single sprite, not new artwork |
| **State is expressed by colour** | Mood can be a **palette swap** on one body sprite rather than a new sprite per mood |
| **Flat colour planes** | Quantizes cleanly to a small palette with no gradient banding |

## 2. Identity

- **Body:** a rounded gel dome in cyan-teal, thick dark outline, a lighter belly
  tone and a small specular shine, with a few drips at the base.
- **Face:** two large round eyes (white highlight) and a small mouth.
- **Mood indicator:** three channels at once — **body colour** (palette swap),
  **silhouette** (squash/stretch/sag), and **face** (eye and mouth shape).

Using colour *and* shape keeps the state readable even at a glance, and matches
the interface requirement that the user can tell what the pet needs.

## 3. State → appearance

Six visual states, mapped one-to-one onto the model states in `pet_core.h`.

| Model state | Trigger | Body colour | Silhouette | Face | Artwork |
|---|---|---|---|---|---|
| `PET_MOOD_DELIGHTED` | happiness ≥ 75 | bright saturated teal | stretched taller | happy upward-arc eyes, open smile | ✅ have |
| `PET_MOOD_CONTENT` | default | base teal | neutral rounded dome | calm open eyes, small smile | ✅ have |
| `PET_MOOD_HUNGRY` | food ≤ 30 (happiness still OK) | pale, slightly washed out | gentle sag | flat wobbly mouth, eyes a little smaller | ✅ have |
| `PET_MOOD_MISERABLE` | happiness ≤ 25 | dull grey-teal | flattened, sagging wide | half-closed downcast eyes, downturned mouth | ✅ have |
| `PET_DEAD` | food reaches 0 | drained grey | collapsed to a flat puddle | eyes closed, no mouth | ✅ have |
| `PET_RUNAWAY` | happiness reaches 0 | fading, low opacity look | sliding off the edge of the stage | back turned / gone | ✅ have |

`PET_MOOD_HUNGRY` is deliberately **distinct from** `PET_MOOD_MISERABLE`: a pet can
be well-loved but starving, and the player must be able to tell those apart.

## 4. Actions and hardware mapping

| Action | Physical input | Model effect | Animation | Artwork |
|---|---|---|---|---|
| **FEED** *(required)* | menu item; **Potentiometer → ADC** selects portion 1–5 | food +12×portion, happiness +4 | squashes down and spreads as the berry goes in | `slime_eating` + scaling |
| **PLAY** | menu item | happiness +22, food −9 | jump: stretches upward while rising | mood sprite + scaling |
| **CLEAN** | menu item | happiness +14 | wobble either side of rest | mood sprite + scaling |
| Menu navigation | GFX01M2 joystick | select / activate / restart | — | code only |
| Mood voice | **PWM buzzer** | tone per mood, chirp per action | — | code only |

The potentiometer gives the exact chain the brief asks for: **raw ADC value
(0–4095) → interpreted value (portion 1–5) → visible effect on the pet**.

Only the three action animations need `pet_render.c` scaling; Feed additionally
uses one of the seven drawn sprites. No animation frames are stored as artwork,
which is what keeps the flash cost at 56 KB instead of 221 KB — see §8.

**Not yet implemented:** triggering Play directly from the accelerometer (shake or
tilt) instead of via the menu, and the buzzer. Both are optional over the brief's
requirements, and neither is wired up.

### Peripherals covered

| Rubric requirement | Covered by |
|---|---|
| GPIO | push-button, joystick |
| ADC | potentiometer |
| Additional I2C / UART / SPI device | MPU-6050 on I2C2 (PF1/PF0) |
| Two purposeful external input/output sources | potentiometer + accelerometer (+ buzzer) |
| Timer / tick scheduling | PWM buzzer + the `HAL_GetTick()` scheduler |
| LCD over SPI | ILI9341 on SPI1 |

## 5. Colour palette (RGB565, verified)

Every value below was computed as `((R>>3)<<11) | ((G>>2)<<5) | (B>>3)` and the
method was checked against the driver's own constants: `#FF0000 → 0xF800`
(matches `ILI9341_COLOR_RED`) and `#0000FF → 0x001F` (matches `ILI9341_COLOR_BLUE`).

| Name | Hex | RGB565 |
|---|---|---|
| Colour-key background | `#FF00FF` | `0xF81F` |
| Outline | `#14201E` | `0x1103` |
| Body — dark | `#2A8C80` | `0x2C70` |
| Body — base | `#46C8B4` | `0x4656` |
| Body — light | `#8AEAD8` | `0x8F5B` |
| Shine | `#D6FFF6` | `0xD7FE` |
| Eye white | `#FFFFFF` | `0xFFFF` |
| Eye black | `#101418` | `0x10A3` |
| Mouth | `#8A2B4A` | `0x8949` |
| Sparkle | `#FFF6B0` | `0xFFB6` |
| Dead grey | `#6E7B7A` | `0x6BCF` |
| Runaway dull | `#3E5A66` | `0x3ACC` |
| Meter fill | `#FFB020` | `0xFD84` |
| Meter empty | `#2A2F36` | `0x2966` |

## 6. Screen layout (240 × 320 portrait)

The authoritative values are the `PET_HUD_*` constants in
`Core/Inc/pet_hud.h`; the table below is the original plan and has been
superseded where the two disagree.

| Region | Extent | Contents | Redraw strategy |
|---|---|---|---|
| Name + age | y 12–48 | "Slimey" and "Day N  HHh", centred; 1 real minute = 1 game hour | name once, age line when the hour changes |
| Stage | y 0–179 | the slime, centred, 64×64 or 96×96 | blit the sprite region only |
| Food meter | y 184–196 | 160×12 bar + label | redraw **changed segment only** |
| Happiness meter | y 200–212 | 160×12 bar + label | redraw **changed segment only** |
| Menu | y 220–319 | three action items, selected one highlighted | redraw on selection change only |

Meters and menu are drawn with `ILI9341_FillRect` and `ILI9341_DrawString` — they
need **no artwork**.

## 7. Rendering rules (derived from measured SPI throughput)

SPI1 runs at 1.775 MHz ≈ 221,875 bytes/s, measured from this project's clock tree:

| Operation | Transfer time |
|---|---|
| Full screen 240×320 | **692 ms** |
| 64×64 sprite | 37 ms |
| 160×12 meter bar | 17 ms |
| One 5×7 character | 2.05 ms |
| 20-character label | 41 ms |

Therefore:

1. **Never redraw the whole screen.** Animate a 64×64 sprite into a fixed region.
2. **Update meters partially** — only the changed segment.
3. **Never redraw text every frame.** `ILI9341_DrawChar` calls `FillRect` per
   pixel (35 address-window setups per character).
4. **`static` buffers only.** The stack is 2 KB and the heap 512 B.
5. **Non-blocking, `HAL_GetTick()`-based.** No `HAL_Delay()` in the main loop, so
   the interface stays responsive while an animation runs.

## 8. Animation set

Minimum required: an idle animation plus a distinct animation for each of the three
actions, plus distinct death and runaway presentations.

| Animation | Frames | Notes |
|---|---|---|
| Idle | 4 | slow breathing squash/stretch, looped |
| Feed | 5 | berry enters, absorbed, ripple |
| Play | 5 | excited jiggle, bounce up |
| Clean | 5 | sparkle wash, brightness lift |
| Death | 4 | collapse into a flat puddle |
| Runaway | 4 | slide off the stage, fading |
| Restart | 3 | droplet re-forming into the slime |

Frame budget at 64×64 (8,192 bytes per frame): the 27 frames across 7 animations
*would* come to about **221 KB** if every frame were stored separately. They are
not. `pet_render.c` builds each frame by scaling one sprite per state, so the real
cost is **7 sprites × 8,192 = 56 KB**, roughly **5% of the 1 MB flash**. Even
allowing the ~120 KB that code and libraries are estimated to take, about 850 KB
of flash is left unused.

## 9. Implementation status

| Component | Status |
|---|---|
| Pet behaviour model (`Core/Src/pet_core.c`) | ✅ implemented, 60 host checks |
| Animation sequencer (`Core/Src/pet_anim.c`) | ✅ implemented, 54 host checks |
| State → animation mapping (`pet_anim_select`) | ✅ implemented |
| Sprite scaler / frame builder (`Core/Src/pet_render.c`) | ✅ implemented, 38 host checks |
| Sprite selection (`Core/Src/pet_view.c`) | ✅ implemented, 27 host checks |
| Menu / screen state machine (`Core/Src/pet_ui.c`) | ✅ implemented, 60 host checks |
| Display driver sprite blit (`ILI9341_DrawBitmap`) | ✅ implemented |
| PNG → RGB565 converter (`tools/png_to_rgb565.py`) | ✅ implemented |
| Artwork: all 7 states drawn and converted | ✅ done |
| HUD layout and meter maths (`Core/Src/pet_hud.c`) | ✅ implemented, 52 host checks |
| Action handling (`Core/Src/pet_ctl.c`) | ✅ implemented, 39 host checks |
| Screen drawing (`Core/Src/pet_screen.c`) | ✅ implemented |
| HUD icons (berry, heart) | ✅ drawn and converted |
| **`main.c` integration** | ✅ **done — the pet runs** |
| User button B1 wired to `PET_INPUT_BACK` | ❌ not wired up |
| I2C input: MPU-6050 shake → Play (`pet_shake`, `mpu6050`) | ✅ implemented, 27 host checks — not yet on hardware |
| Analog input: potentiometer → Feed portion (`pet_pot`) | ✅ implemented, 17 host checks — not yet on hardware |
| Verified on real hardware | ❌ blocked on the unsoldered headers |

**433 host checks total, all passing.** See [`../TODO.md`](../TODO.md) for what is
still outstanding.
