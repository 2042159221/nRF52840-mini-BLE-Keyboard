#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <rgb/rgb_keymap.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>

bool rgb_keymap_lookup(uint16_t input_code, uint8_t *rgb_index)
{
    uint8_t index;

    switch (input_code) {
    case INPUT_KEY_KPSLASH:
        index = 1;
        break;
    case INPUT_KEY_KPASTERISK:
        index = 2;
        break;
    case INPUT_KEY_KPMINUS:
        index = 3;
        break;
    case INPUT_KEY_KP7:
        index = 4;
        break;
    case INPUT_KEY_KP8:
        index = 5;
        break;
    case INPUT_KEY_KP9:
        index = 6;
        break;
    case INPUT_KEY_KP4:
        index = 7;
        break;
    case INPUT_KEY_KP5:
        index = 8;
        break;
    case INPUT_KEY_KP6:
        index = 9;
        break;
    case INPUT_KEY_KPPLUS:
        index = 10;
        break;
    case INPUT_KEY_KP1:
        index = 11;
        break;
    case INPUT_KEY_KP2:
        index = 12;
        break;
    case INPUT_KEY_KP3:
        index = 13;
        break;
    case INPUT_KEY_KP0:
        index = 14;
        break;
    case INPUT_KEY_KPDOT:
        index = 15;
        break;
    case INPUT_KEY_KPENTER:
        index = 16;
        break;
    default:
        return false;
    }

    if (rgb_index != NULL) {
        *rgb_index = index;
    }

    return true;
}
