# Task 9：安全动作、解锁与急停报告

## 范围

本次只实现上位机的 `TEST_UNLOCK`、IMU 校准请求、底盘/机构受控动作、
`STOP`/`EMERGENCY_STOP`/`CLEAR_EMERGENCY_STOP` 及其本地安全互锁。未安装依赖，
未修改 `yyb_stm32/` 或视觉代码。本修复轮同时补齐终端页面和视觉占位页。

## TDD RED

在 `tests/test_device_client.cpp` 中先加入动作 payload 和安全状态测试，覆盖：

- HELLO 前、动作未解锁和急停状态下的本地拒绝；
- `TEST_UNLOCK` 空请求与 30 秒 ACK；
- 底盘及水平/升降/云台/平台/夹爪动作的 payload 与边界；
- 急停立即锁定、解除急停必须在 ACK 后生效；
- 短时解锁到期和动作请求超时后的错误反馈与锁定。

使用现有工具执行：

```powershell
E:\Qt\Tools\CMake_64\bin\cmake.exe --build build --target test_device_client --config Debug -j 4
```

实现前构建按预期失败，编译器报告 `DeviceClient` 缺少 `unlockTests`、
`testChassis`、`testHorizontal` 等新接口。

## GREEN

实现内容：

- `DeviceClient` 对所有动作先检查握手、解锁时效和急停状态，再执行协议范围校验；
- 动作按 `docs/protocol.md` 编码为小端 `i16/u16/f32` payload；
- 解锁 ACK 仅接受协议规定的 30000 ms 时长并启动倒计时，断开、超时、急停或设备错误会撤销本地解锁；
- 急停在握手前也可发送，并在 TX 前立即锁定本地动作；解除急停只接受空 ACK 后清锁；
- 新增动作测试页：所有运动动作二次确认，停止按钮即时发送，动作控件只在解锁后启用；
- 顶部增加解除急停按钮，需确认且仅在设备 ACK 成功后恢复状态。

## 验证

```powershell
E:\Qt\Tools\CMake_64\bin\cmake.exe --build build --config Debug -j 4
E:\Qt\Tools\CMake_64\bin\ctest.exe --test-dir build -C Debug --output-on-failure
```

结果：8/8 CTest 通过，包括新增的 `test_device_client` 安全行为和
`test_ui_smoke` 动作页/急停互锁检查。构建产物为 Debug 配置；CTest 运行时仅将
已有 `E:\Qt\6.11.1\mingw_64\bin` 加入 PATH，没有安装或写入 C 盘。

## 修复轮 1：TDD RED

先加入以下回归断言，再触发构建/运行：

- `TEST_UNLOCK` 的旧序列在新 HELLO、急停后不得因延迟 ACK 重新解锁；只有最近一次
  请求且仍处于握手、非急停状态时才接受 ACK，解锁时长必须为固定的 30000 ms；
- BUSY、参数越界和异常动作 ACK 必须清理解锁；急停必须取消动作 pending，不能在
  STOP/EMERGENCY_STOP 后重试；
- ActionTestPage 的数值输入也必须随握手/解锁/急停互锁；Terminal/Vision 页面及
  Terminal 的显示、raw 发送、清空和暂停控件必须可发现。

RED 结果：`test_device_client` 运行失败于旧 TEST_UNLOCK ACK 重新启用动作，
`test_ui_smoke` 运行失败于 Terminal 控件缺失，`test_protocol_client` 编译失败于
待实现的 `ProtocolClient::cancelPending` 接口；失败均对应缺失行为。

## 修复轮 1：GREEN

- `ProtocolClient::cancelPending(command)` 静默移除指定命令的 pending，避免安全取消
  触发无意义的超时错误；HELLO/急停会取消旧解锁和动作请求；
- `DeviceClient` 记录最近一次解锁序列，并在握手、急停和 pending 状态仍有效时才接受
  ACK；ACK 时长严格校验为 30000 ms；BUSY、范围、异常 ACK 和请求失败均 fail-closed；
- 动作测试页将所有数值输入加入安全控件集合；重连时保留急停锁并同步到 UI，必须显式
  解除急停后才能恢复动作；
- 新增 `TerminalPage`（HEX/ASCII、时间戳、TX/RX、协议帧、清空、暂停、手动 raw 发送
  及绕过请求跟踪提示）和 `VisionPage`（K230/MaixCAM 占位、USART1 说明），接入
  `MainWindow` 与 CMake。

修复轮 GREEN 的单测目标和主程序均构建运行成功，随后执行全量 CTest。

## 修复轮 1：验证

```powershell
E:\Qt\Tools\CMake_64\bin\cmake.exe --build build --target robot_tuner -j 4
E:\Qt\Tools\CMake_64\bin\ctest.exe --test-dir build -C Debug --output-on-failure
```

结果：全量 8/8 CTest 通过；未安装依赖，未修改 `yyb_stm32/`、视觉固件或其目录。
