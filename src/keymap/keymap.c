#include <hid/hid_usage.h>
#include <keymap/keymap.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>

uint8_t keymap_lookup_input_code_usage(uint16_t input_code)
{
    switch (input_code) {
    case INPUT_KEY_ESC:
        return HID_USAGE_KEY_ESC;
    case INPUT_KEY_NUMLOCK:
        return HID_USAGE_KEY_NUM_LOCK;
    case INPUT_KEY_KPSLASH:
        return HID_USAGE_KEY_KP_SLASH;
    case INPUT_KEY_KPASTERISK:
        return HID_USAGE_KEY_KP_ASTERISK;
    case INPUT_KEY_KPMINUS:
        return HID_USAGE_KEY_KP_MINUS;
    case INPUT_KEY_KPPLUS:
        return HID_USAGE_KEY_KP_PLUS;
    case INPUT_KEY_KPENTER:
        return HID_USAGE_KEY_KP_ENTER;
    case INPUT_KEY_KP1:
        return HID_USAGE_KEY_KP_1;
    case INPUT_KEY_KP2:
        return HID_USAGE_KEY_KP_2;
    case INPUT_KEY_KP3:
        return HID_USAGE_KEY_KP_3;
    case INPUT_KEY_KP4:
        return HID_USAGE_KEY_KP_4;
    case INPUT_KEY_KP5:
        return HID_USAGE_KEY_KP_5;
    case INPUT_KEY_KP6:
        return HID_USAGE_KEY_KP_6;
    case INPUT_KEY_KP7:
        return HID_USAGE_KEY_KP_7;
    case INPUT_KEY_KP8:
        return HID_USAGE_KEY_KP_8;
    case INPUT_KEY_KP9:
        return HID_USAGE_KEY_KP_9;
    case INPUT_KEY_KP0:
        return HID_USAGE_KEY_KP_0;
    case INPUT_KEY_KPDOT:
        return HID_USAGE_KEY_KP_DOT;
    default:
        return HID_USAGE_KEY_NONE;
    }
}
