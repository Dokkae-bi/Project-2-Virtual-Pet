# Project 2 — what's left to do

**As of 2026-10-13.** The pet runs. `main.c` starts it, the joystick drives the
menu, and the sprite, both meters and the menu are drawn on the panel. **303 host
checks pass with no board attached.**

What remains is the team's peripherals, wiring up the second input, filling in the
paperwork, and verifying any of it on hardware.

---

## What's already done

| Where | What |
|---|---|
| `pet_core` | pet model: hunger/happiness, limits, rates, action effects |
| `pet_anim` | animation sequencer + state→animation mapping |
| `pet_render` | builds every animation frame by scaling one sprite |
| `pet_view` | sprite selection and frame composition |
| `pet_ui` | screen and menu state machine |
| `pet_hud` | 240×320 layout and meter maths |
| `pet_ctl` | applies an action to the pet and starts its animation |
| `pet_screen` | draws the screen — the only HAL-facing pet module |
| `main.c` | starts the pet, reads the joystick, runs the loop |
| `sprites/` | 7 slime states + 2 HUD icons, all converted |
| `tests/` | 303 checks across 7 suites, all passing |
| `tools/png_to_rgb565.py` | the artwork converter, re-runnable |
| `Drivers/BSP/GFX01M2/ili9341.c` | added `ILI9341_DrawBitmap` — the supplied BSP had no way to draw an image |

Only `pet_screen.c` and `main.c` touch the hardware. Everything above them runs on
a laptop, which is how the whole thing was built without a board.

---

## 1. Wire up the user button B1 (small)

`pet_ui` supports `PET_INPUT_BACK` — close the menu without acting — but nothing
produces that event, so the path is unreachable.

B1 is `PC13`. In `MX_GPIO_Init` it is set to `GPIO_MODE_IT_RISING`, **but the EXTI
interrupt is never enabled and there is no `EXTI15_10_IRQHandler`**, so the
interrupt never fires. Polling it is simpler:

- add a `HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin)` check to `Input_Read()` in `main.c`
- the rising-edge config implies **pressed = `GPIO_PIN_SET`**, but confirm that on
  real hardware before trusting it — the polarity has not been verified here
- it needs the same edge-detection treatment as the joystick, or one press fires
  hundreds of times

---

## 2. The two peripherals (the team's part)

The brief needs **at least one analog input** and **at least one digital device on
I2C, UART or SPI**. The LCD and its built-in joystick explicitly do not count — the
handout says "in addition to the LCD and its built-in joystick".

Nothing is chosen or wired yet. Whatever the team picks, the code is ready for it:

```c
/* The analog input feeds this. pet_feed() clamps it, so a noisy reading
   just gives a bigger or smaller meal rather than breaking anything. */
pet_ctl_action(&g_pet, PET_ACTION_FEED, portion_from_adc(), now);
```

On the **digital device on a bus**: a bare 7-segment display is GPIO, not a bus
device, and would cost 8 pins. One with a **MAX7219** (SPI) or **HT16K33** (I2C)
driver chip genuinely counts. A **TM1637** does not — its own datasheet says the
protocol "is not equal to I2C bus protocol totally because there is no slave
address".

**Use `I2C1` remapped to `PB8`/`PB9`.** The CubeMX default is `PB6`/`PB7`, and
**`PB6` is the joystick LEFT pin** — you would silently lose a direction.

---

## 3. Let a peripheral trigger an action directly (optional)

All three actions come from the menu at the moment. The brief scores "Feed plus two
additional actions; each includes physical input", so if a peripheral is meant to
trigger one directly, call the same entry point the menu uses:

```c
pet_ctl_action(&g_pet, PET_ACTION_PLAY, 1u, HAL_GetTick());
```

That is the whole interface. It applies the same state change and plays the same
animation the menu would, so the pet behaves identically whichever input caused it.
A refused action (pet dead or run away) does not animate.

---

## 4. The paperwork

- `README.md` still has `_TBD_` entries in the **team table** (names and GitHub
  handles), and `docs/ai-use-log.md` has two blank rows. Everything else in the
  README is now filled in from the code.
- Add the other team members as collaborators on the GitHub repo.
- The pet's name is still a working name (`Bloop`).

## 5. Video and slides

Both need a working demo on hardware, so they come last. The video is 5 marks and
the slides 10 — don't leave them to the last minute.

---

## Blocked on hardware

**The morpho male headers are not soldered to the NUCLEO-L496ZG-P**, and they are
required to attach the X-NUCLEO-GFX01M2. Until that happens, nobody can check:

- that the panel lights up at all, and that **red and blue aren't swapped**
- that the sprites and icons look right, and the orientation is correct
- that the joystick and B1 work
- any of the peripheral wiring
- **the main loop has never once run on real hardware**

Everything compiles and links, but none of it has been observed working. Treat the
first hardware session as a bring-up, not a formality.

---

## Things that will bite you

1. **Never add `HAL_Delay` to the main loop.** Everything is elapsed-time driven,
   so blocking freezes the pet's clock and the animation, and it fails the
   "interface stays responsive while animating" requirement. The demo's
   `HAL_Delay(50)` is gone; don't reintroduce the pattern.
2. **The joystick must stay edge-triggered.** `Input_Read()` reports only on the
   released-to-held transition. Returning the held state every pass acts on one
   press hundreds of times.
3. **Don't hand-swap the sprite byte order.** `ILI9341_DrawBitmap` converts RGB565
   to what the panel wants. The arrays are plain RGB565 on purpose.
4. **`PET_STAGE_COLOR` in `pet_view.h` must match the converter's `--stage`.** It is
   `0x1946` and the converter snaps every non-sprite pixel to exactly that value.
   Change one without the other and a rectangle shows around the pet.
5. **Don't enable USB host.** It reclaims `PA9`, which is also the LCD chip select,
   and the display goes blank. `USB_HOST` is still enabled in the `.ioc`.
6. **Watch the panel update cost.** A full 240×320 repaint is **692 ms** over SPI and
   one 64×64 sprite blit is **37 ms**. `pet_screen_update()` only redraws what
   changed for that reason — keep it that way.
7. **The stack is 2 KB.** The pet frame buffer is `static` for that reason. No large
   locals.
8. **Don't edit the files CubeMX generates**, or `syscalls.c`, or the HAL drivers.
   New sources go in `CMakeLists.txt` under `target_sources`, **not** in
   `cmake/stm32cubemx/CMakeLists.txt`.

---

## Building and testing

**Host tests** — no board needed, run these after every change:

```sh
cc -std=c11 -Wall -Wextra -I Core/Inc tests/test_pet_core.c Core/Src/pet_core.c \
   -o /tmp/test_pet_core && /tmp/test_pet_core
```

Same pattern for the others:

| Suite | Extra sources to add |
|---|---|
| `test_pet_anim` | `pet_core.c` |
| `test_pet_render` | `pet_anim.c pet_core.c` |
| `test_pet_ui` | `pet_core.c` |
| `test_pet_hud` | `pet_ui.c pet_core.c` |
| `test_pet_ctl` | `pet_anim.c pet_core.c` |
| `test_pet_view` | `pet_render.c pet_anim.c pet_core.c`, plus `-I sprites` and `sprites/slime_*.c` |

Expected totals: **46 / 54 / 38 / 60 / 27 / 39 / 39 = 303 checks.**

**Firmware build** — use the STM32 VS Code extension, or:

```sh
cmake --build build/Debug
```

Current build: **78,760 bytes of flash, 12,352 bytes of RAM.**

**Regenerating artwork** after changing it:

```sh
python3 tools/png_to_rgb565.py docs/artwork/slime-pose-set-v1.png \
    --grid 2x2 --cell 0,0 --name slime_content --size 64 --colors 16 --out-dir sprites/
```

(Needs `Pillow` and `numpy`.)

**Always check the preview PNG after converting.** `sprites/<name>_preview.png` is
decoded back out of the emitted RGB565, so it shows exactly what the board will
draw. If the colours look wrong there, they are wrong.
