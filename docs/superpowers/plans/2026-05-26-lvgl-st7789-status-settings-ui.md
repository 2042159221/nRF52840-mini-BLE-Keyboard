# LVGL ST7789 Status Settings UI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a flashy but resource-conscious LVGL + ST7789V local status/settings screen while keeping the existing 17-LED WS2812 RGB chain enabled.

**Architecture:** Use a dual-SPIM board layout: ST7789V owns SPI3 on the screen connector pins, while WS2812 moves to a separate SPIM instance with MOSI on P0.20 and an unconnected SCK on P1.04. The UI is split into a pure-C model/input state machine and a CONFIG_LVGL view layer so navigation can be unit-tested without display hardware.

**Tech Stack:** Zephyr/NCS v3.2.3, nRF52840, Devicetree, LVGL, ST7789V MIPI DBI SPI, WS2812 SPI LED strip, assert-based C tests.

---

## Requirements Snapshot

- Screen pins from `docs/hardware_resources_agent_netlist.md`: SCK P1.13, MOSI P0.28, CS P0.02, DC P0.03, RESET P1.10, BLK P1.11.
- RGB pins from the same netlist: DATA P0.20, power enable P0.13, 17 WS2812 LEDs.
- ST7789V active area is 320 x 172 with x offset 0 and y offset 34.
- Backlight uses PWM0 CH0 on P1.11, 100 Hz, inverted polarity, exposed through Zephyr LED style control.
- UI has no Esc dependency; every settings level has visible Exit.
- EC11 controls UI while UI is active; otherwise it preserves the current HID consumer behavior.
- Build commands must use `.\scripts\build.ps1` or explicit NCS environment, never bare `west` or bare `python`.

## Task 1: Board RGB/ST7789 Coexistence

**Files:**
- Modify: `boards/mingDev/mini-keyboard/mini-keyboard.dts`
- Modify: `boards/mingDev/mini-keyboard/mini-keyboard-pinctrl.dtsi`
- Modify: `prj.conf`
- Optional create: `include/display/backlight.h`
- Optional create: `src/display/backlight.c`

- [ ] **Step 1: Move RGB off SPI3**

Keep `rgb_strip` enabled with `chain-length = <17>`, but place it under `&spi2` first. Use P0.20 as MOSI and P1.04 as SCK in a new `spi2_default` pinctrl group.

- [ ] **Step 2: Put ST7789V on SPI3**

Change `spi3_default` to P1.13 SCK and P0.28 MOSI. Add a `zephyr,mipi-dbi-spi` controller and `sitronix,st7789v` child using CS P0.02, DC P0.03, reset P1.10, x offset 0, y offset 34, and ready time 120 ms.

- [ ] **Step 3: Add PWM backlight**

Enable PWM0 CH0 on P1.11 and add a `pwm-leds` node for screen backlight. Use a 100 Hz period and inverted polarity.

- [ ] **Step 4: Enable display stack**

Add the required Kconfig symbols for DISPLAY, LVGL, ST7789V, RGB565 byte swap, PWM/LED support, and only the LVGL widgets/fonts needed for this UI.

- [ ] **Step 5: Build**

Run:

```powershell
.\scripts\build.ps1 -Pristine -BuildDir "D:\b_lvgl_board_coexist" -Board "mini-keyboard/nrf52840" -Snippet "rtt-console"
```

Expected: board DTS compiles with both display and RGB strip present. If not, fix within the board/prj scope and record the exact remaining blocker.

- [ ] **Step 6: Commit**

```powershell
git add boards/mingDev/mini-keyboard/mini-keyboard.dts boards/mingDev/mini-keyboard/mini-keyboard-pinctrl.dtsi prj.conf
git commit -m "feat(board): add rgb coexisting st7789 display"
```

## Task 2: Testable UI Model And Input State

**Files:**
- Create: `include/display/status_screen.h`
- Create: `include/display/status_screen_model.h`
- Create: `src/display/status_screen_model.c`
- Create: `src/display/status_screen_input.c`
- Create: `tests/status_screen_model_test.c`
- Modify if needed: `CMakeLists.txt`

- [ ] **Step 1: Write failing model tests**

Cover Home rotation pass-through, long-press entry to Settings, list focus movement, visible Exit returning Home, Edit apply/cancel, Confirm default Cancel, and UI-active input consumption.

- [ ] **Step 2: Verify tests fail**

Compile the focused assert test before production code exists. Expected failure: missing status screen model API or incorrect action results.

- [ ] **Step 3: Implement pure-C model**

Implement page state, focus index, menu items, edit value cursor, confirm selection, and action outputs. Keep this layer free of Zephyr/LVGL headers.

- [ ] **Step 4: Verify tests pass**

Run the focused status screen model test.

- [ ] **Step 5: Commit**

```powershell
git add include/display/status_screen.h include/display/status_screen_model.h src/display/status_screen_model.c src/display/status_screen_input.c tests/status_screen_model_test.c CMakeLists.txt
git commit -m "feat(display): add status screen model"
```

## Task 3: LVGL Signal Neon View

**Files:**
- Create/modify: `include/display/status_screen.h`
- Create: `src/display/status_screen.c`
- Create: `src/display/status_screen_lvgl.c`
- Modify: `CMakeLists.txt`
- Modify: `src/main.c`

- [ ] **Step 1: Add display init shell**

Add `status_screen_init()` and call it from `main.c`. Failure should log a warning and never block keyboard startup.

- [ ] **Step 2: Build LVGL templates**

Create Home, Settings list, Edit, and Confirm templates for 320 x 172. Use visible Exit on every settings level.

- [ ] **Step 3: Apply Signal Neon theme**

Use LVGL primitives for high-contrast surfaces, neon focus bar, status dots, RGB accent strip, and light scanline/detail effects. Avoid bitmap assets and heavy animation.

- [ ] **Step 4: Subscribe to runtime state**

Subscribe to mode, power, HID LED, and config changes. Update a cached snapshot and schedule a work item for LVGL refresh instead of redrawing inside callbacks.

- [ ] **Step 5: Build**

Run:

```powershell
.\scripts\build.ps1 -Pristine -BuildDir "D:\b_lvgl_view" -Board "mini-keyboard/nrf52840" -Snippet "rtt-console"
```

- [ ] **Step 6: Commit**

```powershell
git add include/display/status_screen.h src/display/status_screen.c src/display/status_screen_lvgl.c CMakeLists.txt src/main.c
git commit -m "feat(display): add lvgl status screen view"
```

## Task 4: Integration And Verification

**Files:**
- Merge all task branches into an integration branch.
- Resolve conflicts in `include/display/status_screen.h`, `CMakeLists.txt`, and display source files.

- [ ] **Step 1: Merge feature branches**

Merge board coexistence, model/input, and LVGL view branches into one integration branch.

- [ ] **Step 2: Run focused tests**

Run the new status screen model test and any existing tests touched by config/input behavior.

- [ ] **Step 3: Run target build**

Run:

```powershell
.\scripts\build.ps1 -Pristine -BuildDir "D:\b_lvgl_st7789_rgb" -Board "mini-keyboard/nrf52840" -Snippet "rtt-console"
```

- [ ] **Step 4: Inspect resolved devicetree evidence**

Check `D:\b_lvgl_st7789_rgb\zephyr\zephyr.dts` for:

- ST7789V uses P1.13, P0.28, P0.02, P0.03, P1.10.
- Backlight uses P1.11.
- RGB strip still has `chain-length = <17>` and uses P0.20, with power enable P0.13 in `zephyr,user`.

- [ ] **Step 5: Final commit**

Commit any integration-only fixes with a message such as:

```powershell
git commit -m "fix(display): integrate status screen build"
```

## Commit Strategy

Use separate commits so rollback is surgical:

1. `feat(board): add rgb coexisting st7789 display`
2. `feat(display): add status screen model`
3. `feat(display): add lvgl status screen view`
4. `fix(display): integrate status screen build` if needed

Do not commit `skills-lock.json` or unrelated ignored Trellis workspace files.
