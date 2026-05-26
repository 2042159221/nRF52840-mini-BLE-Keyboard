# LVGL Status Screen Local Settings Runtime

## Configuration

| Item | Choice |
|---|---|
| Use subagents | Enabled |
| Programming paradigm | Event-driven + modular C |
| Language | C / Zephyr NCS |
| Project type | Embedded HMI firmware |
| Comment style | Minimal, only for non-obvious control flow |
| Code structure | Layered model / runtime / LVGL view / input adapter |
| Error handling | Robust + contextual nonfatal boot failures |
| Performance | Medium + bounded RAM/CPU for nRF52840 |

## Product Goal

Build the local settings system for the nRF52840 mini keyboard status screen:

- ST7789V/LVGL remains a lightweight local HMI, not a full desktop-style UI.
- EC11 rotation is Volume Up/Down HID passthrough on Home.
- Long press opens Settings.
- While local UI is active, rotation/short press/long press are consumed by the UI.
- No Esc dependency. Each non-home level has a visible Exit/Cancel path.
- Changes are event-driven and flow through existing app-config boundaries.

## User-Facing Content Strategy

Home screen should answer, at a glance:

- Current mode/connection state.
- Battery or power state.
- RGB brightness/config summary.
- Keyboard LED status, especially Num Lock.
- A subtle, compact affordance that long press opens local settings.

Settings menu should be shallow and rotary-friendly:

- `RGB Brightness`
  - Edit value with rotation.
  - Short press applies and persists through `app_config_set()` + `app_config_store_save()`.
  - Visible `Cancel` exits edit without applying.
- `Factory Reset`
  - Opens confirmation page.
  - Default focus is `Cancel`.
  - User must rotate to `Confirm` before short press triggers `app_config_store_factory_reset()`.
- `Exit`
  - Always visible in Settings and returns Home.

MVP intentionally excludes text entry, deep nested categories, animations, bitmap assets, and hidden gestures beyond long press.

## Interaction Model

Use a LIT DUO-style rotary interaction:

- Rotation moves focus in menu pages.
- Rotation edits numeric values on edit pages.
- Short press selects the focused item or applies the edited value.
- Long press enters Settings from Home, and backs out from Settings/Edit/Confirm to the previous safe level.

Adopt Xiaomi-like response and theme rules:

- UI state changes must appear immediately after input.
- Destructive action confirmation defaults to the safe option.
- Active/focused controls use one accent color; warnings use a separate warm color.
- Keep contrast high on the 320 x 172 visible area.

Adopt SumUp-style embedded implementation constraints:

- No heavyweight scene graph rebuild per input event.
- Keep widgets templated and update labels/focus styling from snapshots.
- Avoid blocking input callbacks.

Adopt Whisker-style templated pages:

- One Home template.
- One Settings list template.
- One numeric Edit template.
- One Confirm template.

## Theme Direction

Status screen palette:

- Background: near-black neutral `#111418`.
- Surface band: charcoal `#1B2026`.
- Primary text: cool white `#F4F7FA`.
- Secondary text: desaturated gray-blue `#9AA6B2`.
- Accent/focus: cyan `#21C7D9`.
- Positive: green `#42C77A`.
- Warning/destructive: amber `#F5A524`.
- Disabled/divider: `#303742`.

Do not make the UI one-note purple, dark-blue-only, beige, or orange/brown themed. Use restrained color because this is a utility device screen.

## Runtime Requirements

1. `status_screen.c` owns one global `struct status_screen_model`.
2. Runtime input API exposes a hardware-independent boundary, such as:
   - `bool status_screen_handle_encoder_delta(int32_t delta)`
   - `bool status_screen_handle_encoder_press(bool pressed)`
3. Home rotation returns `false` so existing HID volume flow continues.
4. Active UI input returns `true` so HID submitters do not receive local navigation events.
5. EC11 press handling distinguishes:
   - short press: select/apply when UI active, otherwise current HID encoder action.
   - long press: enter/exit UI locally and consume the HID action.
6. Runtime handles model actions:
   - RGB brightness apply updates `app_config.rgb_brightness`, calls `app_config_set(&config, true)`, then `app_config_store_save()`.
   - Factory reset confirmed calls `app_config_store_factory_reset()`.
7. Every model state change schedules/executes LVGL update with a snapshot.

## LVGL View Requirements

1. `status_screen_lvgl_update()` switches visible page based on snapshot/model state.
2. Settings list shows dynamic focus.
3. Edit page shows dynamic value and visible Cancel/Apply affordance.
4. Confirm page shows Cancel/Confirm and makes selected option visually obvious.
5. Use only enabled fonts; default is `lv_font_montserrat_14`.
6. Use LVGL primitives and simple labels, no bitmap assets.

## Tests and Verification

Host tests must cover:

- Home rotation passthrough.
- Long press enters Settings.
- Active UI consumes rotation/press.
- Settings focus movement.
- RGB brightness apply produces and persists the action through the runtime boundary.
- Factory reset only fires after moving from Cancel to Confirm.

Final verification:

- Host model/runtime tests using MinGW GCC.
- `git diff --check`.
- Pristine build:

```powershell
.\scripts\build.ps1 -Pristine -BuildDir "D:\b_lvgl_st7789_rgb" -Board "mini-keyboard/nrf52840" -Snippet "rtt-console"
```

## Multiagent Worktree Plan

- Agent A owns runtime/model/input integration in its own worktree branch.
- Agent B owns LVGL dynamic view updates in its own worktree branch.
- Main thread merges both branches, resolves conflicts, runs verification, and commits final integration/spec updates.

