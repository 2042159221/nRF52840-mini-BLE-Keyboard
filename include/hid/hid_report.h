#ifndef HID_REPORT_H
#define HID_REPORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HID_KEYBOARD_MAX_KEYS 6

struct hid_keyboard_report {
    uint8_t modifiers;
    uint8_t reserved;
    uint8_t keys[HID_KEYBOARD_MAX_KEYS];
};

void hid_keyboard_report_clear(struct hid_keyboard_report *report);
int hid_keyboard_report_add_key(struct hid_keyboard_report *report, uint8_t usage_id);
int hid_keyboard_report_remove_key(struct hid_keyboard_report *report, uint8_t usage_id);

#ifdef __cplusplus
}
#endif

#endif