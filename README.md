# Robot Tuner 上位机

这是 Qt 6 Widgets 上位机，使用版本 1 的 `host_protocol` 帧与 STM32 调试模式通信。
本目录的自动化测试使用内存中的 `FakeDevice`，不需要串口或机器人硬件。

## Windows 环境

推荐使用已经准备好的 Qt/MinGW 工具链，不在本项目中安装依赖，也不要把依赖复制到
C 盘：

- Qt 6.11.1 MinGW 64-bit：`E:\Qt\6.11.1\mingw_64`
- MinGW 13.1：`E:\Qt\Tools\mingw1310_64\bin`
- CMake 3.22 或更高版本（本机为 3.30.5）：`E:\Qt\Tools\CMake_64\bin`
- Ninja 1.12.1：`E:\Qt\Tools\Ninja`

在 PowerShell 中配置本次会话的 PATH（路径顺序有意固定），然后从本目录执行：

```powershell
$qtPrefix = 'E:\Qt\6.11.1\mingw_64'
$mingwBin = 'E:\Qt\Tools\mingw1310_64\bin'
$cmakeBin = 'E:\Qt\Tools\CMake_64\bin'
$ninjaBin = 'E:\Qt\Tools\Ninja'
$env:Path = "$mingwBin;$qtPrefix\bin;$cmakeBin;$ninjaBin;$env:Path"

cmake -S . -B build -G Ninja `
    -DCMAKE_BUILD_TYPE=Debug `
    -DCMAKE_PREFIX_PATH="$qtPrefix" `
    -DCMAKE_C_COMPILER="$mingwBin\gcc.exe" `
    -DCMAKE_CXX_COMPILER="$mingwBin\g++.exe"
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

`CMAKE_AUTOGEN_PARALLEL=1` 已固定 Qt 自动生成步骤的 worker 数量，以避免 Windows 上
并发 `moc.exe` 的不稳定退出。运行 GUI 时仍需保留同一 PATH：

```powershell
.\build\robot_tuner.exe
```

## 串口与固件设置

在“串口”页面只选择当前实际连接的一路链路，再点击连接：

- USB-UART 调试链路连接 STM32 `USART3` 的 `PB10/PB11`，使用 `115200 8-N-1`，无硬件流控。
- HC-05 蓝牙链路使用 STM32 `UART5`，模块链路参数设为 `9600 8-N-1`，无硬件流控。

固件 `yyb_stm32/host_protocol/host_config.h` 中 `HOST_DEBUG_MODE` 默认是 `1`。调试上位机
时保持为 `1`；需要恢复旧的主循环时显式编译为 `0`。该宏控制固件调试入口，不是上位机
运行时选项；两种固件模式都必须在 Keil 环境分别构建后再进行硬件验收。

## 安全首启流程

首次上电前断开电机/执行器或确保机械部分已可靠架空，手边保持物理急停可用。先检查
串口号和波特率，连接后只观察 `HELLO`、状态、参数读取和低频遥测；确认方向、限位、
看门狗和急停方案后，才允许使用 `TEST_UNLOCK` 和小范围动作测试。发现异常立即使用
物理急停，并断开上位机连接。软件测试通过不等于机器人硬件验收通过。

参数写入只存在 STM32 RAM 中，不写 Flash/EEPROM；STM32 重启后会恢复固件默认参数。
因此每次重启都应重新读取并核对参数，不能把上位机显示的上一次值当作持久化配置。
