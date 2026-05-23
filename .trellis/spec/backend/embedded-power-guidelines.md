# Embedded Power Guidelines

## Scenario: IP5306-backed battery power subsystem

### 1. Scope / Trigger

- Trigger: firmware touches hardware power rails, battery ADC, I2C power-management ICs, wakeup pins, or USB VBUS state.
- Hardware authority: `docs/hardware_resources_agent_netlist.md` is the source of truth for pin and net mapping. Do not infer GPIO, ADC, I2C bus, or polarity from naming alone.

### 2. Signatures

- `int ip5306_init(void)`
- `int ip5306_probe(void)`
- `int ip5306_read_reg(uint8_t reg, uint8_t *value)`
- `int ip5306_write_reg(uint8_t reg, uint8_t value)`
- `int battery_monitor_init(void)`
- `int battery_monitor_sample_once(struct battery_monitor_sample *sample)`
- `int power_manager_init(void)`
- `void power_manager_usb_power_present(bool present)`

### 3. Contracts

- IP5306 wakeup uses the netlist-backed active-low GPIO on P0.22 and must be inactive by default.
- Battery ADC uses P0.31 / AIN7 through a gated divider. Enable `BAT_ADC_EN` only around a sample and turn it off on success and failure paths.
- Enabled divider ratio is currently R6 100k to R8 100k, so first-order VBAT estimate is `adc_mv * 2`.
- P0.09 is NFC1, so board DTS must release NFCT pins as GPIO before using `BAT_ADC_EN`.
- USB VBUS state should reuse existing USB stack VBUS events before adding another owner for the same hardware signal.
- Do not write IP5306 registers during boot unless a verified register map and hardware validation plan exist.

### 4. Validation & Error Matrix

- I2C bus not ready -> return `-ENODEV`, log nonfatal warning.
- IP5306 probe read fails -> log `power: ip5306 probe failed: <err>`, continue boot.
- ADC device or divider GPIO not ready -> return `-ENODEV`; do not schedule periodic sampling.
- ADC read fails after divider enable -> disable `BAT_ADC_EN`, then return the ADC error.
- Unknown battery state enum -> report `"UNKNOWN"` in diagnostic helpers.

### 5. Good / Base / Bad Cases

- Good: sample path enables divider, sleeps for settling, reads ADC, disables divider, converts and classifies voltage.
- Base: IP5306 I2C probe fails on a board without the chip populated, but keyboard input and transports still initialize.
- Bad: divider enable remains high outside the sampling window, causing continuous battery drain.

### 6. Tests Required

- Unit-test battery voltage conversion and state thresholds, including boundary values.
- Unit-test low/critical policy actions and repeated-state no-op behavior.
- Build the Zephyr firmware with the project board after DTS or Kconfig changes.
- Use the NCS toolchain Python wrapper for Zephyr builds. In this repository, prefer:
  `.\scripts\build.ps1 -Pristine -BuildDir "D:\b_ip5306_rtt_usb" -Board "mini-keyboard/nrf52840" -Snippet "rtt-console"`.
  Do not call bare `west`; the script loads `scripts/ncs-env.ps1` and runs
  `E:\ncs\toolchains\fd21892d0f\opt\bin\python.exe -m west`.
- Inspect generated DTS when changing hardware resources: confirm GPIO polarity, I2C address, pinctrl selections, and `nfct-pins-as-gpios`.

### 7. Wrong vs Correct

#### Wrong

```c
gpio_pin_set_dt(&battery_adc_enable, 1);
err = adc_read(battery_adc_dev, &battery_adc_sequence);
if (err < 0) {
    return err;
}
```

#### Correct

```c
err = gpio_pin_set_dt(&battery_adc_enable, 1);
if (err < 0) {
    return err;
}

k_sleep(K_MSEC(BATTERY_ADC_SETTLE_MS));
err = adc_read(battery_adc_dev, &battery_adc_sequence);
(void)gpio_pin_set_dt(&battery_adc_enable, 0);
if (err < 0) {
    return err;
}
```
