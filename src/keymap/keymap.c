#include <hid/hid_usage.h>
#include <keymap/keymap.h>

#define KEYMAP_ROWS 6
#define KEYMAP_COLUMNS 4

static const uint8_t usage_map[KEYMAP_ROWS][KEYMAP_COLUMNS] = {
    { HID_USAGE_KEY_NONE, HID_USAGE_KEY_KP_7, HID_USAGE_KEY_KP_8, HID_USAGE_KEY_KP_9 },
    { HID_USAGE_KEY_NONE, HID_USAGE_KEY_KP_4, HID_USAGE_KEY_KP_5, HID_USAGE_KEY_KP_6 },
    { HID_USAGE_KEY_NONE, HID_USAGE_KEY_KP_1, HID_USAGE_KEY_KP_2, HID_USAGE_KEY_KP_3 },
    { HID_USAGE_KEY_KP_ENTER, HID_USAGE_KEY_KP_0, HID_USAGE_KEY_NONE, HID_USAGE_KEY_NONE },
    { HID_USAGE_KEY_NONE, HID_USAGE_KEY_KP_SLASH, HID_USAGE_KEY_KP_ASTERISK, HID_USAGE_KEY_KP_MINUS },
    { HID_USAGE_KEY_NONE, HID_USAGE_KEY_NUM_LOCK, HID_USAGE_KEY_KP_DOT, HID_USAGE_KEY_KP_PLUS },
};

uint8_t keymap_lookup_usage(struct key_position position)
{
    if (position.row >= KEYMAP_ROWS || position.column >= KEYMAP_COLUMNS) {
        return HID_USAGE_KEY_NONE;
    }

    return usage_map[position.row][position.column];
}