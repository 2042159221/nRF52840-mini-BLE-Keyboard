#include <errno.h>
#include <stdint.h>

#include <zephyr/autoconf.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#if defined(CONFIG_INPUT)
#include <zephyr/input/input.h>
#endif
#include <zephyr/logging/log.h>

#include <hid/hid_report.h>
#include <hid/hid_usage.h>
#include <input/input_manager.h>
#include <mode/mode_manager.h>
#include <transport/transport.h>

LOG_MODULE_REGISTER(input_manager, LOG_LEVEL_INF);

#define KBD_MATRIX_NODE DT_NODELABEL(kbd_matrix)
#define MODE_SWITCH_HOLD_MS 2000

static struct hid_keyboard_report keyboard_report;

#if defined(CONFIG_INPUT)
static int send_keyboard_report(void)
{
    const enum kb_mode mode = mode_manager_get_mode();

    if (!transport_ready(mode)) {
        LOG_DBG("transport for mode %d is not ready", mode);
        return -ENOTCONN;
    }

    return transport_send_keyboard_report(mode, &keyboard_report);
}

static void send_tap(uint8_t usage_id)
{
    int err = hid_keyboard_report_add_key(&keyboard_report, usage_id);

    if (err == 0) {
        err = send_keyboard_report();
    }

    if (err != 0 && err != -ENOTCONN) {
        LOG_WRN("keyboard tap press failed: %d", err);
    }

    err = hid_keyboard_report_remove_key(&keyboard_report, usage_id);
    if (err == 0) {
        err = send_keyboard_report();
    }

    if (err != 0 && err != -ENOTCONN) {
        LOG_WRN("keyboard tap release failed: %d", err);
    }
}

static void switch_keyboard_mode(void)
{
    const enum kb_mode current = mode_manager_get_mode();
    const enum kb_mode next = mode_manager_next_supported_mode(current);
    const int err = mode_manager_set_mode(next);

    if (err != 0) {
        LOG_WRN("mode switch from %d to %d failed: %d", current, next, err);
        return;
    }

    LOG_INF("mode switched from %d to %d", current, next);
}

static uint8_t input_code_to_hid_usage(uint16_t code)
{
    switch (code) {
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

static void input_manager_event_cb(struct input_event *evt, void *user_data)
{
    static int64_t mode_key_pressed_at;

    ARG_UNUSED(user_data);

    if (evt->type != INPUT_EV_KEY) {
        return;
    }

    if (evt->code == INPUT_KEY_NUMLOCK) {
        if (evt->value != 0) {
            mode_key_pressed_at = k_uptime_get();
        } else if (mode_key_pressed_at > 0) {
            const int64_t held_ms = k_uptime_get() - mode_key_pressed_at;

            mode_key_pressed_at = 0;
            if (held_ms >= MODE_SWITCH_HOLD_MS) {
                switch_keyboard_mode();
            } else {
                send_tap(HID_USAGE_KEY_NUM_LOCK);
            }
        }

        return;
    }

    const uint8_t usage_id = input_code_to_hid_usage(evt->code);

    if (usage_id == HID_USAGE_KEY_NONE) {
        LOG_DBG("ignored input key code %u", evt->code);
        return;
    }

    int err;

    if (evt->value != 0) {
        err = hid_keyboard_report_add_key(&keyboard_report, usage_id);
    } else {
        err = hid_keyboard_report_remove_key(&keyboard_report, usage_id);
    }

    if (err != 0) {
        LOG_WRN("keyboard report update failed: %d", err);
        return;
    }

    err = send_keyboard_report();
    if (err != 0 && err != -ENOTCONN) {
        LOG_WRN("keyboard report send failed: %d", err);
    }
}

INPUT_CALLBACK_DEFINE(NULL, input_manager_event_cb, NULL);
#endif

int input_manager_init(void)
{
    hid_keyboard_report_clear(&keyboard_report);

#if DT_NODE_HAS_STATUS(KBD_MATRIX_NODE, okay)
    const struct device *matrix = DEVICE_DT_GET(KBD_MATRIX_NODE);

    if (!device_is_ready(matrix)) {
        LOG_ERR("keyboard matrix device is not ready");
        return -ENODEV;
    }

    LOG_INF("keyboard matrix input ready");
    return 0;
#else
    LOG_WRN("keyboard matrix node is disabled or missing");
    return 0;
#endif
}