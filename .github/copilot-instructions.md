# Copilot instructions

## Build and test

- Build the STM32 firmware from the repository root with the ST ARM GNU toolchain:
  ```sh
  cmake -S . -B build/Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake
  cmake --build build/Debug
  ```
  Use the ST bundle's `arm-none-eabi-gcc` on `PATH`, not the Homebrew toolchain;
  see [README.md](../README.md) for setup details and flashing.
- Host tests are standalone C programs, not CMake targets. To run one suite:
  ```sh
  cc -std=c11 -Wall -Wextra -I Core/Inc tests/test_pet_core.c \
     Core/Src/pet_core.c -o /tmp/test_pet_core && /tmp/test_pet_core
  ```
  Compile another `tests/test_*.c` with the corresponding implementation source(s);
  some suites need additional sources or include paths. See the host-test section
  in [README.md](../README.md).

## Architecture

- `Core/Src/main.c` owns HAL initialization and the non-blocking main loop. It polls
  the joystick, ADC potentiometer, and I2C2 MPU-6050, then routes input through
  the pet control/UI modules and refreshes the display.
- `pet_core` is the hardware-independent pet model; `pet_anim`, `pet_render`, and
  `pet_view` map state/actions to composed sprite frames. `pet_ctl` keeps model
  changes and animation in sync. `pet_ui` handles menu/input state, `pet_hud`
  supplies layout and meter calculations, and `pet_screen` draws changed regions
  through the ILI9341 driver.
- `mpu6050` is the HAL-backed sensor driver; `mpu6050_regs`, `pet_shake`, and
  `pet_tilt` keep register decoding and sensor interpretation independently
  testable. The generated RGB565 sprite and icon sources are in `sprites/`.
- Read [docs/pet-design.md](../docs/pet-design.md) for behavior/design rationale,
  [docs/artwork/README.md](../docs/artwork/README.md) for asset constraints, and
  [TODO.md](../TODO.md) for outstanding work.

## Repository-specific conventions

- Keep game, animation, and UI timing elapsed-time based: pass millisecond
  timestamps into logic modules and use non-blocking polling in the main loop.
  Do not add `HAL_Delay()` to the loop. Keep large buffers off the stack; firmware
  stack space is 2 KB.
- Add application and generated-asset sources to `target_sources` in the root
  `CMakeLists.txt`. Do not put them in `cmake/stm32cubemx/CMakeLists.txt`, which
  CubeMX regenerates. Preserve the required `USER CODE` sections in `main.c` and
  `stm32l4xx_hal_msp.c`; I2C2 and ADC3 setup is hand-maintained because the `.ioc`
  does not describe those peripherals. Do not edit vendor HAL files or `syscalls.c`.
- Do not initialize USB Host: `PA9` is also the display chip-select. Keep the
  MPU-6050 on I2C2 (`PF1`/`PF0`); I2C1's default `PB6`/`PB7` conflicts with the
  joystick and LD2. The full pin map and CubeMX regeneration warning are in
  [README.md](../README.md).
- Avoid full-screen redraws; update only changed display regions. Generate sprite
  C/H files from the master artwork with `tools/png_to_rgb565.py` rather than
  editing generated pixel arrays by hand.
