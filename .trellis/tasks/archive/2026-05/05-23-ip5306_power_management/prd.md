# IP5306 Lithium Battery Power Management

## Goal

Add a complete lithium battery power subsystem to the existing nRF52840 / Zephyr / NCS mini keyboard firmware. The subsystem must manage IP5306-I2C probing, battery voltage sampling, gated divider control, wakeup / keepalive pulses, USB VBUS presence reporting, low-battery policy hooks, and restrained diagnostics without breaking current USB HID, BLE HID, or reserved 2.4G mode switching behavior.

## What I Already Know

- Hardware authority is `docs/hardware_resources_agent_netlist.md`; it takes priority over earlier notes and assumptions.
- The repository is an existing Zephyr application, not a blank project.
- Current app initialization is `transport_init()` -> `input_manager_init()` -> `mode_manager_init()` in `src/main.c`.
- Current project has no CAF / app_event_manager usage; it uses direct module APIs, Zephyr drivers, logging, and `k_work_delayable`.
- Existing ADC pattern is in `src/mode/mode_manager.c`, using nRF SAADC with `adc_read()` and `adc_raw_to_millivolts()`.
- Current USB code already receives `USBD_MSG_VBUS_READY` / `USBD_MSG_VBUS_REMOVED` internally in `src/transport/transport_usb.c`; power management should not fight USB stack ownership.
- Git started on `master` with one pre-existing dirty file: `docs/hardware_resources_agent_netlist.md`. Development branch created: `feature/ip5306-power-management`.
- The user clarified that the hardware resource netlist is `docs\hardware_resources_agent_netlist.md`.

## Hardware Resource Mapping

| Function | Netlist name | nRF52840 resource | Firmware use |
|---|---|---|---|
| IP5306 I2C SCL | `IP5306T_I2C_SCK` | P0.24 | I2C SCL |
| IP5306 I2C SDA | `IP5306T_I2C_SDA` | P1.00 | I2C SDA |
| IP5306 wakeup / keepalive | `IP5306T_WAKEUP` | P0.22 | Active-low pulse to IP5306 KEY path |
| Battery ADC | `BAT_ADC` | P0.31 / AIN7 | Gated VBAT divider sampling |
| Battery ADC enable | `BAT_ADC_EN` | P0.09 / NFC1 | Enables low-side divider through Q1 |
| USB VBUS | `VBUS` | nRF VBUS sense / U36.27 | USB presence, preferably via USB stack callback |

Notes:

- User text mentioned `IP5305T_*`; the netlist document uses `IP5306T_*`. Firmware naming should use `ip5306_*` and treat `IP5306T_*` as legacy net names.
- `BAT_ADC_EN` must be off except during sampling.
- `BAT_ADC_EN` is on NFC1/P0.09; DTS/Kconfig must release NFCT pins for GPIO use.
- Netlist says `R6 = 100k` from `VBAT` to `BAT_ADC`, `R8 = 100k` from `BAT_ADC` to Q1-switched low side, so enabled divider is approximately 1:2.
- IP5306 BAT path and VOUT path are not direct logical rails: BAT is through R7, VOUT enters the system through the SW4 path to `SYS_POWER`.

## Requirements

- Add a `power` firmware module group without colliding with Zephyr's own power management naming.
- Add IP5306 driver/adapter:
  - Provide `ip5306_init()`, `ip5306_probe()`, `ip5306_read_reg()`, and `ip5306_write_reg()`.
  - Do not write unknown IP5306 registers during normal boot.
  - Treat probe failure as nonfatal unless a later requirement explicitly makes IP5306 mandatory.
- Add battery monitor:
  - Enable `BAT_ADC_EN`.
  - Wait 5-20 ms for divider settling.
  - Read `BAT_ADC` on AIN7.
  - Immediately disable `BAT_ADC_EN`, including error paths.
  - Convert raw ADC to divider millivolts, then estimate `VBAT = adc_mv * 2`.
  - Log raw and millivolt values at controlled frequency.
- Add first-pass battery state classification:
  - `FULL_OR_HIGH`: `>= 4100 mV`
  - `NORMAL`: `3800-4099 mV`
  - `LOW`: `3600-3799 mV`
  - `CRITICAL`: `< 3500 mV`
  - The gap `3500-3599 mV` should be handled explicitly as low/near-critical, not left undefined.
  - Do not expose precise battery percentage in the first version.
- Add IP5306 keepalive:
  - Configure P0.22 as active-low output, inactive by default.
  - Pulse every 8 seconds.
  - Pulse width 300-500 ms.
  - Use Zephyr sleep/work/thread context; no interrupt-context busy wait.
  - Log `power: ip5306 keepalive pulse` at controlled frequency.
- Add USB VBUS presence reporting:
  - Prefer reusing USB stack VBUS events already received in `transport_usb.c`.
  - Log `power: usb power present` and `power: usb power removed`.
  - Avoid adding a second GPIO/ADC owner for the same VBUS hardware resource unless necessary.
- Add low-battery policy:
  - `NORMAL`: no behavior change.
  - `LOW`: warning log and policy callback/event for future RGB/display throttling.
  - `CRITICAL`: critical log and nonfatal high-power feature shutdown hook where supported.
  - Do not force system shutdown in first version.
- Update DTS / Kconfig / build files as needed:
  - GPIO, ADC, I2C, logging enabled.
  - Release NFC pins as GPIO.
  - Enable a concrete I2C/TWIM bus for P0.24/P1.00.
  - Keep existing USB/BLE/HID config intact.
- Keep changes scoped; do not refactor unrelated transport, input, or mode selector behavior.

## Acceptance Criteria

- [ ] `west build -b mini-keyboard/nrf52840 .` completes successfully.
- [ ] Current USB HID, BLE HID, and reserved 2.4G mode switching source paths still build.
- [ ] Firmware logs `power: init ok`.
- [ ] Firmware logs `power: ip5306 probe ok` or `power: ip5306 probe failed: <err>` without crashing.
- [ ] Firmware periodically logs `power: vbat raw=<n> mv=<n>`.
- [ ] Firmware logs battery state transitions, not uncontrolled repeated spam.
- [ ] Firmware sends an IP5306 keepalive pulse every 8 seconds.
- [ ] `BAT_ADC_EN` is configured inactive by default and is disabled after every sample attempt.
- [ ] USB VBUS present/removed logs are emitted through reused USB stack events or a documented alternative.
- [ ] No unknown IP5306 registers are written automatically.
- [ ] Hardware resources used by firmware map back to `docs/hardware_resources_agent_netlist.md`.

## Technical Approach

Recommended architecture:

- `src/power/ip5306.c`, `include/power/ip5306.h`
  - Owns the I2C bus/device binding, address, probe, and raw register read/write helpers.
  - First version only probes and exposes raw helpers; high-level status decoding is deferred until a reliable register table is available.
- `src/power/battery_monitor.c`, `include/power/battery_monitor.h`
  - Owns SAADC channel AIN7, P0.09 divider enable, sampling cadence, voltage conversion, and state classification.
- `src/power/power_policy.c`, `include/power/power_policy.h`
  - Owns battery state to policy mapping and later RGB/display/scan-rate hooks.
- `src/power/power_manager.c`, `include/power/power_manager.h`
  - Top-level init and periodic scheduling for IP5306 keepalive and battery sampling.
- `transport_usb.c`
  - Minimal integration only: forward VBUS ready/removed events into `power_manager_usb_power_present(bool)` if power manager is initialized.

This is deliberately less invasive than introducing CAF. The current firmware already uses direct module APIs and delayable work; matching that pattern reduces risk to HID and mode switching.

## Power State Machine

- Boot:
  - Initialize IP5306 GPIO/I2C adapter.
  - Initialize battery monitor GPIO/ADC.
  - Initialize policy state.
  - Schedule keepalive work and battery sample work.
- Periodic keepalive:
  - Drive P0.22 active.
  - Sleep for configured pulse width in a thread/work context.
  - Drive P0.22 inactive.
  - Reschedule after 8 seconds.
- Periodic battery sample:
  - Enable divider.
  - Wait for settling.
  - Read ADC and convert to VBAT mV.
  - Disable divider.
  - Classify battery state.
  - Notify policy on state transition.
- USB VBUS event:
  - USB stack reports present/removed.
  - Power manager records state and logs transition.

## Research References

- `research/ip5306.md` - IP5306-I2C capabilities, pin behavior, light-load shutdown, and register-table risk.
- `research/zephyr_power_interfaces.md` - Zephyr GPIO, ADC, I2C, and delayable work APIs relevant to implementation.

## Decision (ADR-lite)

Context: The power subsystem spans battery ADC, IP5306 I2C, wakeup pulses, USB state, and policy hooks. The project currently favors simple C modules and Zephyr work items.

Decision: Implement a small `power` module group and one top-level `power_manager_init()` call from `main.c`; reuse existing USB VBUS events and avoid CAF/event-manager introduction in this task.

Consequences: The first version is easy to inspect and less likely to regress keyboard paths. Future RGB/display throttling can be added through explicit module APIs or a later event bus if the application grows.

User approval: Option 1 confirmed on 2026-05-23. Proceed with low-intrusion `src/power` modules plus isolated multiagent/worktree slices.

## Out of Scope

- Accurate battery percentage / fuel-gauge behavior.
- Writing IP5306 customization registers without a verified register map and hardware validation plan.
- Automatic firmware shutdown on critical battery.
- RGB/display implementation if those modules are not present yet.
- Broad refactors of USB, BLE, input, keymap, or mode selector internals.

## Implementation Plan

Small-step commits:

1. `power: add ip5306 hardware definitions`
   - DTS/Kconfig/CMake/include skeleton and IP5306 adapter.
2. `power: add battery adc monitor`
   - Gated divider control, ADC conversion, state classifier, focused tests if feasible.
3. `power: add ip5306 keepalive`
   - Periodic active-low pulse scheduling.
4. `power: add low battery policy`
   - Policy state transitions and USB VBUS integration logs.

## Multiagent / Worktree Strategy

Preferred execution after PRD approval:

- Use isolated worktrees for parallel exploration or implementation slices when the files do not overlap.
- Do not let multiple agents edit the same source files concurrently.
- Good split:
  - Agent A: DTS/Kconfig/IP5306 I2C adapter.
  - Agent B: Battery ADC monitor and classifier tests.
  - Agent C: Keepalive/policy integration and USB VBUS forwarding.
- Merge in dependency order: hardware definitions -> ADC monitor -> keepalive -> policy/USB integration -> final build check.

## Known Risks

- IP5306-I2C register maps found online are not all primary vendor material. First implementation must avoid automatic register writes.
- I2C address `0x75` is common in IP5306 examples but should remain easy to change and must be confirmed on hardware with probe/logging.
- P0.09 will not work as GPIO if NFCT pin release is missing or incompatible with the active NCS version.
- A 300-500 ms sleep inside the system workqueue can delay other work items. If this proves risky, use a dedicated low-priority thread or a two-stage work pulse.
- USB stack VBUS callbacks may only fire when USB transport is initialized/enabled; if battery mode needs independent VBUS tracking, a later hardware-specific VBUS sense path may be required.

## Manual Test Steps

1. Build firmware with the selected board command.
2. Flash to hardware with battery connected and USB disconnected.
3. Confirm boot log includes power init and battery voltage logs.
4. Confirm `BAT_ADC_EN` is only active around each 5-second sample using meter/scope if possible.
5. Confirm P0.22 produces an active-low keepalive pulse every 8 seconds.
6. Connect USB and confirm `power: usb power present`.
7. Remove USB and confirm `power: usb power removed`.
8. Verify key input still works in USB mode.
9. Verify BLE HID still starts/connects in BLE mode.
10. Move selector to reserved 2.4G position and confirm existing reserved-mode behavior remains unchanged.
