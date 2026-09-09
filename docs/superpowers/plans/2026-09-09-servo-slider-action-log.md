# Three-Servo Slider and Action Log Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make both tuning links use 115200 baud, align the four mecanum translation buttons with the verified jog example, and add safe slider-based target control plus in-memory target logging for servos 2–4.

**Architecture:** Keep the existing framed `TEST_ACTION` transport and safety state machine. Add one `HOST_ACTION_SERVO` payload shared by Qt and the portable STM32 command layer, then adapt that validated action to the existing TIM2 CH2–CH4 PWM objects in `host_debug`. The UI selects an angle with synchronized slider/spin controls and sends only on an explicit confirmed button click.

**Tech Stack:** C++17, Qt 6 Widgets/SerialPort, CMake/CTest, portable C99 host tests, STM32F407 Standard Peripheral Library, Keil project reference firmware.

**Spec:** `docs/superpowers/specs/2026-09-09-servo-slider-action-log-design.md`

## Global Constraints

- USART3 and UART5/HC-05 are both 115200 8N1; Qt permits only one active COM connection.
- Servo IDs are exactly 2, 3, and 4; IDs 2/3 accept 0–270°, ID 4 accepts 0–360°.
- A slider changes only the selected target; serial transmission occurs only after the send button and confirmation.
- Servo actions require HELLO, the existing 30-second action unlock, and a non-emergency state.
- `TEST_ACTION` remains non-retrying because it is not idempotent.
- The action list records only `舵机 <id> -> <angle>°` for confirmed send attempts; it is not completion feedback, persistence, or replay.
- Mecanum buttons remain velocity-mode timed jogs; only their `vx/vy` mapping changes to match `test/mecanum_jog` motor directions.
- Do not install dependencies. Use the existing `E:\Qt` toolchain through explicit paths.
- `upper_computer/` is the Git repository. `../yyb_stm32/` is a non-Git reference tree; capture a baseline and report its changes separately.
- Do not modify the MaixCAM/K230 placeholder or `test/mecanum_jog/`.
- Keil and real-hardware verification remain manual and must not be claimed from host tests.

---

## File Map

- `src/pages/ActionTestPage.h/.cpp`: mecanum button vectors, three slider rows, confirmations, target log, and `servoRequested` signal.
- `src/app/MainWindow.cpp`: fixed 115200 choice and UI-to-DeviceClient servo wiring.
- `src/pages/ImuPage.cpp`: remove the obsolete 9600-baud 20 Hz UI cap.
- `src/device/DeviceClient.h/.cpp`: validate and encode the generic servo action.
- `tests/test_ui_smoke.cpp`: UI ranges, synchronization, direction signals, send log, and 115200 assertions.
- `tests/test_device_client.cpp`: generic servo payload and host-side range/safety validation.
- `tests/test_stm32_protocol.c`: portable STM32 command validation and callback behavior.
- `tests/test_task12_host_compile.c`, `tests/stubs/task12/PWM.h`, `tests/task12_static_test.cmake`: hardware-adapter compile/static contract.
- `../yyb_stm32/USER/main.c`: initialize UART5 at 115200.
- `../yyb_stm32/host_protocol/host_commands.h/.c`: define and dispatch `HOST_ACTION_SERVO`.
- `../yyb_stm32/host_protocol/host_debug.c`: connect the portable callback to the PWM adapter.
- `../yyb_stm32/Hardware/PWM.h/.c`: non-blocking, direct debug target API for TIM2 channels 2–4.
- `README.md`, `docs/protocol.md`: correct baud/rate documentation and define the new action payload.

### Task 1: Correct Link Speed and Mecanum Button Mapping

**Files:**
- Modify: `tests/test_ui_smoke.cpp`
- Modify: `tests/test_stm32_protocol.c`
- Modify: `tests/task12_static_test.cmake`
- Modify: `src/app/MainWindow.cpp:53-57`
- Modify: `src/pages/ImuPage.cpp:307-313`
- Modify: `src/pages/ActionTestPage.cpp:97-104`
- Modify: `../yyb_stm32/USER/main.c:75`
- Modify: `../yyb_stm32/host_protocol/host_commands.c:469-494`
- Modify: `README.md:46-47`
- Modify: `docs/protocol.md:60-70`

**Interfaces:**
- Consumes: existing `ActionTestPage::chassisRequested(qint32, qint32, qint32, qint32)` and `SET_TELEMETRY` response payload.
- Produces: a single 115200 Qt baud choice; corrected vectors `forward=(0,80,0)`, `back=(0,-80,0)`, `left=(-80,0,0)`, `right=(80,0,0)`; 1–50 Hz telemetry on both links.

- [ ] **Step 1: Capture the external firmware baseline**

Create `.superpowers/sdd/2026-09-09-servo-slider-action-log/external-baseline/` and copy only these current files into it before changing them:

```powershell
Copy-Item ..\yyb_stm32\USER\main.c .superpowers\sdd\2026-09-09-servo-slider-action-log\external-baseline\main.c
Copy-Item ..\yyb_stm32\host_protocol\host_commands.c .superpowers\sdd\2026-09-09-servo-slider-action-log\external-baseline\host_commands.c
Copy-Item ..\yyb_stm32\host_protocol\host_commands.h .superpowers\sdd\2026-09-09-servo-slider-action-log\external-baseline\host_commands.h
Copy-Item ..\yyb_stm32\host_protocol\host_debug.c .superpowers\sdd\2026-09-09-servo-slider-action-log\external-baseline\host_debug.c
Copy-Item ..\yyb_stm32\Hardware\PWM.c .superpowers\sdd\2026-09-09-servo-slider-action-log\external-baseline\PWM.c
Copy-Item ..\yyb_stm32\Hardware\PWM.h .superpowers\sdd\2026-09-09-servo-slider-action-log\external-baseline\PWM.h
```

- [ ] **Step 2: Write failing UI and direction tests**

In `tests/test_ui_smoke.cpp`, require one baud entry and capture the direction signals from a standalone unlocked `ActionTestPage`. Use a zero-delay timer to accept the existing modal confirmation:

```cpp
void acceptNextConfirmation() {
    QTimer::singleShot(0, [] {
        auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        if (box != nullptr) {
            box->done(QMessageBox::Yes);
        }
    });
}

// MainWindow assertion
require(baudCombo->count() == 1 && baudCombo->currentData().toInt() == 115200,
        "tuning link is not fixed to 115200 baud");

// Standalone page assertions after setConnected(true) and
// setTestActionsEnabled(true, 30000).
// Click each named button after acceptNextConfirmation() and require:
// forward (0, 80, 0), back (0, -80, 0), left (-80, 0, 0), right (80, 0, 0).
```

In `tests/test_stm32_protocol.c`, set IMU parameter `0x4000` to 50 Hz, send `SET_TELEMETRY` from `HOST_LINK_BLUETOOTH`, and require returned period 20 ms.

In `tests/task12_static_test.cmake`, add exact source checks:

```cmake
set(main_c "${firmware_dir}/USER/main.c")
require_text("${main_c}" "uart5_init(115200);")
```

- [ ] **Step 3: Run focused tests and verify RED**

Run:

```powershell
E:\Qt\Tools\CMake_64\bin\cmake.exe --build build-final-verify --target test_ui_smoke test_stm32_protocol
E:\Qt\Tools\CMake_64\bin\ctest.exe --test-dir build-final-verify -R "test_ui_smoke|test_stm32_protocol|test_task12_host_static" --output-on-failure
```

Expected: UI fails on the two baud entries and old direction vectors; STM32/static tests fail on the Bluetooth 20 Hz cap and `uart5_init(9600)`.

- [ ] **Step 4: Implement the minimal transport and mapping changes**

Use only 115200 in `MainWindow`:

```cpp
baudCombo_->addItem(QStringLiteral("115200"), 115200);
```

Set `ImuPage::updateRateRange()` to `maximumRate_ = 50;`. In `ActionTestPage`, use:

```cpp
addDirection(QStringLiteral("前进"), QStringLiteral("chassisForwardButton"),
             0, 80, 0);
addDirection(QStringLiteral("后退"), QStringLiteral("chassisBackwardButton"),
             0, -80, 0);
addDirection(QStringLiteral("左移"), QStringLiteral("chassisLeftButton"),
             -80, 0, 0);
addDirection(QStringLiteral("右移"), QStringLiteral("chassisRightButton"),
             80, 0, 0);
```

Change `USER/main.c` to `uart5_init(115200);`. In `host_commands.c`, accept telemetry periods down to 20 ms on both links and cap configured rate at 50 Hz without a Bluetooth-only 20 Hz branch.

Update README and protocol text so both USB and HC-05 state 115200 and 1–50 Hz.

- [ ] **Step 5: Run focused tests and verify GREEN**

Run the Step 3 commands again. Expected: all selected tests pass.

- [ ] **Step 6: Commit the tracked half**

```powershell
git add src/app/MainWindow.cpp src/pages/ImuPage.cpp src/pages/ActionTestPage.cpp tests/test_ui_smoke.cpp tests/test_stm32_protocol.c tests/task12_static_test.cmake README.md docs/protocol.md
git commit -m "fix: align tuning links and mecanum controls"
```

Record `../yyb_stm32/USER/main.c` and `host_commands.c` as external non-Git changes in the checkpoint.

### Task 2: Add the Safe Generic Servo Action

**Files:**
- Modify: `tests/test_device_client.cpp`
- Modify: `tests/test_stm32_protocol.c`
- Modify: `src/device/DeviceClient.h`
- Modify: `src/device/DeviceClient.cpp`
- Modify: `../yyb_stm32/host_protocol/host_commands.h`
- Modify: `../yyb_stm32/host_protocol/host_commands.c`
- Modify: `docs/protocol.md`

**Interfaces:**
- Consumes: existing `DeviceClient::sendTestAction(const QByteArray &)`, `HostSafety_CanAct(nowMs)`, and `HostCommandCallbacks`.
- Produces: `bool DeviceClient::testServo(qint32 servoId, qint32 targetAngleDegrees)`; `HOST_ACTION_SERVO 0x22u`; callback `void (*servo)(uint8_t servo_id, uint16_t target_angle_degrees)`.

- [ ] **Step 1: Write the failing Qt payload tests**

Extend the already-unlocked section of `tests/test_device_client.cpp`:

```cpp
require(actionDevice.testServo(2, 270), "servo 2 maximum was rejected");
require(capturedFrame(actionRequests.back()).payload ==
            QByteArray::fromHex("22 02 0e 01"),
        "servo 2 payload changed");
require(actionDevice.testServo(4, 360), "servo 4 maximum was rejected");
require(capturedFrame(actionRequests.back()).payload ==
            QByteArray::fromHex("22 04 68 01"),
        "servo 4 payload changed");
require(!actionDevice.testServo(1, 90), "invalid servo ID was accepted");
require(!actionDevice.testServo(3, 271), "servo 3 overtravel was accepted");
require(!actionDevice.testServo(4, 361), "servo 4 overtravel was accepted");
```

Reuse the existing lost-ACK timeout check to require exactly one emitted `TEST_ACTION` frame for `testServo`.

- [ ] **Step 2: Write the failing portable firmware tests**

Add to `tests/test_stm32_protocol.c`:

```c
static uint8_t fake_servo_id;
static uint16_t fake_servo_angle;
static unsigned int fake_servo_calls;

static void fake_servo(uint8_t id, uint16_t angle)
{
    fake_servo_id = id;
    fake_servo_angle = angle;
    ++fake_servo_calls;
}
```

Install it in `HostCommandCallbacks`, send payload `22 02 0E 01` after HELLO/unlock, and require one callback with `(2, 270)`. Require `HOST_ERROR_PARAM_RANGE` and no callback for IDs outside 2–4, ID 3 angle 271, and ID 4 angle 361. Require `HOST_ERROR_NOT_UNLOCKED` before unlock and `HOST_ERROR_EMERGENCY_LOCKED` after emergency stop.

- [ ] **Step 3: Run focused tests and verify RED**

Run:

```powershell
E:\Qt\Tools\CMake_64\bin\cmake.exe --build build-final-verify --target test_device_client test_stm32_protocol
E:\Qt\Tools\CMake_64\bin\ctest.exe --test-dir build-final-verify -R "test_device_client|test_stm32_protocol" --output-on-failure
```

Expected: compilation fails because `testServo`, `HOST_ACTION_SERVO`, and the callback member do not exist.

- [ ] **Step 4: Implement Qt validation and encoding**

Add the public method and encode exactly four payload bytes:

```cpp
bool DeviceClient::testServo(qint32 servoId, qint32 targetAngleDegrees) {
    if (!requireActionAccess()) {
        return false;
    }
    const qint32 maximum = servoId == 4 ? 360 : 270;
    if (servoId < 2 || servoId > 4 || targetAngleDegrees < 0 ||
        targetAngleDegrees > maximum) {
        return reject(QStringLiteral("舵机目标角度越界"), 0x07);
    }
    QByteArray payload;
    payload.reserve(4);
    payload.append(char(0x22));
    payload.append(static_cast<char>(servoId));
    appendU16(&payload, static_cast<quint16>(targetAngleDegrees));
    return sendTestAction(payload);
}
```

- [ ] **Step 5: Implement portable firmware validation and callback dispatch**

In `host_commands.h` add:

```c
#define HOST_ACTION_SERVO 0x22u
void (*servo)(uint8_t servo_id, uint16_t target_angle_degrees);
```

In `host_commands_handle_test_action`, require length 4, ID 2–4, and the per-ID maximum before `HostSafety_CanAct(nowMs)`, then invoke the callback once. Do not add sequence deduplication or retries; the Qt protocol client already sends physical actions once.

Update `docs/protocol.md` with `action:u8 servo_id:u8 target_angle_deg:u16` and the exact ranges.

- [ ] **Step 6: Run focused tests and verify GREEN**

Run Step 3 again. Expected: both tests pass.

- [ ] **Step 7: Commit the tracked half**

```powershell
git add src/device/DeviceClient.h src/device/DeviceClient.cpp tests/test_device_client.cpp tests/test_stm32_protocol.c docs/protocol.md
git commit -m "feat: add safe three-servo action protocol"
```

Record external `host_commands.h/.c` changes separately.

### Task 3: Connect Servo Actions to TIM2 CH2–CH4

**Files:**
- Modify: `tests/stubs/task12/PWM.h`
- Modify: `tests/test_task12_host_compile.c`
- Modify: `tests/task12_static_test.cmake`
- Modify: `../yyb_stm32/Hardware/PWM.h`
- Modify: `../yyb_stm32/Hardware/PWM.c`
- Modify: `../yyb_stm32/host_protocol/host_debug.c`

**Interfaces:**
- Consumes: Task 2 callback `servo(uint8_t, uint16_t)` and existing `Servo_t` objects `zhuashou`, `wukuaipingtai`, `yuantai`.
- Produces: `float PWM_GetDebugServoCurrentAngle(uint8_t channel)` and `uint8_t PWM_SetDebugServoAngle(uint8_t channel, float target, float durationMs)`; fixed 30 degree/s non-blocking movement through the existing 20 ms `servo_update` ISR.

- [ ] **Step 1: Write failing adapter contracts**

Add declarations to the test stub first but do not add definitions to `tests/test_task12_host_compile.c`:

```c
float PWM_GetDebugServoCurrentAngle(uint8_t channel);
uint8_t PWM_SetDebugServoAngle(uint8_t channel, float target, float duration_ms);
```

Extend `tests/task12_static_test.cmake`:

```cmake
require_text("${pwm_h}" "PWM_GetDebugServoCurrentAngle")
require_text("${pwm_h}" "PWM_SetDebugServoAngle")
require_text("${debug}" "host_debug_servo")
require_text("${debug}" "command_callbacks.servo = host_debug_servo")
```

- [ ] **Step 2: Run adapter tests and verify RED**

Run:

```powershell
E:\Qt\Tools\CMake_64\bin\cmake.exe --build build-final-verify --target test_task12_host_compile
E:\Qt\Tools\CMake_64\bin\ctest.exe --test-dir build-final-verify -R "test_task12_host_compile|test_task12_host_static" --output-on-failure
```

Expected: static checks fail because production PWM/debug symbols are absent; after `host_debug` starts calling them, the host stub link also fails until definitions are added.

- [ ] **Step 3: Implement the direct, non-blocking PWM adapter**

In `PWM.c`, select the existing object without applying the semantic offsets used by `set_zhuashou_Angle` or `set_yuantai_Angle`:

```c
static Servo_t *pwm_debug_servo(uint8_t channel)
{
    if (channel == 2u) return &zhuashou;
    if (channel == 3u) return &wukuaipingtai;
    if (channel == 4u) return &yuantai;
    return (Servo_t *)0;
}

float PWM_GetDebugServoCurrentAngle(uint8_t channel)
{
    Servo_t *servo = pwm_debug_servo(channel);
    return servo != (Servo_t *)0 ? servo->current_angle : -1.0f;
}

uint8_t PWM_SetDebugServoAngle(uint8_t channel, float target, float duration_ms)
{
    Servo_t *servo = pwm_debug_servo(channel);
    if (servo == (Servo_t *)0 || duration_ms <= 0.0f) return 0u;
    servo->start_angle = servo->current_angle;
    servo->target_angle = target;
    servo->time = duration_ms;
    servo->t = 0;
    servo->moving_flag = 1;
    return 1u;
}
```

Keep `PWM_StopAll()` as the common stop path.

- [ ] **Step 4: Implement the host-debug callback**

Use the existing tested duration helper and the reference example's 30 degree/s:

```c
static void host_debug_servo(uint8_t servo_id, uint16_t target_angle)
{
    const float current = PWM_GetDebugServoCurrentAngle(servo_id);
    const uint32_t duration = HostRuntime_TurretDurationMs(
        current, (float)target_angle, 30.0f);
    if (duration != 0u) {
        PWM_SetDebugServoAngle(servo_id, (float)target_angle,
                               (float)duration);
    }
}
```

Assign `command_callbacks.servo = host_debug_servo;`. Add no motion calls to UART ISR code.

- [ ] **Step 5: Complete the host compile stub and verify GREEN**

Add inert definitions to `tests/test_task12_host_compile.c`:

```c
float PWM_GetDebugServoCurrentAngle(uint8_t channel)
{
    (void)channel;
    return 0.0f;
}

uint8_t PWM_SetDebugServoAngle(uint8_t channel, float target,
                               float duration_ms)
{
    (void)channel; (void)target; (void)duration_ms;
    return 1u;
}
```

Run Step 2 again. Expected: both adapter tests pass.

- [ ] **Step 6: Commit the tracked contracts**

```powershell
git add tests/stubs/task12/PWM.h tests/test_task12_host_compile.c tests/task12_static_test.cmake
git commit -m "test: cover three-servo STM32 adapter"
```

Record external `PWM.h/.c` and `host_debug.c` changes separately. Do not claim Keil compilation.

### Task 4: Add Three Slider Rows and In-Memory Target Log

**Files:**
- Modify: `tests/test_ui_smoke.cpp`
- Modify: `src/pages/ActionTestPage.h`
- Modify: `src/pages/ActionTestPage.cpp`
- Modify: `src/app/MainWindow.cpp`
- Modify: `README.md`

**Interfaces:**
- Consumes: Task 2 `DeviceClient::testServo(qint32, qint32)`.
- Produces: signal `servoRequested(qint32 servoId, qint32 targetAngleDegrees)` and object names `servo2Slider`/`servo2AngleSpinBox`/`servo2SendButton` through servo 4, plus `servoActionLog`.

- [ ] **Step 1: Write failing UI structure and synchronization tests**

In `tests/test_ui_smoke.cpp`, require:

```cpp
auto *servo2Slider = window.findChild<QSlider *>("servo2Slider");
auto *servo2Angle = window.findChild<QSpinBox *>("servo2AngleSpinBox");
auto *servo2Send = window.findChild<QPushButton *>("servo2SendButton");
auto *servo3Slider = window.findChild<QSlider *>("servo3Slider");
auto *servo3Angle = window.findChild<QSpinBox *>("servo3AngleSpinBox");
auto *servo3Send = window.findChild<QPushButton *>("servo3SendButton");
auto *servo4Slider = window.findChild<QSlider *>("servo4Slider");
auto *servo4Angle = window.findChild<QSpinBox *>("servo4AngleSpinBox");
auto *servo4Send = window.findChild<QPushButton *>("servo4SendButton");
auto *servoLog = window.findChild<QListWidget *>("servoActionLog");
```

Require maxima 270, 270, and 360. Set each slider and require its spin box follows; set each spin box and require its slider follows. Require all nine controls disabled before unlock and during emergency lock.

- [ ] **Step 2: Write the failing send/log behavior test**

Use a standalone connected/unlocked page, connect `servoRequested` to captured ID/angle, set servo 3 to 125, accept the confirmation dialog, click `servo3SendButton`, then require:

```cpp
require(sentServoId == 3 && sentServoAngle == 125,
        "servo 3 controls emitted the wrong target");
require(servoLog->count() == 1 &&
            servoLog->item(0)->text() == QStringLiteral("舵机 3 -> 125°"),
        "confirmed servo target was not recorded");
```

Cancel a second confirmation and require neither the signal nor list count changes.

- [ ] **Step 3: Run UI smoke and verify RED**

Run:

```powershell
E:\Qt\Tools\CMake_64\bin\cmake.exe --build build-final-verify --target test_ui_smoke
E:\Qt\Tools\CMake_64\bin\ctest.exe --test-dir build-final-verify -R test_ui_smoke --output-on-failure
```

Expected: compilation or assertions fail because the three slider rows, signal, and log do not exist.

- [ ] **Step 4: Implement the slider rows and target log**

Add `QSlider`, `QSpinBox`, and send button members for IDs 2–4 plus `QListWidget *servoActionLog_`. Create a small private helper to avoid repeating the row construction:

```cpp
void ActionTestPage::requestServo(qint32 servoId, QSpinBox *angleSpinBox) {
    const qint32 angle = angleSpinBox->value();
    if (!testActionsEnabled_ ||
        !confirmAction(QStringLiteral("舵机 %1 移动到 %2°")
                           .arg(servoId).arg(angle))) {
        return;
    }
    emit servoRequested(servoId, angle);
    servoActionLog_->addItem(
        QStringLiteral("舵机 %1 -> %2°").arg(servoId).arg(angle));
}
```

Synchronize slider and spin box with direct `valueChanged` connections. Append all sliders, spin boxes, and buttons to `actionWidgets_`; leave the log readable while disconnected. Do not send from `sliderMoved` or `valueChanged`.

- [ ] **Step 5: Wire the signal to DeviceClient**

In `MainWindow.cpp`:

```cpp
connect(actionPage_, &ActionTestPage::servoRequested, &device_,
        &DeviceClient::testServo);
```

Update README with the three angle ranges, explicit-send behavior, and the fact that the list records targets only.

- [ ] **Step 6: Run UI smoke and verify GREEN**

Run Step 3 again. Expected: test passes.

- [ ] **Step 7: Commit**

```powershell
git add src/pages/ActionTestPage.h src/pages/ActionTestPage.cpp src/app/MainWindow.cpp tests/test_ui_smoke.cpp README.md
git commit -m "feat: add three-servo sliders and target log"
```

### Task 5: Full Verification and Portable Release Refresh

**Files:**
- Verify: all tracked source and test files
- Generate only: `build-servo-sliders/`, `build-servo-sliders-release/`, `dist/robot_tuner/`, `dist/robot_tuner-windows-x64.zip`
- Report: `.superpowers/sdd/2026-09-09-servo-slider-action-log/final-report.md`

**Interfaces:**
- Consumes: Tasks 1–4 complete tree.
- Produces: fresh Debug build/test evidence, fresh portable Release package, and explicit manual hardware blockers.

- [ ] **Step 1: Run source and scope checks**

```powershell
git diff --check
rg -n "9600|20 Hz" README.md docs\protocol.md src ..\yyb_stm32\USER\main.c ..\yyb_stm32\host_protocol
rg -n "HOST_ACTION_SERVO|testServo|servoRequested|servoActionLog" src tests docs ..\yyb_stm32\host_protocol
```

Expected: no whitespace errors; any remaining `9600` is unrelated to UART5/HC-05; all new symbols are present in Qt, tests, docs, and firmware.

- [ ] **Step 2: Configure and build a fresh Debug tree**

Prepend `E:\Qt\Tools\mingw1310_64\bin`, `E:\Qt\6.11.1\mingw_64\bin`, CMake, and Ninja to PATH for every command. Run:

```powershell
E:\Qt\Tools\CMake_64\bin\cmake.exe -S . -B build-servo-sliders -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=E:\Qt\6.11.1\mingw_64 -DCMAKE_C_COMPILER=E:\Qt\Tools\mingw1310_64\bin\gcc.exe -DCMAKE_CXX_COMPILER=E:\Qt\Tools\mingw1310_64\bin\g++.exe -DCMAKE_MAKE_PROGRAM=E:\Qt\Tools\Ninja\ninja.exe
E:\Qt\Tools\CMake_64\bin\cmake.exe --build build-servo-sliders --parallel 1
```

Expected: all targets build with exit code 0.

- [ ] **Step 3: Run the complete test suite**

```powershell
E:\Qt\Tools\CMake_64\bin\ctest.exe --test-dir build-servo-sliders -C Debug --output-on-failure
```

Expected: 14/14 tests pass, or the updated discovered total passes with zero failures.

- [ ] **Step 4: Build and deploy the portable Release application**

```powershell
E:\Qt\Tools\CMake_64\bin\cmake.exe -S . -B build-servo-sliders-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=E:\Qt\6.11.1\mingw_64 -DCMAKE_C_COMPILER=E:\Qt\Tools\mingw1310_64\bin\gcc.exe -DCMAKE_CXX_COMPILER=E:\Qt\Tools\mingw1310_64\bin\g++.exe -DCMAKE_MAKE_PROGRAM=E:\Qt\Tools\Ninja\ninja.exe
E:\Qt\Tools\CMake_64\bin\cmake.exe --build build-servo-sliders-release --target robot_tuner --parallel 1
E:\Qt\6.11.1\mingw_64\bin\windeployqt.exe --release --compiler-runtime --no-translations --dir dist\robot_tuner build-servo-sliders-release\robot_tuner.exe
```

Copy the new executable into `dist/robot_tuner/` before deployment and rebuild `dist/robot_tuner-windows-x64.zip`. Do not commit generated build or distribution files.

- [ ] **Step 5: Compare and report external firmware changes**

Compare each baseline file from Task 1 to its exact current counterpart; do not diff the baseline directory against the whole firmware tree. Report changes to `main.c`, `host_commands.[ch]`, `host_debug.c`, and `PWM.[ch]`.

State explicitly that these remain unverified:

```text
Keil HOST_DEBUG_MODE=1 and HOST_DEBUG_MODE=0 builds;
HC-05 115200 communication;
servo 2/3/4 PWM range, direction, power and mechanical limits;
mecanum direction, watchdog and physical emergency stop on a raised chassis.
```

- [ ] **Step 6: Write final report and commit tracked reporting changes**

Write `.superpowers/sdd/2026-09-09-servo-slider-action-log/final-report.md` with RED/GREEN evidence, fresh build/test results, external file list, and manual blockers. If the report is tracked, commit it alone:

```powershell
git add .superpowers/sdd/2026-09-09-servo-slider-action-log/final-report.md
git commit -m "docs: record servo slider verification"
```

Do not claim completion if the fresh build or CTest command fails.
