#ifndef HOST_PROTOCOL_H
#define HOST_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HOST_PROTOCOL_VERSION 1u
#define HOST_PROTOCOL_CAP_RGB_CONFIG (1u << 0)
#define HOST_PROTOCOL_CAP_ENCODER_CONFIG (1u << 1)
#define HOST_PROTOCOL_CAP_SETTINGS_STORE (1u << 2)

int host_protocol_handle_payload(const uint8_t *request, size_t request_len,
                 uint8_t *response, size_t response_size,
                 size_t *response_len);
void host_protocol_reset_session(void);

#ifdef __cplusplus
}
#endif

#endif
