# AI use log

Running record of generative-AI assistance on this project. The project brief
requires the team to identify the AI tools used, summarise how they contributed,
and cite **at least one case where a generated suggestion had to be corrected or
rejected**. This file is that evidence, written as the work happens — it cannot be
reconstructed honestly at the end.

## Ground rules we hold ourselves to

1. Every AI-assisted change is **tested on the real hardware** before it is trusted.
2. Peripheral, register, handle and pin names are **verified against this repo**
   (`.ioc`, `main.h`, `gfx01m2_conf.h`) — never accepted on plausibility.
3. Invented APIs and unsupported library calls are **removed**.
4. Every member can **explain** any subsystem they submit.

## Tools used

| Tool | Areas assisted |
|---|---|
| _TBD_ | _TBD_ |
| _TBD_ | _TBD_ |

---

## Cases where AI output was corrected or rejected

### Case 1 — I2C1 default pins are unusable on this board (rejected)

**Suggested:** connect the required I2C sensor to `I2C1` on its default pins,
`PB6` (SCL) and `PB7` (SDA).

**Problem:** checked against `Drivers/BSP/GFX01M2/gfx01m2_conf.h`:

| Pin | Actually used by |
|---|---|
| `PB6` | GFX01M2 **joystick LEFT** (active low, internal pull-up) |
| `PB7` | on-board **LD2** LED |

So the default assignment collides with the display board's joystick and the user
LED. The suggestion was plausible in general and wrong for this specific board.

**Resolution:** remap `I2C1` to `PB8`/`PB9`, and verify the chosen pins against the
GFX01M2 pin map *before* wiring anything.

**Lesson:** a generically-correct peripheral configuration is not evidence that it
is correct for this board. Always diff the suggestion against the actual
configuration header.

### Case 2 — green LD1 as a status indicator (rejected)

**Suggested:** blink the on-board green LED `LD1` to indicate pet state.

**Problem:** `LD1` is on `PC7`, and the same header shows `PC7` is the GFX01M2's
**joystick CENTER** input. With the display attached, `PC7` is an active-low input
with an internal pull-up, not a spare output.

**Resolution:** dropped. Status is shown on the LCD instead.

### Case 3 — `PA9` double-claim / "just initialise USB host" (rejected)

**Suggested:** the USB host middleware that the generated project already compiles
could be initialised to add a peripheral without extra hardware.

**Problem:** `PA9` is simultaneously the **LCD chip select** (`LCD_CS_Pin` in
`gfx01m2_conf.h`) and `USB_OTG_FS_VBUS` in the `.ioc`. Initialising USB host
reconfigures `PA9` and the display goes blank. The BSP header flags this directly.

**Resolution:** USB host stays uninitialised; `LPUART1` (`PG7`/`PG8`, already wired
to the ST-LINK virtual COM port) is used for serial logging instead.

---

## Template for new entries

```
### Case N — short title (accepted / corrected / rejected)

**Suggested:**
**Problem:**            (what was wrong, and how it was detected)
**Resolution:**         (what replaced it)
**Verified by:**        (build output, hardware observation, register read, ...)
```
