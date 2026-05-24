# EC11 Knob Consumer Controls Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add EC11 knob media controls through HID Consumer reports for USB and BLE while preserving the existing keyboard input path.

**Architecture:** The feature is split into three mergeable tracks: HID/transport contract, USB/BLE report delivery, and encoder/input event generation. Consumer reports are sent as press/release pulses through the active mode only.

**Tech Stack:** Zephyr/NCS, Zephyr input subsystem, `gpio-qdec`, USB HID device stack, BLE HOGP, C module tests with `assert`.

---

### Task 1: HID Consumer And Transport Contract

**Files:**
- Create: `include/hid/hid_consumer.h`
- Create: `src/hid/hid_consumer.c`
- Modify: `include/transport/transport.h`
- Modify: `src/transport/transport.c`
- Modify: `src/transport/transport_24g_stub.c`
- Modify: `CMakeLists.txt`
- Test: `tests/keymap_hid_test.c`

- [ ] **Step 1: Add a failing consumer report test**

Add assertions that `hid_consumer_report_press()` sets a 16-bit usage and `hid_consumer_report_release()` clears it to `0x0000`.

- [ ] **Step 2: Verify the test fails**

Run the local C test build command used in this repo, or compile the test manually with project include paths. Expected failure: missing `hid/hid_consumer.h` or missing helper symbols.

- [ ] **Step 3: Add the HID Consumer module**

Create a small report module with these constants and shape:

```c
#define HID_CONSUMER_NONE        0x0000
#define HID_CONSUMER_MUTE        0x00E2
#define HID_CONSUMER_VOLUME_UP   0x00E9
#define HID_CONSUMER_VOLUME_DOWN 0x00EA

struct hid_consumer_report {
    uint16_t usage;
};
```

Provide `hid_consumer_report_press()` and `hid_consumer_report_release()` helpers.

- [ ] **Step 4: Add transport Consumer dispatch**

Mirror `transport_send_keyboard_report()` with `transport_send_consumer_report()`. USB and BLE forward to their concrete implementations; 2.4G returns `-ENOTSUP`.

- [ ] **Step 5: Run tests and commit**

Run the focused C tests and commit with `feat: add consumer transport contract`.

### Task 2: USB And BLE Consumer Reports

**Files:**
- Modify: `src/transport/transport_usb.c`
- Modify: `src/transport/transport_ble.c`
- Modify: `prj.conf`

- [ ] **Step 1: Extend USB descriptor with Report IDs**

Replace `HID_KEYBOARD_REPORT_DESC()` with an explicit descriptor:
- Report ID 1: keyboard, 8 bytes, existing 6KRO layout.
- Report ID 2: Consumer Control, one 16-bit input usage.

- [ ] **Step 2: Make USB submissions Report-ID aware**

Wrap keyboard reports as `{ 1, struct hid_keyboard_report }` and Consumer reports as `{ 2, uint16_t usage_little_endian }` before calling `hid_device_submit_report()`.

- [ ] **Step 3: Extend BLE HOGP report map**

Add Report ID 1 for keyboard and Report ID 2 for Consumer Control. Increase `CONFIG_BT_HIDS_INPUT_REP_MAX` to `2`, add a second input report entry, and track Consumer notification readiness separately from keyboard/boot readiness.

- [ ] **Step 4: Add BLE Consumer send path**

Implement `transport_ble_send_consumer_report()`. It must reject boot mode with `-ENOTSUP` or use report mode only, and must send on the Consumer report index when connected, encrypted, and subscribed.

- [ ] **Step 5: Build and commit**

Run the project build script if available from the worktree. Commit with `feat: add usb ble consumer reports`.

### Task 3: Encoder And Input Event Generation

**Files:**
- Create: `include/input/encoder_manager.h`
- Create: `src/input/encoder_manager.c`
- Modify: `src/input/input_manager.c`
- Modify: `src/main.c`
- Modify: `boards/mingDev/mini-keyboard/mini-keyboard.dts`
- Modify: `prj.conf`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add encoder DTS and config**

Enable `gpio-qdec` with EC11A P0.10 and EC11B P1.06, `INPUT_REL_WHEEL`, `steps-per-period = <2>`, 2000 us sampling/poll values, and 200 ms idle timeout. Keep NFC pins released through `&uicr`.

- [ ] **Step 2: Change ROW0/COL3 to Mute**

Change the existing matrix mapping from `INPUT_KEY_ESC` to `INPUT_KEY_MUTE` for the EC11 press location only.

- [ ] **Step 3: Add Consumer pulse sending**

In `input_manager.c`, intercept `INPUT_KEY_MUTE` before `keymap_lookup_input_code_usage()`. On press value, log the press and send `Mute` press/release through `transport_send_consumer_report()`. Ignore release.

- [ ] **Step 4: Add encoder manager**

Register an input callback for `INPUT_EV_REL` / `INPUT_REL_WHEEL`. For each positive value, send `Volume Up` pulses; for each negative value, send `Volume Down` pulses. Log `encoder cw` or `encoder ccw`.

- [ ] **Step 5: Initialize encoder manager**

Call `encoder_manager_init()` from `main.c` after input initialization and before the application started log.

- [ ] **Step 6: Build and commit**

Run the project build script if available from the worktree. Commit with `feat: add ec11 encoder input`.

### Integration Task

**Files:** all changed files from Tasks 1-3.

- [ ] Merge Task 1 first because it defines the shared headers and transport APIs.
- [ ] Merge Task 2 next and adapt to the final HID Consumer header.
- [ ] Merge Task 3 last and adapt to the final Consumer pulse helpers.
- [ ] Run `.\scripts\build.ps1 -Pristine -BuildDir "D:\b_ip5306_rtt_usb" -Board "mini-keyboard/nrf52840" -Snippet "rtt-console"`.
- [ ] Run any host C tests that can compile without Zephyr.
- [ ] Review that no stale transport can receive Consumer reports after mode switches.
