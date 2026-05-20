# 蓝牙迷你数字键盘最终方案

## 配置表

| 配置项 | 当前选择 | 说明 |
| --- | --- | --- |
| 项目阶段 | Phase 1 骨架搭建 | 先让输入、HID、模式、传输接口边界稳定。 |
| 目标模式 | USB HID + BLE HID | 第一阶段正式支持两条链路。 |
| 预留模式 | 2.4G reserved | 只保留枚举、接口桩和状态机入口，不做伪功能。 |
| 主控平台 | nRF52840 / Zephyr / NCS | 以当前自定义 board 和 NCS 构建链为准。 |
| 按键输入 | 6x4 矩阵，18 个有效键 | 使用 Zephyr `gpio-kbd-matrix` 与 `input-keymap`。 |
| HID 报告 | 标准键盘 6KRO | 第一阶段优先完成普通键盘 report，媒体键后置。 |
| 传输抽象 | `transport_*` 统一接口 | 上层只调用当前模式的发送接口。 |
| 模式管理 | `mode_manager` 状态切换 | 当前跳过 2.4G，只在 USB/BLE 间切换。 |
| 配置持久化 | 后续接 `settings` | 当前先固定 USB 默认模式，后续保存上次模式。 |
| 构建验证 | VS Code 任务 | 优先使用 `nRF: Build CAF Demo` / `Pristine Build`。 |


## 1. 目标定义

本项目要开发一个迷你数字键盘固件，依据原理图，最终支持三种工作模式：

- USB HID 模式
- BLE HID 模式
- 2.4G 模式

当前阶段要求如下：

- `USB HID` 必须纳入第一阶段正式实现。
- `BLE HID` 必须纳入第一阶段正式实现。
- `2.4G` 暂不实现，但架构上必须预留扩展位，避免后续重构主干逻辑。
- 输出结果以本地 Markdown 文档沉淀到 `docs/` 文件夹。

## 2. 最终结论

最终建议采用 **统一输入层 + 统一 HID 报告层 + 可插拔传输层 + 模式管理状态机** 的架构。

这不是“为了好看”的分层，而是为了规避后续最典型的返工风险：

- 如果先按 USB 逻辑直接写死，后面加 BLE 时会复制一套键值处理流程。
- 如果 BLE 和 USB 各自生成报告，后面改键位、组合键、媒体键时会出现双份维护。
- 如果现在不预留 2.4G 的枚举、接口和状态机入口，将来接私有射频栈时会侵入主流程。

因此，第一阶段的正确方向不是做“三套键盘”，而是做“一套键盘逻辑，两条传输链路，一条预留链路”。

## 3. 推荐架构设计

- 应用层 `app`
  - 负责系统启动、模块初始化、主状态协调。
  - 负责开机读取上次工作模式与用户配置。

- 输入层 `input`
  - 负责矩阵扫描或独立按键采样。
  - 负责按键消抖、长按/短按识别、Fn/模式键识别。
  - 输出统一的按键事件，不直接关心 USB 或 BLE。

- 键盘语义层 `keymap`
  - 将物理键位映射为逻辑键值。
  - 处理 Num 键、功能组合键、媒体键、自定义宏预留。

- HID 报告层 `hid`
  - 负责把逻辑键事件转成标准键盘 HID report。
  - 后续如需消费者控制键（音量、静音等），可扩展第二类 report。

- 传输抽象层 `transport`
  - 定义统一接口，例如：
    - `transport_init()`
    - `transport_enable(mode)`
    - `transport_disable(mode)`
    - `transport_send_keyboard_report()`
    - `transport_ready()`
  - 上层只看“链路是否可发”，不关心底下是 USB、BLE 还是未来 2.4G。

- USB 传输层 `transport_usb`
  - 负责 USB 枚举、HID 描述符、IN report 发送。
  - 处理 USB 挂起、恢复、配置完成等状态。

- BLE 传输层 `transport_ble`
  - 负责 HID over GATT 服务。
  - 负责广播、配对、绑定、加密、连接状态维护。
  - 负责 BLE report map、input report、battery service 可选支持。

- 模式管理层 `mode_mgr`
  - 管理 `USB`、`BLE`、`2.4G_RESERVED` 三个模式。
  - 负责切换策略、优先级策略、异常回退。

- 存储层 `storage`
  - 保存上次模式。
  - 保存 BLE bond 信息。
  - 保存用户配置，如自动回连策略、灯效预留配置。

## 4. 模式定义与策略

建议定义如下模式枚举：

```c
typedef enum {
    KB_MODE_USB = 0,
    KB_MODE_BLE,
    KB_MODE_24G_RESERVED,
} kb_mode_t;
```

### 4.1 第一阶段实际支持模式

- `KB_MODE_USB`
- `KB_MODE_BLE`

### 4.2 预留模式

- `KB_MODE_24G_RESERVED`
  - 当前不编译射频业务逻辑。
  - 仅保留状态机入口、UI 提示入口、配置项和接口桩。

### 4.3 建议切换规则

建议通过一个专用模式键或组合键切换：

- 短按：无动作或执行当前模式下辅助功能。
- 长按 2 秒：进入模式切换。
- 循环切换顺序：`USB -> BLE -> 2.4G(预留占位) -> USB`

但考虑用户体验，当前阶段建议实际行为做成：

- 切到 `USB`：立即启用 USB 链路。
- 切到 `BLE`：立即进入广播/回连。
- 切到 `2.4G`：当前提示为“未实现”，自动回到上一个有效模式，或者直接跳过该模式。

更务实的方案是：

- **UI 层保留 2.4G 提示定义**。
- **第一阶段用户可见行为先跳过 2.4G**。

这样既保留架构完整性，又不会给实际使用造成假功能。

## 5. USB HID 方案

### 5.1 推荐实现路径

基于 Zephyr USB device + HID class 实现标准键盘设备。

### 5.2 建议能力范围

第一阶段 USB HID 至少包含：

- 标准 6-Key Rollover 键盘报告
- 修饰键（Ctrl/Shift/Alt/GUI）
- NumPad 键值
- 必要时增加 LED output report（Num Lock）接收能力

### 5.3 优点

- 枚举快，调试成本低。
- 对 PC、平板、工控设备兼容性更高。
- 适合作为首个稳定 bring-up 通道。

### 5.4 风险点

- 原理图若 USB 只接供电未接 D+/D-，那 USB HID 就根本不成立。
- 必须先由原理图确认：
  - MCU 具备 USB device 控制器
  - USB 数据线已正确连接
  - 必要上拉/ESD/连接器定义齐全

如果原理图不满足以上条件，USB HID 需求要立即回退，不要继续在软件层硬扛。

## 6. BLE HID 方案

### 6.1 推荐实现路径

基于 Zephyr Bluetooth + HID over GATT Profile（HOGP）实现 BLE 键盘。

### 6.2 建议能力范围

第一阶段 BLE HID 包含：

- 可配对、可绑定
- 开机自动回连最近主机
- 连接失败后自动广播
- 标准键盘 input report
- 可选 battery service
- 可选 device information service

### 6.3 关键策略

- 默认保存最近一台主机 bond。
- 后续如果有多设备切换需求，再扩成 3 槽位 host profile。
- 第一期不要急着做多主机切换，否则状态机会显著变脏。

### 6.4 BLE 用户体验建议

- 开机若上次模式为 BLE：
  - 优先尝试回连
  - 回连失败则进入广播
- 长按指定组合键 5 秒：
  - 清除 bond
  - 重新进入可配对广播

## 7. 2.4G 模式的预留方案

虽然当前不实现，但现在必须把接口边界定清楚。

### 7.1 预留对象

- 模式枚举
- 传输层接口
- 状态机入口
- 用户提示定义
- 配置项开关

### 7.2 暂不实现的内容

- 私有 2.4G 协议
- Dongle 枚举
- 配对逻辑
- 射频链路管理
- 加密/跳频/重传细节

### 7.3 为什么现在不能“顺手先做一点”

因为 2.4G 真正的难点不是发包，而是：

- 键盘端和接收器端协议协同
- 射频参数调优
- 唤醒与低功耗策略
- 抗干扰和丢包恢复
- 与 BLE 共存的时序资源分配

这些都不是填几个空函数能解决的。当前阶段正确做法就是 **留接口，不假实现**。

## 8. 软件目录建议

建议后续将源码重组为如下结构：

```text
src/
  main.c
  app/
    app_init.c
    app_event.c
  input/
    key_scan.c
    debounce.c
    mode_key.c
  keymap/
    keymap.c
  hid/
    hid_report.c
    hid_usage.h
  mode/
    mode_manager.c
  transport/
    transport.c
    transport_usb.c
    transport_ble.c
    transport_24g_stub.c
  storage/
    settings_store.c
include/
  app/
  input/
  keymap/
  hid/
  mode/
  transport/
  storage/
boards/
  <board_name>.overlay
Kconfig
Kconfig.transport
prj.conf
```

## 9. 关键状态机设计

建议存在一个全局工作状态机：

```text
BOOT
  -> LOAD_SETTINGS
  -> MODE_SELECT
      -> USB_INIT
      -> BLE_INIT
      -> 24G_RESERVED
  -> IDLE
  -> ACTIVE
  -> SUSPEND
  -> MODE_SWITCH
  -> FACTORY_RESET
```

### 9.1 状态解释

- `BOOT`
  - 上电启动。

- `LOAD_SETTINGS`
  - 读取上次模式、BLE bond、用户配置。

- `MODE_SELECT`
  - 决定本次进入 USB 还是 BLE。

- `IDLE`
  - 链路已就绪，等待按键事件。

- `ACTIVE`
  - 有按键上报活动。

- `SUSPEND`
  - 低功耗待机或链路挂起。

- `MODE_SWITCH`
  - 用户主动切换模式。

- `FACTORY_RESET`
  - 清除配置和 bond。

## 10. 输入事件模型

建议统一事件结构：

```c
typedef struct {
    uint16_t key_id;
    bool pressed;
    uint32_t timestamp_ms;
} key_event_t;
```

处理流程建议固定为：

`按键扫描 -> 消抖 -> 特殊键识别 -> keymap 映射 -> HID 报告生成 -> 当前 transport 发送`

这样后面加 2.4G 时，只替换发送路径，不重写上游逻辑。

## 11. 推荐的 Zephyr / NCS 能力选型

### 11.1 USB 相关

建议使用：

- Zephyr USB device stack
- HID device class

### 11.2 BLE 相关

建议使用：

- `CONFIG_BT`
- `CONFIG_BT_PERIPHERAL`
- `CONFIG_BT_HIDS`
- `CONFIG_BT_SETTINGS`
- `CONFIG_SETTINGS`
- `CONFIG_BT_BAS`（可选）
- `CONFIG_BT_DIS`（可选）

### 11.3 输入与系统能力

建议使用：

- GPIO
- k_work / delayed work
- message queue 或 event manager
- settings/NVS

## 12. prj.conf 建议分阶段配置

当前 `prj.conf` 已包含输入矩阵、keymap、日志和基础栈大小配置，建议继续分阶段推进，而不是一次性堆满。

### 第一阶段最小集合

- 日志：`CONFIG_LOG=y`
- GPIO：`CONFIG_GPIO=y`
- 输入子系统：`CONFIG_INPUT=y`
- 矩阵键盘：`CONFIG_INPUT_GPIO_KBD_MATRIX=y`
- 坐标映射：`CONFIG_INPUT_KEYMAP=y`
- Settings：后续接 BLE bond 和上次模式持久化时开启
- USB HID：USB bring-up 阶段开启
- BLE HID：BLE bring-up 阶段开启

### 建议思路

- 先让 `USB HID` 独立可用。
- 再打开 `BLE HID`。
- 最后接 `mode_mgr`，把二者统一接入。

这样调试路径最短，问题定位也最直接。

## 13. Devicetree / 原理图核对清单

在正式编码前，必须先对照原理图确认以下项目：

- MCU 具体型号
- 键盘矩阵行列数量
- 每个按键接法
- 模式切换键是否独立存在
- USB D+ / D- 是否接入 MCU
- 电池检测脚是否引出
- 充电状态脚是否引出
- LED 指示灯数量与连接脚
- 唤醒脚/中断脚定义
- 是否有外部 2.4G 芯片，还是 MCU 未来复用同一 2.4G 射频资源

如果这些基础硬件信息不清，软件方案再漂亮也会偏航。

## 14. 用户交互建议

建议至少定义以下用户行为：

- 单击按键：正常输入
- 长按模式键 2 秒：切换工作模式
- 长按组合键 5 秒：BLE 清除绑定
- 上电时按住特定组合键：恢复默认配置

建议 LED 或其他提示至少表达：

- USB 已连接
- BLE 广播中
- BLE 已连接
- 模式切换中
- 低电量
- 2.4G 预留/未启用

## 15. 实施路线图

### Phase 1：硬件核对

- 根据原理图确认 USB、按键矩阵、LED、模式键资源。
- 确认芯片型号及可用外设。

### Phase 2：USB HID bring-up

- 跑通 USB 枚举。
- 固定发送测试键值。
- 接入按键扫描。

### Phase 3：BLE HID bring-up

- 跑通广播、配对、绑定、回连。
- 固定发送测试键值。
- 接入按键扫描。

### Phase 4：统一模式管理

- 抽象 transport 接口。
- 模式切换接入状态机。
- 保存上次模式。

### Phase 5：体验完善

- Bond 清除
- LED 提示
- 低功耗
- 电量服务

### Phase 6：2.4G 扩展准备

- 确认 dongle 方案
- 确认链路协议
- 评估 BLE/2.4G 共存策略

## 16. 风险与判断

### 高风险项

- 原理图不支持 USB data，导致 USB HID 从需求层面就不成立。
- 键盘矩阵与 GPIO 资源冲突。
- BLE 与低功耗/唤醒策略处理不好，导致体验很差。

### 中风险项

- 一开始就做多主机 BLE 配对槽位，复杂度膨胀。
- 在没有统一 HID 报告层前同时推进 USB 和 BLE，导致维护重复。

### 低风险项

- 2.4G 先做接口桩、枚举和状态占位，这一步风险低且收益高。

## 17. 最终建议

最终建议非常明确：

- **第一阶段只正式实现 USB HID + BLE HID。**
- **2.4G 只做架构预留，不做伪实现。**
- **所有按键逻辑、键值映射、HID 报告生成都必须统一。**
- **通过传输抽象层隔离 USB 与 BLE。**
- **通过模式管理状态机管理模式切换与配置持久化。**

这是当前最稳、后续返工最少、也最符合嵌入式工程节奏的方案。

## 18. 下一步建议

最值得马上开展的工作顺序如下：

1. 先对照原理图补齐硬件资源表。
2. 在本工程中补 `prj.conf` 的 USB/BLE 基础配置。
3. 建立 `input`、`hid`、`transport`、`mode` 四个核心模块骨架。

## 19. 当前工程落地状态

当前仓库已经不再是纯空模板，核心骨架已经初步存在：

- `include/mode/mode_manager.h`
  - 已定义 `KB_MODE_USB`、`KB_MODE_BLE`、`KB_MODE_24G_RESERVED`。
  - 已提供初始化、读取当前模式、设置模式、获取下一个有效模式的接口。

- `include/transport/transport.h`
  - 已提供统一初始化、启停、ready 判断和键盘 report 发送接口。
  - 上层无需直接感知 USB、BLE 或 2.4G 的具体实现。

- `include/hid/hid_report.h`
  - 已定义标准键盘 6KRO report 结构。
  - 已提供清空、添加按键、移除按键接口。

- `src/transport/transport_24g_stub.c`
  - 应保持为明确的 `-ENOTSUP` 风格占位。
  - 不应在第一阶段加入任何真实 2.4G 协议假实现。

- `prj.conf`
  - 已打开 GPIO、Zephyr input、矩阵键盘、keymap 和日志。
  - 后续应按 bring-up 顺序逐步加入 USB/BLE 配置。

当前代码已经把骨架从“接口存在”推进到“USB/BLE 基础链路可构建”：

- `transport_usb.c` 已基于 Zephyr 新版 USB device stack 注册键盘 HID 设备，并发送标准 8 字节键盘 report。
- `transport_ble.c` 已基于 Nordic `bt_hids` 初始化 HOGP、广播、连接回调、Battery Service 和键盘 input report。
- `input_manager.c` 已把 Zephyr input key code 转换为 HID usage，并按当前模式发送到 transport。
- Num Lock 作为复用模式键：短按发送 Num Lock，长按 2 秒在 USB/BLE 间切换。

因此后续重点不再是“搭骨架”，而是上板实测 USB 枚举、BLE 配对和 18 键矩阵方向。

## 20. 下一阶段开发切入点

### 20.1 USB HID 优先闭环

USB 是最适合作为第一条闭环链路的路径。当前代码已完成基础实现，实测时按下面顺序验证：

1. 插入 USB 后确认主机枚举出 `PRO BLE Mini Keyboard` 键盘设备。
2. 按矩阵数字键，确认主机能收到 NumPad 键值。
3. 松键后确认没有卡键。
4. 断开 USB 后确认固件不崩溃，输入层继续运行。

### 20.2 BLE HID 第二闭环

BLE 当前已完成基础 HOGP 链路，实测时按下面顺序验证：

1. 长按 Num Lock 2 秒切到 BLE 模式。
2. 在主机蓝牙列表中查找 `PRO_Ble_mini_keyboard`。
3. 完成配对后按矩阵数字键。
4. 断开主机后确认设备重新广播。
5. 重新连接后确认仍可发送键值。

### 20.3 模式切换闭环

当前模式切换已经接入输入层：

1. 短按 Num Lock：发送普通 Num Lock。
2. 长按 Num Lock 2 秒：在 USB/BLE 之间切换。
3. `mode_manager_next_supported_mode()` 只返回第一阶段支持的模式。
4. `KB_MODE_24G_RESERVED` 保持不可用，不进入假可用状态。

## 21. 模块职责边界

后续编码时必须维持以下边界，否则很容易重新变成耦合工程：

| 模块 | 可以做 | 不应该做 |
| --- | --- | --- |
| `input` | 扫描、消抖、产生按键事件 | 直接发送 USB/BLE report |
| `keymap` | 物理坐标到逻辑键值映射 | 管理连接状态 |
| `hid` | 维护 HID report 内容 | 判断当前工作模式 |
| `transport` | 根据模式分发到具体链路 | 解释物理按键含义 |
| `transport_usb` | USB 枚举与 report 发送 | 保存 BLE bond |
| `transport_ble` | BLE 广播、连接与 HOGP report | 处理矩阵扫描 |
| `mode_manager` | 管理当前模式和切换策略 | 构造 HID report |
| `storage` | 保存设置和 bond 相关配置 | 决定按键语义 |

## 22. 第一阶段验收清单

### 22.1 USB HID 验收

- 设备能被主机枚举为键盘类 HID。
- 固定测试键值能在主机上输出。
- 矩阵按键能通过同一 HID report 输出。
- 松键后能发送空 report，避免按键卡死。
- USB 未 ready 时不会阻塞输入主流程。

### 22.2 BLE HID 验收

- 设备能广播为 BLE 键盘。
- 主机可完成配对和绑定。
- 断开后可重新广播或回连。
- 固定测试键值能通过 BLE 输出。
- 矩阵按键能复用同一 HID report 输出。

### 22.3 模式管理验收

- 默认模式可启动。
- USB/BLE 之间可切换。
- 切换失败时能回退到原有效模式。
- `KB_MODE_24G_RESERVED` 不会进入假可用状态。
- 当前模式可被后续 storage 模块持久化。

### 22.4 输入矩阵验收

- 18 个有效键全部能产生事件。
- 无效矩阵点被 `actual-key-mask` 屏蔽。
- 单键、多键、快速连击不会产生明显抖动。
- 二极管方向与扫描方向一致，防鬼键有效。

## 23. 立即建议的代码推进顺序

最稳妥的下一轮代码工作如下：

1. 先完善 board DTS 的 `kbd_matrix` 与 `input-keymap` 节点，确认 6x4 矩阵事件能进入 Zephyr input 子系统。
2. 在 `input_manager.c` 中订阅或接收 input 事件，转换为工程内部按键事件。
3. 在 `keymap.c` 中把 18 个物理键映射到 NumPad HID usage。
4. 在 `hid_report.c` 现有 6KRO 基础上确认修饰键处理策略。
5. 先让 `transport_usb.c` 完成最小 HID 发送闭环。
6. 再实现 `transport_ble.c` 的 HOGP 闭环。
7. 最后把长按模式键接入 `mode_manager`。

不要在这个阶段同时加入灯效、多主机 BLE、宏录制、2.4G 协议。那些功能会把第一阶段问题定位变得不必要地困难。

