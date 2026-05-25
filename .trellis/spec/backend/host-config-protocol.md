# Host Configuration Protocol Contracts

## Scenario: Settings-backed host configuration over USB CDC ACM

### 1. Scope / Trigger

- Trigger: firmware adds or changes host configuration, app settings, Settings/NVS persistence, protobuf schema, host frame codec, or USB CDC ACM configuration.
- Product rule: host communication is only a configuration source. It must update `app_config`, then notify consumers. It must not call RGB, encoder, mode, or transport business APIs directly.

### 2. Signatures

- `int app_config_init(void)`
- `int app_config_get_defaults(struct app_config *out)`
- `int app_config_get(struct app_config *out)`
- `int app_config_set(const struct app_config *config, bool notify)`
- `int app_config_reset_defaults(bool notify)`
- `int app_config_validate(const struct app_config *config)`
- `int app_config_subscribe(app_config_listener_t handler, void *user_data)`
- `void app_config_notify_all(void)`
- `uint32_t app_config_compute_crc32(const struct app_config *config)`
- `void app_config_update_crc32(struct app_config *config)`
- `bool app_config_crc32_valid(const struct app_config *config)`
- `int app_config_store_load(void)`
- `int app_config_store_save(void)`
- `int app_config_store_factory_reset(void)`
- `int host_frame_encode(const uint8_t *payload, size_t payload_len, uint8_t *out, size_t out_size, size_t *out_len)`
- `int host_frame_decode(const uint8_t *frame, size_t frame_len, const uint8_t **payload, size_t *payload_len)`
- `int host_protocol_handle_payload(const uint8_t *request, size_t request_len, uint8_t *response, size_t response_size, size_t *response_len)`
- `int host_comm_init(void)`

### 3. Contracts

- The only persisted key for V1 is `app/config/blob`.
- The blob is a packed `struct app_config`; CRC32 covers the struct bytes before `crc32` with `crc32` treated as zero.
- `app_config_validate()` checks version and field ranges, not persisted CRC. The store layer owns CRC acceptance.
- `app_config_set()` recomputes CRC before publishing runtime state.
- `app_config_store_load()` and `app_config_store_save()` must call `settings_subsys_init()` before `settings_load_one()` / `settings_save_one()`. The app loads configuration before BLE transport initialization, so the store layer cannot rely on Bluetooth's later `settings_load()` call to initialize Settings/NVS.
- Host frame format is `0x55 0xAA + version(1) + payload_len_le16 + protobuf_payload + crc16_le`.
- Host frame CRC is non-reflected CRC-16/CCITT polynomial `0x1021`, seed `0xffff`, over `version + length + payload`.
- Protobuf source of truth is `proto/device_comm.proto`; Python GUI copies must stay byte-for-byte equivalent when regenerated.
- Firmware nanopb runtime must be provided by Zephyr's `CONFIG_NANOPB=y` module. Do not link the standalone `nanopb` CMake target into `app`, because it can compile without Zephyr/Cortex-M flags such as `-mthumb` and crash when host requests reach `pb_decode()`.
- `DeviceConfig` protobuf fields are `uint32`, while firmware runtime fields are mostly `uint8_t`; host protocol must reject any value greater than `UINT8_MAX` before narrowing, then call `app_config_validate()` for semantic ranges such as brightness `0..100`.
- Host requests other than `HelloReq` require a successful hello first and return `RESPONSE_CODE_NOT_READY` if the session is not ready.
- `ConfigSetReq(save=true)` is the only normal config-set path that writes NVS. `ConfigSetReq(apply=true, save=false)` must apply runtime config without saving.
- USB CDC ACM uses `zephyr,cdc-acm-uart` under `&zephyr_udc0` and the device-next `CONFIG_USBD_CDC_ACM_CLASS`.

### 4. Validation & Error Matrix

- Missing persisted blob -> reset runtime defaults, return success.
- Settings/NVS subsystem init failure -> reset runtime defaults on load; return storage failure on save.
- Wrong blob length -> reset runtime defaults, return success.
- CRC mismatch -> reset runtime defaults, return success.
- Unsupported config version -> reset runtime defaults, return success.
- Invalid config field from host -> `RESPONSE_CODE_INVALID_PARAM`, do not save.
- Any protobuf `DeviceConfig` integer that would truncate when converted to `uint8_t` -> `RESPONSE_CODE_INVALID_PARAM`, do not save.
- Storage write failure -> `RESPONSE_CODE_STORAGE_ERROR`.
- Protobuf decode failure -> `RESPONSE_CODE_INVALID_LENGTH`.
- Unknown message body -> `RESPONSE_CODE_UNKNOWN_TYPE`.
- Frame bad magic/version/length/CRC -> drop frame and log; do not pass payload to protobuf.
- Missing CDC ACM devicetree node -> `host_comm_init()` returns `-ENODEV`; keyboard HID boot must continue.
- `pb_istream_from_buffer()` / `pb_decode()` hard fault on Cortex-M with an instruction-access violation -> first check that final ELF resolves nanopb symbols from `modules/nanopb` and that the linked module object is compiled with Zephyr flags (`-mcpu=cortex-m4 -mthumb -D__ZEPHYR__`). `compile_commands.json` may also contain an unlinked `CMakeFiles/nanopb.dir` entry from CMake generation; do not treat that bare entry as proof of the final linked runtime.

### 5. Good / Base / Bad Cases

- Good: GUI sends `ConfigSetReq(apply=true, save=false)`, firmware validates, updates `app_config`, notifies RGB/encoder, and does not call `settings_save_one()`.
- Good: GUI sends `ConfigSetReq(apply=true, save=true)`, firmware applies, then stores `app/config/blob`.
- Base: no saved blob exists; defaults preserve current product behavior.
- Base: CDC ACM unavailable; HID keyboard transports still initialize.
- Bad: `host_protocol.c` calls `rgb_manager_set_mode()` directly.
- Bad: using Zephyr `crc16_ccitt()` for the host frame when the PC codec uses a non-reflected CCITT variant.
- Bad: writing Settings/NVS on every input event or RGB refresh.
- Bad: `target_link_libraries(app PRIVATE nanopb)` without `CONFIG_NANOPB=y`, which links a standalone nanopb object built outside Zephyr's target flags.

### 6. Tests Required

- Host-test config defaults, validation, subscription de-duplication, CRC mutation, store fallback, and explicit save semantics.
- Host-test frame round trip, bad magic, bad version, oversized length, truncated frame, empty payload, and CRC mismatch.
- Host-test protocol hello gating, invalid config rejection, apply+save, factory reset, and malformed protobuf response.
- Host-test protocol must include a truncation guard case such as `rgb_brightness = 257`, asserting `RESPONSE_CODE_INVALID_PARAM` and no persisted save.
- Run a pristine Zephyr build after touching `CMakeLists.txt`, `prj.conf`, board DTS, host protocol, Settings store, or USB descriptors:
  `.\scripts\build.ps1 -Pristine -BuildDir "D:\b_ip5306_rtt_usb" -Board "mini-keyboard/nrf52840" -Snippet "rtt-console"`.
- After changing nanopb integration, inspect `zephyr.map` / `arm-zephyr-eabi-nm` to confirm final nanopb symbols come from `modules/nanopb`; if using `compile_commands.json`, inspect the `modules/nanopb/CMakeFiles/modules__nanopb.dir` command and verify it has `-mthumb`.
- Inspect generated `.config`/DTS when changing CDC ACM: confirm `CONFIG_USBD_CDC_ACM_CLASS=y` and `zephyr,cdc-acm-uart` exists under `zephyr_udc0`.

### 7. Wrong vs Correct

#### Wrong

```c
host_config_set_rgb(mode, red, green, blue);
rgb_manager_set_mode(mode);
settings_save_one("app/config/blob", &config, sizeof(config));
```

#### Correct

```c
err = app_config_set(&config, true);
if (err == 0 && save) {
    err = app_config_store_save();
}
```

#### Wrong

```c
config->rgb_brightness = (uint8_t)proto->rgb_brightness;
err = app_config_validate(config);
```

#### Correct

```c
if (proto->rgb_brightness > UINT8_MAX) {
    return -EINVAL;
}
config->rgb_brightness = (uint8_t)proto->rgb_brightness;
err = app_config_validate(config);
```

#### Wrong

```c
uint16_t crc = crc16_ccitt(0xffff, &frame[2], payload_len + 3);
```

#### Correct

```c
uint16_t crc = host_frame_crc16(&frame[2], payload_len + 3);
```
