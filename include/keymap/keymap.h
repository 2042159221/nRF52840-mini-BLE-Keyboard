#ifndef KEYMAP_H
#define KEYMAP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct key_position {
    uint8_t row;
    uint8_t column;
};

uint8_t keymap_lookup_usage(struct key_position position);

#ifdef __cplusplus
}
#endif

#endif