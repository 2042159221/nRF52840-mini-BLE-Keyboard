#ifndef KEYMAP_H
#define KEYMAP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint8_t keymap_lookup_input_code_usage(uint16_t input_code);

#ifdef __cplusplus
}
#endif

#endif
