# LVGL + ST7789V Zephyr Notes

## Local Zephyr/NCS facts

- NCS workspace detected at `E:\ncs\v3.2.3`.
- Zephyr provides `sitronix,st7789v` display binding and driver.
- ST7789V binding includes `mipi-dbi-spi-device.yaml` and `display-controller.yaml`.
- MIPI DBI SPI controller binding is `zephyr,mipi-dbi-spi`.
- `zephyr,mipi-dbi-spi` requires `spi-dev` and supports `dc-gpios`, `reset-gpios`, `write-only`, and `clock-frequency`.
- ST7789V required display properties include `x-offset`, `y-offset`, `vcom`, `gctrl`, `mdac`, `lcm`, `colmod`, `gamma`, porch/gamma/RAM/RGB parameter arrays, `ready-time-ms`, and `mipi-mode`.
- `CONFIG_ST7789V` defaults on when a `sitronix,st7789v` node is enabled.
- LVGL sample enables `CONFIG_DISPLAY=y`, `CONFIG_LVGL=y`, and `CONFIG_LV_Z_MEM_POOL_SIZE=16384`.
- LVGL encoder pseudo-device is `zephyr,lvgl-encoder-input` with `rotation-input-code` and optional `button-input-code`.
- LVGL sample confirms encoder input can navigate widgets and edit values.

## Project facts

- Current `prj.conf` has input, SPI, settings, NVS, BLE, USB, RGB, and host config, but no LVGL/display/ST7789V options yet.
- Current `boards/mingDev/mini-keyboard/mini-keyboard.dts` uses `&spi3` for WS2812 RGB strip at `reg = <0>`.
- Current `mini-keyboard-pinctrl.dtsi` maps `spi3_default` to SCK `P1.04` and MOSI `P0.20`, which matches the old WS2812 SPI waveform path, not the screen.
- Existing EC11 encoder uses `gpio-qdec`, `INPUT_REL_WHEEL`, A `P0.10`, B `P1.06`.
- Existing EC11 press comes through the keyboard matrix as `INPUT_KEY_MUTE`.
- Existing runtime state sources include `mode_manager_register_listener()`, `power_state_subscribe()`, `keyboard_led_state_subscribe()`, and `app_config_subscribe()`.

## RGB + ST7789 coexistence from netlist

- `RGB_DATA` is P0.20 and feeds U35 DIN; it is not electrically tied to the screen connector.
- `RGB_PWR_EN` is P0.13 and controls Q4/Q2 for RGB power.
- `SCREEN_SDA` is P0.28 and `SCREEN_SCL` is P1.13.
- `SCREEN_CS` is P0.02, `SCREEN_DC` is P0.03, `SCREEN_RES` is P1.10, `SCREEN_BLK` is P1.11.
- `P1.04` is marked as not present in `$NETS`, so it can be used as an unconnected SCK output for the WS2812 SPI waveform generator.
- Coexistence recommendation: dedicate `spi3` to ST7789V on the screen pins and move WS2812 to another SPIM instance with MOSI P0.20 and SCK P1.04. Keep `chain-length = <17>` and P0.13 RGB power enable.

## Key engineering risks

- Dual SPIM availability must be proven by build. If `spi2` is unavailable in the resolved SoC devicetree, try `spi1`; do not disable RGB as a fallback.
- The user asked for backlight through `led_on`/`led_off`; Zephyr `pwm-leds` is the likely board-level representation, with a wrapper module for screen power/backlight.
- Display active area is 320 x 172 with y offset 34. UI layout must treat 320 x 172 as the usable canvas, not full 320 x 240.
- nRF52840 RAM is tight with BLE, USB, NVS, RGB, LVGL, and display buffering. Use compact LVGL widgets, small fonts, no full-screen bitmap assets in MVP, and modest draw buffers.

## External reference interpretation

- LIT DUO inspiration: physical control first, glanceable status, one-handed knob interaction, no hidden escape dependency.
- SumUp/LVGL inspiration: lightweight embedded UI, limited widget set, deterministic screens, high trust through clear feedback.
- Whisker inspiration: templated pages where status, list, detail, confirm, and result screens share a predictable layout.
- Xiaomi inspiration: fast visible response, theme consistency, accent color customization, and state changes that feel immediate.

## Recommended approach

Use a small event-driven local UI module and a dual-SPIM board layout:

- `display_hw`: board/display/backlight readiness and simple on/off API.
- `settings_ui`: LVGL screen tree, template helpers, styles, and page navigation.
- `settings_model`: a compact state snapshot fed by app config, mode, power, host LED, and encoder input.
- `settings_controller`: maps EC11 rotation/press/long-press into focus, edit, save, and visible Exit actions.
- `rgb_strip`: stays enabled on P0.20 with 17 LEDs, moved away from the screen SPI controller.

MVP should implement status home, settings menu, detail edit pages, confirmation pages, and visible Exit. Visual design can be neon/cyber styled, but should use LVGL primitives instead of bitmap-heavy assets until hardware bring-up is stable.

