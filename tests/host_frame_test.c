#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <host/host_frame.h>

static void test_frame_round_trip(void)
{
    const uint8_t payload[] = { 0x01, 0x02, 0x03 };
    uint8_t frame[HOST_FRAME_MAX_SIZE];
    const uint8_t *decoded;
    size_t frame_len;
    size_t decoded_len;

    assert(host_frame_encode(payload, sizeof(payload), frame, sizeof(frame),
                 &frame_len) == HOST_FRAME_OK);
    assert(frame[0] == HOST_FRAME_MAGIC0);
    assert(frame[1] == HOST_FRAME_MAGIC1);
    assert(frame[2] == HOST_FRAME_VERSION);
    assert(host_frame_decode(frame, frame_len, &decoded, &decoded_len) ==
           HOST_FRAME_OK);
    assert(decoded_len == sizeof(payload));
    assert(memcmp(decoded, payload, sizeof(payload)) == 0);
}

static void test_decode_rejects_bad_magic(void)
{
    const uint8_t payload[] = { 0x01 };
    uint8_t frame[HOST_FRAME_MAX_SIZE];
    const uint8_t *decoded;
    size_t frame_len;
    size_t decoded_len;

    assert(host_frame_encode(payload, sizeof(payload), frame, sizeof(frame),
                 &frame_len) == HOST_FRAME_OK);
    frame[0] = 0x00;
    assert(host_frame_decode(frame, frame_len, &decoded, &decoded_len) ==
           HOST_FRAME_ERR_BAD_MAGIC);
}

static void test_decode_rejects_bad_version(void)
{
    const uint8_t payload[] = { 0x01 };
    uint8_t frame[HOST_FRAME_MAX_SIZE];
    const uint8_t *decoded;
    size_t frame_len;
    size_t decoded_len;

    assert(host_frame_encode(payload, sizeof(payload), frame, sizeof(frame),
                 &frame_len) == HOST_FRAME_OK);
    frame[2] = HOST_FRAME_VERSION + 1u;
    assert(host_frame_decode(frame, frame_len, &decoded, &decoded_len) ==
           HOST_FRAME_ERR_VERSION);
}

static void test_decode_rejects_bad_length(void)
{
    const uint8_t payload[] = { 0x01 };
    uint8_t frame[HOST_FRAME_MAX_SIZE];
    const uint8_t *decoded;
    size_t frame_len;
    size_t decoded_len;

    assert(host_frame_encode(payload, sizeof(payload), frame, sizeof(frame),
                 &frame_len) == HOST_FRAME_OK);
    frame[3] = (uint8_t)((HOST_FRAME_MAX_PAYLOAD + 1u) & 0xffu);
    frame[4] = (uint8_t)((HOST_FRAME_MAX_PAYLOAD + 1u) >> 8);
    assert(host_frame_decode(frame, frame_len, &decoded, &decoded_len) ==
           HOST_FRAME_ERR_LENGTH);
}

static void test_decode_rejects_bad_crc(void)
{
    const uint8_t payload[] = { 0x01 };
    uint8_t frame[HOST_FRAME_MAX_SIZE];
    const uint8_t *decoded;
    size_t frame_len;
    size_t decoded_len;

    assert(host_frame_encode(payload, sizeof(payload), frame, sizeof(frame),
                 &frame_len) == HOST_FRAME_OK);
    frame[HOST_FRAME_HEADER_SIZE] ^= 0x55u;
    assert(host_frame_decode(frame, frame_len, &decoded, &decoded_len) ==
           HOST_FRAME_ERR_CRC);
}

static void test_decode_reports_incomplete_frame(void)
{
    const uint8_t payload[] = { 0x01, 0x02 };
    uint8_t frame[HOST_FRAME_MAX_SIZE];
    const uint8_t *decoded;
    size_t frame_len;
    size_t decoded_len;

    assert(host_frame_encode(payload, sizeof(payload), frame, sizeof(frame),
                 &frame_len) == HOST_FRAME_OK);
    assert(host_frame_decode(frame, frame_len - 1u, &decoded, &decoded_len) ==
           HOST_FRAME_ERR_INCOMPLETE);
}

static void test_frame_allows_empty_payload(void)
{
    uint8_t frame[HOST_FRAME_MAX_SIZE];
    const uint8_t *decoded;
    size_t frame_len;
    size_t decoded_len;

    assert(host_frame_encode(NULL, 0u, frame, sizeof(frame),
                 &frame_len) == HOST_FRAME_OK);
    assert(frame_len == HOST_FRAME_HEADER_SIZE + HOST_FRAME_CRC_SIZE);
    assert(host_frame_decode(frame, frame_len, &decoded, &decoded_len) ==
           HOST_FRAME_OK);
    assert(decoded_len == 0u);
}

static void test_encode_rejects_oversized_payload(void)
{
    uint8_t payload[HOST_FRAME_MAX_PAYLOAD + 1u] = { 0 };
    uint8_t frame[HOST_FRAME_MAX_SIZE];
    size_t frame_len;

    assert(host_frame_encode(payload, sizeof(payload), frame, sizeof(frame),
                 &frame_len) == HOST_FRAME_ERR_LENGTH);
}

int main(void)
{
    test_frame_round_trip();
    test_decode_rejects_bad_magic();
    test_decode_rejects_bad_version();
    test_decode_rejects_bad_length();
    test_decode_rejects_bad_crc();
    test_decode_reports_incomplete_frame();
    test_frame_allows_empty_payload();
    test_encode_rejects_oversized_payload();
    return 0;
}
