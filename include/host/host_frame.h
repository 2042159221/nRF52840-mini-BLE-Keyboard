#ifndef HOST_FRAME_H
#define HOST_FRAME_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HOST_FRAME_MAGIC0 0x55u
#define HOST_FRAME_MAGIC1 0xAAu
#define HOST_FRAME_VERSION 1u
#define HOST_FRAME_HEADER_SIZE 5u
#define HOST_FRAME_CRC_SIZE 2u
#define HOST_FRAME_MAX_PAYLOAD 512u
#define HOST_FRAME_MAX_SIZE (HOST_FRAME_HEADER_SIZE + HOST_FRAME_MAX_PAYLOAD + \
                 HOST_FRAME_CRC_SIZE)

enum host_frame_error {
    HOST_FRAME_OK = 0,
    HOST_FRAME_ERR_INCOMPLETE = -1,
    HOST_FRAME_ERR_BAD_MAGIC = -2,
    HOST_FRAME_ERR_VERSION = -3,
    HOST_FRAME_ERR_LENGTH = -4,
    HOST_FRAME_ERR_CRC = -5,
    HOST_FRAME_ERR_NO_MEMORY = -6,
    HOST_FRAME_ERR_INVALID_ARG = -7,
};

int host_frame_encode(const uint8_t *payload, size_t payload_len,
              uint8_t *out, size_t out_size, size_t *out_len);
int host_frame_decode(const uint8_t *frame, size_t frame_len,
              const uint8_t **payload, size_t *payload_len);
uint16_t host_frame_crc16(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif
