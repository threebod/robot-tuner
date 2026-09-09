# 三舵机滑块与动作日志验收报告

日期：2026-09-09

## 范围与假设

- 本次改动范围为 Qt 上位机 `upper_computer/`，以及用于协议和硬件映射对照的 `yyb_stm32/` 外部参考树。
- `yyb_stm32/` 不是当前 Git 仓库；修改前的 6 个外部文件已保存到
  `E:\Download\project\gongchuang\.superpowers\sdd\2026-09-09-servo-slider-action-log\external-baseline\`。
- 未安装依赖，未声称完成 Keil 编译或真实硬件验证。

## 已完成内容

- USB 与 HC-05 链路统一为 115200；IMU 遥测上限统一为 50 Hz。
- 修正麦轮方向映射：前进 `vy=80`、后退 `vy=-80`、左移 `vx=-80`、右移 `vx=80`。
- 增加安全的通用舵机动作协议 `0x22`：舵机 2/3 范围 0–270°，舵机 4 范围 0–360°；动作仍受解锁、急停和看门狗条件约束，单次发送不重试。
- 动作测试页增加舵机 2/3/4 的滑块、数值框、发送按钮和动作日志；发送前保留确认框，取消不会发包。
- `yyb_stm32` 参考实现增加舵机 2/3/4 的非阻塞调试适配：2→`zhuashou`、3→`wukuaipingtai`、4→`yuantai`，并使用当前角度计算运动时长。
- 补充上位机协议、UI、STM32 适配静态检查和编译测试。

## Git 提交

1. `9b8d716 fix: align tuning links and mecanum controls`
2. `1e6c885 feat: add safe three-servo action protocol`
3. `2de9d3e test: cover three-servo STM32 adapter`
4. `c442fbc feat: add three-servo sliders and target log`
5. 本报告提交：`docs: record servo slider verification`

## 验证结果

### TDD 红绿验证

- 链路和麦轮映射：先由 UI、STM32 协议和静态检查暴露旧波特率/遥测限制/方向映射，修正后 3 个聚焦测试通过。
- 通用舵机协议：先由 `test_device_client` 和 STM32 协议测试暴露缺少 API、协议常量和回调，补齐后两者通过；同时验证非法舵机号、角度越界、未解锁和单次发送行为。
- STM32 适配：先由静态检查和宿主编译暴露缺少 PWM 接口/回调实现，补齐后 `test_task12_host_static` 与 `test_task12_host_compile` 通过。
- UI：先由 UI 测试暴露缺少 `servoRequested` 信号，补齐后 `test_ui_smoke` 通过；测试覆盖控件范围、滑块/数值框同步、确认发送和取消不发送。

### 全量构建与测试

- 新建 Debug 构建目录 `upper_computer/build-servo-sliders/`，构建结果：127/127 步骤成功。
- Debug CTest：14/14 通过，包含 UI、设备协议、串口、遥测曲线、STM32 协议、静态检查和宿主编译测试。
- 新建 Release 构建目录 `upper_computer/build-servo-sliders-release/`，`robot_tuner` 构建结果：20/20 步骤成功。
- `git diff --check` 通过。

使用的主要命令：

```powershell
E:\Qt\Tools\CMake_64\bin\cmake.exe -S . -B build-servo-sliders -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=E:\Qt\6.11.1\mingw_64 -DCMAKE_C_COMPILER=E:\Qt\Tools\mingw1310_64\bin\gcc.exe -DCMAKE_CXX_COMPILER=E:\Qt\Tools\mingw1310_64\bin\g++.exe -DCMAKE_MAKE_PROGRAM=E:\Qt\Tools\Ninja\ninja.exe
E:\Qt\Tools\CMake_64\bin\cmake.exe --build build-servo-sliders --parallel 1
E:\Qt\Tools\CMake_64\bin\ctest.exe --test-dir build-servo-sliders -C Debug --output-on-failure
E:\Qt\Tools\CMake_64\bin\cmake.exe --build build-servo-sliders-release --target robot_tuner --parallel 1
```

### Windows 发布包

- 发布目录：`E:\Download\project\gongchuang\upper_computer\dist\robot_tuner\`
- 发布目录包含 23 个文件，总大小约 63,780,410 bytes。
- 主程序：`robot_tuner.exe`，760,242 bytes。
- ZIP：`E:\Download\project\gongchuang\upper_computer\dist\robot_tuner-windows-x64.zip`，25,746,285 bytes。
- 仅重建了 `dist\robot_tuner` 和对应 ZIP；已有 `dist\robot_tuner-debug` 等其他产物未删除。

## 外部参考树变更核对

基线与当前文件逐字节 SHA-256 比对均显示已按计划变化：

- `yyb_stm32/USER/main.c`：HC-05 的 UART5 从 9600 改为 115200；UART6 保持原设置。
- `yyb_stm32/host_protocol/host_commands.c`：遥测周期改为通用 20–1000 ms 校验并统一 50 Hz 上限；新增 `0x22` 舵机动作校验、解锁检查和回调分发。
- `yyb_stm32/host_protocol/host_commands.h`：新增 `HOST_ACTION_SERVO` 和舵机回调字段。
- `yyb_stm32/host_protocol/host_debug.c`：增加舵机调试回调，按当前角度和目标角度计算非阻塞运动时长。
- `yyb_stm32/Hardware/PWM.h`：声明舵机当前角度读取和目标角度设置接口。
- `yyb_stm32/Hardware/PWM.c`：实现通道 2/3/4 映射和非阻塞目标设置。

由于这些文件属于外部非 Git 参考树，本次只完成源码级静态检查和上位机/宿主协议验证，没有把它们伪装成已完成的 Keil 固件构建。

## 上电前人工验证清单

在接入真实底盘和舵机前仍需按安全流程完成：

1. 使用 Keil 分别检查 `HOST_DEBUG_MODE=1` 和 `HOST_DEBUG_MODE=0` 配置。
2. 实机核对 USB、HC-05 的 115200 握手、20 ms 遥测、断线和看门狗行为。
3. 抬高底盘后核对麦轮方向、急停、机械限位和舵机 2/3/4 的物理通道、方向、角度范围及供电。
4. 完成台架检查前不要启用底盘联动或完整比赛流程；任何异常先断电并保持安全锁定。
