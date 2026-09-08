# Task 10：可移植 STM32 帧协议与链路环形缓冲报告

## 假设与范围

- `upper_computer/` 是唯一需要提交的 Git 仓库；`yyb_stm32/` 没有 Git，协议文件作为外部树交付。
- 本任务只提供纯 C99 的 CRC、帧解析/编码和两路 RX SPSC 环；不接入 STM32 外设、不修改 `USER/main.c` 或其他固件流程。
- 256 字节环的有效容量为 256 字节；满时保留已有数据并丢弃新字节。

## TDD RED

先新增 `tests/test_stm32_protocol.c` 和 `test_stm32_protocol` CMake 目标，测试覆盖：

- CRC-16/CCITT-FALSE 向量 `123456789 -> 0x29B1`；
- 空 HELLO 帧、逐字节输入、连续帧；
- 不支持版本、错误 CRC、129 字节长度及错误后的下一帧恢复；
- 256 字节 RX 环满时丢新字节、保持顺序，以及 USB/Bluetooth 环隔离；
- 空 HELLO 帧编码的固定字节结果。

使用现有工具链（CMake 未在 PATH，未安装依赖）配置时，首次按预期失败：

```text
CMake Error: Cannot find source file:
E:/Download/project/gongchuang/yyb_stm32/host_protocol/host_crc16.c
```

补充编码断言后，删除编码实现再次构建，链接按预期报告缺少 `HostFrame_Encode`；随后才恢复最小实现。

## GREEN 实现

- `host_config.h` 固定保留 `HOST_DEBUG_MODE 1`、协议版本 `1u`、最大 payload `128u` 和 `256u` RX 环尺寸，并集中定义同步头/帧尺寸常量。
- `host_crc16.c` 实现初值 `0xFFFF`、多项式 `0x1021`、非反射、无异或的 CRC；不使用 STM32 头文件。
- `host_frame.c` 使用固定数组状态机，等待 `AA 55`，校验长度、CRC 和版本，输出 `HostFrame`；提供固定输出缓冲区的 `HostFrame_Encode`，不使用 heap。
- `host_link.c` 为 `HOST_LINK_USB` 和 `HOST_LINK_BLUETOOTH` 各维护一个固定 256 字节环；ISR 只推进 head，主循环只推进 tail，满时返回 `false` 丢弃新字节。
- `test_stm32_protocol` 目标显式使用 C99、关闭该目标的 AUTOMOC 并以 C 链接，避免引入 Qt/STM32 依赖。

## 验证

本机临时将 `TEMP`/`TMP` 指向工作区 `upper_computer/build/tmp`，避免工具链默认向 C 盘临时目录写入；没有安装或写入任何 C 盘依赖。

```text
E:\Qt\Tools\CMake_64\bin\cmake.exe --build build --target test_stm32_protocol   PASS
E:\Qt\Tools\CMake_64\bin\ctest.exe --test-dir build -R ^test_stm32_protocol$ --output-on-failure   PASS
E:\Qt\Tools\CMake_64\bin\cmake.exe --build build --config Debug   PASS
E:\Qt\Tools\CMake_64\bin\ctest.exe --test-dir build -C Debug --output-on-failure   PASS (9/9)
```

定向测试和全量 CTest 均无失败；全量目标包括原有 8 个 Qt 测试及本任务新增的便携 C 测试。

## 文件与外部基线对比

上位机 Git 仓库改动（应提交）：

- `CMakeLists.txt`：启用 C/C++ 双语言，加入 sibling 源文件、C99 `test_stm32_protocol` 目标及 CTest 注册。
- `tests/test_stm32_protocol.c`：便携协议/环形缓冲测试。
- `.superpowers/sdd/2026-09-07-robot-upper-computer/task-10-report.md`：本报告。

外部 `yyb_stm32/host_protocol/` 新增文件（不属于上位机 Git 提交）：

- `host_config.h`
- `host_crc16.h`
- `host_crc16.c`
- `host_frame.h`
- `host_frame.c`
- `host_link.h`
- `host_link.c`

外部基线在任务开始时没有 `host_protocol/` 目录；`git -C ..\yyb_stm32 status` 返回“not a git repository”，因此不能生成 Git commit diff。以空的 `external-baseline-task10/` 与当前目录运行 `git diff --no-index` 返回退出码 `1`（表示发现差异，符合预期），差异仅为上述 7 个新增文件。`yyb_stm32/USER/main.c`、现有硬件驱动和其他固件流程均未修改。
