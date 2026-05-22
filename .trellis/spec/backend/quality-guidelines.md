# Quality Guidelines

> Code quality standards for backend development.

## Overview

This project values small, explicit module boundaries. The keyboard pipeline is split across input, keymap, HID report, mode, and transport layers, and each layer must keep its own responsibility.

## Forbidden Patterns

### Duplicate matrix-to-HID mapping in the input layer

Do not keep a second static matrix lookup table in `input_manager.c` or anywhere else if the board DTS `input-keymap` already defines the matrix coordinates.

**Why it is bad**: the board description and the C lookup table will drift, and future key remapping becomes a two-place change.

**Instead**: let the board DTS define the matrix topology and let `keymap_lookup_input_code_usage()` map Zephyr input codes to HID usage codes.

### Treating transport readiness as a simple connected flag

Do not assume a transport is ready just because the stack is initialized.

**Why it is bad**: USB needs interface readiness, and BLE needs connection state plus security and notification readiness.

**Instead**: gate every send with the transport-specific ready check.

### Switching modes without releasing the current report

Do not switch from USB to BLE, or BLE to USB, while leaving the old report active.

**Why it is bad**: the host can keep a stuck key if the old transport never receives a release report.

**Instead**: clear and send a release report before changing the mode.

## Required Patterns

### Single source of truth for key usage mapping

Use one function for Zephyr input code to HID usage translation.

```c
uint8_t usage = keymap_lookup_input_code_usage(evt->code);
```

### Report lifetime helpers stay in `hid/`

Keyboard report mutation must use the shared helpers.

```c
hid_keyboard_report_clear(&keyboard_report);
hid_keyboard_report_add_key(&keyboard_report, usage);
hid_keyboard_report_remove_key(&keyboard_report, usage);
```

### Mode transitions must release the active report first

A mode switch should send a release on the active transport before enabling the next one.

## Testing Requirements

- Add a host-side regression test for pure C mapping logic when a new mapping function is introduced.
- Re-run a pristine Zephyr build after transport or board changes.
- Verify that build output still matches the intended board and transport configuration.

## Code Review Checklist

- Does any new code duplicate matrix coordinates that already exist in devicetree?
- Does every keyboard send path check its own readiness gate?
- Does every mode switch release the current report before the switch?
- Did the change introduce a new mapping function that deserves a regression test?
