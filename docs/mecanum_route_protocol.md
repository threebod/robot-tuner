# 在线路线编辑的文本协议

上位机逐步执行路线，每次只下发一个运动；需要授权的指令先发送 `arm`，
收到 `ARMED for one enable or motion command` 后再下发。每 250 ms 保持 `hb`。
本协议对应 `test/mecanum_jog`，与二进制 `host_protocol` 无关。

## 相对距离移动

```text
move A 980 100
MOVE RUN dir=A distance_mm=980 rpm=100
MOVE DONE dir=A distance_mm=980
```

W/S/A/D 分别为相对当前车头的前／后／左／右；距离 1～2400 mm，速度 10～250 RPM。
横移实际指令速度受当前横移上限限制，RUN 中反馈有效速度。
方向保持开始时的 IMU 航向，距离通过已发速度、时间与当前标定比例累计估算。
成功只停止底盘、保留机构初始化；故障为 `ERR MOVE: ...` 并全局停车。
相对移动使地图导航估计失效，不依赖 `nav init`，也不会复位到旧地图坐标。

## 转向与视觉

转向复用 `turn L|R 1..180`，等待 `TURN running` 和 `TURN DONE ...`；成功不清除机构初始化。
视觉步骤先发送 `vision scale material F R` 或 `vision scale ring N F R`，
F/R 范围 50～2000，单位 mm/pixel ×1000。收到匹配比例确认后发送
`vision align material N` 或 `vision align ring N`。
物料颜色 N=1～6，圆环 N=1～3。收到本次视觉会话状态后等待 `VISION DONE ...`。
对准位移不反向回退，后续 `move` 从对准后的位置出发。

## 动作组初始状态

动作组开始前发送 `mech status`，必须收到对应姿态与舵机状态：

```text
MECH POS h=0 l=0 t=650
MECH SERVOS gripper_mdeg=70000 platform_mdeg=26000 turret_mdeg=65000 moving=0
```

h/l 为 0.1 mm，t 为 0.1°，舵机角度为 0.001°。
检查姿态、夹爪及平台与动作组初始状态一致，且机构和舵机均不在运行。
这些是已发指令的估计状态，不能验证实际机械位置或夹持结果。
机构姿态动作复用 `mech pose`，按目标 h/l/t 匹配 RUN 和 DONE。
夹爪与平台复用 `servo`，匹配通道、角度及 MOVING/OK/DONE。
云台的直接舵机步骤在路线中转换为保持水平、升降位置的 `mech pose`，以维护角度估计。
等待参数在动作完成后计时，动作组的有限循环参数保持原语义。

## 中止规则

用户停止、急停、断连、授权超时、完成反馈超时和设备错误均取消未执行的步骤。
视觉正常等待目标不设总时限；摄像头通信故障由固件原有重试检查处理。
旧固件缺少新指令或 `MECH SERVOS` 反馈时终止并提示，不模拟成功或继续下发。
