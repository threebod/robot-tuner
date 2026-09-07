# Task 8 实现报告：IMU、PID 与状态遥测显示

## 假设与范围

- 仅修改 `upper_computer`；未修改 `yyb_stm32`。
- 视觉页继续使用现有“视觉（预留）”占位页；本任务不实现急停、解锁、固件升级或动作测试。
- 复用现有“HWT101”导航入口承载 `ImuPage`，保持既有 7 页导航契约不变。
- 状态遥测按协议固定 5 字节解码；`GET_STATUS` 响应按协议固定 6 字节解码。
- 9600 波特率的请求频率控件限制为 1–20 Hz，其他已支持链路限制为 1–50 Hz；设备返回的实际周期单独显示，避免丢失设备实际值。

## TDD 证据

### RED

先新增固定容量 plot 测试和 CTest 目标，生产文件尚未创建时重新配置构建：

```text
CMake Error at CMakeLists.txt:151 (add_executable):
  Cannot find source file:

    src/widgets/TelemetryPlot.cpp
```

### GREEN

- 新增 `TelemetryPlot` 环形缓冲区，容量固定为 1000（可配置），保留时间戳和多通道值；绘制坐标轴、零线、图例和多通道折线，并以合并定时器限制重绘频率不高于 25 FPS。
- 新增 `ImuPage`：显示 9 个 IMU 实时值及 `g`、`°/s`、`°` 单位；加速度、角速度和姿态角分别绑定三张 plot；显示越量程/非有限值/设备错误；提供 1–20/1–50 Hz 周期控件和带确认的 IMU 校准状态。
- 新增 `OverviewPage`：绑定协议/固件版本、链路、握手响应延迟、设备模式、调试锁定/解锁、急停、最新姿态、PID 遥测及设备错误码/错误信息。
- `DeviceClient` 补充状态遥测/`GET_STATUS` 解码、`SET_TELEMETRY` 实际周期响应、`IMU_CALIBRATE` 状态响应信号；MainWindow 将 IMU、PID、状态和错误信号绑定到页面。
- UI smoke 覆盖导航、页面锁定、单位、异常文本、9600 频率上限、设备实际频率显示、实时值入图、校准状态和总览状态字段。
- DeviceClient 测试覆盖状态响应/事件、实际周期及校准响应解码。

## 审查修复第 1 轮

### RED

- `test_protocol_client` 先监听尚不存在的 `requestLatencyChanged`，旧实现编译失败。
- UI smoke 先在握手后检查默认订阅，旧实现输出：

```text
successful HELLO did not subscribe telemetry automatically
```

### GREEN

- 握手成功后由 MainWindow 自动发送 `SET_TELEMETRY(mask=0x07, period=100 ms)`，覆盖 IMU/PID/STATUS，默认 10 Hz。
- ProtocolClient 为每个 pending request 记录单调计时，并在响应、超时或断开失败时发出最近请求延迟；总览页改为绑定该信号，不再只记录 HELLO。
- 已握手设备发生普通请求失败时，DeviceClient 清除握手状态；MainWindow 锁定参数、遥测和动作控件，并显示“未连接/不可用”，直到重新握手。

## 验证

使用已有 `E:\Qt\6.11.1\mingw_64` Qt 6.11.1、CMake、Ninja 和 MinGW 13.1.0；仅为当前构建进程临时补充工具链 PATH，未安装依赖。

```text
cmake --build build -- -j1                          PASS
ctest --test-dir build -C Debug --output-on-failure  PASS
8/8 tests passed, 0 tests failed
```

`build/`、`build-task5-*` 为已有/生成的未跟踪目录，未加入提交。
