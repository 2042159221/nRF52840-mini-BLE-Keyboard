# Directory Structure

> How backend code is organized in this project.

## Overview

This project is an embedded firmware codebase, but the same layering rule applies: each module owns one part of the keyboard pipeline and does not duplicate another layer's responsibility.

The current layout centers on a small set of responsibilities:
- `input/` owns Zephyr input callbacks and keyboard report lifetime.
- `keymap/` owns input-event-code to HID-usage mapping.
- `hid/` owns keyboard report mutation helpers.
- `mode/` owns the active transport mode.
- `transport/` owns protocol-specific enable/ready/send logic.
- `boards/mingDev/mini-keyboard/` owns the hardware description, including the matrix scan topology.

## Directory Layout

```text
include/
  hid/
  input/
  keymap/
  mode/
  transport/
boards/
  mingDev/
    mini-keyboard/
      mini-keyboard.dts
      mini-keyboard-pinctrl.dtsi
src/
  hid/
    hid_report.c
  input/
    input_manager.c
  keymap/
    keymap.c
  mode/
    mode_manager.c
  transport/
    transport.c
    transport_ble.c
    transport_usb.c
    transport_24g_stub.c
```

## Module Organization

- `boards/mingDev/mini-keyboard/mini-keyboard.dts` is the source of truth for matrix wiring and `input-keymap` coordinates.
- `src/input/input_manager.c` translates Zephyr input events into report updates and handles the mode switch key behavior.
- `src/keymap/keymap.c` maps Zephyr input codes to HID usage codes; it does not duplicate matrix coordinates.
- `src/hid/hid_report.c` only mutates the keyboard report payload.
- `src/transport/transport.c` dispatches to the active transport, but does not interpret key meaning.
- `src/transport/transport_usb.c` and `src/transport/transport_ble.c` own transport-specific readiness checks and send calls.
- `src/transport/transport_24g_stub.c` remains an explicit reserved placeholder and must not pretend to be a working implementation.
- `src/mode/mode_manager.c` owns the current mode and mode transitions only.

## Naming Conventions

- Keep module names aligned with their responsibility.
- Prefer one feature boundary per file when possible.
- Keep hardware description in board files, not in C lookup tables.
- Keep protocol logic in transport modules, not in input or HID helpers.

## Examples

- `src/input/input_manager.c` is the place to look for report lifetime and mode-switch behavior.
- `src/keymap/keymap.c` is the place to look for the keyboard semantics mapping.
- `src/transport/transport_ble.c` is the place to look for BLE readiness and send gating.
- `src/transport/transport_usb.c` is the place to look for USB HID registration and report submission.
