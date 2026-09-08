# Task 9：安全动作、解锁与急停报告

## 范围

本次只实现上位机的 `TEST_UNLOCK`、IMU 校准请求、底盘/机构受控动作、
`STOP`/`EMERGENCY_STOP`/`CLEAR_EMERGENCY_STOP` 及其本地安全互锁。未安装依赖，
未修改 `yyb_stm32/` 或视觉代码；终端页面和视觉占位页仍留给后续任务。

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
- 解锁 ACK 启动 30 秒（设备返回时长）倒计时，断开、超时、急停或设备错误会撤销本地解锁；
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
