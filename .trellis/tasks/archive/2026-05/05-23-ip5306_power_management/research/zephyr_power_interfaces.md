# Zephyr Power Interface Research Notes

## Sources

- Zephyr GPIO documentation: https://docs.zephyrproject.org/latest/hardware/peripherals/gpio.html
- Zephyr GPIO API reference: https://docs.zephyrproject.org/latest/doxygen/html/group__gpio__interface.html
- Zephyr ADC API reference: https://docs.zephyrproject.org/apidoc/latest/group__adc__interface.html
- Zephyr I2C documentation: https://docs.zephyrproject.org/latest/hardware/peripherals/i2c.html
- Zephyr I2C API reference: https://docs.zephyrproject.org/apidoc/latest/group__i2c__interface.html
- Zephyr workqueue documentation: https://docs.zephyrproject.org/latest/kernel/services/threads/workqueue.html
- Existing project ADC implementation: `src/mode/mode_manager.c`.
- Existing project USB VBUS callback usage: `src/transport/transport_usb.c`.

## GPIO

- Zephyr GPIO APIs support devicetree-backed `gpio_dt_spec`, readiness checks, configure, get, set, and interrupts.
- Recommended pattern for IP5306 wakeup and BAT_ADC_EN:
  - Define nodes/properties in board DTS.
  - Use `GPIO_DT_SPEC_GET()`.
  - Check `gpio_is_ready_dt()`.
  - Configure outputs with `GPIO_OUTPUT_INACTIVE`.
  - Use `gpio_pin_set_dt()` for logical active/inactive semantics.

## ADC

- Existing project already enables `CONFIG_ADC=y` and uses `&adc { status = "okay"; }`.
- Existing mode selector uses manual nRF SAADC channel configuration with:
  - `ADC_GAIN_1_6`
  - `ADC_REF_INTERNAL`
  - 12-bit resolution
  - `adc_read()`
  - `adc_raw_to_millivolts()`
- Battery monitor can reuse this style for AIN7 to minimize design drift.
- Enabled divider ratio from netlist is approximately:
  - `adc_mv = VBAT * R8 / (R6 + R8)`
  - With `R6 = 100k` and `R8 = 100k`, `VBAT ~= adc_mv * 2`
- Because low side is switched by Q1, the firmware must wait briefly after enabling the divider and must disable it after every sample attempt.

## I2C

- Zephyr I2C controller API is stable and enabled by `CONFIG_I2C`.
- Use controller-mode helpers such as `i2c_write_read()` for register reads and `i2c_write()` for register writes.
- For this hardware, a TWIM/I2C node must be enabled and pinned to:
  - SCL P0.24
  - SDA P1.00
- First version should use a devicetree child node for `ip5306@75` with `reg = <0x75>` so the address is not buried in C.

## Delayable Work / Keepalive

- Zephyr `k_work_delayable` is appropriate for periodic low-priority maintenance.
- Workqueue docs allow handlers to use thread-context APIs, but blocking a workqueue delays later work items.
- For a 300-500 ms keepalive pulse, safest options:
  - Two-stage delayable work: set active, schedule another work item to set inactive, then schedule next pulse.
  - Dedicated low-priority thread.
- Preferred first implementation: two-stage delayable work, so the system workqueue is not blocked for hundreds of milliseconds.

## USB VBUS

- Existing `transport_usb.c` already receives `USBD_MSG_VBUS_READY` and `USBD_MSG_VBUS_REMOVED`.
- Power manager should consume these state changes through a small callback/API instead of independently grabbing USB hardware.
- If USB VBUS events are only available after USB transport init/enable, document that limitation and defer independent VBUS sense to a later hardware-specific task.
