# ECGR 4101/5101 — Virtual Pet (Project 2)

Team repository for the Tamagotchi-style virtual pet project.

Hardware: **NUCLEO-L496ZG-P** with the **X-NUCLEO-GFX01M2** (2.2" QVGA ILI9341 SPI
TFT + joystick) display expansion board.

> **This branch is the untouched baseline.** `main` holds the instructor's
> X-NUCLEO-GFX01M2 LCD starter project as delivered, so the smoke test can be run
> against a known-good starting point before anything else is added.
>
> **The pet implementation lives on the
> [`pet`](https://github.com/Dokkae-bi/Project-2-Virtual-Pet/tree/pet) branch** —
> the pet model, the animation system, the artwork and converter, the menu, the
> meters, and the `main.c` that runs it all.

---

## 1. What is in this branch

The starter project as delivered, plus the repository setup around it:

| Path | What |
|---|---|
| `Core/`, `Drivers/`, `Middlewares/`, `USB_HOST/`, `cmake/` | the starter project |
| `CMakeLists.txt`, `CMakePresets.json` | build definition and Debug/Release presets |
| `Lab1_NUCLEO_L496.ioc` | CubeMX project file |
| `.vscode/` | ST-Link GDB launch configuration |
| `docs/` | AI-use log and artwork pipeline notes |
| `.gitignore` | excludes `build/`, `mx.scratch`, `.DS_Store` |

**Not in this branch:** the pet firmware. `Core/Src/main.c` is still the
instructor's LCD demo — colour bars, shapes, text and a joystick readout.

## 2. Hardware

| Item | Notes |
|---|---|
| NUCLEO-L496ZG-P | STM32L496ZGT6P, Cortex-M4F, 1 MB flash, 320 KB RAM |
| X-NUCLEO-GFX01M2 | 2.2" 240×320 QVGA TFT (ILI9341), SPI NOR flash, 5-way joystick |

### ⚠️ Before attaching the display

1. **The morpho male headers must be soldered** to the NUCLEO-L496ZG-P. They are
   not fitted at the factory, and the GFX01M2 cannot be attached without them.
2. **Orientation is destructive if wrong.** Align the GFX01M2 with the *top* of the
   morpho headers exactly as the course handout shows. Any other orientation may
   damage the board.
3. **Disconnect USB power before attaching or changing any display connection.**

## 3. Pin map (from `Drivers/BSP/GFX01M2/gfx01m2_conf.h`)

| Signal | Pin | Notes |
|---|---|---|
| LCD SPI (SCK/MISO/MOSI) | `PA5` / `PA6` / `PA7` | `SPI1` |
| LCD chip select | `PA9` | ⚠️ shared with `USB_OTG_FS_VBUS` — see below |
| LCD data/command | `PB10` | high = data, low = command |
| LCD hardware reset | `PA1` | active low |
| Joystick LEFT | `PB6` | active low, internal pull-up |
| Joystick CENTER | `PC7` | active low, internal pull-up |
| Joystick DOWN | `PB4` | active low, internal pull-up |
| Joystick RIGHT | `PB0` | active low, internal pull-up |
| Joystick UP | `PC0` | active low, internal pull-up |

### ⚠️ `PA9` is claimed twice — do not initialise USB Host

`PA9` is both the **LCD chip select** and `USB_OTG_FS_VBUS` in the `.ioc`. This
only works because the demo never calls `MX_USB_HOST_Init()`. The project still
compiles `USB_HOST/` and `main.h` still defines `USB_VBUS_Pin`, so **calling
`MX_USB_HOST_Init()` will reconfigure `PA9` and blank the display.** Leave USB host
uninitialised.

## 4. Build and flash

Prerequisites: VS Code with the **STM32CubeIDE for VS Code** extension and its
Bundle Manager toolchain (ARM GCC, CMake, Ninja, ST-LINK GDB server). **Do not**
install Cortex-Debug or the C/C++ extension pack — they conflict with the bundled
clangd and debug adapter.

```bash
cmake -S . -B build/Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake
cmake --build build/Debug
```

In VS Code: `Cmd+Shift+P` → **`CMake: Build`**, or press **`F5`** to build and
flash. To flash from a terminal:

```bash
STM32_Programmer_CLI -c port=SWD mode=normal \
  -w build/Debug/Lab1_NUCLEO_L496.elf -v -rst
```

### Smoke test

Build and flash this unmodified starter before adding anything else. Confirm the
display clears to several colours, draws text and rectangles, and responds to the
joystick. If the screen stays blank, check power, wiring, board revision and SPI
pin assignments **before** debugging application code.

## 5. Repository conventions

- `.gitignore` excludes `build/` (generated artifacts hold absolute paths and must
  not be committed), `mx.scratch`, and macOS `.DS_Store`.
- Commit messages describe the change, not merely that a change happened.
- Every team member commits their own work under their own GitHub account.
- Do not squash or rewrite history before submission — the visible history is
  assessed.
