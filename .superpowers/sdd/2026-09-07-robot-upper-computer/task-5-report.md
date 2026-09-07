# Task 5 实现报告：参数目录与 typed device service

## 假设与范围

- 仅修改 `upper_computer` 上位机仓库；未修改 `../yyb_stm32` sibling firmware。
- 沿用 Task 4 的 `ProtocolClient` 请求/响应/事件信号；不增加串口连接、UI 或测试专用生产 API。
- 参数值只写入设备 RAM，协议中的浮点值按 IEEE-754 `float32` 小端序编码。

## TDD 证据

先加入 `tests/test_parameter_catalog.cpp`、`tests/test_device_client.cpp` 和 CMake 测试目标，
在生产目录尚未创建 device 实现时执行：

```text
cmake -S . -B build-task5-red -G Ninja ...
cmake --build build-task5-red --target test_parameter_catalog test_device_client --verbose
```

按预期在 CMake 生成阶段 RED：

```text
Cannot find source file: src/device/ParameterCatalog.cpp
Cannot find source file: src/device/DeviceClient.cpp
```

随后实现目录、类型和协议文档，保留同一批测试进入 GREEN。

## 实现内容

- `docs/protocol.md`：固定帧标志、命令、错误码、payload、值类型、动作变体、参数 ID/组/范围和遥测换算。
- `ParameterCatalog`：25 个 PID 参数（5 个完整 profile）及底盘、机构、IMU 参数；提供 `find`、`group` 和类型/范围校验。
- `TelemetryTypes`：`ParameterValue`、`DeviceInfo`、`ImuSample`、`PidSample` typed 数据模型。
- `DeviceClient`：HELLO、参数组读写、遥测订阅、IMU 校准；参数写入前本地整组校验；解码参数响应、IMU/PID 遥测和设备错误；错误消息包含中文描述和十六进制错误码，并通过 `terminalLog` 保留数值码。
- 测试覆盖 GET/SET payload、小端 float 编码、越界拒绝、IMU 三类换算和参数固定目录。

## 验证

使用已有 `E:\Qt\6.11.1\mingw_64` Qt kit、CMake、Ninja 和 MinGW（未安装依赖）：

```text
cmake --build build-task5-green --config Debug                         PASS
ctest --test-dir build-task5-green -C Debug -R "test_parameter_catalog|test_device_client" --output-on-failure  2/2 PASS
ctest --test-dir build-task5-green -C Debug --output-on-failure       6/6 PASS
100% tests passed, 0 tests failed
```

工作树中已有的 `build/` 及本次验证用 `build-task5-*` 均为未跟踪构建产物，未加入提交。
