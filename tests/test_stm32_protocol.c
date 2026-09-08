#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "host_config.h"
#include "host_crc16.h"
#include "host_frame.h"
#include "host_link.h"

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

    return 0;
}
