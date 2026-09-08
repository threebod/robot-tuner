#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "host_config.h"
#include "host_crc16.h"
#include "host_frame.h"
#include "host_link.h"
#include "host_commands.h"
#include "host_params.h"
#include "host_safety.h"

PID_Profile_t PID_Profiles[5] = {
    { 2.0f, 0.0f, 0.8f, 7.0f, 230.0f },
    { 3.3f, 0.0f, 1.8f, 7.0f, 30.0f },
    { 5.0f, 0.0f, 5.0f, 7.0f, 230.0f },
    { 2.0f, 0.05f, 0.4f, 7.0f, 230.0f },
    { 2.0f, 0.07f, 0.4f, 7.0f, 230.0f }
};

static unsigned int fake_action_calls;
static unsigned int fake_stop_calls;
static unsigned int fake_emergency_calls;

static void fake_chassis(int16_t vx, int16_t vy, int16_t w,
                         uint16_t duration_ms)
{
    (void)vx;
    (void)vy;
    (void)w;
    (void)duration_ms;
    ++fake_action_calls;
}

static void fake_stop(void)
{
    ++fake_stop_calls;
}

static void fake_emergency_stop(void)
{
    ++fake_emergency_calls;
}

static bool require_condition(bool condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "%s\n", message);
    }
    return condition;
}

static bool frame_matches(const HostFrame *frame,
                          uint8_t version,
                          uint8_t flags,
                          uint8_t sequence,
                          uint8_t command,
                          uint16_t length,
                          const uint8_t *payload)
{
    size_t index;

    if (frame->version != version || frame->flags != flags ||
        frame->sequence != sequence || frame->command != command ||
        frame->length != length) {
        return false;
    }
    for (index = 0u; index < length; ++index) {
        if (frame->payload[index] != payload[index]) {
            return false;
        }
    }
    return true;
}

static bool parse_bytes(HostFrameParser *parser,
                        const uint8_t *bytes,
                        size_t length,
                        HostParseResult expected,
                        HostFrame *out)
{
    size_t index;
    HostParseResult result = HOST_PARSE_NONE;

    for (index = 0u; index < length; ++index) {
        result = HostFrameParser_Push(parser, bytes[index], out);
    }
    return result == expected;
}

int main(void)
{
    static const uint8_t crc_vector[] = "123456789";
    static const uint8_t hello_frame[] = {
        0xAAu, 0x55u, 0x01u, 0x01u, 0x21u, 0x01u,
        0x00u, 0x00u, 0x2Bu, 0x97u
    };
    static const uint8_t status_frame[] = {
        0xAAu, 0x55u, 0x01u, 0x01u, 0x07u, 0x02u,
        0x01u, 0x00u, 0x7Fu, 0xFBu, 0x3Eu
    };
    static const uint8_t unsupported_version_frame[] = {
        0xAAu, 0x55u, 0x02u, 0x01u, 0x01u, 0x01u,
        0x00u, 0x00u, 0x85u, 0x6Eu
    };
    static const uint8_t oversized_header[] = {
        0xAAu, 0x55u, 0x01u, 0x01u, 0x09u, 0x02u,
        0x81u, 0x00u
    };
    static const uint8_t empty_payload[] = { 0u };
    HostFrameParser parser;
    HostFrame frame;
    HostFrame first_concatenated;
    HostFrame second_concatenated;
    HostParseResult result;
    HostFrame hello;
    uint8_t encoded[HOST_FRAME_MAX_SIZE];
    uint16_t encoded_length;
    uint8_t corrupted_status[sizeof(status_frame)];
    size_t index;
    size_t concatenated_index;
    size_t frame_count;
    uint8_t value;

    if (!require_condition(HOST_DEBUG_MODE == 1,
                           "host debug mode default changed")) {
        return 1;
    }
    if (!require_condition(HOST_PROTOCOL_VERSION == 1u,
                           "host protocol version changed")) {
        return 1;
    }
    if (!require_condition(HOST_MAX_PAYLOAD == 128u,
                           "host payload limit changed")) {
        return 1;
    }
    if (!require_condition(HOST_RX_RING_SIZE == 256u,
                           "host RX ring size changed")) {
        return 1;
    }
    if (!require_condition(HostCrc16(crc_vector, 9u) == 0x29B1u,
                           "CRC-16/CCITT-FALSE vector mismatch")) {
        return 1;
    }

    hello = (HostFrame){ 0 };
    hello.version = HOST_PROTOCOL_VERSION;
    hello.flags = 0x01u;
    hello.sequence = 0x21u;
    hello.command = 0x01u;
    if (!require_condition(HostFrame_Encode(&hello, encoded,
                                            (uint16_t)sizeof(encoded),
                                            &encoded_length),
                           "empty HELLO frame was not encoded") ||
        !require_condition(encoded_length == sizeof(hello_frame),
                           "encoded empty HELLO size mismatch")) {
        return 1;
    }
    for (index = 0u; index < sizeof(hello_frame); ++index) {
        if (!require_condition(encoded[index] == hello_frame[index],
                               "encoded empty HELLO bytes mismatch")) {
            return 1;
        }
    }

    HostFrameParser_Init(&parser);
    frame = (HostFrame){ 0 };
    for (index = 0u; index < sizeof(hello_frame); ++index) {
        result = HostFrameParser_Push(&parser, hello_frame[index], &frame);
        if (index + 1u < sizeof(hello_frame) &&
            !require_condition(result == HOST_PARSE_NONE,
                               "partial HELLO frame was emitted")) {
            return 1;
        }
    }
    if (!require_condition(result == HOST_PARSE_FRAME,
                           "empty HELLO frame was not emitted") ||
        !require_condition(frame_matches(&frame, 1u, 0x01u, 0x21u, 0x01u,
                                         0u, empty_payload),
                           "empty HELLO frame fields mismatch")) {
        return 1;
    }

    HostFrameParser_Init(&parser);
    frame = (HostFrame){ 0 };
    first_concatenated = (HostFrame){ 0 };
    second_concatenated = (HostFrame){ 0 };
    frame_count = 0u;
    for (concatenated_index = 0u;
         concatenated_index < sizeof(hello_frame) + sizeof(status_frame);
         ++concatenated_index) {
        const uint8_t *source = concatenated_index < sizeof(hello_frame)
                                    ? hello_frame
                                    : status_frame;
        size_t source_index = concatenated_index < sizeof(hello_frame)
                                  ? concatenated_index
                                  : concatenated_index - sizeof(hello_frame);

        result = HostFrameParser_Push(&parser, source[source_index], &frame);
        if (result == HOST_PARSE_FRAME) {
            if (frame_count == 0u) {
                first_concatenated = frame;
            } else if (frame_count == 1u) {
                second_concatenated = frame;
            }
            ++frame_count;
        }
    }
    if (!require_condition(frame_count == 2u,
                           "concatenated frames were not both parsed") ||
        !require_condition(frame_matches(&first_concatenated, 1u, 0x01u,
                                         0x21u, 0x01u, 0u, empty_payload),
                           "first concatenated frame fields mismatch") ||
        !require_condition(frame_matches(&second_concatenated, 1u, 0x01u,
                                         0x07u, 0x02u, 1u, &status_frame[8]),
                            "second concatenated frame fields mismatch")) {
        return 1;
    }

    HostFrameParser_Init(&parser);
    frame = (HostFrame){ 0 };
    for (index = 0u; index < sizeof(unsupported_version_frame); ++index) {
        result = HostFrameParser_Push(&parser, unsupported_version_frame[index],
                                      &frame);
    }
    if (!require_condition(result == HOST_PARSE_VERSION_ERROR,
                           "unsupported protocol version was accepted")) {
        return 1;
    }

    for (index = 0u; index < sizeof(status_frame); ++index) {
        corrupted_status[index] = status_frame[index];
    }
    corrupted_status[8] ^= 0x01u;
    HostFrameParser_Init(&parser);
    frame = (HostFrame){ 0 };
    result = HOST_PARSE_NONE;
    for (index = 0u; index < sizeof(corrupted_status); ++index) {
        result = HostFrameParser_Push(&parser, corrupted_status[index], &frame);
    }
    if (!require_condition(result == HOST_PARSE_CRC_ERROR,
                           "CRC-corrupted frame was accepted") ||
        !require_condition(parse_bytes(&parser, hello_frame,
                                       sizeof(hello_frame), HOST_PARSE_FRAME,
                                       &frame),
                            "parser did not recover after CRC rejection") ||
        !require_condition(frame.sequence == 0x21u,
                           "recovered frame sequence mismatch")) {
        return 1;
    }

    HostFrameParser_Init(&parser);
    frame = (HostFrame){ 0 };
    result = HOST_PARSE_NONE;
    for (index = 0u; index < sizeof(oversized_header); ++index) {
        result = HostFrameParser_Push(&parser, oversized_header[index], &frame);
    }
    if (!require_condition(result == HOST_PARSE_LENGTH_ERROR,
                           "oversized payload length was accepted") ||
        !require_condition(parse_bytes(&parser, hello_frame,
                                       sizeof(hello_frame), HOST_PARSE_FRAME,
                                       &frame),
                            "parser did not recover after length rejection")) {
        return 1;
    }

    HostLink_Init();
    for (index = 0u; index < HOST_RX_RING_SIZE; ++index) {
        if (!require_condition(HostLink_PushRxFromIsr(HOST_LINK_USB,
                                                       (uint8_t)index),
                               "RX ring rejected a byte before becoming full")) {
            return 1;
        }
    }
    if (!require_condition(!HostLink_PushRxFromIsr(HOST_LINK_USB, 0xEEu),
                           "full RX ring did not drop the new byte")) {
        return 1;
    }
    for (index = 0u; index < HOST_RX_RING_SIZE; ++index) {
        if (!require_condition(HostLink_PopRx(HOST_LINK_USB, &value),
                               "RX ring lost an accepted byte") ||
            !require_condition(value == (uint8_t)index,
                               "RX ring changed byte order")) {
            return 1;
        }
    }
    if (!require_condition(!HostLink_PopRx(HOST_LINK_USB, &value),
                           "empty RX ring returned a byte")) {
        return 1;
    }
    if (!require_condition(HostLink_PushRxFromIsr(HOST_LINK_BLUETOOTH, 0xA5u),
                           "Bluetooth RX ring rejected its first byte") ||
        !require_condition(!HostLink_PopRx(HOST_LINK_USB, &value),
                           "USB and Bluetooth RX rings are not independent") ||
        !require_condition(HostLink_PopRx(HOST_LINK_BLUETOOTH, &value) &&
                               value == 0xA5u,
                           "Bluetooth RX ring changed byte order")) {
        return 1;
    }

    HostParam_Init();
    PID_Profiles[0].kp = 2.0f;
    PID_Profiles[0].ki = 0.0f;
    {
        HostParamGroup update = { 0 };
        HostParamGroup readback = { 0 };

        update.group = 0x10u;
        update.count = 2u;
        update.items[0].id = 0x1000u;
        update.items[0].type = HOST_PARAM_FLOAT32;
        update.items[0].value.f32 = 4.5f;
        update.items[1].id = 0x1001u;
        update.items[1].type = HOST_PARAM_FLOAT32;
        update.items[1].value.f32 = 0.25f;
        if (!require_condition(HostParam_SetGroupAtomic(&update) == HOST_ERROR_NONE,
                               "valid PID parameter group was rejected") ||
            !require_condition(HostParam_GetGroup(0x10u, &readback) == HOST_ERROR_NONE,
                               "PID parameter group readback failed") ||
            !require_condition(readback.items[0].value.f32 == 4.5f &&
                                   readback.items[1].value.f32 == 0.25f,
                               "valid PID parameter group was not applied")) {
            return 1;
        }

        update.items[0].value.f32 = 5.5f;
        update.items[1].value.f32 = 2.5f;
        if (!require_condition(HostParam_SetGroupAtomic(&update) ==
                                   HOST_ERROR_PARAM_RANGE,
                               "out-of-range PID group did not report range error") ||
            !require_condition(HostParam_GetGroup(0x10u, &readback) == HOST_ERROR_NONE,
                               "PID parameter group readback after rejection failed") ||
            !require_condition(readback.items[0].value.f32 == 4.5f &&
                                   readback.items[1].value.f32 == 0.25f,
                               "invalid PID group was partially applied")) {
            return 1;
        }
    }

    {
        const HostSafetyCallbacks safety_callbacks = {
            fake_stop,
            fake_emergency_stop
        };
        const HostCommandCallbacks command_callbacks = {
            fake_chassis,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL
        };
        HostFrame hello_request = { 0 };
        HostFrame action_request = { 0 };
        HostFrame response = { 0 };

        fake_action_calls = 0u;
        fake_stop_calls = 0u;
        fake_emergency_calls = 0u;
        HostSafety_Init(&safety_callbacks);
        HostCommands_Init(&command_callbacks);

        hello_request.version = HOST_PROTOCOL_VERSION;
        hello_request.flags = 0x01u;
        hello_request.command = HOST_COMMAND_HELLO;
        if (!require_condition(HostCommands_Handle(&hello_request, &response,
                                                    HOST_LINK_USB, 1000u) ==
                                   HOST_ERROR_NONE,
                               "HELLO command was rejected")) {
            return 1;
        }

        {
            HostFrame get_pid_request = { 0 };
            HostFrame get_pid_page_request = { 0 };

            get_pid_request.version = HOST_PROTOCOL_VERSION;
            get_pid_request.flags = HOST_FLAG_REQUEST;
            get_pid_request.command = HOST_COMMAND_GET_PARAM_GROUP;
            get_pid_request.length = 1u;
            get_pid_request.payload[0] = 0x10u;
            if (!require_condition(HostCommands_Handle(&get_pid_request,
                                                        &response,
                                                        HOST_LINK_USB,
                                                        1000u) ==
                                       HOST_ERROR_NONE,
                                   "PID parameter page zero was rejected") ||
                !require_condition(response.length == HOST_MAX_PAYLOAD,
                                   "PID parameter page zero was not full") ||
                !require_condition(response.payload[0] == 0x10u &&
                                       response.payload[1] == 18u,
                                   "PID parameter page zero count mismatch") ||
                !require_condition(response.payload[2] == 0x00u &&
                                       response.payload[3] == 0x10u,
                                   "PID parameter page zero first ID mismatch")) {
                return 1;
            }

            get_pid_page_request = get_pid_request;
            get_pid_page_request.length = 2u;
            get_pid_page_request.payload[1] = 1u;
            if (!require_condition(HostCommands_Handle(&get_pid_page_request,
                                                        &response,
                                                        HOST_LINK_USB,
                                                        1000u) ==
                                       HOST_ERROR_NONE,
                                   "PID parameter page one was rejected") ||
                !require_condition(response.length == 51u,
                                   "PID parameter page one length mismatch") ||
                !require_condition(response.payload[0] == 0x10u &&
                                       response.payload[1] == 7u,
                                   "PID parameter page one count mismatch") ||
                !require_condition(response.payload[2] == 0x33u &&
                                       response.payload[3] == 0x10u,
                                   "PID parameter page one first ID mismatch") ||
                !require_condition(response.payload[44u] == 0x44u &&
                                       response.payload[45u] == 0x10u,
                                   "PID parameter page one last ID mismatch")) {
                return 1;
            }
        }

        {
            HostFrame invalid_flags_request = { 0 };

            if (!require_condition(HostSafety_Unlock(1000u),
                                   "strict flag test unlock failed")) {
                return 1;
            }
            fake_stop_calls = 0u;
            invalid_flags_request.version = HOST_PROTOCOL_VERSION;
            invalid_flags_request.flags = HOST_FLAG_REQUEST | HOST_FLAG_RESPONSE;
            invalid_flags_request.command = HOST_COMMAND_GET_STATUS;
            if (!require_condition(HostCommands_Handle(&invalid_flags_request,
                                                        &response,
                                                        HOST_LINK_USB,
                                                        1900u) ==
                                       HOST_ERROR_LENGTH,
                                   "non-request flags were accepted") ||
                !require_condition(response.flags ==
                                       (HOST_FLAG_RESPONSE | HOST_FLAG_ERROR),
                                   "invalid flags response was not an error")) {
                return 1;
            }
            HostSafety_Tick(2000u);
            if (!require_condition(fake_stop_calls == 1u,
                                   "invalid flags refreshed watchdog")) {
                return 1;
            }
            HostSafety_Init(&safety_callbacks);
            HostCommands_Init(&command_callbacks);
            if (!require_condition(HostCommands_Handle(&hello_request,
                                                        &response,
                                                        HOST_LINK_USB,
                                                        2100u) ==
                                       HOST_ERROR_NONE,
                                   "HELLO after strict flag reset was rejected")) {
                return 1;
            }
        }

        action_request.version = HOST_PROTOCOL_VERSION;
        action_request.flags = 0x01u;
        action_request.command = HOST_COMMAND_TEST_ACTION;
        action_request.length = 9u;
        action_request.payload[0] = HOST_ACTION_CHASSIS;
        action_request.payload[7] = 50u;
        action_request.payload[8] = 0u;
        if (!require_condition(HostCommands_Handle(&action_request, &response,
                                                    HOST_LINK_USB, 1000u) ==
                                   HOST_ERROR_NOT_UNLOCKED,
                               "locked chassis action was accepted") ||
            !require_condition(fake_action_calls == 0u,
                               "locked chassis action callback was called") ||
            !require_condition(HostSafety_Unlock(1000u),
                               "safety unlock failed")) {
            return 1;
        }
        if (!require_condition(HostCommands_Handle(&action_request, &response,
                                                    HOST_LINK_USB, 1000u) ==
                                   HOST_ERROR_NONE,
                               "unlocked chassis action was rejected") ||
            !require_condition(fake_action_calls == 1u,
                               "unlocked chassis action callback was not called") ||
            !require_condition(HostCommands_Handle(&action_request, &response,
                                                    HOST_LINK_USB, 31001u) ==
                                   HOST_ERROR_NOT_UNLOCKED,
                               "expired unlock still accepted an action")) {
            return 1;
        }

        HostSafety_EmergencyStop();
        if (!require_condition(fake_emergency_calls == 1u,
                               "emergency callback was not called") ||
            !require_condition(HostCommands_Handle(&action_request, &response,
                                                    HOST_LINK_USB, 31002u) ==
                                   HOST_ERROR_EMERGENCY_LOCKED,
                               "emergency state still accepted an action")) {
            return 1;
        }
    }

    {
        const HostSafetyCallbacks safety_callbacks = {
            fake_stop,
            fake_emergency_stop
        };

        fake_stop_calls = 0u;
        fake_emergency_calls = 0u;
        HostSafety_Init(&safety_callbacks);
        HostSafety_SetActiveLink(HOST_LINK_USB);
        HostSafety_NotifyValidFrame(HOST_LINK_USB, 2000u);
        if (!require_condition(HostSafety_Unlock(2000u),
                               "watchdog test unlock failed")) {
            return 1;
        }
        HostSafety_Tick(2999u);
        if (!require_condition(fake_stop_calls == 0u,
                               "watchdog stopped motion too early")) {
            return 1;
        }
        HostSafety_Tick(3000u);
        HostSafety_Tick(4000u);
        if (!require_condition(fake_stop_calls == 1u,
                               "watchdog stop callback was not exactly once") ||
            !require_condition(!HostSafety_IsUnlocked(),
                               "watchdog did not revoke action unlock")) {
            return 1;
        }
    }

    return 0;
}
