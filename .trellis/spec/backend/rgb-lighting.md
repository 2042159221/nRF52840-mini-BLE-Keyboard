# RGB Lighting Contracts

## Scenario: WS2812 reactive keyboard RGB subsystem

### 1. Scope / Trigger

- Trigger: firmware adds or changes RGB hardware resources, WS2812 DTS nodes,
  reactive lighting effects, host keyboard LED Output Report handling, or
  RGB power policy.
- Hardware authority: `docs/hardware_resources_agent_netlist.md` is the source
  of truth for RGB nets, chain order, GPIO polarity, and unused dummy SCK pins.
- Framework rule: keep the project style as init functions, Zephyr input
  callbacks, `power_state` subscription, and transport Output Report callbacks.
  Do not introduce CAF or a new event framework for RGB.

### 2. Signatures

- `int rgb_manager_init(void)`
- `void rgb_manager_set_mode(enum rgb_mode mode)`
- `enum rgb_mode rgb_manager_get_mode(void)`
- `void rgb_manager_set_color(uint8_t red, uint8_t green, uint8_t blue)`
- `void rgb_manager_mark_dirty(void)`
- `bool rgb_keymap_lookup(uint16_t input_code, uint8_t *rgb_index)`
- `void rgb_policy_update(struct rgb_policy *policy, const struct power_state_snapshot *snapshot)`
- `void rgb_effect_render(struct rgb_effect_state *effect, const struct rgb_policy *policy, bool num_lock, uint32_t now_ms, struct rgb_color *pixels, size_t pixel_count)`
- `int rgb_hw_show(const struct rgb_color *pixels, size_t count)`
- `int rgb_hw_off(void)`
- `void keyboard_led_state_update(uint8_t led_bits)`
- `bool keyboard_led_state_update_from_hid_output_report(uint8_t report_id, uint8_t keyboard_report_id, const uint8_t *buf, uint16_t len)`
- `bool keyboard_led_state_num_lock(void)`

### 3. Contracts

- Hardware:
  - `RGB_DATA` is P0.20 and must be the WS2812 SPI MOSI pin.
  - `RGB_PWR_EN` is P0.13 active high.
  - Dummy SPI SCK may use P1.04 because the netlist marks it unconnected.
  - Chain length is exactly 17.
  - WS2812 color mapping is GRB:
    `LED_COLOR_ID_GREEN`, `LED_COLOR_ID_RED`, `LED_COLOR_ID_BLUE`.
- Key mapping:
  - RGB index 0 is the Num Lock LED.
  - `rgb_keymap_lookup()` must not return index 0 for `INPUT_KEY_NUMLOCK`.
  - `INPUT_KEY_MUTE` and encoder rotation must not produce RGB reactive events.
- Effects:
  - Default mode is `RGB_MODE_KEY_REACTIVE`.
  - Default normal-key color is red.
  - Press lights immediately; release fades to off over `1000 ms`.
  - `RGB_MODE_OFF` turns normal keys off but preserves Num status unless
    policy is critical.
  - `RGB_MODE_STATIC` lights normal keys at policy-limited brightness and
    Num status still overrides index 0.
- Host LED state:
  - Num LED is controlled by host keyboard LED Output Report bit 0 only.
  - USB and BLE transport modules must route HID Output Reports through
    `keyboard_led_state_update_from_hid_output_report()` or the equivalent
    `keyboard_led_state` boundary; they must not parse Num Lock inside RGB.
  - USB device-next `set_report()` passes the report ID as the `id` argument
    for control transfers, while interrupt OUT buffers may include the report
    ID as `buf[0]`; keyboard LED parsing must accept both shapes so report ID
    `1` is not mistaken for `KEYBOARD_LED_NUM_LOCK`.
  - Transport code must not depend on RGB effect internals.
- Power:
  - RGB consumes published `power_state_snapshot` data only.
  - LOW reduces brightness.
  - CRITICAL clears all pixels and disables `RGB_PWR_EN`.
  - Increase `POWER_STATE_MAX_SUBSCRIBERS` when adding RGB or display/UI
    consumers.

### 4. Validation & Error Matrix

- LED strip device not ready -> `rgb_hw_init()` returns `-ENODEV`; app boot
  continues and logs a warning.
- RGB power GPIO missing or not ready -> `rgb_hw_init()` returns `-ENODEV`;
  app boot continues.
- `rgb_hw_show(NULL, ...)` -> `-EINVAL`.
- `rgb_hw_show(..., count > RGB_LED_COUNT)` -> `-ERANGE`.
- `keyboard_led_state_subscribe(NULL, ...)` -> `-EINVAL`.
- Duplicate LED-state subscription -> return `0` and do not duplicate calls.
- Repeated identical LED Output Report -> do not notify subscribers.
- Critical power state -> no Num overlay and no normal key output.

### 5. Good / Base / Bad Cases

- Good: USB host sends keyboard LED Output Report `0x01`; `keyboard_led_state`
  stores Num ON; RGB render overlays dim white at index 0.
- Good: normal keypad `7` press maps to RGB index 4 and renders red at the
  current policy brightness.
- Base: RGB hardware is unavailable on a bring-up board; keyboard input, USB,
  BLE, encoder, and power services still initialize.
- Bad: transport code calls `rgb_manager_*()` directly from USB/BLE callbacks.
- Bad: Num Lock key press shows red reactive lighting before the host reports
  Num state.
- Bad: RGB code reads ADC/IP5306 directly instead of using `power_state`.

### 6. Tests Required

- Host-test `rgb_keymap_lookup()` chain order and Num/Mute exclusions.
- Host-test `rgb_policy_update()` for USB, battery normal, LOW, and CRITICAL.
- Host-test `rgb_effect_render()` for press, hold, 1000 ms fade, re-press reset,
  static/off modes, Num overlay, and critical shutdown.
- Host-test `keyboard_led_state_update()` for Num bit parsing, change-only
  notification, duplicate subscription, and invalid listener.
- Host-test HID Output Report parsing for both payload-only buffers and
  report-ID-prefixed buffers such as `{ keyboard_report_id, 0x00 }`.
- Build the Zephyr firmware after DTS/Kconfig/CMake/transport edits using the
  repository script:
  `.\scripts\build.ps1 -Pristine -BuildDir "D:\b_rgb_rtt_usb" -Board "mini-keyboard/nrf52840" -Snippet "rtt-console"`.
- Inspect generated DTS and `.config` after hardware edits:
  - `rgb-power-enable-gpios = <&gpio0 13 GPIO_ACTIVE_HIGH>`
  - WS2812 node compatible `worldsemi,ws2812-spi`
  - `chain-length = 17`
  - `CONFIG_LED_STRIP=y`
  - `CONFIG_WS2812_STRIP_SPI=y`

### 7. Wrong vs Correct

#### Wrong

```c
if (evt->code == INPUT_KEY_NUMLOCK) {
    rgb_effect_key_pressed(&effect, 0, now_ms);
}
```

#### Correct

```c
if (!rgb_keymap_lookup(evt->code, &rgb_index)) {
    return;
}
```

#### Wrong

```c
keyboard_led_state_update(led_bits);
rgb_manager_set_mode(RGB_MODE_STATIC);
```

#### Correct

```c
keyboard_led_state_update(led_bits);
```

The RGB manager subscribes to keyboard LED state changes and renders the Num
overlay itself.
