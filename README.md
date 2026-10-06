# ECGR 4101/5101 — Virtual Pet (Project 2)

Tamagotchi-style virtual pet running on a **NUCLEO-L496ZG-P** with an
**X-NUCLEO-GFX01M2** (2.2" QVGA ILI9341 SPI TFT + joystick) display expansion board.

> **Status:** starter project baseline committed; pet firmware not yet started.

---

## 1. The pet

**Name:** _TBD_
**Concept:** _TBD — describe the creature, its personality, and why it is original._
**Artwork:** master PNGs live in `docs/artwork/`. Converted RGB565 assets live in
`Drivers/BSP/GFX01M2/` (or wherever the build expects them) and are generated, not
hand-edited.

### Statistics and consequences

| Stat | Range | Changes over time | Changed by |
|---|---|---|---|
| Hunger | _TBD (documented limits)_ | _TBD rate_ | feeding |
| Happiness | _TBD (documented limits)_ | _TBD rate_ | play, petting, neglect |

| Outcome | Trigger | Screen / animation | Restart procedure |
|---|---|---|---|
| Death | hunger exceeds limit | _TBD_ | _TBD_ |
| Runaway | happiness falls below limit | _TBD_ | _TBD_ |

### Actions

| Action | Physical input | State effect | Animation |
|---|---|---|---|
| **Feed** (required) | _TBD_ | _TBD_ | _TBD_ |
| _Action 2_ | _TBD_ | _TBD_ | _TBD_ |
| _Action 3_ | _TBD_ | _TBD_ | _TBD_ |
| Idle | — | passive decay | idle loop |

---

## 2. Hardware

### Boards

| Item | Notes |
|---|---|
| NUCLEO-L496ZG-P | STM32L496ZGT6P, Cortex-M4F, 1 MB flash, 320 KB RAM |
| X-NUCLEO-GFX01M2 | 2.2" 240×320 QVGA TFT (ILI9341), SPI NOR flash, 5-way joystick |

### ⚠️ Assembly prerequisites and hazards

1. **Morpho headers must be soldered** on the NUCLEO-L496ZG-P. They are *not*
   populated from the factory, and the GFX01M2 cannot be attached without them.
2. **Orientation is destructive if wrong.** The GFX01M2 must be aligned with the
   *top* of the morpho headers exactly as shown in the course handout. Plugging it
   any other way may damage the board.
3. **Disconnect USB power before attaching or changing any display connection.**

### Pin map (from `Drivers/BSP/GFX01M2/gfx01m2_conf.h`)

| Signal | Pin | Notes |
|---|---|---|
| LCD SPI (SCK/MISO/MOSI) | `PA5` / `PA6` / `PA7` | `SPI1` |
| LCD chip select | `PA9` | ⚠️ see conflict note below |
| LCD data/command | `PB10` | high = data, low = command |
| LCD hardware reset | `PA1` | active low |
| Joystick LEFT | `PB6` | active low, internal pull-up |
| Joystick CENTER | `PC7` | active low, internal pull-up |
| Joystick DOWN | `PB4` | active low, internal pull-up |
| Joystick RIGHT | `PB0` | active low, internal pull-up |
| Joystick UP | `PC0` | active low, internal pull-up |

### ⚠️ `PA9` is claimed twice — do not initialise USB Host

`PA9` is both the **LCD chip select** and `USB_OTG_FS_VBUS` in the `.ioc`. The demo
avoids this only because it never calls `MX_USB_HOST_Init()`. The project still
compiles `USB_HOST/` and `main.h` still defines `USB_VBUS_Pin`, so **calling
`MX_USB_HOST_Init()` will reconfigure `PA9` and blank the display.** Leave USB host
uninitialised. Use `LPUART1` (`PG7`/`PG8`, already wired to the ST-LINK virtual COM
port) for any serial logging instead.

### Additional peripherals (required: ≥1 analog + ≥1 I2C/UART/SPI device)

_Pins to be confirmed in CubeMX against the map above before wiring._

- **Analog / ADC input:** _TBD_ — candidates `PC1`–`PC5`, `PA0`, `PA2`, `PA3`, `PA4`
- **I2C device:** _TBD_ — **use `I2C1` remapped to `PB8`/`PB9`**; the default
  `I2C1` pins `PB6`/`PB7` are already used by joystick LEFT and LD2
- **Additional output:** _TBD_ (e.g. PWM buzzer on a free timer channel)

---

## 3. Controls

| Input | Action |
|---|---|
| Joystick UP / DOWN | move menu selection |
| Joystick CENTER | confirm / activate |
| Joystick LEFT / RIGHT | _TBD_ |
| User button B1 (`PC13`) | _TBD_ |
| _Analog input_ | _TBD_ |
| _I2C device_ | _TBD_ |

---

## 4. Project structure

```
VirtualPet/
├── CMakeLists.txt              top-level CMake definition (project name is hardcoded)
├── CMakePresets.json           Debug / Release presets (Ninja, portable ${sourceDir})
├── Lab1_NUCLEO_L496.ioc        CubeMX project file
├── STM32L496xx_FLASH.ld        linker script
├── startup_stm32l496xx.s       startup / vector table
├── .clangd .settings/ .vscode/ IDE + toolchain configuration
├── Core/
│   ├── Inc/                    main.h, HAL config, IRQ handlers
│   └── Src/                    main.c, MSP, SysTick, system file
├── Drivers/
│   ├── BSP/GFX01M2/            ← display driver (ours to extend)
│   │   ├── ili9341.c/.h        ILI9341 SPI driver: the graphics interface
│   │   ├── gfx01m2_conf.h      pin mapping for this board
│   │   └── gfx_font5x7.c/.h    5x7 bitmap font
│   ├── CMSIS/                  ARM core headers
│   └── STM32L4xx_HAL_Driver/   ST HAL
├── Middlewares/                ST USB host library (compiled, NOT initialised)
├── USB_HOST/                   USB host application (unused — see PA9 warning)
└── docs/
    ├── artwork/                original master PNGs
    └── ai-use-log.md           running log of AI tools and their contributions
```

### Graphics interface (`ili9341.h`)

Supplied: `ILI9341_Init`, `FillScreen`, `DrawPixel`, `FillRect`, `DrawHLine`,
`DrawVLine`, `DrawLine`, `DrawRect`, `DrawChar`, `DrawString`.

**Not supplied, must be written by the team:** an RGB565 **bitmap/sprite blit**
(and a region-update helper). `LCD_SetAddressWindow()` is `static` inside
`ili9341.c`, so any blit has to be implemented in that file.

---

## 5. Build and flash

### Prerequisites

- VS Code with **STM32CubeIDE for VS Code** (`stmicroelectronics.stm32-vscode-extension`)
- Bundle Manager toolchain installed (ARM GCC 14.3.1, CMake, Ninja, ST-LINK GDB server)
- **Do not** install Cortex-Debug or the C/C++ extension pack — they conflict with the
  bundled clangd and debug adapter

### Build

Open the folder in VS Code and either:

- `Cmd+Shift+P` → **`CMake: Build`**, or
- press **`F5`** — the launch config has `preBuild`, so it builds then flashes

From a terminal:

```bash
cmake -S . -B build/Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake
cmake --build build/Debug
```

### Flash

**Run and Debug** (`Cmd+Shift+D`) → **`STM32Cube: Launch ST-Link GDB Server`** → `F5`.
`Shift+F5` stops the session; the image stays in flash. Press the board's black
**RESET** to re-run standalone.

CLI alternative:

```bash
STM32_Programmer_CLI -c port=SWD mode=normal \
  -w build/Debug/Lab1_NUCLEO_L496.elf -v -rst
```

### Smoke test

Build and flash the **unmodified** starter before adding any pet code. Verify the
display clears to several colours, draws text and rectangles, and responds to the
joystick. If the screen stays blank, check power, wiring, board revision and SPI
pins **before** debugging application code.

---

## 6. Performance constraints (measured, not guessed)

SPI1 runs at **1.775 MHz** (`PCLK2 56.8 MHz ÷ 32`) ≈ **221,875 bytes/s**:

| Operation | Transfer time |
|---|---|
| Full screen 240×320 | **692 ms** |
| 128×128 sprite | 148 ms |
| 64×64 sprite | 37 ms |
| One 5×7 character | 2.05 ms |
| A 20-character label | 41 ms |

**Design rules that follow from this:**

1. **Never redraw the whole screen per frame.** Animate by blitting a sprite into a
   fixed region.
2. **Update meters partially** — redraw only the changed segment of a bar.
3. **Never redraw text every frame.** `DrawChar` calls `FillRect` *per pixel*
   (35 address-window setups per character), so cache labels and update only on change.
4. **The stack is only 2 KB** (`_Min_Stack_Size = 0x800`) and the heap 512 B. Use
   `static` buffers, never large locals — the existing driver already does this.
5. A non-blocking, `HAL_GetTick()`-based scheduler is required so the UI stays
   responsive during animations. **Do not use `HAL_Delay()` in the main loop.**

### Sprite memory budget (RGB565 = 2 bytes/pixel)

| Sprite | Bytes |
|---|---|
| 240×320 (full frame) | 153,600 |
| 128×128 | 32,768 |
| 96×96 | 18,432 |
| **64×64** | **8,192** |

With ~120 KB for code and libraries, ~904 KB remains for assets. A 64×64 sprite at
6 frames × 6 animations = 288 KB (~28% of flash). **Recommended sprite size: 64×64.**

### Panel colour order

`MADCTL = 0x08` selects **BGR**, and the driver writes the high byte of each RGB565
word first (big-endian on the wire). Converted bitmap assets must match this
convention or red and blue will be swapped.

---

## 7. Team

| Member | GitHub | Primary responsibility |
|---|---|---|
| Anthony Kang | @Dokkae-bi | sprite artwork, asset pipeline |
| _TBD_ | _TBD_ | |
| _TBD_ | _TBD_ | |

### Individual contributions

_TBD — each member records what they implemented as work progresses._

---

## 8. AI tools used

Generative AI tools are used in this project. Per the project brief, all AI-assisted
code is tested on real hardware, peripheral and register names are verified against
this project, invented APIs are removed, and the team can explain any subsystem it
submits.

| Tool | Used for |
|---|---|
| _TBD_ | _TBD_ |

### Case study: AI suggestion that had to be corrected

**Recommendation rejected:** an early suggestion was to attach a potentiometer to the
ADC and an MPU-6050 accelerometer to **`I2C1` on its default pins `PB6`/`PB7`**.

On checking `Drivers/BSP/GFX01M2/gfx01m2_conf.h` against the pin map, `PB6` is the
GFX01M2's **joystick LEFT** input and `PB7` drives the on-board **LD2** LED. The
default `I2C1` pin assignment is therefore unusable on this board, and the peripheral
plan had to be changed to remap `I2C1` to `PB8`/`PB9`. This is exactly the kind of
invented/unverified detail the brief warns about: the suggestion was plausible in
general but wrong for this specific board, and was only caught by checking the
actual configuration header.

Similarly, an early plan to blink the green **LD1** (`PC7`) for a status indicator was
discarded after the same header showed `PC7` is the **joystick CENTER** input.

_Additional cases will be appended to `docs/ai-use-log.md` as the project proceeds._

---

## 9. Repository conventions

- `.gitignore` excludes `build/` (generated artifacts hold absolute paths and must
  never be committed), `mx.scratch`, and macOS `.DS_Store`.
- Commit messages describe the change (e.g. "Implement two-frame feeding animation"),
  not merely that a change happened.
- Every team member commits their own work under their own GitHub account.
- Do not squash or rewrite history before submission — the visible history is
  assessed.
