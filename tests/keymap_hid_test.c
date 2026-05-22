#include <assert.h>
#include <stdint.h>

#include <hid/hid_report.h>
#include <hid/hid_usage.h>
#include <keymap/keymap.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>

static void test_input_key_codes_map_to_hid_usages(void)
{
    assert(keymap_lookup_input_code_usage(INPUT_KEY_ESC) == HID_USAGE_KEY_ESC);
    assert(keymap_lookup_input_code_usage(INPUT_KEY_NUMLOCK) == HID_USAGE_KEY_NUM_LOCK);
    assert(keymap_lookup_input_code_usage(INPUT_KEY_KPSLASH) == HID_USAGE_KEY_KP_SLASH);
    assert(keymap_lookup_input_code_usage(INPUT_KEY_KPASTERISK) == HID_USAGE_KEY_KP_ASTERISK);
    assert(keymap_lookup_input_code_usage(INPUT_KEY_KPMINUS) == HID_USAGE_KEY_KP_MINUS);
    assert(keymap_lookup_input_code_usage(INPUT_KEY_KPPLUS) == HID_USAGE_KEY_KP_PLUS);
    assert(keymap_lookup_input_code_usage(INPUT_KEY_KPENTER) == HID_USAGE_KEY_KP_ENTER);
    assert(keymap_lookup_input_code_usage(INPUT_KEY_KP0) == HID_USAGE_KEY_KP_0);
    assert(keymap_lookup_input_code_usage(INPUT_KEY_KP1) == HID_USAGE_KEY_KP_1);
    assert(keymap_lookup_input_code_usage(INPUT_KEY_KP2) == HID_USAGE_KEY_KP_2);
    assert(keymap_lookup_input_code_usage(INPUT_KEY_KP3) == HID_USAGE_KEY_KP_3);
    assert(keymap_lookup_input_code_usage(INPUT_KEY_KP4) == HID_USAGE_KEY_KP_4);
    assert(keymap_lookup_input_code_usage(INPUT_KEY_KP5) == HID_USAGE_KEY_KP_5);
    assert(keymap_lookup_input_code_usage(INPUT_KEY_KP6) == HID_USAGE_KEY_KP_6);
    assert(keymap_lookup_input_code_usage(INPUT_KEY_KP7) == HID_USAGE_KEY_KP_7);
    assert(keymap_lookup_input_code_usage(INPUT_KEY_KP8) == HID_USAGE_KEY_KP_8);
    assert(keymap_lookup_input_code_usage(INPUT_KEY_KP9) == HID_USAGE_KEY_KP_9);
    assert(keymap_lookup_input_code_usage(INPUT_KEY_KPDOT) == HID_USAGE_KEY_KP_DOT);
    assert(keymap_lookup_input_code_usage(0xffff) == HID_USAGE_KEY_NONE);
}

static void test_keyboard_report_deduplicates_and_releases_keys(void)
{
    struct hid_keyboard_report report;

    hid_keyboard_report_clear(&report);

    assert(hid_keyboard_report_add_key(&report, HID_USAGE_KEY_KP_1) == 0);
    assert(hid_keyboard_report_add_key(&report, HID_USAGE_KEY_KP_1) == 0);
    assert(report.keys[0] == HID_USAGE_KEY_KP_1);
    assert(report.keys[1] == HID_USAGE_KEY_NONE);

    assert(hid_keyboard_report_remove_key(&report, HID_USAGE_KEY_KP_1) == 0);
    assert(report.keys[0] == HID_USAGE_KEY_NONE);
}

int main(void)
{
    test_input_key_codes_map_to_hid_usages();
    test_keyboard_report_deduplicates_and_releases_keys();
    return 0;
}
