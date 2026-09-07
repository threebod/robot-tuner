# Task 6 实现报告：串口连接与连接状态管理

## 假设与范围

- USB-UART 与 HC-05 均由同一个 `SerialController` 和一个 `QSerialPort` 管理；端口枚举来自 `QSerialPortInfo::availablePorts()`，因此任一时刻只允许一条串口连接。
- 波特率由连接栏提供 `9600`（HC-05 默认链路）和 `115200`（USB-UART/USART3）两个选项；打开时统一配置 8 数据位、无校验、1 停止位、无流控。
- 仅修改上位机串口/连接管理与对应测试；未修改 `yyb_stm32`、未来参数页/动作页实现，也未加入测试注入 API。不存在端口的错误路径按预检裁定直接调用 `SerialController::open()` 验证。

## TDD 证据

### RED

先加入 `tests/test_serial_controller.cpp`、断开态 smoke 断言和 CMake 目标，生产串口目录尚未创建；使用已有 Qt 工具链配置时按预期在生成阶段失败：

```text
Cannot find source file:
  src/serial/SerialController.cpp
```

随后补充断开清理回归断言，在初版实现上运行定向测试，按预期失败：

```text
closing the controller did not clear protocol requests
0% tests passed, 1 tests failed out of 1
```

### GREEN

实现最小串口传输和连接状态后，定向测试通过。`SerialController`：

- 枚举可用端口，配置 8-N-1/无流控并打开 `ReadWrite`；已有连接再次打开其他端口会被拒绝，保证 USB-UART 与 HC-05 互斥。
- `readyRead` 发出 `bytesReceived`，并把同一批字节送入 `ProtocolClient::ingestBytes`；监听 `ProtocolClient::bytesReady` 并写入串口。
- `ResourceError` 关闭底层端口、清理 `ProtocolClient::clearPending()`、发出 `closed()` 和一次用户可见 `serialError`；显式 `close()` 也清理 pending，请求未连接时不会伪造 `closed()`。
- 不存在的 COM 端口由测试直接调用 `open("COM_DOES_NOT_EXIST_6", 115200)`，确认返回失败、错误信号仅一次且失败后不可写。

MainWindow 仅补连接管理：串口/波特率/刷新/连接/状态控件具备任务要求的 objectName；连接成功后锁定选择器并显示 `串口已连接，等待设备握手`，调用 `DeviceClient::hello()`；握手成功启用设备页和急停；断开恢复选择器并禁用设备页。

## 验证

使用现有 `E:\Qt\6.11.1\mingw_64` Qt 6.11.1、CMake、Ninja 和 MinGW 13.1.0，未安装或下载依赖。构建时显式加入已有 Qt/MinGW/Ninja `bin` 到 PATH：

```text
E:\Qt\Tools\Ninja\ninja.exe -C build -j1                         PASS
E:\Qt\Tools\CMake_64\ctest.exe --test-dir build -R test_serial_controller --output-on-failure  PASS
E:\Qt\Tools\CMake_64\ctest.exe --test-dir build -R test_ui_smoke --output-on-failure          PASS
E:\Qt\Tools\CMake_64\ctest.exe --test-dir build --output-on-failure                         7/7 PASS
```

最终结果：`100% tests passed, 0 tests failed out of 7`。`build/`、`build-task5-*` 等构建目录均为未跟踪生成物，未加入提交。
