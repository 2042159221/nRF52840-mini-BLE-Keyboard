# BLE Battery Service and Evented Power State

## Goal

Implement BLE Battery Service integration backed by real estimated battery state, and introduce a lightweight project-local power state publish/subscribe layer that can later be replaced by CAF without rewriting consumers.

## Requirements

- Keep hardware access centralized:
  - `battery_monitor` owns ADC setup, raw sampling, voltage conversion, and simple voltage classification.
  - `ip5306` owns register access and semantic IP5306 status decoding.
  - `power_manager` owns periodic sampling, state fusion, hysteresis/debounce, latest snapshot storage, and state publication.
  - BLE BAS, power policy, future display, RGB, or host protocol code must consume published power state instead of reading hardware directly.
- Define a unified power state snapshot:
  - `voltage_mv`
  - `estimated_percent`
  - `level_state`
  - `usb_present`
  - `charging_state`
  - `valid_flags`
  - `timestamp_ms`
- Include `valid_flags` so partial information is explicit:
  - ADC voltage may be valid while IP5306 charging status is unavailable.
  - USB presence may be known while full/charging state is unknown.
  - Startup may have no completed sample yet.
- Add lightweight event/subscription API:
  - `power_state_subscribe(listener, user_data)`
  - `power_state_get_latest(snapshot)`
  - internal publish-if-changed helper
  - fixed small subscriber capacity is acceptable for V1.
- Ensure callbacks receive complete snapshots and consumers can safely query the latest state.
- Add battery percent estimation with product-level semantics, named `estimated_percent`:
  - charging full -> `100`
  - `>= 4100 mV` -> `95`
  - `3800-4099 mV` -> `60`
  - `3500-3799 mV` -> `15`
  - `< 3500 mV` -> `5`
- Add level hysteresis/debounce in the power state fusion layer:
  - `NORMAL -> LOW`: `vbat < 3500 mV` for N consecutive samples.
  - `LOW -> NORMAL`: `vbat > 3600 mV` for N consecutive samples.
  - `LOW -> CRITICAL`: `vbat < 3300 mV` for N consecutive samples.
  - `CRITICAL -> LOW`: `vbat > 3450 mV` for N consecutive samples.
  - Use a small named sample count constant for V1.
- Extend IP5306 with semantic APIs:
  - `ip5306_get_charging_state()`
  - `ip5306_is_usb_present()`
  - `ip5306_is_charge_full()`
  - `ip5306_get_status()`
- Do not expose IP5306 register magic numbers to upper layers.
- Avoid boot-time IP5306 writes; read-only semantic status is in scope.
- Upgrade `power_manager` to:
  - preserve IP5306 wakeup keepalive behavior.
  - initialize battery monitor and IP5306 non-fatally where appropriate.
  - trigger an initial battery sample as soon as practical.
  - periodically publish fused snapshots.
  - update power policy through the published state path, not through scattered direct battery enum calls.
  - update USB presence into the latest snapshot and publish changes.
- Add a BLE BAS adapter:
  - subscribes to power state.
  - reads only `estimated_percent` from valid snapshots.
  - calls `bt_bas_set_battery_level(percent)`.
  - filters duplicate or insignificant updates.
  - updates immediately on first valid percent.
  - updates when percent changes by at least 5%.
  - forces update when level enters LOW or CRITICAL.
  - forces update when charging state becomes FULL and uses `100`.
- Remove `bt_bas_set_battery_level(100)` as the hard-coded BLE source of truth from `transport_ble.c`.
- BLE initialization may set a conservative default only through the BAS adapter if no valid snapshot exists yet.
- Add Device Information Service config to `prj.conf`:
  - `CONFIG_BT_DIS=y`
  - `CONFIG_BT_DIS_PNP=y`
  - firmware and hardware version strings or equivalent available Kconfig values.
- Keep current HIDS and BAS advertising behavior intact.

## Acceptance Criteria

- [ ] Host C unit tests cover battery percent estimation boundaries.
- [ ] Host C unit tests cover power state latest snapshot, subscription, change filtering, partial validity, and hysteresis behavior.
- [ ] Host C unit tests cover BLE BAS adapter update filtering using test stubs.
- [ ] Existing `battery_monitor` and `power_policy` tests still pass.
- [ ] `transport_ble.c` no longer hard-codes `bt_bas_set_battery_level(100)` as the active battery value.
- [ ] BLE BAS receives estimated battery percent from power state snapshots.
- [ ] `power_policy` consumes published power state snapshots and still produces LOW/CRITICAL actions once per state transition.
- [ ] `ip5306` semantic APIs hide register addresses and bit masks from `power_manager`.
- [ ] `prj.conf` enables DIS without disabling existing BLE HIDS/BAS settings.
- [ ] Zephyr build succeeds with the project script:
  `.\scripts\build.ps1 -Pristine -BuildDir "D:\b_ip5306_rtt_usb" -Board "mini-keyboard/nrf52840" -Snippet "rtt-console"`.

## Technical Approach

- Add a focused `power_state` module under `include/power/` and `src/power/`.
- Keep `battery_monitor` unchanged except for any small helper that is directly covered by tests and still hardware-agnostic.
- Add IP5306 semantic status structs/enums in `include/power/ip5306.h` and implement decoding in `src/power/ip5306.c`.
- Add `ble_bas_adapter` under `include/transport/` and `src/transport/` so BLE service code remains outside `power_manager`.
- Adjust `power_manager` to build snapshots and publish them through `power_state`.
- Adjust `power_policy` API to accept a snapshot or add a small adapter function, while preserving current enum-based behavior for compatibility tests if useful.
- Update `CMakeLists.txt` for new modules.
- Keep code comments sparse and only where register or hysteresis decisions would otherwise be unclear.

## Parallel Worktree Plan

- Agent A, `power-state-core`: owns `power_state`, battery estimation, hysteresis helpers, `power_policy` snapshot integration, and associated host tests.
- Agent B, `ip5306-status`: owns IP5306 semantic enums/status APIs and read-only register decoding tests/stubs.
- Agent C, `ble-bas-adapter`: owns BLE BAS adapter, `transport_ble.c` BAS hookup removal, DIS config, and BAS adapter host tests/stubs.
- Main integrator: owns `power_manager` fusion, CMake integration, cross-module conflict resolution, build/test execution, Trellis quality check, spec update, and commit.

## Out of Scope

- Do not migrate to CAF in this task.
- Do not add RGB, LCD, host protocol, or fuel-gauge SOC algorithms.
- Do not change battery ADC hardware resources or divider ratio.
- Do not implement linear SOC estimation.
- Do not write IP5306 configuration registers.
- Do not change USB HID, keyboard input, EC11, or mode-switch behavior except where power event consumers require it.

## Technical Notes

- The project currently has host C tests based on `assert`, not a Zephyr unit-test harness.
- `UNIT_TEST` guards already exist in `battery_monitor.c`; new hardware-coupled modules should be testable with small compile-time stubs.
- `power_manager_init()` runs before `transport_init()` in `main.c`, so an initial power snapshot can exist before BLE initializes if sampling succeeds.
- `power_manager_usb_power_present(bool present)` is already called from USB transport VBUS handling and should feed the snapshot valid flag instead of only logging.
- `docs/hardware_resources_agent_netlist.md` and `.trellis/spec/backend/embedded-power-guidelines.md` remain the hardware authority.
