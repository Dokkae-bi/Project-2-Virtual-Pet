# Project 2 - what's left to do

**As of 2026-10-13.** The pet's logic and artwork are finished and tested, but
**none of it is wired into the program yet**. `main.c` is still the instructor's
demo, so flashing the board today shows colour bars and a row of 0s and 1s - not
the pet.

If you're picking this up: **start with task 1**, everything else depends on it.

---

## What's already done

| Where | What |
|---|---|
| `Core/Inc` + `Core/Src` | `pet_core`, `pet_anim`, `pet_render`, `pet_view`, `pet_ui` - all the pet's behaviour |
| `tests/` | 225 checks, all passing, run on a PC with **no board attached** |
| `sprites/` | 7 sprites converted to RGB565 (64x64, 16 colours each) |
| `tools/png_to_rgb565.py` | the converter, re-runnable whenever the artwork changes |
| `Drivers/BSP/GFX01M2/ili9341.c` | added `ILI9341_DrawBitmap` - the supplied BSP had no way to draw an image at all |

None of the pet modules touch the HAL. They take a millisecond timestamp as an
argument and hand back plain data, which is why the tests run on a laptop. The
animation frames are produced by **scaling one sprite per state**, so there are no
extra drawings to make.

---

## Task 1 - wire it up in `main.c` (start here)

This is the biggest job and the one that turns this from a library into a
program. It's roughly 100 lines.

Replace the `while (1)` demo loop at the bottom of `main.c`. Everything it calls
already exists and is tested:

```c
static pet_t      pet;
static pet_anim_t anim;
static pet_ui_t   ui;

pet_init(&pet, HAL_GetTick());
pet_anim_start(&anim, PET_ANIM_IDLE, HAL_GetTick());
pet_ui_init(&ui);

while (1)
{
  uint32_t now = HAL_GetTick();

  pet_tick(&pet, now);              /* advance hunger/happiness */
  pet_anim_update(&anim, now);      /* advance the animation  */

  pet_input_t in = read_input_event();   /* see below */
  pet_ui_result_t r = pet_ui_handle(&ui, in, &pet);

  if (r.restart) {
    pet_restart(&pet, now);
    pet_anim_start(&anim, PET_ANIM_RESTART, now);
  } else if (r.action != PET_ACTION_NONE) {
    bool ok = false;
    switch (r.action) {
      case PET_ACTION_FEED:  ok = pet_feed(&pet, portion_from_adc(), now); break;
      case PET_ACTION_PLAY:  ok = pet_play(&pet, now);  break;
      case PET_ACTION_CLEAN: ok = pet_clean(&pet, now); break;
      default: break;
    }
    if (ok) { pet_anim_start(&anim, pet_anim_for_action(r.action), now); }
  }

  draw(&pet, &anim, &ui, r.redraw);
}
```

Then delete the demo functions (`Demo_ColorBars`, `Demo_Shapes`, `Demo_Text`,
`Demo_Joystick`) and `Joystick_Read` if you don't reuse it.

### `read_input_event()`

`Joystick_Read()` already exists in `main.c` and returns bit masks for all five
directions - just map those onto `pet_input_t` (`PET_INPUT_UP`, `_DOWN`,
`PET_INPUT_SELECT` for centre, etc).

**Important: it must be edge triggered.** The loop runs thousands of times a
second, so if you return `PET_INPUT_SELECT` on every pass while the button is
held, one tap will feed the pet a few thousand times. Store the previous mask and
only report a press on the transition from not-pressed to pressed.

`PET_INPUT_BACK` is the user button `B1` (PC13). It is set up as a rising-edge
interrupt in `MX_GPIO_Init`, **but the EXTI interrupt is never enabled or handled**
- there is no `EXTI15_10_IRQHandler` in `stm32l4xx_it.c`. Poll it in the loop
(simplest) or enable the EXTI yourself.

### `draw()`

```c
static uint16_t fb[PET_SPRITE_W * PET_SPRITE_H];   /* 8 KB */

pet_view_compose_stage(fb, PET_SPRITE_W, PET_SPRITE_H, &pet, &anim);
ILI9341_DrawBitmap(sprite_x, sprite_y, PET_SPRITE_W, PET_SPRITE_H, fb);
```

Only redraw when something actually changed - `r.redraw` is true when the screen
changed, and otherwise redraw when the animation frame index moved. A full-frame
repaint every pass will make the display flicker and waste the whole SPI budget.

---

## Task 2 - the two peripherals

The brief requires **at least one analog input** and **at least one digital
device on I2C/UART/SPI**. The code is written to accept these but nothing reads
them yet. Decide on the parts first:

| Decision | Notes |
|---|---|
| Analog input + pin | `PC1`-`PC5`, `PA0`, `PA2`-`PA4` are free. Drives the feed portion, which the menu already displays. Values clamp to `PET_FEED_PORTION_MIN`..`MAX` (1-5). |
| I2C device + address | MPU-6050 at `0x68` is the easy option. |
| Optional third output | A PWM buzzer is easy marks but not required. |

**Use `I2C1` remapped to `PB8`/`PB9`.** The CubeMX default is `PB6`/`PB7`, and
**`PB6` is the joystick LEFT pin** - you'd lose the left direction, and `PB7` is
`LD2`. Also enable the pull-ups: the joystick pins are active low with internal
pull-ups.

`pet_feed()` already clamps whatever the ADC gives it, so a noisy reading just
means a bigger or smaller meal rather than anything breaking.

---

## Task 3 - draw the status readout and the menu

Nothing draws the hunger/happiness values or the action menu yet. Everything you
need is there:

- `pet.hunger`, `pet.happiness` (0-100), `pet_state_name()`, `pet_mood_name()`
- `pet_ui_item_count()`, `pet_ui_item_label(i)`, `pet_ui_cursor()`, `pet_ui_portion()`
- `pet_ui_screen()` to tell you whether to draw the pet or the menu
- `ILI9341_DrawString`, `ILI9341_DrawRect`, `ILI9341_FillRect`

Suggested layout: sprite centred around `y = 60`, two labelled bars for the
statistics, and the menu drawn over the middle of the screen when
`pet_ui_screen() == PET_SCREEN_MENU`, with the row at `pet_ui_cursor()`
highlighted.

When the pet is finished, `pet_is_interactive()` returns false and `pet_ui` turns
SELECT into a restart request - so show a "press select to restart" prompt on the
death and runaway screens so the user knows.

---

## Task 4 - the paperwork

- `README.md` still has about 18 `_TBD_` entries: the pet's **name**, its concept,
  the three-actions table, the hunger/happiness **limits and rates**, the pin
  assignment table, and the team member list. Almost all of it is now answerable
  from the code - the limits, rates and action effects are all `#define`d at the
  top of `Core/Inc/pet_core.h`.
- `docs/ai-use-log.md` has two blank rows. The assignment wants at least one case
  where AI output had to be corrected or rejected. There are three good ones
  already recorded at the bottom of that file, plus the uint8 colour bug described
  in the commit history.
- Add the other team members as collaborators on the GitHub repo (needs their
  usernames).

## Task 5 - video and slides

Both need a working demo, so they come after task 1. The video is 5 marks and the
slides are 10, so don't leave them to the last minute.

---

## Blocked on hardware

**The morpho male headers are not soldered to the NUCLEO-L496ZG-P**, and they are
required to attach the X-NUCLEO-GFX01M2. Until that's done, nobody can verify:

- that the panel lights up at all, and that **red and blue aren't swapped**
- that the sprites look right on screen and the orientation is correct
- that the joystick works
- any of the peripheral wiring
- the demo itself

Do this first if you can - it gates every visual check on the list.

---

## Things that will bite you

1. **Never use `HAL_Delay` in the main loop.** The demo has `HAL_Delay(50)` in it;
   that has to go. Everything is elapsed-time based so a blocked loop freezes the
   pet's clock and the animation, and it fails the "interface stays responsive
   while animating" requirement.
2. **Don't hand-swap the byte order** in the sprite arrays. `ILI9341_DrawBitmap`
   already converts RGB565 to what the panel wants. The arrays are plain RGB565 on
   purpose. (The panel itself is configured BGR.)
3. **`PET_STAGE_COLOR` in `pet_view.h` must match the converter's `--stage`.** It's
   `0x1946` (`#1E2A32`) and the converter snaps every non-sprite pixel to exactly
   that value. Change one without the other and you get a visible rectangle around
   the pet.
4. **The sprites are opaque**, composited onto the stage colour at conversion time.
   Don't add a transparency test - the whole point is one SPI transfer per sprite.
5. **Don't enable USB host.** It reclaims `PA9`, which is also the LCD chip select,
   and the display goes blank.
6. **`PA9` is LCD_CS *and* USB_VBUS** on this board - see the note at the top of
   `gfx01m2_conf.h`. Another reason to leave USB alone.
7. **Watch the SRAM.** A full-frame `240x320` RGB565 buffer is **150 KB** of the
   320 KB available, and two of them is 300 KB. Either draw the pet into a small
   sprite-sized buffer (8 KB for 64x64, which is what `pet_view_compose_stage` is
   written for) or plan the memory carefully.
8. **Don't edit the files CubeMX generates**, and don't edit `syscalls.c` or the
   HAL drivers in `Drivers/STM32L4xx_HAL_Driver/`. New sources go in
   `CMakeLists.txt` under `target_sources`, not in `cmake/stm32cubemx/`.

---

## Building and testing

**Host tests** - no board needed, run these after every change:

```sh
cc -std=c11 -Wall -Wextra -I Core/Inc tests/test_pet_core.c Core/Src/pet_core.c \
   -o /tmp/test_pet_core && /tmp/test_pet_core
```

Same pattern for `test_pet_anim` (add `pet_core.c`), `test_pet_render` (add
`pet_anim.c pet_core.c`) and `test_pet_ui` (add `pet_core.c`). For
`test_pet_view` you also need `-I sprites` and `sprites/slime_*.c`.

Expected totals: **46 / 54 / 38 / 60 / 27 = 225 checks.**

**Firmware build** - use the STM32 VS Code extension, or from a terminal with the
CMake preset:

```sh
cmake --build build/Debug
```

**Regenerating sprites** after changing artwork:

```sh
python3 tools/png_to_rgb565.py docs/artwork/slime-pose-set-v1.png \
    --grid 2x2 --cell 0,0 --name slime_content --size 64 --colors 16 --out-dir sprites/
```

(Needs `Pillow` and `numpy`.)

**Always check the preview PNG after converting.** `sprites/<name>_preview.png` is
decoded back out of the emitted RGB565, so it shows exactly what the board will
draw - if the colours look wrong there, they're wrong.
