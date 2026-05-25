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

### Clearing only the transport report during mode switch

Do not send a temporary empty HID report from the mode layer while leaving
`input_manager.c`'s active `keyboard_report` unchanged.

**Why it is bad**: the old host may receive a release, but the next key event on
the new transport can re-send stale keys that were still stored in the input
layer.

**Instead**: mode transitions must call `input_manager_release_all()` before
disabling the previous transport so the active transport receives a release and
the input layer state is cleared from the same source of truth.

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

```c
if (previous_mode != KB_MODE_24G_RESERVED) {
    input_manager_release_all();
    transport_disable(previous_mode);
}
```

### BLE stale bond recovery

Treat `BT_SECURITY_ERR_PIN_OR_KEY_MISSING` as a stale bond condition. The
transport should remove the local bond with `bt_unpair(BT_ID_DEFAULT,
bt_conn_get_dst(conn))` and log that the host may still need to forget the
keyboard if it keeps reconnecting with an old key.

After a stale bond cleanup, delay the advertising restart longer than the normal
disconnect retry. The host can still hold the stale bond and immediately
reconnect with the old key, so a short fixed retry delay creates a connection
storm while the user is trying to remove the host-side pairing. Keep the delay
policy in a small helper and cover it with a host regression test.

Other security failures should be logged but must not blindly erase bonds.
They should also actively disconnect the failed link so the product does not
remain in a visually connected but non-HID-ready state after
`BT_SECURITY_ERR_UNSPECIFIED`, authentication failure, or similar pairing
errors.

### Mode selector calibration must follow measured board voltages

Do not treat datasheet threshold values as final truth for the mode selector.
The ADC-based selector must be implemented as a pure helper that can be tested
on the host, and its decision points must be calibrated against the measured
voltages of the actual board.

**Why it matters**: the BLE switch position on this board has been observed at
roughly `1885 mV`, while the reserved 2.4G position sits around `1021/1027 mV`.
Using the theoretical `2475 mV` BLE threshold misclassifies the BLE position as
reserved and leaves the keyboard stuck in a non-HID mode.

**Instead**: keep the selector logic in one helper and cover it with a
regression test for the real voltages:

```c
enum kb_mode mode_selector_classify_mv(int32_t mv, enum kb_mode current_mode);

assert(mode_selector_classify_mv(1885, KB_MODE_24G_RESERVED) == KB_MODE_BLE);
assert(mode_selector_classify_mv(1021, KB_MODE_24G_RESERVED) == KB_MODE_24G_RESERVED);
```

Use hysteresis around the measured boundaries so the selector does not chatter
when the ADC value hovers near a transition point.

### Mode switches must actively shut down the old transport

When leaving BLE mode, the BLE transport must not remain connected in the
background. `transport_ble_disable()` should cancel advertising restart work,
stop advertising if needed, and disconnect any live connection with a remote
user termination reason before another transport takes over.

**Why it matters**: the product policy is one active HID channel per mode.
Leaving BLE connected while USB is active creates ambiguous state and can keep
the host linked to the wrong channel.

## Testing Requirements

- Add a host-side regression test for pure C mapping logic when a new mapping function is introduced.
- Re-run a pristine Zephyr build after transport or board changes.
- Verify that build output still matches the intended board and transport configuration.

## Code Review Checklist

- Does any new code duplicate matrix coordinates that already exist in devicetree?
- Does every keyboard send path check its own readiness gate?
- Does every mode switch release the current report before the switch?
- Did the change introduce a new mapping function that deserves a regression test?
- If BLE security handling changed, is `BT_SECURITY_ERR_PIN_OR_KEY_MISSING`
  covered by a regression test or a small policy helper?
