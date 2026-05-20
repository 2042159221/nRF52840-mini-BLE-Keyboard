# AGENTS.md

## 项目概览
- 项目类型：基于 Nordic nRF Connect SDK / Zephyr 的蓝牙迷你数字键盘固件。
- 当前阶段：Phase 1 可构建闭环阶段，`input`、`keymap`、`hid`、`mode`、`transport_usb`、`transport_ble` 已初步接通。
- 当前目标：上板实测 6x4 矩阵输入到 `USB HID` / `BLE HID` 的完整路径；`2.4G` 模式仅做架构预留，暂不进入第一阶段开发。

## 代码与架构约束
- 优先复用 Zephyr/NCS 官方能力：USB HID、Bluetooth HID、settings、battery、gpio、workqueue、message queue。
- 不在第一阶段引入自定义复杂框架，先建立稳定的输入事件层、模式管理层、传输抽象层。
- 模式切换、按键扫描、HID 报告构造三者解耦，避免后续增加 2.4G 路径时重写业务逻辑。
- 文档统一沉淀到 `docs/` 目录。

## 建议的软件分层
- `app`：启动、系统初始化、主循环。
- `input`：矩阵扫描、消抖、功能键识别。
- `keymap`：物理键到逻辑键/组合键映射。
- `hid`：统一 HID 报告生成。
- `transport_usb`：USB HID 发送。
- `transport_ble`：BLE HID over GATT 发送。
- `mode_mgr`：USB / BLE / 预留 2.4G 模式状态机。
- `storage`：配对信息、用户配置、最后模式持久化。

## 实施原则
- 新增功能前先补文档和配置说明。
- 保持改动最小、边界清晰，避免在空工程阶段一次性堆入所有代码。
- 后续编码优先围绕上板验证修正矩阵方向、USB 枚举、BLE 配对与模式切换；不要在验证前扩展复杂功能。
- `transport_24g_stub` 必须保持明确不可用状态，不做假实现。

## 构建说明
- 当前仓库已提供 VS Code 任务：
  - `nRF: Configure environment`
  - `nRF: Build CAF Demo`
  - `nRF: Pristine Build CAF Demo`
- 若需要验证配置，优先用现有任务而不是自写终端命令。
