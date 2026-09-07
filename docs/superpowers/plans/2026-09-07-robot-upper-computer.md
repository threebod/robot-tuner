# Robot Upper Computer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a Windows Qt 6/C++ tuning application and a compatible STM32F407 standalone debug-mode protocol integration for wired USB-UART and HC-05 links.

**Architecture:** The Qt application separates serial transport, binary framing, request/response state, device services, and seven Widgets pages. The STM32 adds a pure-C parser and command layer plus thin USART3/UART5 adapters, entered only when `HOST_DEBUG_MODE == 1`; the original blocking competition flow remains the `HOST_DEBUG_MODE == 0` path.

**Tech Stack:** C++17, Qt 6 Widgets, Qt 6 SerialPort, CMake/CTest, STM32F4 Standard Peripheral Library, Keil µVision project, portable C99 tests.

**Spec:** `docs/superpowers/specs/2026-09-07-robot-upper-computer-design.md`

## Global Constraints

- Target Windows and Qt 6; do not add runtime libraries beyond Qt6 Widgets and Qt6 SerialPort.
- Parameters are written only to STM32 RAM; do not write Flash and do not rewrite source files from the application.
- The UI opens only one COM port at a time; USB-UART and HC-05 are mutually exclusive at application level.
- Keep USART1 for K230/MaixCAM, USART2 for HWT101, USART6 for HMI, UART5 for HC-05, and provision USART3 on PB10/PB11 for wired tuning.
- Preserve the UART5 legacy `0x04 0x05 ... 0x06` parser and the USART1 visual `0xFF + 9 bytes + 0xFE` protocol.
- Use the confirmed `AA 55` version-1 binary frame, 128-byte payload limit, little-endian fields, and CRC-16/CCITT-FALSE.
- Enter tuning through the compile-time `HOST_DEBUG_MODE` switch; do not make the upper computer live inside the blocking competition path.
- Keep motion bounds enforced on both PC and STM32; software emergency stop does not replace a physical stop.
- Do not install Qt, CMake, Ninja, compilers, or packages on C: without explicit user approval.
- `upper_computer/` is the Git repository. Changes under sibling `../yyb_stm32/` are outside that repository and must be listed separately in every firmware checkpoint.

---

## Planned File Map

Qt repository files:

```text
CMakeLists.txt                         build, app and CTest targets
README.md                              build, run and hardware-check instructions
docs/protocol.md                       exact wire format, commands, IDs and bounds
src/main.cpp                           QApplication entry point
src/app/MainWindow.h/.cpp              persistent top bar, sidebar and page stack
src/protocol/ProtocolTypes.h            frame flags, commands, errors and Frame
src/protocol/Crc16.h/.cpp               CRC-16/CCITT-FALSE only
src/protocol/FrameCodec.h/.cpp          frame encoder
src/protocol/FrameParser.h/.cpp         incremental byte-stream parser
src/protocol/ProtocolClient.h/.cpp      sequence, pending request, timeout and retry
src/serial/SerialController.h/.cpp      QSerialPort ownership and errors
src/device/ParameterCatalog.h/.cpp      IDs, types, units and bounds
src/device/DeviceClient.h/.cpp          typed commands and decoded device events
src/device/TelemetryTypes.h             IMU/PID/status sample structures
src/widgets/TelemetryPlot.h/.cpp        fixed-capacity QPainter curve widget
src/pages/OverviewPage.h/.cpp           connection and device summary
src/pages/ChassisPage.h/.cpp            PID profiles and motion tuning
src/pages/MechanismPage.h/.cpp          arm/servo tuning
src/pages/ImuPage.h/.cpp                HWT101 values, plots and calibration
src/pages/ActionTestPage.h/.cpp          unlock, bounded action, stop and emergency stop
src/pages/TerminalPage.h/.cpp            HEX/ASCII raw terminal and frame log
src/pages/VisionPage.h/.cpp              explicit K230/MaixCAM placeholder
tests/test_crc_codec.cpp                 CRC and encoding tests
tests/test_frame_parser.cpp              stream parser tests
tests/test_protocol_client.cpp           timeout, retry and response matching tests
tests/test_parameter_catalog.cpp         parameter IDs, types and bounds tests
tests/test_device_client.cpp             command payload and event decoding tests
tests/test_ui_smoke.cpp                  offscreen page and safety-state smoke test
tests/test_telemetry_plot.cpp             ring capacity and refresh behavior tests
tests/test_stm32_protocol.c              portable C firmware parser tests
```

STM32 sibling-tree files:

```text
../yyb_stm32/host_protocol/host_config.h       HOST_DEBUG_MODE and protocol constants
../yyb_stm32/host_protocol/host_crc16.h/.c     portable CRC
../yyb_stm32/host_protocol/host_frame.h/.c     incremental parser and encoder
../yyb_stm32/host_protocol/host_link.h/.c      per-UART RX rings and TX callbacks
../yyb_stm32/host_protocol/host_params.h/.c    runtime parameter table and atomic groups
../yyb_stm32/host_protocol/host_safety.h/.c    unlock, watchdog, stop and limits
../yyb_stm32/host_protocol/host_commands.h/.c  command validation and dispatch
../yyb_stm32/host_protocol/host_debug.h/.c     standalone debug loop and telemetry
../yyb_stm32/host_protocol/host_time.h/.c      TIM7 1 ms monotonic counter
../yyb_stm32/host_protocol/host_uart3.h/.c     USART3 PB10/PB11 adapter
../yyb_stm32/Hardware/pid.h/.c                 five runtime PID profiles
../yyb_stm32/lanya/usart5.c                    feed new RX ring while retaining old parser
../yyb_stm32/USER/main.c                       debug-mode branch after peripheral init
../yyb_stm32/USER/Template.uvprojx             include new source/header group and path
```

---

### Task 1: Create a buildable Qt shell and offline smoke test

**Files:**
- Create: `CMakeLists.txt`
- Create: `src/main.cpp`
- Create: `src/app/MainWindow.h`
- Create: `src/app/MainWindow.cpp`
- Create: `tests/test_ui_smoke.cpp`

**Interfaces:**
- Produces: `MainWindow::MainWindow(QWidget *parent = nullptr)` and a `QListWidget` named `navigationList` with seven entries.
- Produces: a `QPushButton` named `emergencyStopButton`, initially disabled while disconnected.

- [ ] **Step 1: Check the existing toolchain without installing anything**

Run:

```powershell
cmake --version
where.exe qmake6
where.exe qt-cmake
```

Then attempt configuration only after `CMakeLists.txt` exists. If Qt 6 cannot be located, stop and ask the user for the installed Qt path or approval before any installation.

- [ ] **Step 2: Write the failing UI smoke test**

```cpp
#include <QApplication>
#include <QListWidget>
#include <QPushButton>
#include "app/MainWindow.h"

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    MainWindow window;
    auto *nav = window.findChild<QListWidget *>("navigationList");
    auto *stop = window.findChild<QPushButton *>("emergencyStopButton");
    if (!nav || nav->count() != 7 || !stop || stop->isEnabled()) return 1;
    return 0;
}
```

- [ ] **Step 3: Create CMake targets and run the test to verify failure**

`CMakeLists.txt` must enable `AUTOMOC`, require C++17, find `Qt6 COMPONENTS Widgets SerialPort REQUIRED`, create `robot_tuner`, create `test_ui_smoke`, and register it with CTest.

Run:

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Expected: build failure because `MainWindow` has not been implemented.

- [ ] **Step 4: Implement the minimal seven-page shell**

Create a `QMainWindow` with a top connection bar, `QListWidget` navigation, `QStackedWidget`, and seven placeholder widgets named exactly:

```cpp
static const QStringList kPages = {
    "总览", "底盘与 PID", "机械臂与舵机", "HWT101",
    "动作测试", "串口终端", "视觉（预留）"
};
```

Set the emergency-stop object name and disabled state. Connect navigation row changes to the stacked widget index.

- [ ] **Step 5: Build and run the smoke test**

Run the three configure/build/test commands from Step 3. Expected: `test_ui_smoke` passes.

- [ ] **Step 6: Commit the shell**

```powershell
git add CMakeLists.txt src/main.cpp src/app tests/test_ui_smoke.cpp
git commit -m "feat: scaffold Qt tuning application"
```

---

### Task 2: Implement CRC and frame encoding

**Files:**
- Create: `src/protocol/ProtocolTypes.h`
- Create: `src/protocol/Crc16.h`
- Create: `src/protocol/Crc16.cpp`
- Create: `src/protocol/FrameCodec.h`
- Create: `src/protocol/FrameCodec.cpp`
- Create: `tests/test_crc_codec.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `quint16 crc16CcittFalse(QByteArrayView bytes)`.
- Produces: `QByteArray encodeFrame(const protocol::Frame &frame)`.
- Produces: `protocol::Frame { quint8 version, flags, sequence, command; QByteArray payload; }`.

- [ ] **Step 1: Define protocol enums and the failing CRC test**

```cpp
namespace protocol {
enum Flag : quint8 { Request = 0x01, Response = 0x02, Event = 0x04, Error = 0x08 };
enum class Command : quint8 {
    Hello = 0x01, GetStatus = 0x02,
    GetParamGroup = 0x10, SetParamGroup = 0x11,
    SetTelemetry = 0x20, ImuCalibrate = 0x21,
    TestUnlock = 0x30, TestAction = 0x31, Stop = 0x32,
    EmergencyStop = 0x33, ClearEmergencyStop = 0x34,
    StatusTelemetry = 0x80, ImuTelemetry = 0x81, PidTelemetry = 0x82
};
struct Frame { quint8 version{1}; quint8 flags{}; quint8 sequence{}; quint8 command{}; QByteArray payload; };
}
```

Test vector:

```cpp
if (crc16CcittFalse(QByteArrayView("123456789", 9)) != 0x29B1) return 1;
```

- [ ] **Step 2: Add the test target and verify it fails**

Run `cmake --build build --config Debug` followed by CTest. Expected: undefined CRC/encoder symbols.

- [ ] **Step 3: Implement CRC and encoder**

Encoder output must be:

```text
AA 55 version flags sequence command length_lo length_hi payload crc_lo crc_hi
```

Reject payloads longer than 128 by returning an empty `QByteArray`. Calculate CRC over `version` through the final payload byte.

- [ ] **Step 4: Add encoding assertions**

Verify header bytes, little-endian length, total size `10 + payload.size()`, payload preservation, and that recomputing CRC over bytes `[2, size-4]` equals the trailing little-endian CRC.

- [ ] **Step 5: Run all tests and commit**

```powershell
ctest --test-dir build -C Debug --output-on-failure
git add CMakeLists.txt src/protocol tests/test_crc_codec.cpp
git commit -m "feat: encode versioned CRC serial frames"
```

---

### Task 3: Implement the incremental frame parser

**Files:**
- Create: `src/protocol/FrameParser.h`
- Create: `src/protocol/FrameParser.cpp`
- Create: `tests/test_frame_parser.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `protocol::Frame`, `crc16CcittFalse()` and `encodeFrame()` from Task 2.
- Produces: `QVector<protocol::Frame> FrameParser::push(QByteArrayView bytes)`.
- Produces: `FrameParser::Stats { quint64 crcErrors, lengthErrors, discardedBytes }` and `void reset()`.

- [ ] **Step 1: Write failing split-frame and sticky-frame tests**

```cpp
FrameParser parser;
const QByteArray a = encodeFrame({1, protocol::Request, 1, 0x01, "A"});
const QByteArray b = encodeFrame({1, protocol::Request, 2, 0x02, "BC"});
if (!parser.push(QByteArrayView(a.constData(), 3)).isEmpty()) return 1;
if (parser.push(QByteArrayView(a.constData() + 3, a.size() - 3)).size() != 1) return 2;
if (parser.push(QByteArrayView(a + b)).size() != 2) return 3;
```

- [ ] **Step 2: Add noise, CRC and oversized-length tests**

Feed `00 04 05 06` before a valid `AA 55` frame and require recovery. Flip one payload bit and require no output plus `crcErrors == 1`. Feed a header with length 129 and require `lengthErrors == 1` and recovery on the next valid frame.

- [ ] **Step 3: Run tests to verify failure**

Run `ctest --test-dir build -C Debug --output-on-failure`. Expected: parser target does not compile.

- [ ] **Step 4: Implement a bounded-buffer state machine**

Maintain a `QByteArray buffer_`. Search for `AA 55`, discard preceding bytes, wait for the 8-byte pre-payload header, reject length above 128, wait for `10 + length` bytes, validate CRC, emit one frame, remove it, and continue until no complete frame remains. Keep at most one trailing `0xAA` when no full header is present.

- [ ] **Step 5: Run tests and commit**

```powershell
ctest --test-dir build -C Debug --output-on-failure
git add CMakeLists.txt src/protocol/FrameParser.* tests/test_frame_parser.cpp
git commit -m "feat: parse fragmented serial frame streams"
```

---

### Task 4: Add request matching, timeout and one retry

**Files:**
- Create: `src/protocol/ProtocolClient.h`
- Create: `src/protocol/ProtocolClient.cpp`
- Create: `tests/test_protocol_client.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `FrameParser` and `encodeFrame()`.
- Produces: `quint8 ProtocolClient::sendRequest(protocol::Command command, QByteArray payload)`.
- Produces: `void ProtocolClient::ingestBytes(QByteArrayView bytes)` and `void ProtocolClient::clearPending()`.
- Emits: `bytesReady(QByteArray)`, `responseReceived(protocol::Frame)`, `eventReceived(protocol::Frame)`, and `requestFailed(quint8 sequence, QString reason)`.

- [ ] **Step 1: Write a failing response-matching test**

Connect test lambdas to integer counters so the test needs no Qt Test module. Send a request, capture `bytesReady`, decode its sequence, feed a matching response, and require one `responseReceived` emission and zero pending requests.

- [ ] **Step 2: Write a failing timeout/retry test**

Expose `setTimeoutMsForTest(10)`. Start a request and run a `QEventLoop` for 35 ms. Require exactly two `bytesReady` emissions—the initial frame and one retry—then one `requestFailed` emission.

- [ ] **Step 3: Implement pending requests**

Store:

```cpp
struct PendingRequest {
    protocol::Frame frame;
    int retriesRemaining{1};
    QDeadlineTimer deadline;
};
QHash<quint8, PendingRequest> pending_;
```

Use one periodic `QTimer` to inspect deadlines. Sequence values wrap naturally but must skip values still present in `pending_`. Match only frames with `Response` or `Error` flag and the same sequence.

- [ ] **Step 4: Implement event handling and disconnect cleanup**

Frames with `Event` bypass pending matching. `clearPending()` emits failure for each request with reason `连接已断开`, clears the map, resets the parser, and stops the deadline timer.

- [ ] **Step 5: Run tests and commit**

```powershell
ctest --test-dir build -C Debug --output-on-failure
git add CMakeLists.txt src/protocol/ProtocolClient.* tests/test_protocol_client.cpp
git commit -m "feat: manage protocol requests and retries"
```

---

### Task 5: Define parameter catalog and typed device service

**Files:**
- Create: `docs/protocol.md`
- Create: `src/device/ParameterCatalog.h`
- Create: `src/device/ParameterCatalog.cpp`
- Create: `src/device/TelemetryTypes.h`
- Create: `src/device/DeviceClient.h`
- Create: `src/device/DeviceClient.cpp`
- Create: `tests/test_parameter_catalog.cpp`
- Create: `tests/test_device_client.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `ProtocolClient::sendRequest()` and protocol response/event signals.
- Produces: `ParameterSpec { quint16 id; QString key, label, unit; ValueType type; double minimum, maximum; quint8 group; }`.
- Produces: `DeviceClient::hello()`, `getParameterGroup(quint8)`, `setParameterGroup(quint8, QVector<ParameterValue>)`, `setTelemetry(mask, periodMs)`, and `calibrateImu()`.
- Emits: `handshakeCompleted(DeviceInfo)`, `parameterGroupReceived(quint8, QVector<ParameterValue>)`, `imuSampleReceived(ImuSample)`, `pidSampleReceived(PidSample)`, and `deviceError(QString)`.

- [ ] **Step 1: Write the exact protocol table in `docs/protocol.md`**

Define flag values, command values, error codes, payload layouts and these fixed parameter IDs:

Use these error values: `0x01 VERSION`, `0x02 COMMAND`, `0x03 CRC`, `0x04 LENGTH`, `0x05 PARAM_NOT_FOUND`, `0x06 PARAM_TYPE`, `0x07 PARAM_RANGE`, `0x08 BUSY`, `0x09 NOT_UNLOCKED`, and `0x0A EMERGENCY_LOCKED`. An error response uses `Response | Error`, preserves the request sequence/command, and carries `error:u8` as its first payload byte.

Use these command payloads:

```text
HELLO request: empty
HELLO response: protocol:u8 fw_major:u8 fw_minor:u8 fw_patch:u8 capabilities:u32
GET_STATUS request: empty
GET_STATUS response: mode:u8 emergency:u8 unlocked:u8 active_link:u8 last_error:u16
GET_PARAM_GROUP request: group:u8
GET_PARAM_GROUP response: group:u8 count:u8 [id:u16 type:u8 value:typed] * count
SET_PARAM_GROUP request: group:u8 count:u8 [id:u16 type:u8 value:typed] * count
SET_PARAM_GROUP response: group:u8 count:u8 [id:u16 type:u8 applied_value:typed] * count
SET_TELEMETRY request: mask:u8 period_ms:u16
SET_TELEMETRY response: accepted_mask:u8 actual_period_ms:u16
IMU_CALIBRATE request: empty
IMU_CALIBRATE response: state:u8, where 0=started, 1=completed, 2=failed
TEST_UNLOCK request: empty
TEST_UNLOCK response: unlock_duration_ms:u16, fixed at 30000
STOP, EMERGENCY_STOP and CLEAR_EMERGENCY_STOP request/response: empty
STATUS_TELEMETRY event: mode:u8 emergency:u8 unlocked:u8 last_error:u16
IMU_TELEMETRY event: timestamp_ms:u32 ax:i16 ay:i16 az:i16 gx:i16 gy:i16 gz:i16 roll:i16 pitch:i16 yaw:i16
PID_TELEMETRY event: timestamp_ms:u32 target_cdeg:i16 actual_cdeg:i16 output_centi:i16
```

IMU conversion is `acceleration = raw / 32768 * 16 g`, `angular velocity = raw / 32768 * 2000 degree/s`, and `angle = raw / 32768 * 180 degree`. PID target/actual use 0.01 degree per count and output uses 0.01 output-unit per count.

Define TEST_ACTION variants exactly as:

```text
0x01 chassis: action:u8 vx:i16 vy:i16 w:i16 duration_ms:u16
0x10 horizontal mechanism: action:u8 target:f32 speed:u16 accel:u8
0x11 lift mechanism: action:u8 target:f32 speed:u16 accel:u8
0x12 turret: action:u8 target_angle:f32 interpolation_speed:f32
0x20 platform named position: action:u8 position:u8, position in 1..3
0x21 gripper named state: action:u8 state:u8, 0=close and 1=open
```

```text
0x1000 + profile*0x10 + 0  PID Kp          float32  0..20
0x1000 + profile*0x10 + 1  PID Ki          float32  0..2
0x1000 + profile*0x10 + 2  PID Kd          float32  0..20
0x1000 + profile*0x10 + 3  integral limit  float32  0..100
0x1000 + profile*0x10 + 4  output limit    float32  0..230
0x2000  chassis vx test limit              int32   -80..80
0x2001  chassis vy test limit              int32   -80..80
0x2002  chassis w test limit               int32   -30..30
0x2003  chassis test duration ms            uint16   50..1000
0x2004  motion acceleration                 uint16   1..230
0x3000  horizontal mechanism position       float32 -120..63
0x3001  lift mechanism position             float32  0..50
0x3002  turret angle                        float32 135..295
0x3003  mechanism motor speed               uint16 100..2000
0x3004  mechanism acceleration              uint8    1..220
0x3005  turret interpolation speed          float32  1..20
0x4000  IMU telemetry rate Hz               uint16   1..50
```

Document groups `0x10` PID, `0x20` chassis, `0x30` mechanism, `0x40` IMU. Platform positions 1–3 and gripper open/close remain action enums, not unrestricted numeric parameters.

- [ ] **Step 2: Write failing catalog tests**

Require 25 PID specs, unique IDs, five complete profiles, exact bounds above, and rejection of values below/above each range.

- [ ] **Step 3: Implement `ParameterCatalog`**

Provide `const ParameterSpec *find(quint16 id)`, `QVector<ParameterSpec> group(quint8 groupId)`, and `bool validate(quint16 id, const QVariant &value, QString *error)`.

- [ ] **Step 4: Write failing DeviceClient payload tests**

Capture the frame emitted by `getParameterGroup(0x10)` and require payload `[0x10]`. For a two-value write, require payload layout:

```text
group:u8 count:u8 [id:u16 type:u8 value:typed] * count
```

Feed a synthetic IMU event containing nine little-endian `int16` values and verify conversion to acceleration, angular velocity and angle members using documented scale factors.

- [ ] **Step 5: Implement DeviceClient encode/decode paths**

Reject a parameter group locally if any item fails catalog validation. Decode device errors into Chinese messages while preserving the numeric code in the terminal log signal.

- [ ] **Step 6: Run tests and commit**

```powershell
ctest --test-dir build -C Debug --output-on-failure
git add CMakeLists.txt docs/protocol.md src/device tests/test_parameter_catalog.cpp tests/test_device_client.cpp
git commit -m "feat: add typed tuning device service"
```

---

### Task 6: Connect QSerialPort and complete the main window connection state

**Files:**
- Create: `src/serial/SerialController.h`
- Create: `src/serial/SerialController.cpp`
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`
- Modify: `tests/test_ui_smoke.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `QStringList SerialController::availablePorts() const`, `bool open(QString portName, qint32 baudRate)`, `void close()`, and `qint64 write(QByteArrayView)`.
- Emits: `opened()`, `closed()`, `bytesReceived(QByteArray)`, and `serialError(QString)`.
- Consumes: `ProtocolClient::bytesReady`; feeds received bytes to `ProtocolClient::ingestBytes`.

- [ ] **Step 1: Extend the smoke test for disconnected state**

Require object names `portCombo`, `baudCombo`, `refreshPortsButton`, `connectButton`, `connectionStatusLabel`, and `emergencyStopButton`. Require parameter/action pages disabled before handshake.

- [ ] **Step 2: Implement SerialController**

Own one `QSerialPort`. Configure 8 data bits, no parity, one stop bit and no flow control. Enumerate `QSerialPortInfo::availablePorts()`. On `ResourceError`, close the port and emit one user-facing error.

- [ ] **Step 3: Wire connection lifecycle**

On connect: open selected COM port, disable port/baud controls, show `串口已连接，等待设备握手`, and call `DeviceClient::hello()`. On handshake: enable tuning pages and emergency stop. On disconnect: call `ProtocolClient::clearPending()`, disable all device controls, and restore selectors.

- [ ] **Step 4: Verify with a nonexistent COM port**

Run the app and select a deliberately nonexistent port name injected in a debug build. Expected: a visible open error, selectors remain enabled, and action controls remain disabled.

- [ ] **Step 5: Run tests and commit**

```powershell
ctest --test-dir build -C Debug --output-on-failure
git add CMakeLists.txt src/serial src/app tests/test_ui_smoke.cpp
git commit -m "feat: connect serial ports and device handshake"
```

---

### Task 7: Build PID, chassis and mechanism parameter pages

**Files:**
- Create: `src/pages/ChassisPage.h`
- Create: `src/pages/ChassisPage.cpp`
- Create: `src/pages/MechanismPage.h`
- Create: `src/pages/MechanismPage.cpp`
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`
- Modify: `tests/test_ui_smoke.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `ParameterCatalog` groups and `DeviceClient` group read/write methods.
- Produces from each page: `setConnected(bool)`, `setValues(QVector<ParameterValue>)`, `readRequested(quint8 group)`, and `writeRequested(quint8 group, QVector<ParameterValue>)`.

- [ ] **Step 1: Add failing page-state smoke assertions**

Find each page by object name. Require five PID profile selectors, bounded spin boxes derived from `ParameterCatalog`, and disabled write buttons before handshake.

- [ ] **Step 2: Implement ChassisPage**

Use one profile selector for profiles 0–4 and fields Kp, Ki, Kd, integral limit and output limit. Add chassis vx/vy/w limits, duration and acceleration. Build controls from catalog metadata rather than duplicating bounds in widgets.

- [ ] **Step 3: Implement MechanismPage**

Expose horizontal position, lift position, turret angle, motor speed, acceleration and interpolation speed. Display units next to each field. Do not expose unrestricted platform/gripper numeric controls.

- [ ] **Step 4: Implement connection snapshots**

After first successful group read, store the returned values as `connectionInitialValues_`. “恢复本次连接初值” repopulates fields and requires an explicit “写入 RAM” action; it must not transmit automatically.

- [ ] **Step 5: Run tests and commit**

```powershell
ctest --test-dir build -C Debug --output-on-failure
git add CMakeLists.txt src/pages/ChassisPage.* src/pages/MechanismPage.* src/app tests/test_ui_smoke.cpp
git commit -m "feat: add chassis and mechanism tuning pages"
```

---

### Task 8: Add overview, IMU telemetry and bounded curve rendering

**Files:**
- Create: `src/pages/OverviewPage.h`
- Create: `src/pages/OverviewPage.cpp`
- Create: `src/pages/ImuPage.h`
- Create: `src/pages/ImuPage.cpp`
- Create: `src/widgets/TelemetryPlot.h`
- Create: `src/widgets/TelemetryPlot.cpp`
- Create: `tests/test_telemetry_plot.cpp`
- Modify: `src/app/MainWindow.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `DeviceClient::imuSampleReceived`, `handshakeCompleted`, `deviceError`, and telemetry subscription methods.
- Produces: `TelemetryPlot::TelemetryPlot(int channelCount, int capacity = 1000, QWidget *parent = nullptr)`, `append(qint64 timestampMs, QVector<double> values)`, `clear()`, and `sampleCount() const`.
- Produces: `ImuPage::calibrationRequested()` and `telemetryRateChanged(quint16 hz)`.

- [ ] **Step 1: Write a failing fixed-capacity plot test**

Append 1,100 samples to a plot configured with capacity 1,000. Require `sampleCount() == 1000` and that the oldest retained timestamp is sample 100.

- [ ] **Step 2: Implement TelemetryPlot without Qt Charts**

Store a ring of timestamp plus channel values. In `paintEvent`, draw axes, zero line, channel legend and polylines from the visible range. Repaint at no more than 25 FPS using a coalescing timer even when data arrives at 50 Hz.

- [ ] **Step 3: Implement ImuPage**

Show nine numeric values and three plots: acceleration, angular velocity and attitude. Offer rate 1–20 Hz for 9600-baud links and 1–50 Hz otherwise; accept the actual rate returned by STM32. Calibration requires confirmation and shows start/result state.

- [ ] **Step 4: Implement OverviewPage**

Show protocol/firmware version, link state, last response latency, debug/locked/emergency state, latest roll/pitch/yaw and most recent device error.

- [ ] **Step 5: Run tests and commit**

```powershell
ctest --test-dir build -C Debug --output-on-failure
git add CMakeLists.txt src/widgets src/pages/OverviewPage.* src/pages/ImuPage.* src/app tests/test_telemetry_plot.cpp
git commit -m "feat: display bounded IMU telemetry plots"
```

---

### Task 9: Add safe action testing, terminal and visual placeholder

**Files:**
- Create: `src/pages/ActionTestPage.h`
- Create: `src/pages/ActionTestPage.cpp`
- Create: `src/pages/TerminalPage.h`
- Create: `src/pages/TerminalPage.cpp`
- Create: `src/pages/VisionPage.h`
- Create: `src/pages/VisionPage.cpp`
- Modify: `src/device/DeviceClient.h`
- Modify: `src/device/DeviceClient.cpp`
- Modify: `src/app/MainWindow.cpp`
- Modify: `tests/test_device_client.cpp`
- Modify: `tests/test_ui_smoke.cpp`

**Interfaces:**
- Produces: `DeviceClient::unlockTests()`, `testChassis(vx, vy, w, durationMs)`, `testHorizontal(target, speed, accel)`, `testLift(target, speed, accel)`, `testTurret(angle, interpolationSpeed)`, `setPlatformPosition(position)`, `setGripperOpen(open)`, `stop()`, `emergencyStop()`, and `clearEmergencyStop()`.
- Terminal consumes raw TX/RX notifications from `SerialController` and decoded-frame notifications from `ProtocolClient`.

- [ ] **Step 1: Write failing action payload and safety-state tests**

Require local rejection for chassis values outside `vx/vy ±80`, `w ±30`, or duration `50..1000`. Require Test Action disabled before unlock, enabled after successful unlock ACK, and disabled again after 30 seconds, disconnect or emergency state.

- [ ] **Step 2: Implement ActionTestPage**

Provide press-to-send directional actions, explicit duration, immediate stop, named platform positions 1–3, gripper open/close, and bounded mechanism single-step actions. Display the remaining unlock time. Do not allow queued multi-step scripts.

- [ ] **Step 3: Implement emergency behavior in the UI**

The top emergency button calls `DeviceClient::emergencyStop()` immediately without a confirmation dialog. Clearing emergency state requires a confirmation dialog and successful device ACK. Any disconnect locks the page.

- [ ] **Step 4: Implement TerminalPage**

Support HEX and ASCII display modes, timestamps, TX/RX direction, clear, pause display, and manual send. Manual send writes raw serial bytes but UI labels it as bypassing request tracking; STM32 still enforces unlock and limits.

- [ ] **Step 5: Implement VisionPage**

Show only `K230 / MaixCAM 视觉调参将在后续版本实现` plus current USART1 interface description. Do not add inactive sliders or camera controls.

- [ ] **Step 6: Run tests and commit**

```powershell
ctest --test-dir build -C Debug --output-on-failure
git add src/pages/ActionTestPage.* src/pages/TerminalPage.* src/pages/VisionPage.* src/device src/app tests
git commit -m "feat: add safe actions and serial terminal"
```

---

### Task 10: Implement portable STM32 framing and link buffers

**Files:**
- Create: `../yyb_stm32/host_protocol/host_config.h`
- Create: `../yyb_stm32/host_protocol/host_crc16.h`
- Create: `../yyb_stm32/host_protocol/host_crc16.c`
- Create: `../yyb_stm32/host_protocol/host_frame.h`
- Create: `../yyb_stm32/host_protocol/host_frame.c`
- Create: `../yyb_stm32/host_protocol/host_link.h`
- Create: `../yyb_stm32/host_protocol/host_link.c`
- Create: `tests/test_stm32_protocol.c`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `uint16_t HostCrc16(const uint8_t *data, uint16_t length)`.
- Produces: `void HostFrameParser_Init(HostFrameParser *)` and `HostParseResult HostFrameParser_Push(HostFrameParser *, uint8_t byte, HostFrame *out)`.
- Produces: `bool HostLink_PushRxFromIsr(HostLinkId, uint8_t)` and `bool HostLink_PopRx(HostLinkId, uint8_t *)` using fixed 256-byte rings.

- [ ] **Step 1: Write the portable C failing test**

Test CRC `123456789 == 0x29B1`, an empty HELLO frame, one-byte-at-a-time parsing, two concatenated frames, CRC rejection, length 129 rejection and recovery on the next valid frame.

- [ ] **Step 2: Add a CTest target using sibling firmware sources**

Create `test_stm32_protocol` as a C99 executable whose sources include the test plus `../yyb_stm32/host_protocol/host_crc16.c`, `host_frame.c`, and `host_link.c`. Do not include STM32 headers in these three modules.

- [ ] **Step 3: Run the test to verify failure**

Run configure, build and CTest. Expected: missing firmware protocol files or symbols.

- [ ] **Step 4: Implement fixed-storage CRC, parser and rings**

Use arrays only—no heap allocation. `host_config.h` must contain:

```c
#define HOST_DEBUG_MODE 1
#define HOST_PROTOCOL_VERSION 1u
#define HOST_MAX_PAYLOAD 128u
#define HOST_RX_RING_SIZE 256u
```

Protect ISR/main ring indices with single-producer/single-consumer rules; count and drop a new byte when the ring is full.

- [ ] **Step 5: Run tests and record the external-tree diff**

```powershell
ctest --test-dir build -C Debug --output-on-failure
git status --short
```

Expected: upper-computer test/CMake changes are tracked; `../yyb_stm32/host_protocol/` does not appear because it belongs to the sibling non-Git tree. List the created sibling files in the checkpoint message.

- [ ] **Step 6: Commit the host-side test harness**

```powershell
git add CMakeLists.txt tests/test_stm32_protocol.c
git commit -m "test: verify portable STM32 protocol core"
```

---

### Task 11: Add STM32 parameters, commands and safety state machine

**Files:**
- Create: `../yyb_stm32/host_protocol/host_params.h`
- Create: `../yyb_stm32/host_protocol/host_params.c`
- Create: `../yyb_stm32/host_protocol/host_safety.h`
- Create: `../yyb_stm32/host_protocol/host_safety.c`
- Create: `../yyb_stm32/host_protocol/host_commands.h`
- Create: `../yyb_stm32/host_protocol/host_commands.c`
- Modify: `../yyb_stm32/Hardware/pid.h`
- Modify: `../yyb_stm32/Hardware/pid.c:22-85`
- Extend: `tests/test_stm32_protocol.c`

**Interfaces:**
- Consumes: command IDs and parameter IDs from `docs/protocol.md`.
- Produces: `HostParam_GetGroup()`, `HostParam_SetGroupAtomic()`, `HostCommands_Handle()`, `HostSafety_Unlock(nowMs)`, `HostSafety_Tick(nowMs)`, `HostSafety_Stop()`, and `HostSafety_EmergencyStop()`.
- Produces: `PID_Profile[5]` initialized to the five currently hardcoded `PID_move` profiles.

- [ ] **Step 1: Extend portable tests for atomic parameter writes**

Write one valid two-parameter PID update and require both values to change. Write a group containing one valid and one out-of-range value and require neither value to change plus `HOST_ERROR_PARAM_RANGE`.

- [ ] **Step 2: Refactor only PID profile selection**

Before editing, preserve a temporary comparison copy:

```powershell
Copy-Item -LiteralPath ..\yyb_stm32\Hardware\pid.c -Destination "$env:TEMP\yyb_pid_before_host_tuning.c"
```

Add:

```c
typedef struct {
    float kp, ki, kd, max_integral, max_output;
} PID_Profile_t;
extern PID_Profile_t PID_Profiles[5];
```

Initialize the table from the existing `pid_choose` branches and make `PID_move` copy the chosen profile into `mypid`. Preserve all existing numeric defaults and behavior when the host has not changed RAM.

- [ ] **Step 3: Implement the parameter table**

Use a fixed descriptor array containing ID, type, pointer, minimum and maximum. For atomic writes, decode and validate every item into a temporary array first, then apply all values only after the full group succeeds.

- [ ] **Step 4: Write failing safety tests with fake motion callbacks**

Require test actions to fail before unlock, pass immediately after unlock, fail after `now + 30001`, and fail during emergency state. Require watchdog expiry to call the fake stop callback exactly once.

- [ ] **Step 5: Implement command and safety dispatch**

Keep hardware calls behind callbacks so portable tests compile. The real emergency callback must execute:

```c
car_move(0, 0, 0);
for (uint8_t address = 1; address <= 6; ++address) {
    Emm_V5_Stop_Now(address, false);
}
dianji_move_flag = 0;
PID_control_flag = 0;
```

Validate all action payloads before invoking hardware. Unlock lasts 30,000 ms. Refresh the 1,000 ms communication watchdog only on a valid frame from the active debug link.

- [ ] **Step 6: Run portable tests and inspect PID diff**

Run CTest. Then run:

```powershell
git diff --no-index -- "$env:TEMP\yyb_pid_before_host_tuning.c" ..\yyb_stm32\Hardware\pid.c
```

Manually confirm that only profile storage/selection changed in `pid.c`; do not reformat unrelated legacy code.

- [ ] **Step 7: Commit tracked test changes**

```powershell
git add tests/test_stm32_protocol.c
git commit -m "test: cover STM32 parameters and safety"
```

In the checkpoint message, separately list all new `../yyb_stm32/host_protocol/` files and the two modified PID files.

---

### Task 12: Wire USART3, UART5, TIM7 and standalone debug mode

**Files:**
- Create: `../yyb_stm32/host_protocol/host_uart3.h`
- Create: `../yyb_stm32/host_protocol/host_uart3.c`
- Create: `../yyb_stm32/host_protocol/host_time.h`
- Create: `../yyb_stm32/host_protocol/host_time.c`
- Create: `../yyb_stm32/host_protocol/host_debug.h`
- Create: `../yyb_stm32/host_protocol/host_debug.c`
- Modify: `../yyb_stm32/lanya/usart5.c:70-140`
- Modify: `../yyb_stm32/USER/main.c:1-91`
- Modify: `../yyb_stm32/USER/Template.uvprojx:342,815-846`

**Interfaces:**
- Consumes: `HostLink_PushRxFromIsr`, `HostCommands_Handle`, HWT101 `imu_scan`, motion APIs and safety APIs.
- Produces: `HostUart3_Init(115200)`, `HostUart3_Send()`, `HostTime_Init()`, `HostTime_Millis()`, `HostDebug_Init()`, and non-returning `HostDebug_Run()`.

- [ ] **Step 1: Implement USART3 PB10/PB11 adapter**

Use `RCC_APB1Periph_USART3`, `RCC_AHB1Periph_GPIOB`, AF7, 8-N-1, RXNE interrupt and a lower interrupt priority than emergency-relevant motor control. `USART3_IRQHandler` must only read DR, push one byte to `HOST_LINK_USB`, clear RXNE and return.

- [ ] **Step 2: Preserve UART5 legacy parsing while feeding the new ring**

Immediately after reading `r = USART_ReceiveData(UART5)`, call:

```c
HostLink_PushRxFromIsr(HOST_LINK_BLUETOOTH, r);
```

Leave the existing `0x04 0x05 ... 0x06` state machine behavior intact. The new parser ignores bytes until `AA 55`; the old parser resets on unrelated bytes.

- [ ] **Step 3: Implement a TIM7 millisecond counter**

Configure TIM7 update at 1 kHz and increment a `volatile uint32_t` in `TIM7_IRQHandler`. `HostTime_Millis()` returns a consistent snapshot. Search the project again for TIM7 before editing and stop if another non-startup use has appeared.

- [ ] **Step 4: Implement standalone debug service**

`HostDebug_Run()` continuously drains both RX rings, parses complete frames, dispatches commands, sends responses on the originating link, ticks safety, and emits subscribed telemetry. At 9600 baud cap IMU at 20 Hz; at 115200 cap at 50 Hz. Use fixed buffers and no heap.

- [ ] **Step 5: Add the compile-time branch to main**

After the existing peripheral initialization and before the current test/competition sequence:

```c
#if HOST_DEBUG_MODE
    HostDebug_Init();
    HostDebug_Run();
#endif
```

When `HOST_DEBUG_MODE` is `0`, preprocessor output must fall through to the exact existing flow. Do not delete the current line-91 debug loop or restructure the remaining competition code in this task.

- [ ] **Step 6: Add the `host_protocol` group to the Keil project**

Add the include path `..\host_protocol` and each new `.c`/`.h` file exactly once. Reopen the XML or use `rg` to verify no duplicate `<FilePath>` entries.

- [ ] **Step 7: Build both firmware modes**

Build once with `HOST_DEBUG_MODE 1` and once with `0` using the existing Keil-supported build path. Expected: zero compile/link errors in both modes. If only Keil GUI is available, record the two build commands as a manual verification item rather than claiming success.

- [ ] **Step 8: Run all portable tests**

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Expected: all Qt and portable STM32 tests pass.

---

### Task 13: End-to-end simulator, documentation and final verification

**Files:**
- Create: `tests/support/FakeDevice.h`
- Create: `tests/support/FakeDevice.cpp`
- Create: `tests/test_end_to_end.cpp`
- Create: `README.md`
- Modify: `CMakeLists.txt`
- Modify: `docs/protocol.md`

**Interfaces:**
- Consumes: the complete Qt protocol/device stack.
- Produces: an in-memory fake device that answers HELLO, parameter, telemetry, unlock, action and emergency commands with the same frames as STM32.

- [ ] **Step 1: Write an end-to-end failing test**

Drive this sequence through `ProtocolClient` and `FakeDevice`:

```text
HELLO -> debug capability confirmed
GET_PARAM_GROUP(0x10) -> five PID profiles returned
SET_PARAM_GROUP(valid) -> ACK and readback changed
SET_PARAM_GROUP(out of range) -> error and readback unchanged
SET_TELEMETRY -> two IMU events received
TEST_ACTION before unlock -> rejected
TEST_UNLOCK then TEST_ACTION -> accepted
EMERGENCY_STOP -> emergency status event and later action rejected
disconnect -> pending request failed and UI locked
```

- [ ] **Step 2: Implement FakeDevice minimally**

Use the production `FrameParser` and `encodeFrame`; do not duplicate wire parsing. Store parameters in a `QHash<quint16, QVariant>`, validate with `ParameterCatalog`, and emit deterministic IMU samples.

- [ ] **Step 3: Write README build and use instructions**

Include Windows prerequisites, configuration/build/test commands, application launch command, COM selection, HC-05 9600 setting, USART3 115200 setting, `HOST_DEBUG_MODE` usage, safe first-run procedure, and the fact that RAM values reset after STM32 restart.

- [ ] **Step 4: Run the complete software verification**

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Expected: configure succeeds, application and all test executables build, and CTest reports 100% passing.

- [ ] **Step 5: Perform static scope checks**

```powershell
rg -n "FLASH|EEPROM|HAL_FLASH" src ..\yyb_stm32\host_protocol
rg -n "HOST_DEBUG_MODE" ..\yyb_stm32\host_protocol ..\yyb_stm32\USER\main.c
rg -n "AA|55|CRC-16/CCITT-FALSE" docs src ..\yyb_stm32\host_protocol
```

Expected: no Flash-writing implementation; debug-mode references exist in config and main; protocol constants/documentation exist on both sides.

- [ ] **Step 6: Commit final tracked files**

```powershell
git add CMakeLists.txt README.md docs/protocol.md tests/support tests/test_end_to_end.cpp
git commit -m "test: verify upper computer workflows end to end"
```

- [ ] **Step 7: Prepare the hardware acceptance checklist without claiming it passed**

Report software test/build evidence and separately list unchecked real-hardware items: PB10/PB11 electrical availability, USART3 handshake, HC-05 handshake, HWT101 streaming/calibration, RAM reset behavior, motor/arm limits, watchdog stop, emergency stop and both firmware-mode builds. Only mark an item passed after observing it on the actual robot.
