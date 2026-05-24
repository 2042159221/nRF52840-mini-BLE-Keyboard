#ifndef RGB_KEYMAP_H
#define RGB_KEYMAP_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool rgb_keymap_lookup(uint16_t input_code, uint8_t *rgb_index);

#ifdef __cplusplus
}
#endif

#endif
