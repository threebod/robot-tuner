cmake_minimum_required(VERSION 3.21)

project(robot_tuner LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets SerialPort)

add_executable(robot_tuner
    src/main.cpp
    src/pages/ChassisPage.cpp
    src/pages/ChassisPage.h
    src/pages/MechanismPage.cpp
    src/pages/MechanismPage.h
    src/pages/OverviewPage.cpp
    src/pages/OverviewPage.h
    src/pages/ImuPage.cpp
    src/pages/ImuPage.h
    src/pages/ActionTestPage.cpp
    src/pages/ActionTestPage.h
    src/widgets/TelemetryPlot.cpp
    src/widgets/TelemetryPlot.h
    src/app/MainWindow.cpp
    src/app/MainWindow.h
    src/device/DeviceClient.cpp
    src/device/DeviceClient.h
    src/device/ParameterCatalog.cpp
    src/device/ParameterCatalog.h
    src/device/TelemetryTypes.h
    src/protocol/Crc16.cpp
    src/protocol/Crc16.h
    src/protocol/FrameCodec.cpp
    src/protocol/FrameCodec.h
    src/protocol/FrameParser.cpp
    src/protocol/FrameParser.h
    src/protocol/ProtocolClient.cpp
    src/protocol/ProtocolClient.h
    src/protocol/ProtocolTypes.h
    src/serial/SerialController.cpp
    src/serial/SerialController.h
)
target_include_directories(robot_tuner PRIVATE src)
target_link_libraries(robot_tuner PRIVATE Qt6::Widgets Qt6::SerialPort)

add_executable(test_ui_smoke
    tests/test_ui_smoke.cpp
    src/pages/ChassisPage.cpp
    src/pages/ChassisPage.h
    src/pages/MechanismPage.cpp
    src/pages/MechanismPage.h
    src/pages/OverviewPage.cpp
    src/pages/OverviewPage.h
    src/pages/ImuPage.cpp
    src/pages/ImuPage.h
    src/pages/ActionTestPage.cpp
    src/pages/ActionTestPage.h
    src/widgets/TelemetryPlot.cpp
    src/widgets/TelemetryPlot.h
    src/app/MainWindow.cpp
    src/app/MainWindow.h
    src/device/DeviceClient.cpp
    src/device/DeviceClient.h
    src/device/ParameterCatalog.cpp
    src/device/ParameterCatalog.h
    src/device/TelemetryTypes.h
    src/protocol/Crc16.cpp
    src/protocol/Crc16.h
    src/protocol/FrameCodec.cpp
    src/protocol/FrameCodec.h
    src/protocol/FrameParser.cpp
    src/protocol/FrameParser.h
    src/protocol/ProtocolClient.cpp
    src/protocol/ProtocolClient.h
    src/protocol/ProtocolTypes.h
    src/serial/SerialController.cpp
    src/serial/SerialController.h
)
target_include_directories(test_ui_smoke PRIVATE src)
target_link_libraries(test_ui_smoke PRIVATE Qt6::Widgets Qt6::SerialPort)

add_executable(test_serial_controller
    tests/test_serial_controller.cpp
    src/protocol/Crc16.cpp
    src/protocol/Crc16.h
    src/protocol/FrameCodec.cpp
    src/protocol/FrameCodec.h
    src/protocol/FrameParser.cpp
    src/protocol/FrameParser.h
    src/protocol/ProtocolClient.cpp
    src/protocol/ProtocolClient.h
    src/protocol/ProtocolTypes.h
    src/serial/SerialController.cpp
    src/serial/SerialController.h
)
target_include_directories(test_serial_controller PRIVATE src)
target_link_libraries(test_serial_controller PRIVATE Qt6::Core Qt6::SerialPort)

add_executable(test_crc_codec
    tests/test_crc_codec.cpp
    src/protocol/Crc16.cpp
    src/protocol/Crc16.h
    src/protocol/FrameCodec.cpp
    src/protocol/FrameCodec.h
    src/protocol/ProtocolTypes.h
)
target_include_directories(test_crc_codec PRIVATE src)
target_link_libraries(test_crc_codec PRIVATE Qt6::Core)

add_executable(test_frame_parser
    tests/test_frame_parser.cpp
    src/protocol/Crc16.cpp
    src/protocol/Crc16.h
    src/protocol/FrameCodec.cpp
    src/protocol/FrameCodec.h
    src/protocol/FrameParser.cpp
    src/protocol/FrameParser.h
    src/protocol/ProtocolTypes.h
)
target_include_directories(test_frame_parser PRIVATE src)
target_link_libraries(test_frame_parser PRIVATE Qt6::Core)

add_executable(test_protocol_client
    tests/test_protocol_client.cpp
    src/protocol/Crc16.cpp
    src/protocol/Crc16.h
    src/protocol/FrameCodec.cpp
    src/protocol/FrameCodec.h
    src/protocol/FrameParser.cpp
    src/protocol/FrameParser.h
    src/protocol/ProtocolClient.cpp
    src/protocol/ProtocolClient.h
    src/protocol/ProtocolTypes.h
)
target_include_directories(test_protocol_client PRIVATE src)
target_link_libraries(test_protocol_client PRIVATE Qt6::Core)

add_executable(test_parameter_catalog
    tests/test_parameter_catalog.cpp
    src/device/ParameterCatalog.cpp
    src/device/ParameterCatalog.h
)
target_include_directories(test_parameter_catalog PRIVATE src)
target_link_libraries(test_parameter_catalog PRIVATE Qt6::Core)

add_executable(test_device_client
    tests/test_device_client.cpp
    src/device/DeviceClient.cpp
    src/device/DeviceClient.h
    src/device/ParameterCatalog.cpp
    src/device/ParameterCatalog.h
    src/device/TelemetryTypes.h
    src/protocol/Crc16.cpp
    src/protocol/Crc16.h
    src/protocol/FrameCodec.cpp
    src/protocol/FrameCodec.h
    src/protocol/FrameParser.cpp
    src/protocol/FrameParser.h
    src/protocol/ProtocolClient.cpp
    src/protocol/ProtocolClient.h
    src/protocol/ProtocolTypes.h
)
target_include_directories(test_device_client PRIVATE src)
target_link_libraries(test_device_client PRIVATE Qt6::Core)

add_executable(test_telemetry_plot
    tests/test_telemetry_plot.cpp
    src/widgets/TelemetryPlot.cpp
    src/widgets/TelemetryPlot.h
)
target_include_directories(test_telemetry_plot PRIVATE src)
target_link_libraries(test_telemetry_plot PRIVATE Qt6::Widgets)

enable_testing()
add_test(NAME test_ui_smoke COMMAND test_ui_smoke)
set_tests_properties(test_ui_smoke PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)
add_test(NAME test_serial_controller COMMAND test_serial_controller)
set_tests_properties(test_serial_controller PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)
add_test(NAME test_crc_codec COMMAND test_crc_codec)
set_tests_properties(test_crc_codec PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)
add_test(NAME test_frame_parser COMMAND test_frame_parser)
set_tests_properties(test_frame_parser PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)
add_test(NAME test_protocol_client COMMAND test_protocol_client)
set_tests_properties(test_protocol_client PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)
add_test(NAME test_parameter_catalog COMMAND test_parameter_catalog)
set_tests_properties(test_parameter_catalog PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)
add_test(NAME test_device_client COMMAND test_device_client)
set_tests_properties(test_device_client PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)
add_test(NAME test_telemetry_plot COMMAND test_telemetry_plot)
set_tests_properties(test_telemetry_plot PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)
