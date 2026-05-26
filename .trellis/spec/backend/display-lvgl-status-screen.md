# LVGL Status Screen Contracts

## Scenario: ST7789V local status/settings screen

### 1. Scope / Trigger

- Trigger: firmware touches the local display stack, ST7789V Devicetree nodes,
  LVGL configuration, status screen model/view code, EC11 UI routing, or screen
  backlight resources.
- Hardware authority: `docs/hardware_resources_agent_netlist.md` is the source
  of truth for screen and RGB pins. Do not infer pins from connector labels
  alone.
- Product rule: display bring-up must coexist with the 17-LED WS2812 RGB chain.
  Do not disable RGB to make the screen compile.

### 2. Signatures

- `int status_screen_init(void)`
- `bool status_screen_input_route(struct status_screen_model *model, enum status_screen_input input, struct status_screen_event_result *result)`
- `void status_screen_model_init(struct status_screen_model *model)`
- `struct status_screen_event_result status_screen_model_handle_input(struct status_screen_model *model, enum status_screen_input input)`
- `bool status_screen_model_ui_active(const struct status_screen_model *model)`
- `int status_screen_lvgl_init(const struct status_screen_snapshot *snapshot)`
- `void status_screen_lvgl_update(const struct status_screen_snapshot *snapshot)`

### 3. Contracts

- ST7789V uses SPI3 through `zephyr,mipi-dbi-spi`:
  - SCK P1.13
  - MOSI P0.28
  - CS P0.02 active low
  - DC P0.03 active high
  - RESET P1.10 active low
  - 320 x 172 active area, `x-offset = <0>`, `y-offset = <34>`,
    `ready-time-ms = <120>`, `mipi-max-frequency = <32000000>`.
- RGB remains enabled on a separate SPIM instance:
  - WS2812 MOSI P0.20
  - dummy SCK P1.04, because the netlist marks it unconnected
  - `chain-length = <17>`
  - `rgb-power-enable-gpios = <&gpio0 13 GPIO_ACTIVE_HIGH>`.
- Backlight uses `pwm-leds` on PWM0 channel 0, P1.11, 100 Hz period
  (`PWM_MSEC(10)`), inverted polarity.
- Required Kconfig when display is enabled:
  - `CONFIG_DISPLAY=y`
  - `CONFIG_MIPI_DBI=y`
  - `CONFIG_LVGL=y`
  - `CONFIG_LV_COLOR_DEPTH_16=y`
  - `CONFIG_LV_COLOR_16_SWAP=y`
  - `CONFIG_PWM=y`
  - `CONFIG_LED_PWM=y`
  - RGB symbols must remain enabled: `CONFIG_LED_STRIP=y` and
    `CONFIG_WS2812_STRIP_SPI=y`.
- Keep `status_screen_model.*` free of Zephyr and LVGL headers so it can be
  compiled by host-side C tests.
- `status_screen_init()` must not block keyboard boot. If LVGL is disabled it
  returns `0`; if display initialization fails, `main.c` logs a warning and
  continues other keyboard services.
- LVGL code must only reference font symbols enabled by Kconfig. The default
  lightweight font in this project is `lv_font_montserrat_14`; using
  `lv_font_montserrat_20` requires enabling `CONFIG_LV_FONT_MONTSERRAT_20=y`.
- Local UI navigation has no Esc dependency. Every settings/edit/confirm level
  must expose a visible `Exit` path. Home rotation is HID passthrough; long
  press enters settings; active UI consumes EC11 navigation input.

### 4. Validation & Error Matrix

- Display device not ready -> `status_screen_init()` returns `-ENODEV`; caller
  logs nonfatal warning and continues boot.
- `status_screen_lvgl_init(NULL)` -> `-EINVAL`.
- LVGL root screen unavailable -> `-ENODEV`.
- Missing power/mode/config snapshot -> render defaults and refresh when
  publisher callbacks arrive; do not read hardware directly from the view.
- Referencing a disabled LVGL font symbol -> compile failure; either use
  `lv_font_montserrat_14` or enable the matching Kconfig symbol.
- RGB moved off P0.20, chain length changed, or RGB disabled -> reject the
  change unless the hardware netlist and product requirements are updated.

### 5. Good/Base/Bad Cases

- Good: generated `zephyr.dts` shows `zephyr,display = &st7789v`, `st7789v`
  under `mipi_dbi` using `spi3`, and `rgb_strip: ws2812@0` under `spi2`.
- Good: status screen subscribes to mode, power, keyboard LED state, and
  app-config snapshots, then schedules a work item for LVGL refresh.
- Good: the pure C model test covers Home passthrough, long-press entry,
  settings focus movement, visible Exit, edit apply/cancel, and factory reset
  confirm defaulting to Cancel.
- Base: the display is not populated or not ready; keyboard input, USB, BLE,
  RGB, and host configuration still initialize.
- Bad: disabling `CONFIG_LED_STRIP` or moving RGB away from P0.20 to free SPI3.
- Bad: putting LVGL object logic into the pure model file, making host tests
  require Zephyr or display hardware.
- Bad: drawing directly from power/RGB/transport callbacks instead of caching a
  snapshot and scheduling a refresh.

### 6. Tests Required

- Host-test the pure C status model/input behavior with assertions for:
  - Home rotation returns HID passthrough and leaves UI inactive.
  - Long press enters Settings and makes UI active.
  - Settings rotation changes focus and visible `Exit` returns Home.
  - Edit page apply produces an action value; long press cancel does not apply.
  - Confirm page starts on Cancel and only produces factory reset after focus
    moves to Confirm.
- Run `git diff --check` after conflict or LVGL view edits.
- Run a pristine Zephyr build with the project script after DTS, Kconfig,
  CMake, LVGL, or display source changes:

```powershell
.\scripts\build.ps1 -Pristine -BuildDir "D:\b_lvgl_st7789_rgb" -Board "mini-keyboard/nrf52840" -Snippet "rtt-console"
```

- Inspect generated DTS and `.config` after hardware/display changes:
  - `D:\b_lvgl_st7789_rgb\lvgl-st7789-status-settings-ui\zephyr\zephyr.dts`
  - `D:\b_lvgl_st7789_rgb\lvgl-st7789-status-settings-ui\zephyr\.config`

### 7. Wrong vs Correct

#### Wrong

```c
lv_obj_set_style_text_font(home.title, &lv_font_montserrat_20, LV_PART_MAIN);
```

#### Correct

```c
lv_obj_set_style_text_font(home.title, &lv_font_montserrat_14, LV_PART_MAIN);
```

or explicitly enable the larger font in `prj.conf` before referencing it.

#### Wrong

```dts
/* Free SPI3 for the display by deleting the RGB strip. */
/delete-node/ &rgb_strip;
```

#### Correct

```dts
&spi2 {
	status = "okay";
	rgb_strip: ws2812@0 {
		compatible = "worldsemi,ws2812-spi";
		chain-length = <17>;
	};
};

mipi_dbi {
	compatible = "zephyr,mipi-dbi-spi";
	spi-dev = <&spi3>;
};
```
