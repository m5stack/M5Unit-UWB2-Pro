# Unit UWB2-Pro Arduino 驱动库

[English](README.md) | [简体中文](README_zh.md)

## 概述

### SKU: U237

通过 ESP32 主机的 UART 或 I2C 接口配置 Unit UWB2-Pro，读取测距和 2D/3D 定位数据。
适用于出厂固件 v0.1，也支持使用相同协议的 Unit UWB Angle-Pro。

## 相关链接

- [Arduino 驱动库](https://github.com/m5stack/M5Unit-UWB2-Pro)
- [示例程序](https://github.com/m5stack/M5Unit-UWB2-Pro/tree/main/examples)

## 依赖库

UART 和 I2C 基础示例使用 ESP32 Arduino 开发框架，无需额外安装依赖库。
屏幕示例需要安装以下库：

| 库 | 用途 |
| --- | --- |
| [M5Unified](https://github.com/m5stack/M5Unified) | M5Stack Basic 主机初始化和按键输入 |
| [M5GFX](https://github.com/m5stack/M5GFX) | 屏幕与文字绘制 |

## 安装

### Arduino IDE

1. 安装主机对应的 ESP32 开发板支持包。
2. 下载 [**main 分支 ZIP 文件**](https://github.com/m5stack/M5Unit-UWB2-Pro/archive/refs/heads/main.zip)。
3. 选择 **项目 → 加载库 → 添加 .ZIP 库**，导入下载的文件。
4. 在 **文件 → 示例 → M5UnitUWB2Pro** 中打开示例。

屏幕示例 `Unit_UWB2Pro_Demo` 还需要 **M5Unified** 和 **M5GFX**，可通过库管理器安装。

### PlatformIO

在项目的 `platformio.ini` 中添加：

```ini
lib_deps =
    https://github.com/m5stack/M5Unit-UWB2-Pro.git#main
```

在 **M5Stack Basic 主机**上运行屏幕示例时，将 `examples/Unit_UWB2Pro_Demo` 中的文件
复制到项目的 `src/` 目录，并使用以下配置：

```ini
[env:m5stack_basic]
platform = espressif32
board = m5stack-core-esp32
framework = arduino
monitor_speed = 115200
lib_deps =
    https://github.com/m5stack/M5Unit-UWB2-Pro.git#main
    m5stack/M5Unified
    m5stack/M5GFX
```

## 接线与通信参数

使用 Grove 线连接 Unit 与 M5Stack Basic 主机，并在 Unit 的 **SETUP → COMM** 中选择通信方式。
UART 和 I2C 共用信号引脚，主机初始化方式应与 Unit 当前设置一致。

| 接口 | Unit 引脚 | M5Stack Basic 主机引脚 | 参数 |
| --- | --- | --- | --- |
| UART | G12：TX；G13：RX | G22：RX；G21：TX | 115200，8 数据位，无校验，1 停止位 |
| I2C | G12：SCL；G13：SDA | G22：SCL；G21：SDA | 100 kHz 或 400 kHz，默认地址 `0x64` |

自行接线时，两端需共地。I2C 使用 3.3 V 信号电平和外部上拉电阻，主机需支持时钟延展。
连接时使用 Unit 已配置的 7 位地址。

## 快速开始：双机测距

准备两台主机，每台连接一台 Unit。根据通信方式，打开
[UART_Basic](examples/UART_Basic/UART_Basic.ino) 或
[I2C_Basic](examples/I2C_Basic/I2C_Basic.ino)。两个示例的配置和测距功能相同。

1. 将两台 Unit 设置为所选示例对应的 UART 或 I2C 模式。
2. 修改示例顶部的 `UWB2PRO_ROLE_TAG`，分别选择 TAG 和 Anchor 1。
3. 编译并上传到对应主机，打开 **115200 波特率**的串口监视器。

```cpp
#define UWB2PRO_ROLE_TAG 1
```

| 宏值 | Unit 角色 | 参数 |
| --- | --- | --- |
| `1`（默认） | TAG（标签） | 测量 Anchor 1 |
| `0` | ANCHOR（基站） | 本机 ID 为 1 |

示例读取型号和固件版本，保存 RANGING、Group 1、850K、BOOT APP 配置，
随后每秒打印一次距离和状态。上述配置会保留至下次上电。

```text
Anchor 1: distance=123 cm, state=Valid
Anchor 1: distance=--, state=NoData
```

I2C 示例使用地址 `0x64` 和 100 kHz；按实际地址修改 `beginI2C()` 的地址参数，
将频率参数 `100000` 改为 `400000` 可使用 400 kHz。

## API 使用

### 初始化

声明驱动对象和配置快照：

```cpp
#include <M5UnitUWB2Pro.h>
#include <Wire.h>

M5UnitUWB2Pro unit;
UWB2ProConfig activeConfig;
```

在 `setup()` 中初始化串口输出，并连接 Unit：

```cpp
Serial.begin(115200);

if (!unit.beginUART(Serial2, 22, 21) || !unit.probe()) {
    Serial.println(unit.lastError());
    return;
}
```

使用 I2C 时，将上面的 `beginUART()` 调用替换为：

```cpp
unit.beginI2C(Wire, 21, 22, 0x64, 100000)
```

`probe()` 用于检查通信。`getVersion()`、`getBoard()` 可读取固件版本和型号；
`getMAC()` 通过 UART 读取 MAC 地址，输出缓冲区至少为 18 字节。

`getAppRunning(running)` 查询应用当前是否正在运行。它与配置草稿中的 `bootApp`
及测量数据是否有效相互独立；没有无线对端、数据状态为 `NoData` 时，应用仍可能正在运行。

`end()` 关闭接口并释放资源。`beginUART()` 和 `beginI2C()` 会管理传入的串口或总线；
需要由应用管理共享总线时，使用 `beginWithIO()` 提供通信回调。

### 配置与保存

`getConfig()` 返回当前配置草稿。设置方法修改草稿，`save()` 将其保存并应用；
`cancel()` 放弃未保存修改。`setLED()` 会立即改变 LED，仍需 `save()` 才能在上电后保留。

例如，在连接后配置 RANGING TAG，测量一个基站：

```cpp
if (!unit.cancel() ||
    !unit.setApp(UWB2ProApp::Ranging, UWB2ProRole::Tag, 1) ||
    !unit.setGroup(1) ||
    !unit.setDataRate(UWB2ProDataRate::Rate850K) ||
    !unit.setBootApp(true) || !unit.save()) {
    Serial.println(unit.lastError());
    return;
}

delay(500);
if (!unit.getConfig(activeConfig)) {
    Serial.println(unit.lastError());
    return;
}
```

| 方法 | 用途与参数 |
| --- | --- |
| `setApp(app, role, value)` | RANGING TAG：`value` 为基站数量 1–9；RANGING ANCHOR：ID 1–9；定位 ANCHOR：1=O、2=X、3=Y、4=Z（仅 3D） |
| `setLayout(app, x, y, z)` | 配置 POS2D/POS3D TAG，轴长为 1–9 整米；2D 省略 Z |
| `setGroup(group)` | 无线分组 1–9，同组设备使用相同值 |
| `setDataRate(rate)` | `Rate850K` 或 `Rate6M8`，同组设备使用相同速率 |
| `setLED(mode)` | `Off`、`On` 或 `Auto` |
| `setBootApp(enabled)` | `true`：保存后运行应用，并在下次上电自动运行；`false`：进入设置界面 |
| `setComm(comm)` | 选择 `UART` 或 `I2C` |
| `setI2CAddress(address)` | 通过 I2C 连接时设置地址，范围 `0x08`–`0x77`，保存后生效 |

`restore()` 恢复出厂设置，包括 UART、I2C 地址 `0x64`、SETUP、Group 1、850K 和 LED AUTO。
执行后按 UART 方式重新连接。

### 读取距离与坐标

在 `loop()` 中调用 `readData(activeConfig, data)`，传入已保存并生效的配置快照。
下面的片段适用于前述单基站测距配置：

```cpp
delay(1000);
UWB2ProData data;
if (!unit.readData(activeConfig, data)) {
    Serial.println(unit.lastError());
    return;
}

const UWB2ProDistance& distance = data.anchors[0];
if (distance.state == UWB2ProState::Valid) {
    Serial.printf("Anchor 1: %ld cm\n", static_cast<long>(distance.centimeters));
}
```

距离和坐标的单位为**厘米**；`setLayout()` 的轴长单位为**米**。
测距 ID 1–9 对应 `anchors[0]`–`anchors[8]`；定位 O/X/Y/Z 对应 `anchors[0]`–`anchors[3]`。
坐标位于 `xCentimeters`、`yCentimeters`、`zCentimeters`，由 `positionState` 表示有效状态。

| 状态 | 含义 | 数值处理 |
| --- | --- | --- |
| `NoData` | 暂无测量结果，例如无线对端尚未运行 | 不使用 |
| `Valid` | 当前测量有效 | 可使用 |
| `Timeout` | 数据已超时 | 不使用旧值 |
| `Error` | 测量错误 | 不使用 |

缺失数值使用 `INT32_MIN` 或 `INT16_MIN`。2D 定位需要 O/X/Y 三个基站，
3D 定位需要 O/X/Y/Z 四个基站；基站位置与 TAG 的轴长配置应一致。

修改应用配置后，应重新保存并更新 `activeConfig`。读取失败时，输出参数保持不变；
通过 `lastErrorCode()` 和 `lastError()` 获取错误原因。

### 切换接口或 I2C 地址

1. 调用 `setComm()` 或 `setI2CAddress()`，然后执行 `save()`。
2. 调用 `end()` 关闭旧接口。
3. 使用新的接口或地址初始化，再调用 `probe()` 确认连接。

旧接口返回成功表示切换命令已提交，新接口是否可用需重新连接确认。

## 屏幕示例

[Unit_UWB2Pro_Demo](examples/Unit_UWB2Pro_Demo) 使用 M5Unified/M5GFX，
适配 M5Stack Basic 主机的 320×240 屏幕和三个按键。

首页选择 UART 或 I2C；I2C 先选择目标地址。连接后显示 **Firmware Version**。
选择 **Continue** 后，若 Unit 正在运行应用则进入对应的数据页；若处于 SETUP 则进入设置菜单。
Continue 保持配置和 BOOT 设置不变。

| 页面 | 按键 A | 按键 B | 按键 C |
| --- | --- | --- | --- |
| 运行页 | 打开 Setup | 切换 Details/Data | 切换 Pause/Live |
| Setup 主菜单 | Prev | Next | Select |

子菜单操作见屏幕底部提示。通过 **Application → Role → Parameters** 配置应用；
通过 **Communication → I2C Address** 修改 Unit 的地址。

## 文件说明

| 文件 | 内容 |
| --- | --- |
| [src/M5UnitUWB2Pro.h](src/M5UnitUWB2Pro.h) | 公开 API、枚举和数据结构 |
| [src/M5UnitUWB2Pro.cpp](src/M5UnitUWB2Pro.cpp) | 协议解析、配置和数据读取 |
| [src/M5UnitUWB2ProArduino.cpp](src/M5UnitUWB2ProArduino.cpp) | Arduino UART/I2C 通信 |
| `examples/UART_Basic`、`examples/I2C_Basic` | 双机测距和串口输出 |
| `examples/Unit_UWB2Pro_Demo/Unit_UWB2Pro_Demo.ino` | 示例入口 |
| `examples/Unit_UWB2Pro_Demo/UWB2ProDemo.h/.cpp` | 示例状态、初始化和更新 |
| `examples/Unit_UWB2Pro_Demo/UWB2ProDevice.cpp` | 设备连接和配置 |
| `examples/Unit_UWB2Pro_Demo/UWB2ProView.cpp` | 界面绘制 |
| `examples/Unit_UWB2Pro_Demo/UWB2ProControls.cpp` | 按键与菜单操作 |

## 许可证

[M5Unit-UWB2-Pro - MIT](LICENSE) · Copyright © 2026 M5Stack Technology Co., Ltd.
