# Codebase State Research: Bluetooth Keyboard Implementation

## Source Documents

* `docs/第05章项目实现.md`
* `docs/mini_keyboard_solution.md`
* `docs/按键矩阵实现方案.md`

## Current Architecture

The repository already contains a first-stage keyboard firmware skeleton. It is not an empty NCS project.

Existing module boundaries:

* `src/input/input_manager.c`: receives Zephyr input events, updates keyboard report state, and sends reports through the current transport.
* `src/keymap/keymap.c`: defines a row/column to HID usage matrix, but is currently not used by `input_manager.c`.
* `src/hid/hid_report.c`: maintains a standard 6KRO keyboard report.
* `src/mode/mode_manager.c`: controls current mode and switches between USB and BLE.
* `src/transport/transport.c`: dispatches transport operations by mode.
* `src/transport/transport_usb.c`: implements USB HID keyboard report sending.
* `src/transport/transport_ble.c`: implements BLE HOGP keyboard report sending.
* `src/transport/transport_24g_stub.c`: keeps 2.4G as an explicit unsupported placeholder.

## Matrix Input State

`boards/mingDev/mini-keyboard/mini-keyboard.dts` already defines:

* `compatible = "gpio-kbd-matrix"`
* six rows and four columns
* `actual-key-mask = <0x3e 0x3e 0x1e 0x2b>`
* `input-keymap` entries for 18 valid keys

The DTS path follows the document recommendation: Zephyr `gpio-kbd-matrix` plus `input-keymap`.

## Key Mapping Concern

There are two different key mapping concepts in the codebase:

1. DTS `input-keymap`: maps matrix coordinates to Zephyr input key codes.
2. `src/keymap/keymap.c`: maps row/column to HID usages.

`input_manager.c` currently bypasses `keymap.c` and maps Zephyr input key codes directly to HID usages.

This is a real maintenance risk. The implementation should converge to one source of truth:

* Preferred for this stage: keep DTS `input-keymap` as the matrix coordinate source, and keep C code focused on input key code to HID usage/action mapping.
* Avoid maintaining a second row/column matrix in C unless the input layer is redesigned to consume raw matrix coordinates.

## USB HID State

`transport_usb.c` already uses Zephyr's next USB device stack and registers a HID keyboard device.

Notable issue:

* Current VID/PID is `0x1915:0x5201`.
* `docs/第05章项目实现.md` lists `0x1915:0x52F0`.

The implementation scope should either align the value with the document or explicitly record why the code value is retained.

## BLE HID State

`transport_ble.c` already initializes:

* Bluetooth peripheral
* Nordic `bt_hids`
* Battery Service
* Settings
* HID report map
* advertising with HIDS and BAS UUIDs

Potential gap:

* `transport_ble_ready()` currently checks only enabled + connected.
* The project document expects reports to be sent only after secure connection and HID notification readiness.

The implementation should strengthen BLE report gating where practical.

## Recommended MVP Boundary

Recommended first implementation scope:

* matrix input to HID report convergence
* USB keyboard report path
* BLE HOGP keyboard report path
* USB/BLE mode switching
* 2.4G placeholder retained as unsupported

Explicitly defer:

* IP5306 driver
* battery SOC
* low-power wake integration
* EC11 encoder and consumer reports
* LED strip
* LVGL UI
* host protocol
* real 2.4G dongle protocol

This boundary matches the current codebase state and keeps bring-up testable.
