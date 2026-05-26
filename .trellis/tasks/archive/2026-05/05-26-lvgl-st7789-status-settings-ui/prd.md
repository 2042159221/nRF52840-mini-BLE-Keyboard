# LVGL ST7789 状态屏设置系统

## Goal

为 nRF52840 小键盘增加一套炫酷但可落地的本机状态屏与设置系统：基于 LVGL + ST7789V，在 320 x 172 有效显示区域内提供可扫视的设备状态、可见 Exit 的本机设置菜单、EC11 旋钮驱动的事件式交互，并且必须按 `docs/hardware_resources_agent_netlist.md` 的网表结论实现 RGB 与屏幕共存。

## Hard Requirements

- 必须使用网表中的真实硬件资源，不允许为了屏幕 bring-up 牺牲 RGB。
- RGB 必须继续使用 `RGB_DATA` / P0.20 和 `RGB_PWR_EN` / P0.13，17 颗 WS2812 数量不能改成 16。
- 屏幕必须使用 `SCREEN_SCL` / P1.13、`SCREEN_SDA` / P0.28、`SCREEN_CS` / P0.02、`SCREEN_DC` / P0.03、`SCREEN_RES` / P1.10、`SCREEN_BLK` / P1.11。
- 屏幕有效区域按 320 x 172，`x-offset = 0`，`y-offset = 34`，`ready-time-ms = 120`。
- LVGL 像素格式为 RGB565，并启用 `CONFIG_LV_COLOR_16_SWAP=y`。
- 设置系统无 Esc 依赖，所有设置层级必须提供可见 `Exit`。
- UI 激活时 EC11 进入本机 UI 导航；退出 UI 后恢复现有 HID consumer 旋钮行为。
- 构建和验证必须通过项目脚本或显式 NCS 环境，不调用裸 `west`、裸 `python` 或 Anaconda Python。
- 代码修改要形成清晰 git 提交，方便导师验收失败后回滚。

## Hardware Coexistence Decision

### Netlist Evidence

- `RGB_DATA ; U35.4 U36.32`，对应 P0.20。
- `RGB_PWR_EN ; Q4.1 R15.2 U36.33`，对应 P0.13。
- `SCREEN_SDA ; P3.4 U36.4`，对应 P0.28。
- `SCREEN_SCL ; P3.5 U36.6`，对应 P1.13。
- `SCREEN_CS ; P3.7 U36.7`，对应 P0.02。
- `SCREEN_DC ; P3.3 U36.3`，对应 P0.03。
- `SCREEN_RES ; P3.2 U36.2`，对应 P1.10。
- `SCREEN_BLK ; P3.1 U36.1`，对应 P1.11。
- 网表明确写到 U36.40 / P1.04 未连接，可作为 WS2812 SPI 波形生成所需的 SCK 占位引脚。

### Decision

采用双 SPIM 共存：

- ST7789V 使用 `spi3`，SCK=P1.13，MOSI=P0.28，CS=P0.02，经 `zephyr,mipi-dbi-spi` + `sitronix,st7789v` 接入。
- WS2812 RGB 从当前 `spi3` 迁移到另一个空闲 SPIM 控制器，MOSI=P0.20，SCK=P1.04。P1.04 在网表中未连接，只作为 `ws2812-spi` 驱动需要的时钟输出，不连接外设也不影响 RGB_DATA。
- RGB 电源仍由 P0.13 控制；`chain-length = <17>` 保持不变。
- 如果构建发现该 SoC/Zephyr 版本没有可用空闲 SPIM 实例，优先选择 `spi2`；若 `spi2` 不可用，再评估 `spi1`。不得回退到禁用 RGB。

## Product Strategy

状态屏不是装饰屏，而是小键盘的本机信任层。用户需要的是：

- 不接 PC GUI 也能知道设备当前模式、连接状态、电量、充电状态、NumLock 状态和 RGB 状态。
- 常用设置可以直接在设备上完成，不依赖 Esc，不记隐藏路径。
- 旋钮交互像实体仪表：转动移动/调值，短按确认，长按进入/退出设置，屏幕立即反馈。
- 失败状态必须明确：保存失败、存储错误、BLE 未连接、USB 未就绪不能静默。

## Display Content Plan

### Home Status Page

- 顶部状态条：`USB` / `BLE` / `2.4G Reserved`，连接/广播状态，电量百分比，充电/满电标识。
- 主信息区：大号状态，例如 `USB Ready`、`BLE Pairing`、`BLE Connected`、`2.4G Reserved`。
- 视觉氛围：用小型霓虹扫描线、细分隔线、RGB 色条和状态光点做炫酷效果，但不使用大图资产和重动画。
- 次信息区：NumLock、RGB 状态、旋钮映射摘要，例如 `Knob: Vol +/- / Mute`。
- 底部提示：Home 显示 `Hold: Settings`；设置页内始终显示可见 `Exit`。

### Settings Menu

每页最多 4 行，避免 172 px 高度拥挤：

- `Mode`：Follow Hardware、USB Preferred、BLE Preferred。
- `RGB`：Enable、Mode、Brightness、Theme Color。
- `Knob`：CW Action、CCW Action、Press Action。
- `NumLock`：Follow Host、Always White、Off When Low Power。
- `System`：Save、Factory Reset、About。
- `Exit`：固定可见，短按返回 Home。

### Detail/Edit Pages

- 单选项使用分段列表：当前值左侧高亮，短按确认。
- 数值项使用条形/弧形控件：旋转调节，短按应用，长按取消。
- 高风险动作使用确认页：`Factory Reset` 默认聚焦 `Cancel`。
- 保存结果使用短暂 result 行：`Saved`、`Save failed: storage`。

## Interaction Design

- Home：
  - 旋转：保留现有 HID consumer 行为。
  - 长按：进入 Settings。
- Settings list：
  - 旋转：移动焦点。
  - 短按：进入页面或执行当前项。
  - 长按：返回 Home。
  - `Exit`：可见列表项，短按返回 Home。
- Edit page：
  - 旋转：调整候选值。
  - 短按：应用当前值。
  - 长按：取消并返回上一级。
- Confirm page：
  - 旋转：在 `Cancel` / `Confirm` 间移动。
  - 短按：执行。

## UI Visual Direction

### Theme Name

`Signal Neon`

### Color Palette

- Background: `#0B0F14`
- Surface: `#151B22`
- Surface elevated: `#202833`
- Primary text: `#F4F8FB`
- Secondary text: `#93A1AF`
- Divider: `#2B3642`
- Accent cyan: `#23E6D2`
- Accent magenta: `#FF4FD8`
- Accent green: `#7CFF6B`
- Warning amber: `#F8BA4C`
- Danger red: `#FF5A68`
- BLE blue: `#4DA3FF`
- USB violet: `#A78BFA`

### Style Rules

- 小屏工具面板优先：高密度、高对比、强焦点，不做营销式大卡片。
- 焦点使用左侧 neon bar + 背景亮度变化，不只靠颜色区分。
- Home 可使用轻量扫描线/状态光点/条形能量槽，LVGL 对象实现，不引入位图资产。
- 动效只做短时局部反馈，避免全屏频繁重绘拖垮 HID/BLE/USB。
- 字号和行高固定，所有文本必须在 320 x 172 内可读且不重叠。

## Technical Approach

### Proposed Modules

- `include/display/status_screen.h` / `src/display/status_screen.c`
  - 初始化显示系统。
  - 管理 Home/Settings/Edit/Confirm 页面状态。
  - 订阅配置、模式、电源、HID LED 状态变化。
- `include/display/status_screen_model.h` / `src/display/status_screen_model.c`
  - 纯 C 状态机和显示文本生成，便于单元测试。
- `src/display/status_screen_lvgl.c`
  - LVGL 对象、样式、Home/List/Edit/Confirm 模板和炫酷主题。
- `src/display/status_screen_input.c`
  - EC11 旋转、短按、长按与 UI/HID 事件路由。
- `include/display/backlight.h` / `src/display/backlight.c`
  - 包装 Zephyr LED API，控制 PWM 背光。

### Board/Kconfig Work

- 更新 `mini-keyboard-pinctrl.dtsi`：
  - `spi3_default` 改为 ST7789V：SCK P1.13，MOSI P0.28。
  - 新增 RGB 专用 SPI pinctrl：MOSI P0.20，SCK P1.04。
- 更新 `mini-keyboard.dts`：
  - ST7789V 走 `zephyr,mipi-dbi-spi` + `sitronix,st7789v`。
  - WS2812 保留并迁移到 RGB 专用 SPI。
  - 增加 `pwm-leds` 背光节点，PWM0 CH0 P1.11，100 Hz，反转极性。
- 更新 `prj.conf`：
  - `CONFIG_DISPLAY=y`
  - `CONFIG_LVGL=y`
  - `CONFIG_LV_COLOR_16_SWAP=y`
  - 必要 LVGL widget/font
  - `CONFIG_PWM=y`
  - LED/PWM 背光相关配置

## Acceptance Criteria

- [ ] 构建中同时存在可用 ST7789V display 和 WS2812 RGB，RGB 未被禁用，`chain-length = <17>`。
- [ ] 最终 DTS 中 ST7789V 使用 P1.13/P0.28/P0.02/P0.03/P1.10，背光使用 P1.11。
- [ ] 最终 DTS 中 RGB 使用 P0.20 数据线和 P0.13 电源使能，且不再占用屏幕的 SPI3 引脚。
- [ ] 开机后初始化状态屏并显示 Home 页面。
- [ ] Home 至少显示当前模式、电量、充电/USB、NumLock、RGB 状态和旋钮映射摘要。
- [ ] 长按旋钮进入 Settings；旋转移动焦点，短按进入/确认，长按返回；所有设置层级都能看到 `Exit`。
- [ ] UI 激活时旋钮事件不再发送 HID consumer 操作；退出 UI 后恢复原有行为。
- [ ] 修改配置项时调用现有 `app_config_set()`；保存动作调用现有持久化路径并显示成功/失败反馈。
- [ ] 至少一组 model/input 单元测试覆盖导航、Exit、编辑确认、取消和 UI/HID 路由。
- [ ] 使用项目脚本完成目标板构建。
- [ ] 形成可回滚 git 提交，不包含无关 dirty 文件。

## Definition of Done

- 代码遵守模块边界：display/UI 不直接构造 HID report，不直接管理 BLE/USB transport。
- RGB 与屏幕共存方案来自网表证据，并在 DTS/pinctrl 中落地。
- 新增纯 C 逻辑具备单测。
- 构建通过或明确记录未通过的真实阻塞原因和当前证据。
- 修改按逻辑拆分提交，便于导师回滚。

## Out of Scope

- 不实现 2.4G 真协议，只显示 reserved 状态。
- 不实现触摸、Esc 键依赖或复杂键盘快捷键。
- 不实现完整 PC GUI 替代品。
- 不加入大图资产、主题商店或高成本动画。

## Implementation Decomposition

### Agent A: Hardware/Board Coexistence Worktree

- Owner files: `boards/mingDev/mini-keyboard/*`, `prj.conf`, low-level backlight files if needed.
- Responsibilities: ST7789V/MIPI DBI DTS, RGB SPI migration, PWM backlight, LVGL/display Kconfig.

### Agent B: UI Model/Input Worktree

- Owner files: `include/display/status_screen*.h`, `src/display/status_screen_model.c`, `src/display/status_screen_input.c`, `tests/status_screen_*`.
- Responsibilities: state model, navigation state machine, EC11 UI/HID routing contract, unit tests.

### Agent C: LVGL View Worktree

- Owner files: `src/display/status_screen.c`, `src/display/status_screen_lvgl.c`, display CMake integration.
- Responsibilities: LVGL styles, Home/List/Edit/Confirm templates, Signal Neon theme and app init integration.

### Agent D: Integration/Check Worktree

- Owner responsibilities: merge branches, resolve conflicts, run tests/build, fix spec drift.
- Must not include `skills-lock.json` unless explicitly requested.

## Research References

- `research/lvgl-st7789-zephyr-notes.md`

