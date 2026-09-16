# Mechanism Action Recorder Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a text-mode mechanism action editor that initializes estimated mechanism coordinates, records and tests actions, saves JSON, and exports `mecanum_jog` C action tables.

**Architecture:** Keep hardware state and command parsing in `test/mecanum_jog`, with pure conversion/table helpers in one C99 header. Reuse `MecanumJogClient` for text transport and add one Qt data model plus one page; the page owns the small sequential replay state machine.

**Tech Stack:** C99/ARMCC-compatible STM32 code, Qt 6 Widgets/Core, C++17, CMake/CTest, PowerShell host runtime tests.

**Spec:** `docs/superpowers/specs/2026-09-16-mechanism-action-recorder-design.md`

## Global Constraints

- Actual firmware is `test/mecanum_jog`; `yyb_stm32` is reference-only.
- No new dependencies and no STM32 Flash action upload.
- Mechanism position is command-integrated estimation, never sensor measurement.
- Motor 5 lift range is 0..1350 dmm at 80 pulses/mm; motor 6 horizontal range is -1220..650 dmm at 25.465 pulses/mm.
- Motor speed is 10..2000 RPM, acceleration 1..240, turret speed 10..300 in 0.1 degree/s.
- Stop, emergency stop, disconnect, heartbeat timeout, CAN fault, timeout, disable, or direct motor 5/6 jog invalidates the estimate.
- Do not run a complete Qt or Keil build; run only focused targets and tests.

---

### Task 1: Pure mechanism math and action table types

**Files:**
- Create: `../test/mecanum_jog/mechanism_action.h`
- Create: `../test/mecanum_jog/tests/test_mechanism_action.c`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces `MechanismPose`, `MechanismInitialState`, `MechanismActionType`, and `MechanismAction` C99 structs.
- Produces `mechanismPoseValid(const MechanismPose *)`, `mechanismHorizontalPulses(int16_t deltaDmm)`, `mechanismLiftPulses(int16_t deltaDmm)`, and `mechanismMoveDurationMs(...)`.
- Produces `MECH_INITIAL_STATE`, `MECH_POSE`, `MECH_GRIPPER_OPEN`, `MECH_GRIPPER_CLOSE`, `MECH_PLATFORM`, `MECH_SERVO`, and `MECH_WAIT` initializer macros used by exported files.

- [ ] **Step 1: Write the failing pure-C test**

Create assertions for all parameter boundaries, signed direction deltas, `480 dmm -> 1222` horizontal pulses, `250 dmm -> 2000` lift pulses, zero delta, conservative duration, and every action initializer macro.

- [ ] **Step 2: Register and run the test to verify RED**

Run:

```powershell
& 'E:\Qt\Tools\CMake_64\bin\cmake.exe' --build build --config Debug --target test_mecanum_mechanism_action
```

Expected: compile failure because `mechanism_action.h` does not exist.

- [ ] **Step 3: Implement the minimal header-only helpers**

Use fixed-width integers and named constants. Use integer rounding for pulse conversion; do not add floating-point parsing or a generic serialization layer.

- [ ] **Step 4: Build and run GREEN**

Run the target and:

```powershell
& 'E:\Qt\Tools\CMake_64\bin\ctest.exe' --test-dir build -C Debug --output-on-failure -R '^test_mecanum_mechanism_action$'
```

- [ ] **Step 5: Commit firmware math separately**

Commit in `test/mecanum_jog` as `feat: add mechanism action model`.

### Task 2: Firmware text mechanism controller

**Files:**
- Modify: `../test/mecanum_jog/main.c`
- Modify: `../test/mecanum_jog/tests/test_contract.ps1`
- Create: `../test/mecanum_jog/tests/test_mechanism_runtime.ps1`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes Task 1 `MechanismPose` validation and conversion helpers.
- Produces commands `mech init H L T`, `mech pose H L T HR HA LR LA TS`, and `mech status`.
- Produces `MECH INIT`, `MECH RUN`, `MECH POS`, `MECH DONE`, `MECH INVALID`, and `ERR MECH:` lines.
- Produces `mechanismActionStart(const MechanismAction *, uint16_t)` and
  `mechanismActionService()` for compile-time action arrays; callers explicitly register and start an exported table.

- [ ] **Step 1: Add failing parser/runtime coverage**

The PowerShell harness must extract the real parser/controller from `main.c` and assert:

```c
assert(processMechanismCommand("mech init 0 0 684"));
assert(mechanismValid && !mechanismRunning);
armed = 1;
assert(processMechanismCommand("mech pose -480 250 1350 30 50 30 50 130"));
assert(mechanismRunning && !armed);
```

It must also reject missing initialization and out-of-range values, verify motor IDs 5/6 and synchronized launch, emit position reports at about 200 ms, commit targets only on completion, invalidate on stop/fault/direct auxiliary jog, and advance a two-step compile-time action table without blocking.

- [ ] **Step 2: Run the runtime and contract tests to verify RED**

Run both PowerShell scripts directly. Expected: missing mechanism symbols/patterns.

- [ ] **Step 3: Implement the controller in `main.c`**

Increase `RX_LINE_SIZE` to 80. Add signed integer parsing only for mechanism coordinates. Keep one mechanism state struct and one `serviceMechanism()` function. Stage motors 5 and 6 with `snF=true`, start turret interpolation, then call `Emm_V5_Synchronous_motion(0x00)`. Reuse existing arm, CAN fault, heartbeat, servo, stop, and serial helpers.

Implement the action-table runner as an index plus deadline serviced from the existing main loop. It must dispatch through the same pose/servo helpers as text commands and expose no dynamic allocation or registry.

- [ ] **Step 4: Wire invalidation once at shared stop boundaries**

Invalidate from `stopAllMotors()` and the direct `motor 5|6` path so every existing stop/fault caller is covered without duplicating guards.

- [ ] **Step 5: Run focused firmware tests GREEN**

Run `test_contract.ps1`, `test_mechanism_runtime.ps1`, and existing route runtime to protect navigation behavior.

- [ ] **Step 6: Commit the controller**

Commit in `test/mecanum_jog` as `feat: add estimated mechanism pose control`.

### Task 3: Qt action data, JSON, and C export

**Files:**
- Create: `src/device/MechanismActionModel.h`
- Create: `src/device/MechanismActionModel.cpp`
- Create: `tests/test_mechanism_action_model.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces `MechanismPoseData`, `MechanismInitialData`, `MechanismStep`, and `MechanismSequence` value types.
- Produces `bool loadMechanismSequence(const QString &, MechanismSequence *, QString *)`.
- Produces `bool saveMechanismSequence(const QString &, const MechanismSequence &, QString *)` using `QSaveFile`.
- Produces `QString exportMechanismActionC(const MechanismSequence &, QString *)`.

- [ ] **Step 1: Write failing model tests**

Cover one valid round trip containing every step type, unknown version, missing field, every numeric range, preservation of the destination object on failed load, C identifier cleanup, initial-state export, and macro ordering.

- [ ] **Step 2: Build the model target to verify RED**

Expected: missing model header/types.

- [ ] **Step 3: Implement plain structs and three free functions**

Use `QJsonDocument`, `QJsonObject`, `QJsonArray`, `QFile`, and `QSaveFile`. Keep validation in one `validateMechanismSequence()` helper; do not add QObject inheritance or a repository layer.

- [ ] **Step 4: Run model tests GREEN**

Run only `test_mechanism_action_model`.

- [ ] **Step 5: Commit the model**

Commit in `upper_computer` as `feat: add mechanism action files`.

### Task 4: Text client mechanism state

**Files:**
- Modify: `src/device/MecanumJogClient.h`
- Modify: `src/device/MecanumJogClient.cpp`
- Modify: `tests/test_mecanum_jog_client.cpp`

**Interfaces:**
- Produces `initializeMechanism(const MechanismPoseData &)`, `moveMechanism(const MechanismPoseData &)`, and `requestMechanismStatus()`.
- Emits `mechanismValidityChanged(bool)`, `mechanismEstimateReceived(...)`, `mechanismCompleted(...)`, and `mechanismInvalidated(QString)`.

- [ ] **Step 1: Add failing client tests**

Assert exact scaled text commands, `arm\r\n` before `mech pose`, parsing of every `MECH` line, invalidation on stop/emergency/disconnect, and rejection of invalid values before bytes are sent.

- [ ] **Step 2: Run client test RED**

Expected: missing methods/signals.

- [ ] **Step 3: Implement minimal command formatting and parsing**

Reuse `sendCommand`, `sendArmedCommand`, `handleLine`, and the existing pending authorization timer. Raise the command length ceiling only to the longest valid `mech pose` command (79 characters including safety margin), while keeping ASCII and `!` rejection.

- [ ] **Step 4: Run client test GREEN**

Run only `test_mecanum_jog_client`.

- [ ] **Step 5: Commit client support**

Commit in `upper_computer` as `feat: add mechanism text protocol client`.

### Task 5: Independent action recorder page and replay

**Files:**
- Create: `src/pages/MechanismActionPage.h`
- Create: `src/pages/MechanismActionPage.cpp`
- Create: `tests/test_mechanism_action_page.cpp`
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`
- Modify: `tests/test_ui_smoke.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes Task 3 value types/files and Task 4 client signals through MainWindow connections.
- Emits `mechanismInitRequested(MechanismPoseData)`, `mechanismMoveRequested(MechanismPoseData)`, `textCommandRequested(QString)`, `mechanismStatusRequested()`, and `stopRequested()`.
- Public slots update connection/mode, estimate validity, pose completion, servo completion, and errors.

- [ ] **Step 1: Write the failing page test**

Find widgets by stable object names and cover initialization, record/edit/copy/delete/reorder, single-step replay, full replay ordering, step wait, return-to-initial ordering, invalid-state gating, stop, and high-speed confirmation bypass via an injectable confirmation callback used only by the test.

- [ ] **Step 2: Run page/UI tests RED**

Expected: missing page class/navigation item.

- [ ] **Step 3: Build the smallest useful page**

Use a `QTableWidget`, existing Qt spin boxes/combos/buttons, one `QTimer`, and a `QVector<MechanismStep>`. Keep replay state in the page; do not create a separate controller class. File dialogs call Task 3 functions.

- [ ] **Step 4: Wire MainWindow text mode**

Add “动作录入” to the page list. Enable it only for text mode. Connect page requests to `MecanumJogClient`; forward parsed mechanism and servo completion lines back to the page. Preserve existing binary `ActionTestPage` behavior.

- [ ] **Step 5: Run focused Qt tests GREEN**

Build and run `test_mechanism_action_page`, `test_mecanum_jog_client`, and `test_ui_smoke` only.

- [ ] **Step 6: Commit the page**

Commit in `upper_computer` as `feat: add mechanism action recorder page`.

### Task 6: Documentation and final focused verification

**Files:**
- Modify: `../test/mecanum_jog/README.md`
- Modify: `README.md`

**Interfaces:**
- Documents the exact commands, manual initialization, estimated-position warning, JSON/C workflow, safety invalidation, and low-speed first-test order.

- [ ] **Step 1: Update both READMEs**

Document examples for `mech init`, `mech pose`, `mech status`, and the action recorder workflow. State explicitly that `MECH DONE` is time-estimated and that no automatic homing exists.

- [ ] **Step 2: Run fresh focused verification**

Build only changed Qt/C test targets. Run mechanism model/page/client tests, UI smoke, mechanism C test/runtime/contract, and existing route/navigation regression tests. Run `git diff --check` in both repositories.

- [ ] **Step 3: Review staged files and commit docs**

Stage only named source/test/doc files; leave existing build directories untracked. Commit documentation separately in each repository if changed after the feature commits.

- [ ] **Step 4: Push only after final verification**

Push each repository's `main` to its already configured `origin/main` without force, using the user's existing explicit authorization for this task.
