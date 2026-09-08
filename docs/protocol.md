# 上位机—STM32 调试协议（版本 1）

本文档是 Qt 上位机和 STM32 `host_protocol` 模块之间的人工维护协议契约。所有
多字节整数、IEEE-754 单精度浮点数和 CRC 字段均为小端序。参数只写入 STM32
RAM；设备重启后恢复固件默认值。

## 帧

```text
AA 55 | version:u8 | flags:u8 | sequence:u8 | command:u8 |
length:u16_le | payload:length | crc16:u16_le
```

`AA 55` 是固定同步头，`version` 固定为 `0x01`，payload 最大 128 字节。CRC16
覆盖 `version` 到 payload 的全部字节，算法为 CRC-16/CCITT-FALSE（初值 `0xFFFF`，
多项式 `0x1021`，不反射，结果不异或）。请求方递增 `sequence`；响应沿用请求序号。

### flags

| 标志 | 值 | 含义 |
| --- | ---: | --- |
| `Request` | `0x01` | 请求帧 |
| `Response` | `0x02` | 响应帧 |
| `Event` | `0x04` | 主动事件/遥测帧 |
| `Error` | `0x08` | 错误标志，可与 `Response` 合用 |

错误响应使用 `Response | Error`（值 `0x0A`），保留请求的 sequence 和 command，
payload 的第一个字节是 `error:u8`。

## 命令

| 命令 | 值 | 请求 payload | 成功响应 payload |
| --- | ---: | --- | --- |
| `HELLO` | `0x01` | 空 | `protocol:u8 fw_major:u8 fw_minor:u8 fw_patch:u8 capabilities:u32` |
| `GET_STATUS` | `0x02` | 空 | `mode:u8 emergency:u8 unlocked:u8 active_link:u8 last_error:u16` |
| `GET_PARAM_GROUP` | `0x10` | `group:u8 [page:u8]` | `group:u8 count:u8 [id:u16 type:u8 value:typed] * count` |
| `SET_PARAM_GROUP` | `0x11` | `group:u8 count:u8 [id:u16 type:u8 value:typed] * count` | `group:u8 count:u8 [id:u16 type:u8 applied_value:typed] * count` |
| `SET_TELEMETRY` | `0x20` | `mask:u8 period_ms:u16` | `accepted_mask:u8 actual_period_ms:u16` |
| `IMU_CALIBRATE` | `0x21` | 空 | `state:u8` |
| `TEST_UNLOCK` | `0x30` | 空 | `unlock_duration_ms:u16` |
| `TEST_ACTION` | `0x31` | 见动作表 | 空 |
| `STOP` | `0x32` | 空 | 空 |
| `EMERGENCY_STOP` | `0x33` | 空 | 空 |
| `CLEAR_EMERGENCY_STOP` | `0x34` | 空 | 空 |
| `STATUS_TELEMETRY` | `0x80` | 事件 | `mode:u8 emergency:u8 unlocked:u8 last_error:u16` |
| `IMU_TELEMETRY` | `0x81` | 事件 | `timestamp_ms:u32 ax:i16 ay:i16 az:i16 gx:i16 gy:i16 gz:i16 roll:i16 pitch:i16 yaw:i16` |
| `PID_TELEMETRY` | `0x82` | 事件 | `timestamp_ms:u32 target_cdeg:i16 actual_cdeg:i16 output_centi:i16` |

`IMU_CALIBRATE` 的 `state` 为 `0=started`、`1=completed`、`2=failed`。设备支持
的遥测 mask 由固件定义；9600 baud 链路的 IMU 频率范围为 1–20 Hz，115200 baud
链路为 1–50 Hz，设备在响应中返回实际采用周期。

`GET_PARAM_GROUP` 的 `page` 为可选的从 0 开始的分页号；省略时等同于 `page=0`。
单帧最多承载 18 条参数记录（响应仍使用原有的 `group:u8 count:u8` 记录布局）。
PID 组 `0x10` 共 25 条记录，因此 `page=0` 返回目录中的前 18 条、`page=1` 返回后 7 条；
上位机必须在两页均完整且顺序正确后，再发出一次完整的参数组通知。能够在单帧容纳的
其他参数组继续使用省略 `page` 的单页请求。

## 错误码

| 错误 | 值 | 含义 |
| --- | ---: | --- |
| `VERSION` | `0x01` | 不支持的协议版本 |
| `COMMAND` | `0x02` | 不支持的命令 |
| `CRC` | `0x03` | CRC 错误 |
| `LENGTH` | `0x04` | 长度错误 |
| `PARAM_NOT_FOUND` | `0x05` | 参数不存在 |
| `PARAM_TYPE` | `0x06` | 参数类型错误 |
| `PARAM_RANGE` | `0x07` | 参数越界 |
| `BUSY` | `0x08` | 设备忙 |
| `NOT_UNLOCKED` | `0x09` | 未完成动作解锁 |
| `EMERGENCY_LOCKED` | `0x0A` | 急停锁定 |

## 值类型

| 类型 | 编码 | 字节数 |
| --- | ---: | ---: |
| `uint8` | `0x01` | 1 |
| `uint16` | `0x02` | 2 |
| `uint32` | `0x03` | 4 |
| `int8` | `0x04` | 1 |
| `int16` | `0x05` | 2 |
| `int32` | `0x06` | 4 |
| `float32` | `0x07` | 4 |

## 固定参数目录

参数记录统一为 `id:u16 type:u8 value:typed`。写入组必须先完整校验，组内任一
参数无效则整组拒绝，不得只应用部分值。组编号为 `0x10` PID、`0x20` 底盘、
`0x30` 机构、`0x40` IMU。

### PID（组 `0x10`）

对 `profile=0..4`，ID 为 `0x1000 + profile*0x10 + offset`：

| offset | 参数 | 类型 | 范围 |
| ---: | --- | --- | ---: |
| 0 | PID Kp | `float32` | 0..20 |
| 1 | PID Ki | `float32` | 0..2 |
| 2 | PID Kd | `float32` | 0..20 |
| 3 | integral limit | `float32` | 0..100 |
| 4 | output limit | `float32` | 0..230 |

### 底盘（组 `0x20`）

| ID | 参数 | 类型 | 范围 |
| ---: | --- | --- | ---: |
| `0x2000` | chassis vx test limit | `int32` | -80..80 |
| `0x2001` | chassis vy test limit | `int32` | -80..80 |
| `0x2002` | chassis w test limit | `int32` | -30..30 |
| `0x2003` | chassis test duration ms | `uint16` | 50..1000 |
| `0x2004` | motion acceleration | `uint16` | 1..230 |

### 机构（组 `0x30`）

| ID | 参数 | 类型 | 范围 |
| ---: | --- | --- | ---: |
| `0x3000` | horizontal mechanism position | `float32` | -120..63 |
| `0x3001` | lift mechanism position | `float32` | 0..50 |
| `0x3002` | turret angle | `float32` | 135..295 |
| `0x3003` | mechanism motor speed | `uint16` | 100..2000 |
| `0x3004` | mechanism acceleration | `uint8` | 1..220 |
| `0x3005` | turret interpolation speed | `float32` | 1..20 |

### IMU（组 `0x40`）

| ID | 参数 | 类型 | 范围 |
| ---: | --- | --- | ---: |
| `0x4000` | IMU telemetry rate Hz | `uint16` | 1..50 |

平台命名位置（1–3）和夹爪命名状态（close/open）是动作枚举，不是可任意填写的
数值参数。

## 受控动作

`TEST_ACTION` 的 action 字段和后续 payload 必须按如下变体编码：

| action | 变体 payload |
| ---: | --- |
| `0x01` chassis | `action:u8 vx:i16 vy:i16 w:i16 duration_ms:u16` |
| `0x10` horizontal mechanism | `action:u8 target:f32 speed:u16 accel:u8` |
| `0x11` lift mechanism | `action:u8 target:f32 speed:u16 accel:u8` |
| `0x12` turret | `action:u8 target_angle:f32 interpolation_speed:f32` |
| `0x20` platform named position | `action:u8 position:u8`，position 为 1..3 |
| `0x21` gripper named state | `action:u8 state:u8`，0=close、1=open |

动作测试必须先 `TEST_UNLOCK`；设备端解锁时长固定为 30000 ms。断开、通信看门狗
超时或进入急停都会使动作权限失效。

## 遥测换算

IMU 原始值是有符号 16 位计数：

- acceleration = `raw / 32768 * 16` g；
- angular velocity = `raw / 32768 * 2000` degree/s；
- angle = `raw / 32768 * 180` degree。

PID `target_cdeg` 和 `actual_cdeg` 每计数为 0.01 degree，`output_centi` 每计数为
0.01 output-unit。状态遥测字段为 `mode:u8 emergency:u8 unlocked:u8 last_error:u16`。
