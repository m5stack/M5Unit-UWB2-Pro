# Unit UWB2-Pro Arduino Library

[English](README.md) | [简体中文](README_zh.md)

## Overview

### SKU: U237

Configure Unit UWB2-Pro and read ranging or 2D/3D positioning data over UART or I2C
from an ESP32 host. Supports factory firmware v0.1 and Unit UWB Angle-Pro devices
using the same protocol.

## Related Links

- [Arduino Library](https://github.com/m5stack/M5Unit-UWB2-Pro)
- [Examples](https://github.com/m5stack/M5Unit-UWB2-Pro/tree/main/examples)

## Required Libraries

The UART and I2C basic examples use the ESP32 Arduino core without additional libraries.
The display example requires:

| Library | Purpose |
| --- | --- |
| [M5Unified](https://github.com/m5stack/M5Unified) | M5Stack Basic host initialization and button input |
| [M5GFX](https://github.com/m5stack/M5GFX) | Display and text rendering |

## Installation

### Arduino IDE

1. Install the ESP32 board package for your host.
2. Download the [**main branch ZIP**](https://github.com/m5stack/M5Unit-UWB2-Pro/archive/refs/heads/main.zip).
3. Select **Sketch → Include Library → Add .ZIP Library** and import the file.
4. Open an example under **File → Examples → M5UnitUWB2Pro**.

The `Unit_UWB2Pro_Demo` display example also requires **M5Unified** and **M5GFX**,
available through the Library Manager.

### PlatformIO

Add the library to your project's `platformio.ini`:

```ini
lib_deps =
    https://github.com/m5stack/M5Unit-UWB2-Pro.git#main
```

To run the display example on an **M5Stack Basic host**, copy the files from
`examples/Unit_UWB2Pro_Demo` into your project's `src/` directory and use:

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

## Wiring and communication

Connect the Unit to your M5Stack Basic host with a Grove cable. Select the communication mode
in the Unit's **SETUP → COMM** menu. UART and I2C share the signal pins, so initialize
the host with the mode currently selected on the Unit.

| Interface | Unit pins | M5Stack Basic host pins | Settings |
| --- | --- | --- | --- |
| UART | G12: TX; G13: RX | G22: RX; G21: TX | 115200 baud, 8 data bits, no parity, 1 stop bit |
| I2C | G12: SCL; G13: SDA | G22: SCL; G21: SDA | 100 kHz or 400 kHz; default address `0x64` |

For custom wiring, connect the grounds. I2C requires 3.3 V signal levels, external
pull-ups, and a host that supports clock stretching. Use the Unit's configured 7-bit address.

## Quick start: two-device ranging

Use two hosts, each connected to one Unit. Open
[UART_Basic](examples/UART_Basic/UART_Basic.ino) or
[I2C_Basic](examples/I2C_Basic/I2C_Basic.ino) for your chosen interface.
Both examples provide the same configuration and ranging functions.

1. Set both Units to the example's UART or I2C mode.
2. Set `UWB2PRO_ROLE_TAG` at the top of each sketch to select a Tag and Anchor 1.
3. Compile and upload to each host, then open its serial monitor at **115200 baud**.

```cpp
#define UWB2PRO_ROLE_TAG 1
```

| Value | Unit role | Parameter |
| --- | --- | --- |
| `1` (default) | TAG | Range to Anchor 1 |
| `0` | ANCHOR | Local ID 1 |

The examples read the model and firmware version, save RANGING, Group 1, 850K,
and BOOT APP, then print distance and state once per second. These settings persist
across power cycles.

```text
Anchor 1: distance=123 cm, state=Valid
Anchor 1: distance=--, state=NoData
```

The I2C example uses address `0x64` at 100 kHz. Set the address argument in
`beginI2C()` to your Unit's address; change the frequency argument from `100000`
to `400000` for 400 kHz.

## API usage

### Initialization

Declare the driver and an applied configuration snapshot:

```cpp
#include <M5UnitUWB2Pro.h>
#include <Wire.h>

M5UnitUWB2Pro unit;
UWB2ProConfig activeConfig;
```

In `setup()`, initialize serial output and connect to the Unit:

```cpp
Serial.begin(115200);

if (!unit.beginUART(Serial2, 22, 21) || !unit.probe()) {
    Serial.println(unit.lastError());
    return;
}
```

For I2C, replace the `beginUART()` call above with:

```cpp
unit.beginI2C(Wire, 21, 22, 0x64, 100000)
```

`probe()` checks communication. Use `getVersion()` and `getBoard()` to read the
firmware version and model. `getMAC()` reads the MAC address over UART and requires
an output buffer of at least 18 bytes.

`getAppRunning(running)` reads whether an application is currently running. This is
independent of the draft's `bootApp` setting and measurement validity: an application
without a radio peer is still running even when its data state is `NoData`.

`end()` closes the interface and releases resources. `beginUART()` and `beginI2C()`
manage the supplied serial port or bus. To manage a shared bus in your application,
provide transport callbacks through `beginWithIO()`.

### Configuration and saving

`getConfig()` reads the current configuration draft. Setters edit the draft;
`save()` persists and applies it, and `cancel()` discards unsaved changes.
`setLED()` changes the LED immediately but still requires `save()` to persist across power cycles.

For example, after connecting, configure a RANGING Tag with one Anchor:

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

| Method | Purpose and parameters |
| --- | --- |
| `setApp(app, role, value)` | RANGING TAG: Anchor count 1–9; RANGING ANCHOR: ID 1–9; positioning ANCHOR: 1=O, 2=X, 3=Y, 4=Z (3D only) |
| `setLayout(app, x, y, z)` | Configure a POS2D/POS3D Tag; axis lengths are whole metres, 1–9; omit Z for 2D |
| `setGroup(group)` | Radio group 1–9; use the same value on all devices in a group |
| `setDataRate(rate)` | `Rate850K` or `Rate6M8`; use the same rate on all devices in a group |
| `setLED(mode)` | `Off`, `On`, or `Auto` |
| `setBootApp(enabled)` | `true`: run the application after saving and on subsequent boots; `false`: enter setup mode |
| `setComm(comm)` | Select `UART` or `I2C` |
| `setI2CAddress(address)` | Set an address from `0x08` to `0x77` while connected over I2C; takes effect after saving |

`restore()` restores factory settings, including UART, I2C address `0x64`, SETUP,
Group 1, 850K, and LED AUTO. Reconnect over UART afterward.

### Reading distances and coordinates

Call `readData(activeConfig, data)` in `loop()` with a configuration snapshot that
has been saved and applied. This fragment reads the single-Anchor setup above:

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

Distances and coordinates are in **centimetres**; `setLayout()` axis lengths are in
**metres**. Ranging IDs 1–9 map to `anchors[0]`–`anchors[8]`; positioning O/X/Y/Z map
to `anchors[0]`–`anchors[3]`. Coordinates are stored in `xCentimeters`, `yCentimeters`,
and `zCentimeters`, with their validity indicated by `positionState`.

| State | Meaning | Numeric value |
| --- | --- | --- |
| `NoData` | No measurement available, for example when the radio peer is not running | Do not use |
| `Valid` | Current measurement is valid | Use |
| `Timeout` | Measurement has expired | Do not use the old value |
| `Error` | Measurement error | Do not use |

Missing numbers use `INT32_MIN` or `INT16_MIN`. 2D positioning requires O/X/Y Anchors;
3D positioning requires O/X/Y/Z Anchors. Their placement must match the Tag's
configured axis lengths.

After changing the application configuration, save it and refresh `activeConfig`.
Failed reads leave output arguments unchanged. Use `lastErrorCode()` and
`lastError()` to inspect a failure.

### Switching interfaces or I2C addresses

1. Call `setComm()` or `setI2CAddress()`, then `save()`.
2. Call `end()` to close the old interface.
3. Initialize the new interface or address, then call `probe()` to confirm the connection.

Success on the old interface confirms submission of the change. Reconnecting
confirms that the new interface is available.

## Display example

[Unit_UWB2Pro_Demo](examples/Unit_UWB2Pro_Demo) uses M5Unified/M5GFX with the M5Stack
Basic host's 320×240 display and three buttons.

Select UART or I2C on the home page; for I2C, select the target address first.
The connection page displays **Firmware Version**. **Continue** checks the Unit's
current runtime state: a running application opens its data page; a Unit in SETUP
opens the setup menu. Continue leaves the configuration and BOOT setting unchanged.

| Page | Button A | Button B | Button C |
| --- | --- | --- | --- |
| Run | Open Setup | Switch Details/Data | Switch Pause/Live |
| Setup main menu | Prev | Next | Select |

Submenus show button actions at the bottom of the screen. Configure the application
through **Application → Role → Parameters**. Change the Unit's address through
**Communication → I2C Address**.

## Files

| File | Contents |
| --- | --- |
| [src/M5UnitUWB2Pro.h](src/M5UnitUWB2Pro.h) | Public API, enums, and data structures |
| [src/M5UnitUWB2Pro.cpp](src/M5UnitUWB2Pro.cpp) | Protocol parsing, configuration, and measurement reads |
| [src/M5UnitUWB2ProArduino.cpp](src/M5UnitUWB2ProArduino.cpp) | Arduino UART/I2C transport |
| `examples/UART_Basic`, `examples/I2C_Basic` | Two-device ranging and serial output |
| `examples/Unit_UWB2Pro_Demo/Unit_UWB2Pro_Demo.ino` | Example entry point |
| `examples/Unit_UWB2Pro_Demo/UWB2ProDemo.h/.cpp` | Demo state, initialization, and updates |
| `examples/Unit_UWB2Pro_Demo/UWB2ProDevice.cpp` | Device connections and configuration |
| `examples/Unit_UWB2Pro_Demo/UWB2ProView.cpp` | Display rendering |
| `examples/Unit_UWB2Pro_Demo/UWB2ProControls.cpp` | Buttons and menu actions |

## License

[M5Unit-UWB2-Pro - MIT](LICENSE) · Copyright © 2026 M5Stack Technology Co., Ltd.
