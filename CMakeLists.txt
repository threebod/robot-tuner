cmake_minimum_required(VERSION 3.22)

project(robot_tuner LANGUAGES C CXX)

set(CMAKE_AUTOMOC ON)
# Keep moc worker count deterministic on Windows.  Historical clean builds
# intermittently crashed with 0xC0000005 while several autogen workers ran.
set(CMAKE_AUTOGEN_PARALLEL 1)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets SerialPort)

add_executable(robot_tuner
    src/main.cpp
    src/device/MechanismActionModel.cpp
    src/device/MechanismActionModel.h
    src/pages/ChassisPage.cpp
    src/pages/ChassisPage.h
    src/pages/FieldPositionPage.cpp
    src/pages/FieldPositionPage.h
    src/pages/MechanismPage.cpp
    src/pages/MechanismPage.h
    src/pages/MecanumJogPage.cpp
    src/pages/MecanumJogPage.h
    src/pages/OverviewPage.cpp
    src/pages/OverviewPage.h
    src/pages/ImuPage.cpp
    src/pages/ImuPage.h
    src/pages/ActionTestPage.cpp
    src/pages/ActionTestPage.h
    src/pages/TerminalPage.cpp
    src/pages/TerminalPage.h
    src/pages/VisionPage.cpp
    src/pages/VisionPage.h
    src/widgets/TelemetryPlot.cpp
    src/widgets/TelemetryPlot.h
    src/widgets/SerialDebugPanel.cpp
    src/widgets/SerialDebugPanel.h
    src/app/MainWindow.cpp
    src/app/MainWindow.h
    src/device/DeviceClient.cpp
    src/device/DeviceClient.h
    src/device/MecanumJogClient.cpp
    src/device/MecanumJogClient.h
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
    src/pages/FieldPositionPage.cpp
    src/pages/FieldPositionPage.h
    src/pages/MechanismPage.cpp
    src/pages/MechanismPage.h
    src/pages/MecanumJogPage.cpp
    src/pages/MecanumJogPage.h
    src/pages/OverviewPage.cpp
    src/pages/OverviewPage.h
    src/pages/ImuPage.cpp
    src/pages/ImuPage.h
    src/pages/ActionTestPage.cpp
    src/pages/ActionTestPage.h
    src/pages/TerminalPage.cpp
    src/pages/TerminalPage.h
    src/pages/VisionPage.cpp
    src/pages/VisionPage.h
    src/widgets/TelemetryPlot.cpp
    src/widgets/TelemetryPlot.h
    src/widgets/SerialDebugPanel.cpp
    src/widgets/SerialDebugPanel.h
    src/app/MainWindow.cpp
    src/app/MainWindow.h
    src/device/DeviceClient.cpp
    src/device/DeviceClient.h
    src/device/MecanumJogClient.cpp
    src/device/MecanumJogClient.h
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

add_executable(test_mecanum_jog_client
    tests/test_mecanum_jog_client.cpp
    src/device/MechanismActionModel.cpp
    src/device/MechanismActionModel.h
    src/device/MecanumJogClient.cpp
    src/device/MecanumJogClient.h
)
target_include_directories(test_mecanum_jog_client PRIVATE src)
target_link_libraries(test_mecanum_jog_client PRIVATE Qt6::Core)

add_executable(test_mecanum_jog_page
    tests/test_mecanum_jog_page.cpp
    src/pages/MecanumJogPage.cpp
    src/pages/MecanumJogPage.h
)
target_include_directories(test_mecanum_jog_page PRIVATE src)
target_link_libraries(test_mecanum_jog_page PRIVATE Qt6::Widgets)

add_executable(test_mechanism_action_model
    tests/test_mechanism_action_model.cpp
    src/device/MechanismActionModel.cpp
    src/device/MechanismActionModel.h
)
target_include_directories(test_mechanism_action_model PRIVATE src)
target_link_libraries(test_mechanism_action_model PRIVATE Qt6::Core)

add_executable(test_end_to_end
    tests/test_end_to_end.cpp
    tests/support/FakeDevice.cpp
    tests/support/FakeDevice.h
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
target_include_directories(test_end_to_end PRIVATE src tests)
target_link_libraries(test_end_to_end PRIVATE Qt6::Core)

add_executable(test_telemetry_plot
    tests/test_telemetry_plot.cpp
    src/widgets/TelemetryPlot.cpp
    src/widgets/TelemetryPlot.h
)
target_include_directories(test_telemetry_plot PRIVATE src)
target_link_libraries(test_telemetry_plot PRIVATE Qt6::Widgets)

add_executable(test_field_position
    tests/test_field_position.cpp
    src/pages/FieldPositionPage.cpp
    src/pages/FieldPositionPage.h
    src/device/TelemetryTypes.h
    src/widgets/SerialDebugPanel.cpp
    src/widgets/SerialDebugPanel.h
)
target_include_directories(test_field_position PRIVATE src)
target_link_libraries(test_field_position PRIVATE Qt6::Widgets)

add_executable(test_serial_debug_panel
    tests/test_serial_debug_panel.cpp
    src/widgets/SerialDebugPanel.cpp
    src/widgets/SerialDebugPanel.h
)
target_include_directories(test_serial_debug_panel PRIVATE src)
target_link_libraries(test_serial_debug_panel PRIVATE Qt6::Widgets)

set(STM32_HOST_PROTOCOL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../yyb_stm32/host_protocol")
add_executable(test_stm32_protocol
    tests/test_stm32_protocol.c
    ${STM32_HOST_PROTOCOL_DIR}/host_crc16.c
    ${STM32_HOST_PROTOCOL_DIR}/host_frame.c
    ${STM32_HOST_PROTOCOL_DIR}/host_link.c
    ${STM32_HOST_PROTOCOL_DIR}/host_params.c
    ${STM32_HOST_PROTOCOL_DIR}/host_runtime.c
    ${STM32_HOST_PROTOCOL_DIR}/host_safety.c
    ${STM32_HOST_PROTOCOL_DIR}/host_commands.c
)
target_include_directories(test_stm32_protocol PRIVATE ${STM32_HOST_PROTOCOL_DIR})
set_target_properties(test_stm32_protocol PROPERTIES
    AUTOMOC OFF
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
    LINKER_LANGUAGE C
)

add_executable(test_stm32_armcc_headers
    tests/test_stm32_armcc_headers.c
)
target_include_directories(test_stm32_armcc_headers PRIVATE
    ${STM32_HOST_PROTOCOL_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/stubs/armcc
)
set_target_properties(test_stm32_armcc_headers PROPERTIES
    AUTOMOC OFF
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
    LINKER_LANGUAGE C
)

option(TASK12_ENABLE_HOST_STUB_COMPILE
    "Build the Task 12 host stub compile probe" ON)
if(TASK12_ENABLE_HOST_STUB_COMPILE)
    add_executable(test_task12_host_compile
        tests/test_task12_host_compile.c
        ${STM32_HOST_PROTOCOL_DIR}/host_crc16.c
        ${STM32_HOST_PROTOCOL_DIR}/host_frame.c
        ${STM32_HOST_PROTOCOL_DIR}/host_link.c
        ${STM32_HOST_PROTOCOL_DIR}/host_params.c
        ${STM32_HOST_PROTOCOL_DIR}/host_runtime.c
        ${STM32_HOST_PROTOCOL_DIR}/host_safety.c
        ${STM32_HOST_PROTOCOL_DIR}/host_commands.c
        ${STM32_HOST_PROTOCOL_DIR}/host_uart3.c
        ${STM32_HOST_PROTOCOL_DIR}/host_time.c
        ${STM32_HOST_PROTOCOL_DIR}/host_debug.c
    )
    target_include_directories(test_task12_host_compile PRIVATE
        ${STM32_HOST_PROTOCOL_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/stubs/task12
    )
    set_target_properties(test_task12_host_compile PROPERTIES
        AUTOMOC OFF
        C_STANDARD 99
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
        LINKER_LANGUAGE C
    )
    target_compile_definitions(test_task12_host_compile PRIVATE HOST_DEBUG_MODE=0)
endif()

add_executable(test_pid_profile_baseline
    tests/test_pid_profile_baseline.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../yyb_stm32/Hardware/pid.c
)
target_include_directories(test_pid_profile_baseline PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../yyb_stm32/USER
    ${CMAKE_CURRENT_SOURCE_DIR}/../yyb_stm32/Hardware
    ${CMAKE_CURRENT_SOURCE_DIR}/../yyb_stm32/SYSTEM/sys
    ${CMAKE_CURRENT_SOURCE_DIR}/../yyb_stm32/SYSTEM/delay
    ${CMAKE_CURRENT_SOURCE_DIR}/../yyb_stm32/can_ZDT
    ${CMAKE_CURRENT_SOURCE_DIR}/../yyb_stm32/CORE
    ${CMAKE_CURRENT_SOURCE_DIR}/../yyb_stm32/FWLIB/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../yyb_stm32/imu
    ${CMAKE_CURRENT_SOURCE_DIR}/../yyb_stm32/LED
    ${CMAKE_CURRENT_SOURCE_DIR}/../yyb_stm32/OLED
)
target_compile_definitions(test_pid_profile_baseline PRIVATE STM32F40_41xxx)
target_compile_options(test_pid_profile_baseline PRIVATE -ffunction-sections -fdata-sections)
target_link_options(test_pid_profile_baseline PRIVATE -Wl,--gc-sections)
set_target_properties(test_pid_profile_baseline PROPERTIES
    AUTOMOC OFF
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
    LINKER_LANGUAGE C
)

set(MECANUM_JOG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../test/mecanum_jog")
add_executable(test_mecanum_straight
    ${MECANUM_JOG_DIR}/tests/test_straight.c
)
add_executable(test_mecanum_route
    ${MECANUM_JOG_DIR}/tests/test_route.c
)
add_executable(test_mecanum_navigation
    ${MECANUM_JOG_DIR}/tests/test_navigation.c
)
add_executable(test_mecanum_mechanism_action
    ${MECANUM_JOG_DIR}/tests/test_mechanism_action.c
)
add_executable(test_mecanum_can
    ${MECANUM_JOG_DIR}/tests/test_can.c
    ${MECANUM_JOG_DIR}/jog_can.c
)
target_include_directories(test_mecanum_can PRIVATE
    ${MECANUM_JOG_DIR}/tests/stubs
)
foreach(target test_mecanum_straight test_mecanum_route test_mecanum_navigation test_mecanum_mechanism_action test_mecanum_can)
    set_target_properties(${target} PROPERTIES
        AUTOMOC OFF
        C_STANDARD 99
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
        LINKER_LANGUAGE C
    )
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
endforeach()

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
add_test(NAME test_mecanum_jog_client COMMAND test_mecanum_jog_client)
set_tests_properties(test_mecanum_jog_client PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)
add_test(NAME test_mecanum_jog_page COMMAND test_mecanum_jog_page)
set_tests_properties(test_mecanum_jog_page PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)
add_test(NAME test_mechanism_action_model COMMAND test_mechanism_action_model)
set_tests_properties(test_mechanism_action_model PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)
add_test(NAME test_end_to_end COMMAND test_end_to_end)
set_tests_properties(test_end_to_end PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)
add_test(NAME test_telemetry_plot COMMAND test_telemetry_plot)
set_tests_properties(test_telemetry_plot PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)
add_test(NAME test_field_position COMMAND test_field_position)
set_tests_properties(test_field_position PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)
add_test(NAME test_serial_debug_panel COMMAND test_serial_debug_panel)
set_tests_properties(test_serial_debug_panel PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)
add_test(NAME test_stm32_protocol COMMAND test_stm32_protocol)
add_test(NAME test_stm32_armcc_headers COMMAND test_stm32_armcc_headers)
add_test(NAME test_task12_host_static
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/task12_static_test.cmake)
if(TASK12_ENABLE_HOST_STUB_COMPILE)
    add_test(NAME test_task12_host_compile COMMAND test_task12_host_compile)
endif()
add_test(NAME test_pid_profile_baseline COMMAND test_pid_profile_baseline)
add_test(NAME test_mecanum_straight COMMAND test_mecanum_straight)
add_test(NAME test_mecanum_route COMMAND test_mecanum_route)
add_test(NAME test_mecanum_navigation COMMAND test_mecanum_navigation)
add_test(NAME test_mecanum_mechanism_action COMMAND test_mecanum_mechanism_action)
add_test(NAME test_mecanum_can COMMAND test_mecanum_can)
add_test(NAME test_mecanum_route_runtime
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File
            ${MECANUM_JOG_DIR}/tests/test_route_runtime.ps1)
