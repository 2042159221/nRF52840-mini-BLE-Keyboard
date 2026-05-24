#ifndef RGB_TYPES_H
#define RGB_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RGB_LED_COUNT 17u
#define RGB_NUM_LOCK_INDEX 0u
#define RGB_FADE_DURATION_MS 1000u

enum rgb_mode {
    RGB_MODE_OFF,
    RGB_MODE_STATIC,
    RGB_MODE_KEY_REACTIVE,
};

struct rgb_color {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

#ifdef __cplusplus
}
#endif

#endif
