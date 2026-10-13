# 路线编辑实施计划

依据：docs/superpowers/specs/2026-10-06-route-editor-design.md。
本次在当前工作区逐项实施，不安装依赖，不覆盖已有修改，不自动运行实车。

- [x] 固件：增加 move 四向相对距离指令，复用距离估算、加减速和航向 PID；转向成功保留机构初始化。扩展 test_route_runtime.ps1 验证方向、距离、限速、故障和完成反馈。
- [x] 数据：新增 RouteSequenceModel，保存六类步骤及内嵌动作组；从 MechanismActionModel 提取 JSON 对象读写，文件格式不变。增加 JSON 往返和错误输入测试。
- [x] 执行：新增 RouteSequenceRunner，使用 MecanumJogClient 授权发送；每步先登记等待状态，再发送，匹配完成反馈、处理超时、停止与断连。增加模拟串口执行测试。
- [x] 页面：新增 RouteEditorPage，编辑、增删复制排序、动作组导入、JSON 文件、单步与整段执行、人工确认。MainWindow 接入页面和运行期间的控制互斥；增加页面与集成测试。
- [x] 验证：使用现有 upper_computer/build 构建，运行完整 CTest、固件宿主测试及可用的 Keil 编译，生成截图并检查；补充协议及使用说明。报告实机验证状态。

验证命令：cmake -S upper_computer -B upper_computer/build；cmake --build upper_computer/build --config Debug；ctest --test-dir upper_computer/build -C Debug --output-on-failure。

## 验证结果（2026-10-06）

- Qt/MinGW 构建通过；完整 CTest 33/33 通过。
- Keil STM32F407ZG 编译：0 Error(s)、0 Warning(s)，已生成 Objects/MecanumJog.hex。
- 路线页面中文截图已检查：upper_computer/build/route-editor.png。
- 两个仓库的 git diff --check 通过。
- 未连接实车，未烧录；距离、转向、视觉与机构联动仍需实机分段验证。
