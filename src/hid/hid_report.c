#include <errno.h>
#include <stddef.h>
#include <string.h>

#include <hid/hid_report.h>
#include <hid/hid_usage.h>

void hid_keyboard_report_clear(struct hid_keyboard_report *report)
{
    if (report == NULL) {
        return;
    }

    memset(report, 0, sizeof(*report));
}

int hid_keyboard_report_add_key(struct hid_keyboard_report *report, uint8_t usage_id)
{
    if (report == NULL) {
        return -EINVAL;
    }

    if (usage_id == HID_USAGE_KEY_NONE) {
        return 0;
    }

    for (size_t index = 0; index < HID_KEYBOARD_MAX_KEYS; index++) {
        if (report->keys[index] == usage_id) {
            return 0;
        }
    }

    for (size_t index = 0; index < HID_KEYBOARD_MAX_KEYS; index++) {
        if (report->keys[index] == HID_USAGE_KEY_NONE) {
            report->keys[index] = usage_id;
            return 0;
        }
    }

    return -ENOSPC;
}

int hid_keyboard_report_remove_key(struct hid_keyboard_report *report, uint8_t usage_id)
{
    if (report == NULL) {
        return -EINVAL;
    }

    if (usage_id == HID_USAGE_KEY_NONE) {
        return 0;
    }

    for (size_t index = 0; index < HID_KEYBOARD_MAX_KEYS; index++) {
        if (report->keys[index] == usage_id) {
            report->keys[index] = HID_USAGE_KEY_NONE;
            return 0;
        }
    }

    return 0;
}