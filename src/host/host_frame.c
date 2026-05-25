#include <stddef.h>
#include <stdint.h>

#include <host/host_frame.h>

static uint16_t get_le16(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static void put_le16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)(value & 0xffu);
    data[1] = (uint8_t)(value >> 8);
}

uint16_t host_frame_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xffffu;

    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t bit = 0; bit < 8u; bit++) {
            if ((crc & 0x8000u) != 0u) {
                crc = (uint16_t)((crc << 1) ^ 0x1021u);
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}

int host_frame_encode(const uint8_t *payload, size_t payload_len,
              uint8_t *out, size_t out_size, size_t *out_len)
{
    size_t frame_len;
    uint16_t crc;

    if (out == NULL || out_len == NULL ||
        (payload_len > 0u && payload == NULL)) {
        return HOST_FRAME_ERR_INVALID_ARG;
    }

    if (payload_len > HOST_FRAME_MAX_PAYLOAD || payload_len > UINT16_MAX) {
        return HOST_FRAME_ERR_LENGTH;
    }

    frame_len = HOST_FRAME_HEADER_SIZE + payload_len + HOST_FRAME_CRC_SIZE;
    if (out_size < frame_len) {
        return HOST_FRAME_ERR_NO_MEMORY;
    }

    out[0] = HOST_FRAME_MAGIC0;
    out[1] = HOST_FRAME_MAGIC1;
    out[2] = HOST_FRAME_VERSION;
    put_le16(&out[3], (uint16_t)payload_len);
    for (size_t i = 0; i < payload_len; i++) {
        out[HOST_FRAME_HEADER_SIZE + i] = payload[i];
    }

    crc = host_frame_crc16(&out[2], 3u + payload_len);
    put_le16(&out[HOST_FRAME_HEADER_SIZE + payload_len], crc);
    *out_len = frame_len;
    return HOST_FRAME_OK;
}

int host_frame_decode(const uint8_t *frame, size_t frame_len,
              const uint8_t **payload, size_t *payload_len)
{
    uint16_t length;
    uint16_t expected_crc;
    uint16_t actual_crc;
    size_t expected_len;

    if (frame == NULL || payload == NULL || payload_len == NULL) {
        return HOST_FRAME_ERR_INVALID_ARG;
    }

    if (frame_len < HOST_FRAME_HEADER_SIZE) {
        return HOST_FRAME_ERR_INCOMPLETE;
    }

    if (frame[0] != HOST_FRAME_MAGIC0 || frame[1] != HOST_FRAME_MAGIC1) {
        return HOST_FRAME_ERR_BAD_MAGIC;
    }

    if (frame[2] != HOST_FRAME_VERSION) {
        return HOST_FRAME_ERR_VERSION;
    }

    length = get_le16(&frame[3]);
    if (length > HOST_FRAME_MAX_PAYLOAD) {
        return HOST_FRAME_ERR_LENGTH;
    }

    expected_len = HOST_FRAME_HEADER_SIZE + (size_t)length + HOST_FRAME_CRC_SIZE;
    if (frame_len < expected_len) {
        return HOST_FRAME_ERR_INCOMPLETE;
    }

    if (frame_len != expected_len) {
        return HOST_FRAME_ERR_LENGTH;
    }

    expected_crc = get_le16(&frame[HOST_FRAME_HEADER_SIZE + length]);
    actual_crc = host_frame_crc16(&frame[2], 3u + length);
    if (expected_crc != actual_crc) {
        return HOST_FRAME_ERR_CRC;
    }

    *payload = &frame[HOST_FRAME_HEADER_SIZE];
    *payload_len = length;
    return HOST_FRAME_OK;
}
