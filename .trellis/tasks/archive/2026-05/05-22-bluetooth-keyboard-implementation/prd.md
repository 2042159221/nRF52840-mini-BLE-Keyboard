# 实现蓝牙键盘

## 配置表

| 配置项 | 当前选择 | 说明 |
| --- | --- | --- |
| 使用子任务 | 禁用 | 先确认单一 MVP 范围，避免把全章功能一次性铺开。 |
| 编程范式 | 混合 | 以 Zephyr/NCS 的 C 模块化接口为主，事件/回调驱动输入和链路状态。 |
| 语言 | C | 当前固件代码为 Zephyr C 工程。 |
| 项目类型 | 嵌入式固件 | nRF52840 / Zephyr / NCS 蓝牙小键盘。 |
| 注释风格 | 极简 | 只在硬件约束、协议边界或非显然逻辑处补充说明。 |
| 代码结构 | 模块化 | 沿用 `input`、`keymap`、`hid`、`mode`、`transport` 边界。 |
| 错误处理策略 | 稳健+上下文 | 返回 Zephyr 风格错误码，并记录关键失败点日志。 |
| 性能优化级别 | 中+可扩展性焦点 | 优先保证 HID 链路可靠，保留 2.4G 和后续低功耗扩展边界。 |

## Goal

在现有 Zephyr/NCS 固件骨架上，实现并收敛第一阶段蓝牙键盘能力：6x4 矩阵的 18 个有效按键进入统一 HID 报告层，经当前模式发送到 USB 或 BLE HID；BLE 侧支持可发现、连接、HOGP 键盘报告发送；2.4G 保持明确占位，不做伪实现。

本任务先确认范围，不直接修改业务代码。确认后再进入 Trellis Phase 2 实现。

## What I Already Know

* 用户要求基于 Trellis workflow，参考 `docs/第05章项目实现.md` 实现蓝牙键盘。
* 当前 Trellis 状态为新建规划任务：`.trellis/tasks/05-22-bluetooth-keyboard-implementation`。
* `docs/第05章项目实现.md` 覆盖完整键盘产品路线：矩阵按键、按键唤醒、编码器、电源管理、模式切换、USB HID、BLE HID、HID 流控、LED、UI。
* `docs/mini_keyboard_solution.md` 明确第一阶段应聚焦 USB HID + BLE HID，2.4G 只预留，不实现真实协议。
* `docs/按键矩阵实现方案.md` 明确矩阵为 6 行 x 4 列，18 个有效键，推荐 Zephyr `gpio-kbd-matrix` + `input-keymap`。
* 当前仓库已经不是空工程，已有 `input`、`keymap`、`hid`、`mode`、`transport` 模块骨架。

## Current Code State

* `prj.conf`
  * 已启用 GPIO、Zephyr input、`gpio-kbd-matrix`、`input-keymap`、日志。
  * 已启用 Zephyr next USB device stack、USBD HID。
  * 已启用 BLE peripheral、HIDS、BAS、Settings、NVS。
* `boards/mingDev/mini-keyboard/mini-keyboard.dts`
  * 已定义 `kbd_matrix`，包含 6x4 行列 GPIO、`actual-key-mask = <0x3e 0x3e 0x1e 0x2b>` 和 18 个 `MATRIX_KEY` 映射。
  * 已定义 `zephyr,hid-device` 节点。
  * 已启用 `gpio0`、`gpio1`、`gpiote`、`usbd` 和 flash partitions。
* `src/input/input_manager.c`
  * 已注册 Zephyr input callback。
  * 已把 Zephyr input key code 直接映射到 HID usage。
  * 已维护 6KRO keyboard report，并按当前模式发给 transport。
  * 已把 NumLock 作为模式键：短按发送 Num Lock，长按 2 秒切换 USB/BLE。
* `src/transport/transport_usb.c`
  * 已基于 Zephyr next USB device stack 注册键盘 HID。
  * 已提供 8 字节 keyboard report 发送路径。
  * 当前 VID/PID 为 `0x1915:0x5201`，与文档里 `0x1915:0x52F0` 不一致，需要确认或收敛。
* `src/transport/transport_ble.c`
  * 已初始化 Bluetooth、Nordic `bt_hids`、BAS、Settings。
  * 已广播 HIDS/BAS UUID，设备名来自 `CONFIG_BT_DEVICE_NAME`。
  * 已支持连接/断开回调、boot/report mode 和 keyboard input report 发送。
  * 当前 `transport_ble_ready()` 只检查 enabled + connected，尚未严格检查加密状态和通知订阅状态。
* `src/mode/mode_manager.c`
  * 默认 USB 模式。
  * 支持 USB/BLE 切换。
  * `KB_MODE_24G_RESERVED` 返回 `-ENOTSUP`，符合占位要求。
* `src/keymap/keymap.c`
  * 存在独立 row/column 到 HID usage 的静态映射。
  * 当前没有被 `input_manager.c` 使用，且其矩阵语义与 DTS `input-keymap` 映射不一致。这个重复边界需要收敛，否则后续调键位会出现双源真相。

## Recommended MVP Scope

本任务建议采用“第一阶段闭环”范围，而不是一次实现第 5 章全部模块。

### In Scope

* 收敛 18 键矩阵输入到 HID 的单一映射来源。
  * 推荐保留 Zephyr `input-keymap` 作为坐标到 input key code 的硬件/设备树映射。
  * 应删除、改造或重新定位 `keymap.c` 的重复静态矩阵语义，避免与 DTS 分叉。
* 完成 USB/BLE 共用的 keyboard HID report 发送闭环。
  * 普通按键按下、松开都必须发送 report。
  * transport 未 ready 时不能阻塞输入主流程。
  * 模式切换时应避免旧通道卡键。
* 补强 BLE HID 发送门槛。
  * BLE 必须连接后才能发送。
  * 推荐进一步要求加密完成，并尽量确认 HID input report CCC 通知已打开后再发送。
* 保持 2.4G 为清晰占位。
  * 允许保留 enum、transport stub、状态机入口。
  * 不加入任何虚假的 2.4G 发送逻辑。
* 保留当前长按 NumLock 切换 USB/BLE 的第一阶段交互，除非用户确认改为其他模式键。
* 补必要日志，便于上板验证 USB 枚举、BLE 连接和按键输入路径。

### Out of Scope

* IP5306 PMIC 驱动、电池 SOC、电源管理完整闭环。
* CAF Power Manager 低功耗唤醒。
* EC11 编码器和 consumer control 音量报告。
* LED 灯效、LVGL UI 显示、上位机协议。
* 多主机 BLE 切换、Swift Pair、2.4G dongle 协议。
* 大规模目录重构或切换到完整 CAF 事件架构。

## Acceptance Criteria

* [ ] 工程能完成一次目标 board 构建。
* [ ] 设备树中 6x4 矩阵 GPIO、18 键 `actual-key-mask` 和 `input-keymap` 与文档一致，或差异被明确记录。
* [ ] 18 个有效矩阵键都有明确 HID usage；无效矩阵点不生成业务按键。
* [ ] 按键按下发送非空 keyboard report，松开后发送释放 report，不出现卡键。
* [ ] USB 模式下，USB HID ready 后矩阵按键可通过 USB keyboard report 输出。
* [ ] BLE 模式下，设备可广播为 BLE 键盘，连接后可发送 keyboard input report。
* [ ] BLE 未连接、未 ready 或不满足安全条件时，输入路径不崩溃、不阻塞。
* [ ] 长按模式键可在 USB/BLE 之间切换；2.4G 不进入假可用状态。
* [ ] 模式切换不会继续向旧 transport 发送新的按键状态。

## Technical Approach

推荐方向：以现有模块为主线做收敛，而不是推倒重写。

* 输入层继续使用 Zephyr input callback。
* 矩阵坐标到按键码继续由 devicetree `input-keymap` 负责。
* `input_manager.c` 负责把 input key code 转换到 HID usage、处理长按模式键、维护 keyboard report。
* `hid_report.c` 保持纯 6KRO report 操作，不感知 mode 或 transport。
* `transport.c` 保持统一分发入口。
* `transport_usb.c` 和 `transport_ble.c` 只处理各自链路 ready、协议注册和 report 发送。
* `mode_manager.c` 继续只管理 USB/BLE/2.4G_RESERVED 模式状态，不生成 HID report。

## Decision (ADR-lite)

**Context**: 文档描述的是完整产品路线，但当前仓库已有第一阶段骨架。继续按“全章功能”推进会把电源、UI、LED、上位机协议、BLE HID、USB HID 混在一个任务里，问题定位会变差。

**Decision**: 本任务只做第一阶段键盘闭环：矩阵输入 + HID report + USB/BLE transport + BLE HID 可用性收敛；2.4G 只保留占位。

**Consequences**: 该范围更容易构建和上板验证，但不会在本任务内交付低功耗、电池、灯效、显示、多主机和 2.4G。

## Research References

* [`research/codebase-state.md`](research/codebase-state.md) - 记录当前工程骨架、文档要求、矩阵映射风险、USB/BLE HID 状态和推荐 MVP 边界。

## Feasible Approaches

### Approach A: 第一阶段闭环（推荐）

* 范围：矩阵输入收敛、USB HID、BLE HID、USB/BLE 模式切换、2.4G 占位。
* 优点：最符合当前代码结构和文档路线，能同时验证有线和蓝牙两条核心链路。
* 代价：范围比 BLE-only 稍大，需要处理 USB/BLE 两边 ready 和切换边界。

### Approach B: BLE-only 闭环

* 范围：只强化 BLE HOGP 键盘，暂不触碰 USB 发送路径。
* 优点：更贴近“蓝牙键盘”字面目标，改动面更窄。
* 代价：会放弃文档中“USB 先跑通、便于调试”的路线，后续仍要回头处理 USB 与共享 HID 边界。

### Approach C: 全章功能推进

* 范围：同时推进矩阵、USB、BLE、低功耗、电池、编码器、灯效、UI、上位机协议。
* 优点：看起来覆盖完整产品路线。
* 代价：风险很高，任务过大，调试变量过多；不建议当前阶段采用。

## Risks

* `keymap.c` 与 DTS `input-keymap` 的重复映射如果不收敛，后续实测调键位会出现“改了一处，另一处还错”的隐性维护风险。
* USB VID/PID 与文档不一致，可能影响用户期望或主机识别记录。
* BLE 当前 ready 判断可能过宽，未加密或未订阅通知时发送 HID report 可能失败或被主机忽略。
* 如果 USB D+/D- 硬件未实际连接，USB HID 软件闭环无法通过，需要回退为 BLE-only 第一阶段。
* 当前 Trellis spec 多为模板状态，对本嵌入式工程的约束较弱，实现时需要以现有代码和文档为准。

## Open Questions

* 已确认：采用 Approach A，按“矩阵输入 + USB/BLE HID 闭环 + 2.4G 占位”执行。

## Confirmed Scope

用户已确认选择方案 1：

* 矩阵输入收敛。
* USB HID 路径纳入本次闭环。
* BLE HID 路径纳入本次闭环。
* USB/BLE 模式切换纳入本次闭环。
* 2.4G 只保留明确占位，不实现真实协议。

## Definition of Done

* PRD 经用户确认。
* Trellis context 按实现范围补齐后进入 `in_progress`。
* 代码实现后通过目标构建或记录构建环境阻塞原因。
* 关键链路日志足够支撑上板验证。
* 不把本任务范围外的电源、UI、LED、2.4G 协议混入实现。

## Technical Notes

* 参考文档：
  * `docs/第05章项目实现.md`
  * `docs/mini_keyboard_solution.md`
  * `docs/按键矩阵实现方案.md`
* 已检查的关键文件：
  * `prj.conf`
  * `CMakeLists.txt`
  * `boards/mingDev/mini-keyboard/mini-keyboard.dts`
  * `src/main.c`
  * `src/input/input_manager.c`
  * `src/keymap/keymap.c`
  * `src/hid/hid_report.c`
  * `src/mode/mode_manager.c`
  * `src/transport/transport.c`
  * `src/transport/transport_usb.c`
  * `src/transport/transport_ble.c`
  * `src/transport/transport_24g_stub.c`
* Trellis workflow 当前仍处于 Phase 1 planning；尚未运行 `task.py start`，尚未进入代码实现。
