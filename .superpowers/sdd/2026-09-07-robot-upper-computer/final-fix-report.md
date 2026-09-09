# Final fix wave

## 范围与假设

本轮只处理最终审查列出的五类行为缺口及两个低成本一致性项。PID 参数原本已经
直接读写 `PID_Profiles`，因此保持不动；没有修改视觉算法、没有安装依赖，也没有
把软件测试结果当作 Keil 或实机验收。

## 已完成

- 固件 `HostCommands` 在动作/遥测路径读取 RAM 参数：底盘速度限值、点动时长和加速度，
  机构位置/速度/加速度/云台插补速度，以及 IMU 频率。底盘限值按绝对值作为幅值上限，
  写 0 可禁用该轴；点动到期调用 stop-motion，仅停止当前运动并保留 30 秒解锁。
  解锁到期或通信看门狗超时才清除解锁并 fail-closed。
- Qt 在串口已连接、握手完成且动作解锁期间以 250 ms 周期发送 `GET_STATUS`；状态失效、
  急停或断连停止定时器。状态回复继续更新 UI 的解锁/急停状态。
- `TEST_ACTION` 不再自动重试；普通请求保留一次超时重试。
- 云台协议速度按 degree/s 解释，固件通过当前角度与目标角度计算向上取整的
  `duration_ms`；零位移使用 1 ms，非法/零速度不启动动作。
- 固件握手改为第一条合法 `HELLO` 绑定 active link，并按链路记录握手；非 active link 的
  `HELLO` 和受控命令不会改变活动链路或可用状态。
- Qt flags 仅接受 `Request`、`Response`、`Response|Error`、`Event`；CMake 最低版本改为
  3.22，与 `ENVIRONMENT_MODIFICATION` 一致，README 同步说明。

## TDD 证据

先添加失败测试并执行了显式 Qt 配置；旧实现在生成阶段因缺失待实现的
`host_runtime.c` 失败，随后新增行为测试先观察到心跳、动作重试和运行时参数断言失败，
再补生产实现。

## 验证

使用 `E:\Qt\Tools\mingw1310_64\bin`、`E:\Qt\6.11.1\mingw_64\bin`、
`E:\Qt\Tools\CMake_64\bin` 和 `E:\Qt\Tools\Ninja` 的显式 PATH，在
`build-final-fix` 执行干净 Debug 构建及完整 CTest：

```
cmake -S . -B build-final-fix -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=E:\Qt\6.11.1\mingw_64 -DCMAKE_C_COMPILER=E:\Qt\Tools\mingw1310_64\bin\gcc.exe -DCMAKE_CXX_COMPILER=E:\Qt\Tools\mingw1310_64\bin\g++.exe
cmake --build build-final-fix --config Debug --clean-first
ctest --test-dir build-final-fix -C Debug --output-on-failure
```

结果：14/14 CTest 通过。

## 外部固件基线与限制

修改的外部文件位于 `E:\Download\project\gongchuang\yyb_stm32`。修改前的既有文件
基线副本保存在
`.superpowers\sdd\2026-09-07-robot-upper-computer\external-baseline-final-fix\`：
`host_commands.[ch]`、`host_safety.[ch]`、`host_debug.c`、`host_params.[ch]`、
`host_config.h` 和 `Hardware\PWM.[ch]`。`host_runtime.[ch]` 是本轮新增文件，未有
历史基线副本；同时已将它们纳入 `USER\Template.uvprojx`。外部目录不是 Git 仓库，
本报告列出文件变化但不声称已完成 Keil 构建或硬件验证。
