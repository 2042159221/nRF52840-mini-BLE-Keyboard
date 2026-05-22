# 蓝牙小键盘硬件资源说明（基于 TEL Netlist 整理）

> 本文档把 TEL Netlist 中可见的硬件连接关系整理为可直接用于 Zephyr / NCS BSP、Devicetree、驱动开发和硬件排障的资源说明。
>
> 重要原则：**以网表连接为准**。若旧文档、旧 DTS 或示例代码与本文档冲突，优先按本文档核对。

---

## 1. 硬件总体结构

本开发板是一块基于 `E73-2G4M08S1C` 模组的蓝牙小键盘控制板。核心资源如下：

| 模块 | 网表器件/网络 | 说明 |
|---|---|---|
| 主控 | `U36` | `E73-2G4M08S1C`，底层为 nRF52840 模组 |
| 矩阵键盘 | `ROW0~ROW5`, `COL0~COL3`, `U4~U34/U38`, `D1~D20` | 6 行 4 列矩阵，实际 18 个有效按键位 |
| 旋钮 | `SW3`, `EC11A`, `EC11B`, `ROW0`, `COL3` | EC11 编码器，A/B 两相独立 GPIO，按压接入矩阵 |
| RGB 灯链 | `U3~U35`, `RGB_DATA`, `RGB_VCC_OUT` | 17 颗 WS2812B-MINI-V3J 串联 |
| RGB 电源开关 | `Q2`, `Q4`, `RGB_PWR_EN`, `RGB_VCC_OUT` | MCU 控制 RGB 供电开关 |
| 电源管理 | `U2`, `U37`, `U41`, `SW4`, `CN1` | IP5306-I2C + 3.3V LDO + 硬件唤醒脉冲 |
| 电池检测 | `BAT_ADC`, `BAT_ADC_EN`, `R6`, `R8`, `Q1` | 电池电压 ADC 采样，带使能开关 |
| 三模切换 | `U1`, `MODE` | 三档拨动开关，通过 ADC 电压识别模式 |
| USB | `USB1`, `U39`, `DP`, `DN`, `VBUS` | Type-C Device，带 ESD 与 CC 下拉 |
| 屏幕接口 | `P3`, `SCREEN_*` | 8Pin 屏幕接口，SPI-like 信号 |
| 调试下载 | `H1`, `SWD`, `SCK` | 3Pin SWD 下载口 |
| 状态 LED | `LED1`, `LED2` | 两颗 MCU 控制 LED，均为低电平点亮 |

---

## 2. 主控 U36：E73-2G4M08S1C 引脚资源

> 表中 `U36 Pin` 来自 TEL 网表；`nRF GPIO/功能` 根据 E73/nRF52840 常见符号命名整理，固件实现时以该映射建立 DTS。

| U36 Pin | Netlist 网络名 | nRF GPIO / 复用功能 | 固件用途 | 备注 |
|---:|---|---|---|---|
| 1 | `SCREEN_BLK` | P1.11 | 屏幕背光 | 输出 GPIO/PWM 均可 |
| 2 | `SCREEN_RES` | P1.10 | 屏幕复位 | 输出 GPIO |
| 3 | `SCREEN_DC` | P0.03 | 屏幕 DC | 输出 GPIO |
| 4 | `SCREEN_SDA` | P0.28 / AIN4 | 屏幕数据 | SPI MOSI 或 GPIO bit-bang |
| 5 | `GND` | GND | 地 |  |
| 6 | `SCREEN_SCL` | P1.13 | 屏幕时钟 | SPI SCK 或 GPIO bit-bang |
| 7 | `SCREEN_CS` | P0.02 / AIN0 | 屏幕片选 | 输出 GPIO |
| 8 | `MODE` | P0.29 / AIN5 | 三模 ADC 检测 | ADC 输入 |
| 9 | `BAT_ADC` | P0.31 / AIN7 | 电池电压采样 | ADC 输入 |
| 10 | `COL3` | P0.30 / AIN6 | 键盘矩阵列 3 | GPIO 输出/扫描列 |
| 11 | `$1N8329` | XL1 | 32.768kHz 晶振 | 接 X1.1、C13 |
| 12 | `COL2` | P0.26 | 键盘矩阵列 2 | GPIO 输出/扫描列 |
| 13 | `$1N8328` | XL2 | 32.768kHz 晶振 | 接 X1.2、C12 |
| 14 | `COL1` | P0.06 | 键盘矩阵列 1 | GPIO 输出/扫描列 |
| 15 | `COL0` | P0.05 / AIN3 | 键盘矩阵列 0 | GPIO 输出/扫描列 |
| 16 | `ROW5` | P0.08 | 键盘矩阵行 5 | GPIO 输入/扫描行 |
| 17 | `ROW4` | P1.09 | 键盘矩阵行 4 | GPIO 输入/扫描行 |
| 18 | `ROW3` | P0.04 / AIN2 | 键盘矩阵行 3 | GPIO 输入/扫描行 |
| 19 | `3V3` | VCC | 主控供电 | 3.3V |
| 20 | `ROW2` | P0.12 | 键盘矩阵行 2 | GPIO 输入/扫描行 |
| 21 | `GND` | GND | 地 |  |
| 22 | `ROW1` | P0.07 | 键盘矩阵行 1 | GPIO 输入/扫描行 |
| 23 | `3V3` | VDH/VDDH | 主控供电 | 3.3V |
| 24 | `GND` | GND | 地 |  |
| 25 | 未出现在 `$NETS` | DCH | 未连接 | TEL 中无网络 |
| 26 | `$1N8557` | RESET | 复位 | R1 上拉，SW2 下拉复位 |
| 27 | `VBUS` | USB VBUS sense | USB 供电检测 | 同接 USB VBUS / IP5306 VIN |
| 28 | `ROW0` | P0.15 | 键盘矩阵行 0 | 仅旋钮按压占用 |
| 29 | `DN` | USB D- | USB FS D- | 经 R23 0Ω 与 ESD |
| 30 | `$1N8402` | P0.17 | LED2 | 低电平点亮 |
| 31 | `DP` | USB D+ | USB FS D+ | 经 R24 0Ω 与 ESD |
| 32 | `RGB_DATA` | P0.20 | WS2812 数据输出 | 接第一颗 RGB DIN |
| 33 | `RGB_PWR_EN` | P0.13 | RGB 电源使能 | 控制 Q4/Q2 |
| 34 | `IP5305T_WAKEUP` | P0.22 | IP5306 keepalive/wakeup | legacy 命名，实际用于 IP5306 |
| 35 | `IP5305T_I2C_SCK` | P0.24 | IP5306 I2C SCL | R31 10k 上拉到 3V3 |
| 36 | `IP5305T_I2C_SDA` | P1.00 | IP5306 I2C SDA | R28 10k 上拉到 3V3 |
| 37 | `SWD` | SWDIO | 调试下载 | 接 H1.2 |
| 38 | `$1N8549` | P1.02 | LED1 | 低电平点亮 |
| 39 | `SCK` | SWDCLK/SWC | 调试下载 | 接 H1.3 |
| 40 | 未出现在 `$NETS` | P1.04 | 未连接 | TEL 中无网络 |
| 41 | `BAT_ADC_EN` | NFC1 / P0.09 | 电池采样使能 | NFC 复用脚，固件需配置为 GPIO |
| 42 | `EC11B` | P1.06 | 编码器 B 相 | GPIO/QDEC 输入 |
| 43 | `EC11A` | NFC2 / P0.10 | 编码器 A 相 | NFC 复用脚，固件需配置为 GPIO |

### 2.1 需要特别注意的复用脚

- `BAT_ADC_EN` 使用 U36.41，对应 NFC1/P0.09。
- `EC11A` 使用 U36.43，对应 NFC2/P0.10。
- 在 Zephyr/NCS 中，这两个脚默认可能被 NFCT 占用。固件应配置为普通 GPIO，例如：

```dts
&uicr {
    nfct-pins-as-gpios;
};
```

或在 Kconfig / board 配置中启用等效选项，避免 NFC 外设占用 P0.09/P0.10。

---

## 3. 键盘矩阵资源

### 3.1 行列 GPIO

| 矩阵线 | U36 Pin | nRF GPIO | Netlist 网络 |
|---|---:|---|---|
| ROW0 | 28 | P0.15 | `ROW0` |
| ROW1 | 22 | P0.07 | `ROW1` |
| ROW2 | 20 | P0.12 | `ROW2` |
| ROW3 | 18 | P0.04 | `ROW3` |
| ROW4 | 17 | P1.09 | `ROW4` |
| ROW5 | 16 | P0.08 | `ROW5` |
| COL0 | 15 | P0.05 | `COL0` |
| COL1 | 14 | P0.06 | `COL1` |
| COL2 | 12 | P0.26 | `COL2` |
| COL3 | 10 | P0.30 | `COL3` |

推荐扫描模型：

- 6 行 4 列。
- 行作为输入，建议使用 pull-down。
- 列作为输出扫描，active-high。
- 每个普通按键都串联一个 1N4148W 二极管。
- 旋钮按压也被放入矩阵，不是独立 GPIO key。

### 3.2 有效矩阵坐标

TEL 网表中实际存在 18 个按键位：17 个机械轴 + 1 个旋钮按压。

| Row | Col | 按键器件 | RGB 器件 | 二极管 | TEL 连接摘要 |
|---:|---:|---|---|---|---|
| 0 | 3 | `SW3` 按压 | 无 | `D20` | `ROW0 -> D20 -> SW3.E`, `COL3 -> SW3.D` |
| 1 | 0 | `U38` | `U35` | `D1` | `ROW1 -> D1 -> U38.2`, `COL0 -> U38.1` |
| 1 | 1 | `U4` | `U3` | `D2` | `ROW1 -> D2 -> U4.2`, `COL1 -> U4.1` |
| 1 | 2 | `U6` | `U5` | `D3` | `ROW1 -> D3 -> U6.2`, `COL2 -> U6.1` |
| 1 | 3 | `U8` | `U7` | `D4` | `ROW1 -> D4 -> U8.2`, `COL3 -> U8.1` |
| 2 | 0 | `U10` | `U9` | `D5` | `ROW2 -> D5 -> U10.2`, `COL0 -> U10.1` |
| 2 | 1 | `U12` | `U11` | `D6` | `ROW2 -> D6 -> U12.2`, `COL1 -> U12.1` |
| 2 | 2 | `U14` | `U13` | `D7` | `ROW2 -> D7 -> U14.2`, `COL2 -> U14.1` |
| 3 | 0 | `U16` | `U15` | `D8` | `ROW3 -> D8 -> U16.2`, `COL0 -> U16.1` |
| 3 | 1 | `U18` | `U17` | `D9` | `ROW3 -> D9 -> U18.2`, `COL1 -> U18.1` |
| 3 | 2 | `U20` | `U19` | `D10` | `ROW3 -> D10 -> U20.2`, `COL2 -> U20.1` |
| 3 | 3 | `U22` | `U21` | `D11` | `ROW3 -> D11 -> U22.2`, `COL3 -> U22.1` |
| 4 | 0 | `U24` | `U23` | `D12` | `ROW4 -> D12 -> U24.2`, `COL0 -> U24.1` |
| 4 | 1 | `U26` | `U25` | `D13` | `ROW4 -> D13 -> U26.2`, `COL1 -> U26.1` |
| 4 | 2 | `U28` | `U27` | `D14` | `ROW4 -> D14 -> U28.2`, `COL2 -> U28.1` |
| 5 | 0 | `U30` | `U29` | `D17` | `ROW5 -> D17 -> U30.2`, `COL0 -> U30.1` |
| 5 | 1 | `U32` | `U31` | `D18` | `ROW5 -> D18 -> U32.2`, `COL1 -> U32.1` |
| 5 | 3 | `U34` | `U33` | `D19` | `ROW5 -> D19 -> U34.2`, `COL3 -> U34.1` |

### 3.3 空矩阵位

以下位置在 TEL 中没有按键连接，不应映射为有效按键：

| Row | 空缺 Col |
|---:|---|
| 0 | COL0, COL1, COL2 |
| 2 | COL3 |
| 4 | COL3 |
| 5 | COL2 |

### 3.4 矩阵掩码建议

为避免旧资料误导，这里直接按 TEL 网表重新计算占位掩码。

按“每行一个 bitmask，bit0~bit3 对应 COL0~COL3”：

```text
ROW0 = 0x08   # C3
ROW1 = 0x0F   # C0 C1 C2 C3
ROW2 = 0x07   # C0 C1 C2
ROW3 = 0x0F   # C0 C1 C2 C3
ROW4 = 0x07   # C0 C1 C2
ROW5 = 0x0B   # C0 C1 C3
```

按“每列一个 bitmask，bit0~bit5 对应 ROW0~ROW5”：

```text
COL0 = 0x3E   # R1 R2 R3 R4 R5
COL1 = 0x3E   # R1 R2 R3 R4 R5
COL2 = 0x1E   # R1 R2 R3 R4
COL3 = 0x2B   # R0 R1 R3 R5
```

如果使用 Zephyr `gpio-kbd-matrix` 的 `actual-key-mask`，务必先确认该属性是按“行”还是按“列”编码，然后选择对应掩码。不要直接沿用旧的 `<0x3e 0x3e 0x3e 0x0b>`，它与 TEL 中 ROW5/COL3、ROW5/COL2、ROW4/COL3 等实际连接不一致。

### 3.5 推荐 DTS 片段

```dts
/* GPIO 方向和 pull 配置需结合所用 matrix 驱动确认。 */
row-gpios = <&gpio0 15 (GPIO_PULL_DOWN | GPIO_ACTIVE_HIGH)>,
            <&gpio0 7  (GPIO_PULL_DOWN | GPIO_ACTIVE_HIGH)>,
            <&gpio0 12 (GPIO_PULL_DOWN | GPIO_ACTIVE_HIGH)>,
            <&gpio0 4  (GPIO_PULL_DOWN | GPIO_ACTIVE_HIGH)>,
            <&gpio1 9  (GPIO_PULL_DOWN | GPIO_ACTIVE_HIGH)>,
            <&gpio0 8  (GPIO_PULL_DOWN | GPIO_ACTIVE_HIGH)>;

col-gpios = <&gpio0 5  GPIO_ACTIVE_HIGH>,
            <&gpio0 6  GPIO_ACTIVE_HIGH>,
            <&gpio0 26 GPIO_ACTIVE_HIGH>,
            <&gpio0 30 GPIO_ACTIVE_HIGH>;
```

---

## 4. EC11 旋钮资源

### 4.1 旋转 A/B 相

| 旋钮信号 | SW3 引脚 | Netlist 网络 | U36 Pin | nRF GPIO | 说明 |
|---|---|---|---:|---|---|
| A 相 | `SW3.A` | `EC11A` | 43 | NFC2 / P0.10 | 可用 GPIO interrupt 或 QDEC A |
| B 相 | `SW3.B` | `EC11B` | 42 | P1.06 | 可用 GPIO interrupt 或 QDEC B |
| 公共端 | `SW3.C` | `GND` | - | GND | 公共地 |

附属滤波：

- `EC11A` 接 `C40` 到 GND。
- `EC11B` 接 `C31` 到 GND。

注意：`EC11A` 走 NFC2/P0.10，固件必须释放 NFC 引脚。

### 4.2 旋钮按压

旋钮按压接入键盘矩阵：

| 功能 | Netlist 连接 |
|---|---|
| 按压一端 | `SW3.D -> COL3` |
| 按压另一端 | `SW3.E -> D20.2 -> D20.1 -> ROW0` |
| 矩阵坐标 | ROW0 / COL3 |

固件实现原则：

- 旋钮旋转：独立 GPIO/QDEC 处理。
- 旋钮按压：作为矩阵键盘的 ROW0/COL3 处理。

---

## 5. RGB 灯链资源

### 5.1 RGB 数据线

| 信号 | U36 Pin | nRF GPIO | Netlist 网络 | 说明 |
|---|---:|---|---|---|
| RGB Data | 32 | P0.20 | `RGB_DATA` | MCU 输出 WS2812 单线数据 |

### 5.2 RGB 串联顺序

TEL 网表显示 17 颗 WS2812 串联，MCU 数据从 `RGB_DATA` 进入 `U35`，最后 `U33.DOUT` 悬空。

| RGB 顺序 | LED 器件 | DIN 来源 | DOUT 去向 |
|---:|---|---|---|
| 1 | `U35` | `RGB_DATA` / U36.32 | `U3.DIN` |
| 2 | `U3` | `U35.DOUT` | `U5.DIN` |
| 3 | `U5` | `U3.DOUT` | `U7.DIN` |
| 4 | `U7` | `U5.DOUT` | `U9.DIN` |
| 5 | `U9` | `U7.DOUT` | `U11.DIN` |
| 6 | `U11` | `U9.DOUT` | `U13.DIN` |
| 7 | `U13` | `U11.DOUT` | `U15.DIN` |
| 8 | `U15` | `U13.DOUT` | `U17.DIN` |
| 9 | `U17` | `U15.DOUT` | `U19.DIN` |
| 10 | `U19` | `U17.DOUT` | `U21.DIN` |
| 11 | `U21` | `U19.DOUT` | `U23.DIN` |
| 12 | `U23` | `U21.DOUT` | `U25.DIN` |
| 13 | `U25` | `U23.DOUT` | `U27.DIN` |
| 14 | `U27` | `U25.DOUT` | `U29.DIN` |
| 15 | `U29` | `U27.DOUT` | `U31.DIN` |
| 16 | `U31` | `U29.DOUT` | `U33.DIN` |
| 17 | `U33` | `U31.DOUT` | 悬空，正常 |

### 5.3 RGB 供电

| 网络 | 连接 |
|---|---|
| `RGB_VCC_OUT` | 所有 WS2812 VDD、17 个 100nF 去耦电容、Q2 输出 |
| `GND` | 所有 WS2812 GND、去耦电容另一端 |
| `RGB_PWR_EN` | U36.33 / P0.13，控制 Q4，再控制 Q2 给 RGB 上电 |
| `SYS_POWER` | Q2 输入侧，来自系统电源 |

固件实现建议：

- 在发送 WS2812 数据前先使能 `RGB_PWR_EN`，等待 RGB 供电稳定后再输出数据。
- 关闭灯效时可关闭 `RGB_PWR_EN` 以降低待机电流。
- 从网表拓扑推断 `RGB_PWR_EN` 很可能是 active-high 使能，但量产前应实测确认。

### 5.4 RGB 与按键的一一对应关系

| 按键器件 | RGB 器件 | 矩阵坐标 |
|---|---|---|
| `U38` | `U35` | R1/C0 |
| `U4` | `U3` | R1/C1 |
| `U6` | `U5` | R1/C2 |
| `U8` | `U7` | R1/C3 |
| `U10` | `U9` | R2/C0 |
| `U12` | `U11` | R2/C1 |
| `U14` | `U13` | R2/C2 |
| `U16` | `U15` | R3/C0 |
| `U18` | `U17` | R3/C1 |
| `U20` | `U19` | R3/C2 |
| `U22` | `U21` | R3/C3 |
| `U24` | `U23` | R4/C0 |
| `U26` | `U25` | R4/C1 |
| `U28` | `U27` | R4/C2 |
| `U30` | `U29` | R5/C0 |
| `U32` | `U31` | R5/C1 |
| `U34` | `U33` | R5/C3 |

旋钮按压 ROW0/COL3 没有对应 RGB。

---

## 6. 屏幕接口 P3

P3 是 8Pin 屏幕接口。TEL 中连接如下：

| P3 Pin | Netlist 网络 | U36 Pin | nRF GPIO | 建议用途 |
|---:|---|---:|---|---|
| 1 | `SCREEN_BLK` | 1 | P1.11 | 背光控制 |
| 2 | `SCREEN_RES` | 2 | P1.10 | 屏幕复位 |
| 3 | `SCREEN_DC` | 3 | P0.03 | 数据/命令选择 |
| 4 | `SCREEN_SDA` | 4 | P0.28 / AIN4 | SPI MOSI / 数据 |
| 5 | `SCREEN_SCL` | 6 | P1.13 | SPI SCK / 时钟 |
| 6 | `3V3` | - | 3.3V | 屏幕供电 |
| 7 | `SCREEN_CS` | 7 | P0.02 / AIN0 | 片选 |
| 8 | `GND` | - | GND | 地 |

该接口是典型 SPI-like 屏幕接口：`SCL + SDA + DC + CS + RES + BLK`。

---

## 7. USB Type-C 与 USB Device 资源

### 7.1 USB 数据线

| USB 侧 | 中间器件 | MCU 侧 | 说明 |
|---|---|---|---|
| `USB1.A6/B6` | `U39`, `R24 0Ω` | `DP -> U36.31` | USB D+ |
| `USB1.A7/B7` | `U39`, `R23 0Ω` | `DN -> U36.29` | USB D- |
| `USB1.A4B9/B4A9` | `U39.5` | `VBUS -> U36.27 / U2.1` | USB VBUS |
| `USB1.A1B12/B1A12` | - | `GND` | USB GND |

### 7.2 CC 下拉

| 信号 | 连接 | 说明 |
|---|---|---|
| CC1 | `USB1.A5 -> R11 5.1k -> GND` | Type-C 设备端 Rd |
| CC2 | `USB1.B5 -> R12 5.1k -> GND` | Type-C 设备端 Rd |

结论：该板在 Type-C 上表现为 USB Device/UFP。

### 7.3 ESD 器件

`U39` 为 USB ESD 保护器件，连接到 `DP`、`DN`、`VBUS`、`GND` 和 USB 侧数据线。

---

## 8. SWD 下载与复位

### 8.1 SWD Header：H1

| H1 Pin | Netlist 网络 | MCU 连接 | 说明 |
|---:|---|---|---|
| 1 | `GND` | GND | 调试器地 |
| 2 | `SWD` | U36.37 | SWDIO |
| 3 | `SCK` | U36.39 | SWDCLK / SWC |

### 8.2 复位按键：SW2

| 网络 | 连接 |
|---|---|
| `$1N8557` | `U36.26 RESET`, `R1.1`, `C11.2`, `SW2.1` |
| `R1.2` | `3V3`，复位上拉 |
| `SW2.2/SW2.3/SW2.4` | `GND`，按下拉低复位 |
| `C11.1` | `GND`，复位滤波/延时 |

复位为 active-low。

---

## 9. 电源系统

### 9.1 电源路径概览

```text
USB VBUS / Type-C
    -> VBUS
    -> U2 IP5306-I2C VIN
    -> U36 VBUS sense

Battery CN1
    -> VBAT
    -> U2 IP5306 BAT/SW 外围

U2 IP5306 VOUT
    -> SW4 电源开关
    -> SYS_POWER
    -> U37 LDO
    -> 3V3
    -> U36 / 屏幕 / I2C pull-up / LED pull-up

SYS_POWER
    -> Q2/Q4 RGB 电源开关
    -> RGB_VCC_OUT
    -> 17 颗 WS2812
```

### 9.2 电池接口 CN1

| CN1 Pin | Netlist 网络 | 说明 |
|---:|---|---|
| 1 | `VBAT` | 锂电池正极 |
| 2 | `GND` | 锂电池负极 |

### 9.3 IP5306-I2C：U2

| U2 Pin | Netlist 网络 | 作用 |
|---:|---|---|
| 1 | `VBUS` | USB 5V 输入 / 充电输入 |
| 2 | `IP5305T_I2C_SCK` | I2C SCL，legacy 网络名 |
| 3 | `IP5305T_I2C_SDA` | I2C SDA，legacy 网络名 |
| 4 | 未出现在 `$NETS` | 未连接 |
| 5 | `$1N8491` | KEY / wakeup 控制节点 |
| 6 | `$1N8461` | BAT 侧，经过 R7 接 VBAT |
| 7 | `$1N8521` | SW，接电感 U40 |
| 8 | `$1N8445` | VOUT，接 SW4 和输出电容 |
| 9 | `GND` | EP/GND |

说明：网络名中出现 `IP5305T_*`，但器件封装对应 `U2 = IP5306-I2C`。固件命名建议统一为 `ip5306`，不要继续扩散 `ip5305t` 命名。

### 9.4 IP5306 I2C

| 功能 | Netlist 网络 | U36 Pin | nRF GPIO | 外部上拉 |
|---|---|---:|---|---|
| SCL | `IP5305T_I2C_SCK` | 35 | P0.24 | `R31 10k -> 3V3` |
| SDA | `IP5305T_I2C_SDA` | 36 | P1.00 | `R28 10k -> 3V3` |

I2C 地址不在 TEL 网表中编码；若使用 IP5306-I2C 常见驱动，通常按器件资料配置地址，例如 `0x75`，但该值应由芯片手册或实测确认。

### 9.5 IP5306 keepalive / wakeup

| 功能 | Netlist 网络 | MCU | 说明 |
|---|---|---|---|
| 软件唤醒/保活 | `IP5305T_WAKEUP` | U36.34 / P0.22 | 经 R26 接到 U2 KEY 相关节点 |
| 硬件手动唤醒 | `MANUAL_WAKE` | SW4.2 / U41.1 | 触发 SN74LVC1G123 单稳态 |
| 单稳态输出 | `$1N8506` | U41.5 / Q3.1 | 通过 Q3 影响 IP5306 KEY 节点 |

实现建议：

- `IP5305T_WAKEUP` 实际应视为 `IP5306_WAKEUP`。
- P0.22 建议作为 active-low 脉冲输出使用。
- 周期性 keepalive 脉冲可防止 IP5306 在轻载时关闭输出。
- 脉冲周期、脉宽不是 TEL 信息，应由固件策略和实测决定。

### 9.6 LDO 3.3V：U37

| U37 Pin | Netlist 网络 | 说明 |
|---:|---|---|
| 1 | `SYS_POWER` | 输入 |
| 2 | `GND` | 地 |
| 3 | `SYS_POWER` | EN 或输入相关脚，接系统电源 |
| 4 | 未连接 | NC |
| 5 | `3V3` | 3.3V 输出 |

`3V3` 供电对象包括：U36、P3.6、I2C 上拉、LED 上拉、若干电源滤波电容等。

### 9.7 电池电压采样

| 功能 | Netlist 网络 | MCU | 外围 |
|---|---|---|---|
| ADC 输入 | `BAT_ADC` | U36.9 / P0.31 / AIN7 | `R6`, `R8`, `C29` |
| 采样使能 | `BAT_ADC_EN` | U36.41 / NFC1/P0.09 | `R9`, `R10`, `Q1` |
| 电池正极 | `VBAT` | - | `R6.2`, `R7.2`, `CN1.1` |

网表显示：

- `R6 = 100kΩ` 从 `VBAT` 到 `BAT_ADC`。
- `R8 = 100kΩ` 从 `BAT_ADC` 到 Q1 控制的低侧节点。
- `C29 = 100nF` 从 `BAT_ADC` 到 GND。
- `BAT_ADC_EN` 通过 `R9 = 1kΩ` 控制 Q1，`R10 = 100kΩ` 下拉。

因此启用采样时，ADC 看到的近似电压为 `VBAT / 2`。禁用时低侧被断开，用于降低电池分压静态耗电。实际换算应结合 Q1 导通压降和 ADC 参考电压校准。

---

## 10. 三模切换 ADC：MODE

### 10.1 MCU 连接

| 功能 | Netlist 网络 | U36 Pin | nRF GPIO |
|---|---|---:|---|
| 模式 ADC | `MODE` | 8 | P0.29 / AIN5 |

### 10.2 U1 三档拨动开关网络

| U1 Pin / 外围 | Netlist 网络 | 说明 |
|---|---|---|
| U1.2 | `$1N8108 -> R5 -> MODE` | 通过 R5 1k 接到 MCU ADC |
| U1.1 | `$1N8286 -> R18 -> GND` | 档位电阻网络 |
| U1.3 | `$1N8291 -> R16/R17` | R16 到 3V3，R17 到 GND，形成中间电压 |
| U1.4 | `$1N8311 -> R19 -> 3V3` | 高电平档位网络 |
| U1.5 | `$1N8424 -> R20 -> GND` | 低电平档位网络 |
| `MODE` | `C9 -> GND` | ADC 滤波 |

固件建议：

- 用 ADC 读取 `P0.29 / AIN5`。
- 三档阈值应基于实测电压确定。
- 必须做防抖、迟滞和稳定采样窗口，避免模式切换边界抖动。
- TEL 只能证明它是 ADC 档位识别电路，不能给出可靠阈值。

---

## 11. 状态 LED

| LED | MCU 网络 | U36 Pin | nRF GPIO | 上拉电阻 | 点亮逻辑 |
|---|---|---:|---|---|---|
| LED1 | `$1N8549` | 38 | P1.02 | `R130 4.7k -> 3V3` | GPIO 输出低点亮 |
| LED2 | `$1N8402` | 30 | P0.17 | `R13 4.7k -> 3V3` | GPIO 输出低点亮 |

DTS 示例：

```dts
leds {
    compatible = "gpio-leds";

    led1: led_1 {
        gpios = <&gpio1 2 GPIO_ACTIVE_LOW>;
        label = "LED1";
    };

    led2: led_2 {
        gpios = <&gpio0 17 GPIO_ACTIVE_LOW>;
        label = "LED2";
    };
};
```

---

## 12. 32.768kHz 低频晶振

| 器件 | Netlist 网络 | MCU Pin | 说明 |
|---|---|---:|---|
| X1.1 | `$1N8329` | U36.11 / XL1 | 32.768kHz 晶振一端 |
| X1.2 | `$1N8328` | U36.13 / XL2 | 32.768kHz 晶振另一端 |
| C13 | `$1N8329 -> GND` | - | 12pF |
| C12 | `$1N8328 -> GND` | - | 12pF |

固件建议：如果板级 DTS 使用外部 LFXO，应确保对应 clock 配置启用。若实测启动异常，再切回 LFRC 对比排查。

---

## 13. 未连接/容易误判的点

| 对象 | 状态 | 说明 |
|---|---|---|
| U36.25 `DCH` | TEL 中未连接 | 不要在 DTS 中占用 |
| U36.40 `P1.04` | TEL 中未连接 | 可视为未用 GPIO |
| U2.4 | TEL 中未连接 | IP5306 相关 LED3/NC 不使用 |
| U37.4 | TEL 中未连接 | LDO NC |
| U33.DOUT | 仅 `U33.2` 单点网络 | 最后一颗 WS2812 DOUT 悬空，正常 |
| `IP5305T_*` 网络名 | 命名不准确 | 实际器件是 `U2 IP5306-I2C`，建议固件统一命名为 IP5306 |
| `EC11A` / `BAT_ADC_EN` | NFC 复用脚 | 必须释放 NFC 引脚作为 GPIO |

---

## 14. 建议的固件资源命名

为降低 Agent 写代码时的歧义，建议在固件中使用以下统一命名：

| 固件命名 | 对应 Netlist 网络 | GPIO / 外设 |
|---|---|---|
| `kbd_row0` | `ROW0` | P0.15 |
| `kbd_row1` | `ROW1` | P0.07 |
| `kbd_row2` | `ROW2` | P0.12 |
| `kbd_row3` | `ROW3` | P0.04 |
| `kbd_row4` | `ROW4` | P1.09 |
| `kbd_row5` | `ROW5` | P0.08 |
| `kbd_col0` | `COL0` | P0.05 |
| `kbd_col1` | `COL1` | P0.06 |
| `kbd_col2` | `COL2` | P0.26 |
| `kbd_col3` | `COL3` | P0.30 |
| `encoder_a` | `EC11A` | P0.10 / NFC2 |
| `encoder_b` | `EC11B` | P1.06 |
| `rgb_data` | `RGB_DATA` | P0.20 |
| `rgb_power_enable` | `RGB_PWR_EN` | P0.13 |
| `ip5306_wakeup` | `IP5305T_WAKEUP` | P0.22 |
| `ip5306_scl` | `IP5305T_I2C_SCK` | P0.24 |
| `ip5306_sda` | `IP5305T_I2C_SDA` | P1.00 |
| `battery_adc` | `BAT_ADC` | P0.31 / AIN7 |
| `battery_adc_enable` | `BAT_ADC_EN` | P0.09 / NFC1 |
| `mode_adc` | `MODE` | P0.29 / AIN5 |
| `status_led_1` | `$1N8549` | P1.02 active-low |
| `status_led_2` | `$1N8402` | P0.17 active-low |
| `screen_blk` | `SCREEN_BLK` | P1.11 |
| `screen_res` | `SCREEN_RES` | P1.10 |
| `screen_dc` | `SCREEN_DC` | P0.03 |
| `screen_sda` | `SCREEN_SDA` | P0.28 |
| `screen_scl` | `SCREEN_SCL` | P1.13 |
| `screen_cs` | `SCREEN_CS` | P0.02 |

---

## 15. Agent 开发注意事项

1. **不要假设旋钮按压是独立 GPIO。** 旋钮按压在矩阵 ROW0/COL3。
2. **不要忽略 NFC 复用脚。** `BAT_ADC_EN` 和 `EC11A` 分别占用 NFC1/NFC2。
3. **不要沿用旧矩阵掩码。** 应按本文第 3.4 节重新计算。
4. **不要把 RGB 数量写成 16。** TEL 中是 17 颗 WS2812，顺序从 `U35` 开始，到 `U33` 结束。
5. **不要把 `IP5305T_*` 当作芯片型号。** 它只是网络名遗留；板上器件是 IP5306-I2C。
6. **LED1/LED2 是 active-low。** GPIO 输出低电平点亮。
7. **电池 ADC 需要先打开 `BAT_ADC_EN`。** 否则分压低侧可能断开，读数不可靠。
8. **三模 MODE 阈值需要实测。** TEL 仅描述电路连接，不包含实际档位电压。
9. **USB 是 Device 端。** CC1/CC2 都有 5.1k 下拉。
10. **SWD 只有 3Pin。** H1 为 GND/SWDIO/SWCLK，没有独立 RESET 引出。

---

## 16. 最小 DTS 资源草案

以下片段不是完整 board DTS，只是硬件资源草案。Agent 可据此生成正式 DTS。

```dts
/ {
    aliases {
        led0 = &led1;
        led1 = &led2;
    };

    leds {
        compatible = "gpio-leds";
        led1: led_1 {
            gpios = <&gpio1 2 GPIO_ACTIVE_LOW>;
            label = "LED1";
        };
        led2: led_2 {
            gpios = <&gpio0 17 GPIO_ACTIVE_LOW>;
            label = "LED2";
        };
    };

    rgb_power {
        compatible = "regulator-fixed";
        regulator-name = "rgb_vcc_out";
        enable-gpios = <&gpio0 13 GPIO_ACTIVE_HIGH>;
    };

    ip5306_control {
        compatible = "gpio-keys";
        ip5306_wakeup: ip5306_wakeup {
            gpios = <&gpio0 22 GPIO_ACTIVE_LOW>;
            label = "IP5306_WAKEUP";
        };
    };
};

&uicr {
    nfct-pins-as-gpios;
};

/* ADC channels to define in application or DTS:
 * - battery_adc: P0.31 / AIN7
 * - mode_adc:    P0.29 / AIN5
 */

/* Keyboard matrix GPIOs:
 * rows: P0.15, P0.07, P0.12, P0.04, P1.09, P0.08
 * cols: P0.05, P0.06, P0.26, P0.30
 */
```

---

## 17. 原始关键网络摘录

以下是从 TEL `$NETS` 中抽象出的关键连接，便于 Agent 反查：

```text
ROW0 ; D20.1 U36.28
ROW1 ; D1.1 D2.1 D3.1 D4.1 U36.22
ROW2 ; D5.1 D6.1 D7.1 U36.20
ROW3 ; D8.1 D9.1 D10.1 D11.1 U36.18
ROW4 ; D12.1 D13.1 D14.1 U36.17
ROW5 ; D17.1 D18.1 D19.1 U36.16

COL0 ; U10.1 U16.1 U24.1 U30.1 U36.15 U38.1
COL1 ; U4.1 U12.1 U18.1 U26.1 U32.1 U36.14
COL2 ; U6.1 U14.1 U20.1 U28.1 U36.12
COL3 ; SW3.D U8.1 U22.1 U34.1 U36.10

EC11A ; C40.1 SW3.A U36.43
EC11B ; C31.2 SW3.B U36.42

RGB_DATA ; U35.4 U36.32
RGB_PWR_EN ; Q4.1 R15.2 U36.33
RGB_VCC_OUT ; Q2.3 U3.1 U5.1 U7.1 U9.1 U11.1 U13.1 U15.1 U17.1 U19.1 U21.1 U23.1 U25.1 U27.1 U29.1 U31.1 U33.1 U35.1

BAT_ADC ; C29.1 R6.1 R8.2 U36.9
BAT_ADC_EN ; R9.2 U36.41
MODE ; C9.1 R5.2 U36.8

IP5305T_I2C_SCK ; R31.1 U2.2 U36.35
IP5305T_I2C_SDA ; R28.1 U2.3 U36.36
IP5305T_WAKEUP ; R26.1 U36.34

SCREEN_BLK ; P3.1 U36.1
SCREEN_RES ; P3.2 U36.2
SCREEN_DC ; P3.3 U36.3
SCREEN_SDA ; P3.4 U36.4
SCREEN_SCL ; P3.5 U36.6
SCREEN_CS ; P3.7 U36.7

DP ; R24.2 U36.31 U39.3
DN ; R23.2 U36.29 U39.1
VBUS ; C39.2 R29.2 U2.1 U36.27 U39.5 USB1.A4B9 USB1.B4A9

SWD ; H1.2 U36.37
SCK ; H1.3 U36.39
3V3 ; P3.6 U36.19 U36.23 U37.5 ...
GND ; H1.1 P3.8 U36.5 U36.21 U36.24 ...
```
