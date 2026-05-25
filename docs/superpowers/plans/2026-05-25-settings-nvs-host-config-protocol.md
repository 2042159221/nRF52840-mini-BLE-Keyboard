# Settings NVS Host Config Protocol Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a USB CDC ACM based host configuration loop with firmware Settings/NVS persistence and a PySide6 configuration GUI.

**Architecture:** Firmware owns a versioned `app_config` service and saves only explicit requests to Settings/NVS. Host communication is layered as CDC transport, frame codec, protobuf protocol, and session dispatcher. GUI talks only to `DeviceClient` and keeps protocol bytes out of UI widgets.

**Tech Stack:** Zephyr/NCS v3.2.3, Settings/NVS, USB device stack next, nanopb, Python 3, PySide6, pyserial, pytest.

---

### Task 1: Firmware Config Core

**Files:**
- Create: `include/config/app_config.h`
- Create: `include/config/app_config_store.h`
- Create: `src/config/app_config.c`
- Create: `src/config/app_config_store.c`
- Create: `tests/app_config_test.c`
- Modify: `CMakeLists.txt`
- Modify: `src/main.c`

- [ ] **Step 1: Write config default and validation tests**

Add `tests/app_config_test.c` covering:

```c
assert(app_config_get_defaults(&cfg) == 0);
assert(cfg.version == APP_CONFIG_VERSION);
assert(cfg.rgb_enable);
assert(cfg.rgb_mode == APP_RGB_MODE_KEY_REACTIVE);
assert(cfg.rgb_brightness == 30);
assert(cfg.rgb_red == 255);
assert(cfg.rgb_green == 80);
assert(cfg.rgb_blue == 20);
assert(cfg.numlock_policy == APP_NUMLOCK_ALWAYS_WHITE);
assert(cfg.encoder_cw_action == APP_ENCODER_ACTION_VOLUME_UP);
assert(cfg.encoder_ccw_action == APP_ENCODER_ACTION_VOLUME_DOWN);
assert(cfg.encoder_press_action == APP_ENCODER_ACTION_MUTE);
assert(cfg.mode_policy == APP_MODE_POLICY_FOLLOW_HARDWARE);
assert(cfg.default_mode == APP_DEFAULT_MODE_USB);
```

Also test invalid brightness `101`, invalid RGB mode `99`, invalid encoder action `99`, invalid mode policy `99`, and invalid default mode `99` return `-EINVAL`.

- [ ] **Step 2: Verify tests fail before implementation**

Run a host compile where available for the new test. If no host C compiler is available, record the failure as "test target not yet wired" and continue with firmware build verification later.

- [ ] **Step 3: Implement config API**

Create `app_config` with:

```c
#define APP_CONFIG_VERSION 1u

enum app_rgb_mode { APP_RGB_MODE_OFF = 0, APP_RGB_MODE_STATIC = 1, APP_RGB_MODE_KEY_REACTIVE = 2 };
enum app_encoder_action { APP_ENCODER_ACTION_NONE = 0, APP_ENCODER_ACTION_VOLUME_UP = 1, APP_ENCODER_ACTION_VOLUME_DOWN = 2, APP_ENCODER_ACTION_MUTE = 3, APP_ENCODER_ACTION_PLAY_PAUSE = 4, APP_ENCODER_ACTION_NEXT_TRACK = 5, APP_ENCODER_ACTION_PREV_TRACK = 6, APP_ENCODER_ACTION_RGB_MODE_NEXT = 7 };
enum app_numlock_policy { APP_NUMLOCK_FOLLOW_HOST = 0, APP_NUMLOCK_ALWAYS_WHITE = 1, APP_NUMLOCK_OFF_WHEN_LOW_POWER = 2 };
enum app_mode_policy { APP_MODE_POLICY_FOLLOW_HARDWARE = 0, APP_MODE_POLICY_LAST_MODE = 1, APP_MODE_POLICY_USB_PREFERRED = 2, APP_MODE_POLICY_BLE_PREFERRED = 3 };
enum app_default_mode { APP_DEFAULT_MODE_USB = 0, APP_DEFAULT_MODE_BLE = 1 };
```

Add functions:

```c
int app_config_init(void);
int app_config_get_defaults(struct app_config *out);
int app_config_get(struct app_config *out);
int app_config_set(const struct app_config *config, bool notify);
int app_config_reset_defaults(bool notify);
int app_config_validate(const struct app_config *config);
int app_config_subscribe(app_config_listener_t handler, void *user_data);
void app_config_notify_all(void);
```

- [ ] **Step 4: Implement Settings/NVS store**

Store blob key `app/config/blob`. Use `settings_load_one()` and `settings_save_one()`. Compute CRC over the struct bytes before `crc32`; reject missing, wrong-sized, wrong-version, invalid, or CRC-mismatched blobs and fall back to defaults.

- [ ] **Step 5: Wire startup order**

Update `main()` to call config before business modules:

```text
power_manager_init()
app_config_init()
app_config_store_load()
transport_init()
host_comm_init()
input_manager_init()
encoder_manager_init()
rgb_manager_init()
mode_manager_init()
app_config_notify_all()
```

If `host_comm_init()` lands in Task 2, declare it in a header and keep call guarded by the real implementation after merge.

- [ ] **Step 6: Commit**

Commit message: `feat(config): add app settings persistence core`.

---

### Task 2: Firmware Protocol And USB CDC Host Communication

**Files:**
- Create: `proto/device_comm.proto`
- Create: `proto/device_comm.options`
- Create: `include/host/host_frame.h`
- Create: `include/host/host_protocol.h`
- Create: `include/host/host_session.h`
- Create: `include/host/host_comm.h`
- Create: `src/host/host_frame.c`
- Create: `src/host/host_protocol.c`
- Create: `src/host/host_session.c`
- Create: `src/host/host_transport_usb_cdc.c`
- Create: `tests/host_frame_test.c`
- Modify: `CMakeLists.txt`
- Modify: `prj.conf`
- Modify: `boards/mingDev/mini-keyboard/mini-keyboard.dts`

- [ ] **Step 1: Write frame codec tests**

Cover valid encode/decode, bad magic, unsupported frame version, truncated length, payload too large, and CRC mismatch. Use CRC16 CCITT with seed `0xffff` on bytes from `Version` through payload.

- [ ] **Step 2: Verify frame tests fail before implementation**

Run the focused host test target when available.

- [ ] **Step 3: Add protobuf schema and nanopb options**

Use the schema from the PRD. Add bounded nanopb options for strings:

```text
keyboard.HelloReq.app_name max_size:32
keyboard.Response.message max_size:64
```

- [ ] **Step 4: Wire nanopb CMake generation**

Add:

```cmake
list(APPEND CMAKE_MODULE_PATH ${ZEPHYR_BASE}/modules/nanopb)
include(nanopb)
zephyr_nanopb_sources(app proto/device_comm.proto)
```

Then add host sources to `target_sources(app PRIVATE ...)`.

- [ ] **Step 5: Implement host frame codec**

Expose incremental-safe functions:

```c
int host_frame_encode(const uint8_t *payload, size_t payload_len, uint8_t *out, size_t out_size, size_t *out_len);
int host_frame_decode(const uint8_t *frame, size_t frame_len, const uint8_t **payload, size_t *payload_len);
```

- [ ] **Step 6: Implement host protocol/session**

Decode `DeviceMessage`, require hello before config/status requests, map invalid params/storage errors to `ResponseCode`, and reply with matching `reply_to`.

- [ ] **Step 7: Implement USB CDC ACM transport**

Add `CONFIG_USBD_CDC_ACM_CLASS=y`, `CONFIG_UART_INTERRUPT_DRIVEN=y`, and a `cdc_acm_uart0` devicetree node compatible with `zephyr,cdc-acm-uart`. Use UART IRQ RX to feed bytes into the frame parser and UART FIFO TX to send encoded replies.

- [ ] **Step 8: Commit**

Commit message: `feat(host): add protobuf config protocol over cdc`.

---

### Task 3: RGB And Encoder Config Integration

**Files:**
- Create: `include/input/encoder_action.h`
- Create: `src/input/encoder_action.c`
- Create: `tests/encoder_action_test.c`
- Modify: `include/hid/hid_consumer.h`
- Modify: `include/rgb/rgb_manager.h`
- Modify: `include/rgb/rgb_policy.h`
- Modify: `src/input/encoder_manager.c`
- Modify: `src/input/input_manager.c`
- Modify: `src/rgb/rgb_manager.c`
- Modify: `src/rgb/rgb_policy.c`
- Modify: `tests/rgb_policy_test.c`

- [ ] **Step 1: Update RGB policy tests first**

Add cases proving USB user brightness `80` remains `80` when limit is `100`, battery caps `80` to `50`, low caps `80` to `20`, and critical still forces `0`.

- [ ] **Step 2: Add encoder action tests first**

Use a test stub for consumer pulse sending and assert each action maps to the expected usage or no-op.

- [ ] **Step 3: Implement RGB brightness cap**

Add user brightness to `struct rgb_policy`, expose `rgb_policy_set_user_brightness()`, and keep power-derived max brightness separate from effective brightness.

- [ ] **Step 4: Apply config snapshot to RGB**

Add `rgb_manager_apply_config(const struct app_config *config)` and subscribe to `app_config` during init. Map app RGB mode to existing `enum rgb_mode`.

- [ ] **Step 5: Implement encoder action module**

Map app actions to HID usage constants. Add HID constants for play/pause `0x00CD`, next track `0x00B5`, previous track `0x00B6`.

- [ ] **Step 6: Replace hardcoded encoder behavior**

Encoder rotation reads `encoder_cw_action`/`encoder_ccw_action`; `INPUT_KEY_MUTE` press reads `encoder_press_action`. Preserve current defaults.

- [ ] **Step 7: Commit**

Commit message: `feat(input): apply config to rgb and encoder`.

---

### Task 4: PC GUI And Device Client

**Files:**
- Create: `pc_gui/main.py`
- Create: `pc_gui/requirements.txt`
- Create: `pc_gui/proto/device_comm.proto`
- Create: `pc_gui/device/frame_codec.py`
- Create: `pc_gui/device/device_models.py`
- Create: `pc_gui/device/device_protocol.py`
- Create: `pc_gui/device/device_client.py`
- Create: `pc_gui/device/serial_transport.py`
- Create: `pc_gui/device/transport_base.py`
- Create: `pc_gui/device/error_codes.py`
- Create: `pc_gui/ui/main_window.py`
- Create: `pc_gui/ui/device_page.py`
- Create: `pc_gui/ui/rgb_page.py`
- Create: `pc_gui/ui/encoder_page.py`
- Create: `pc_gui/ui/system_page.py`
- Create: `pc_gui/ui/developer_log_page.py`
- Create: `pc_gui/resources/style.qss`
- Create: `pc_gui/tests/test_frame_codec.py`
- Create: `pc_gui/tests/test_device_models.py`

- [ ] **Step 1: Write Python frame codec tests**

Use the same frame vectors as firmware tests: round trip, bad magic, CRC mismatch, partial frame, and multiple frames in a stream buffer.

- [ ] **Step 2: Write device model tests**

Assert default config values and conversion to/from protobuf/dict preserve all fields.

- [ ] **Step 3: Implement device frame/protocol/client layers**

`DeviceClient` exposes `connect()`, `disconnect()`, `hello()`, `get_config()`, `set_config()`, `save_config()`, `factory_reset()`, and `get_status()`. The protocol owns `msg_id`, `reply_to`, timeouts, and log events.

- [ ] **Step 4: Implement serial transport**

Use `pyserial` if installed. Keep `MockTransport` or test doubles independent so unit tests do not require a serial device.

- [ ] **Step 5: Implement PySide6 shell**

Build the five pages from the PRD. UI widgets call only `DeviceClient` APIs and append developer log lines through signals.

- [ ] **Step 6: Commit**

Commit message: `feat(gui): add host configuration client`.

---

### Task 5: Integration And Verification

**Files:**
- Modify only files needed to resolve merge/build/test issues.

- [ ] **Step 1: Merge config core branch**
- [ ] **Step 2: Run focused config tests/build checks**
- [ ] **Step 3: Merge RGB/encoder branch**
- [ ] **Step 4: Run focused RGB/encoder checks**
- [ ] **Step 5: Merge host protocol branch**
- [ ] **Step 6: Run firmware build**

Run:

```powershell
.\scripts\build.ps1 -Pristine -BuildDir "D:\b_ip5306_rtt_usb" -Board "mini-keyboard/nrf52840" -Snippet "rtt-console"
```

- [ ] **Step 7: Merge GUI branch**
- [ ] **Step 8: Run Python tests**

Run:

```powershell
python -m pytest pc_gui/tests
```

- [ ] **Step 9: Run final Trellis check**

Dispatch/execute Trellis check for spec compliance, lint/type-check/tests, and cross-layer consistency.

- [ ] **Step 10: Commit integration fixes**

Commit message: `fix(config): integrate host configuration flow`.
