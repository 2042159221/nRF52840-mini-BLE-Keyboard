# HID Consumer Transport Contracts

## Scenario: EC11 knob Consumer Control reports

### 1. Scope / Trigger

- Trigger: firmware adds or changes HID Consumer usages, USB HID report descriptors,
  BLE HOGP report maps, Zephyr input events for the EC11 encoder, or transport
  send APIs.
- Goal: keep keyboard 6KRO reports and Consumer Control reports independent while
  sharing the active mode dispatcher.
- Hardware authority: the EC11 push position is part of the keyboard matrix and
  should remain described by board DTS/input-keymap coordinates. Encoder A/B
  pins should be described by the board DTS encoder node, not by C lookup tables.

### 2. Signatures

- `struct hid_consumer_report { uint16_t usage; }`
- `void hid_consumer_report_press(struct hid_consumer_report *report, uint16_t usage)`
- `void hid_consumer_report_release(struct hid_consumer_report *report)`
- `int transport_send_consumer_report(enum kb_mode mode, const struct hid_consumer_report *report)`
- `bool transport_consumer_ready(enum kb_mode mode)`
- `int encoder_action_init(void)`
- `int encoder_action_submit(uint8_t action)`
- `int transport_usb_send_consumer_report(const struct hid_consumer_report *report)`
- `int transport_ble_send_consumer_report(const struct hid_consumer_report *report)`
- `int transport_24g_send_consumer_report(const struct hid_consumer_report *report)`

### 3. Contracts

- Consumer usages currently used by the product:
  - `HID_CONSUMER_MUTE = 0x00E2`
  - `HID_CONSUMER_VOLUME_UP = 0x00E9`
  - `HID_CONSUMER_VOLUME_DOWN = 0x00EA`
- Every Consumer action is a pulse: send the usage first, then send
  `HID_CONSUMER_NONE` / `0x0000` as the release report.
- Do not add Mute, Volume Up, or Volume Down to `hid_keyboard_report`; they are
  Consumer Page usages, not Keyboard Page usages.
- USB report descriptors that use report IDs must submit report-id-prefixed
  buffers:

```c
usb_report[0] = USB_REPORT_ID_CONSUMER;
sys_put_le16(report->usage, &usb_report[1]);
return hid_device_submit_report(hid_device, sizeof(usb_report), usb_report);
```

- BLE HOGP report references identify reports at the GATT/service layer. The
  payload sent with `bt_hids_inp_rep_send()` must not include the report ID:

```c
sys_put_le16(report->usage, consumer_report);
return bt_hids_inp_rep_send(&hids_obj, ble_conn, INPUT_REP_CONSUMER_IDX,
                            consumer_report, sizeof(consumer_report), NULL);
```

- BLE Consumer readiness is not the same as keyboard readiness. Consumer reports
  require connection, security, report mode, and the Consumer input report CCCD
  subscription. Do not gate Consumer reports with `transport_ready()`, because
  that API currently means keyboard-report readiness.
- Zephyr input callbacks must not send Consumer HID reports directly. Encoder
  rotation and EC11 push actions must call `encoder_action_submit()` so the
  actual press/release pulse runs in the dedicated `encoder_action` thread.
- `encoder_action_init()` starts the worker during boot. `encoder_action_submit()`
  uses `k_msgq_put(..., K_NO_WAIT)` and must never block the input callback. If
  the queue is full it returns `-ENOMSG`; callers should log a rate-limited
  warning and drop the excess action.
- If the Consumer transport is not ready, the worker skips that action and logs
  a rate-limited warning. It does not retry the skipped action, because queued
  stale knob detents would surprise the host after reconnect.
- BLE boot protocol does not carry Consumer Control reports; return `-ENOTSUP`
  from the BLE Consumer send path while in boot mode.
- The reserved 2.4G transport must return `-ENOTSUP` for Consumer reports until
  a real 2.4G HID protocol is implemented.

### 4. Validation & Error Matrix

- `report == NULL` -> return `-EINVAL`.
- USB transport disabled or HID interface not ready -> return `-ENOTCONN`.
- BLE disconnected, not encrypted to `BT_SECURITY_L2`, or Consumer notifications
  not subscribed -> return `-ENOTCONN`.
- `encoder_action_submit()` queue full in input callback -> return `-ENOMSG` and
  drop the action without blocking Zephyr's input/sysworkqueue path.
- BLE boot mode Consumer send -> return `-ENOTSUP`.
- 2.4G Consumer send before implementation -> return `-ENOTSUP`.
- First half of a Consumer pulse fails -> do not send the release half; return
  the original error so the caller can log the failed action.

### 5. Good / Base / Bad Cases

- Good: encoder clockwise event logs `encoder cw`, sends Volume Up, then sends
  `0x0000` through the currently selected transport.
- Good: matrix EC11 push maps to `INPUT_KEY_MUTE`; `input_manager.c` intercepts
  the press and sends one Mute pulse, while release is ignored.
- Good: input callback enqueues one action with `encoder_action_submit()` and
  returns quickly; the worker thread performs `transport_consumer_ready()` and
  sends the HID pulse.
- Base: USB and BLE descriptors both expose keyboard Report ID 1 and Consumer
  Report ID 2, but USB send buffers include the ID while BLE payloads do not.
- Bad: using `transport_ready(mode)` before a Consumer send in BLE mode. Keyboard
  notifications may be subscribed while Consumer notifications are not, or vice
  versa.
- Bad: calling `encoder_action_trigger()` from a Zephyr input callback. Slow or
  disconnected transports can fill the input queue and produce `input: Event
  dropped, queue full`.
- Bad: sending only `0x00E9` without a following `0x0000`, which can leave the
  host seeing a held Consumer key or coalescing later events incorrectly.

### 6. Tests Required

- Unit-test `hid_consumer_report_press()` and `hid_consumer_report_release()`.
- Host-test pure mapping logic when adding new input-code to HID-usage
  translation.
- Unit-test Consumer action mapping and the `transport_consumer_ready()` guard
  so not-ready transports do not receive partial press/release reports.
- Run `git diff --check` after descriptor or report-buffer edits.
- Run a pristine Zephyr build after touching USB/BLE descriptors, board DTS,
  Kconfig, CMake, or transport signatures. Prefer:
  `.\scripts\build.ps1 -Pristine -BuildDir "D:\b_ip5306_rtt_usb" -Board "mini-keyboard/nrf52840" -Snippet "rtt-console"`.
- Inspect generated DTS and `.config` after encoder or HID-size changes:
  confirm `gpio-qdec`, EC11 GPIOs, `CONFIG_INPUT_GPIO_QDEC=y`,
  `CONFIG_BT_HIDS_INPUT_REP_MAX >= 2`, and USB HID `in-report-size` large
  enough for the largest report including report ID.
- Hardware validation still requires flashing and testing USB and BLE hosts,
  because host Consumer Control behavior cannot be proven by a build alone.

### 7. Wrong vs Correct

#### Wrong

```c
if (!transport_ready(mode)) {
    return -ENOTCONN;
}

report.usage = HID_CONSUMER_MUTE;
return transport_send_consumer_report(mode, &report);
```

#### Correct

```c
hid_consumer_report_press(&report, HID_CONSUMER_MUTE);
err = transport_send_consumer_report(mode, &report);
if (err != 0) {
    return err;
}

hid_consumer_report_release(&report);
return transport_send_consumer_report(mode, &report);
```

#### Wrong

```c
consumer_report[0] = INPUT_REP_CONSUMER_REF_ID;
sys_put_le16(report->usage, &consumer_report[1]);
bt_hids_inp_rep_send(&hids_obj, ble_conn, INPUT_REP_CONSUMER_IDX,
                     consumer_report, sizeof(consumer_report), NULL);
```

#### Correct

```c
sys_put_le16(report->usage, consumer_report);
bt_hids_inp_rep_send(&hids_obj, ble_conn, INPUT_REP_CONSUMER_IDX,
                     consumer_report, sizeof(consumer_report), NULL);
```
