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

#include <config/app_config.h>
#include <hid/hid_flowctrl.h>
#include <hid/hid_report.h>
#include <hid/hid_usage.h>
#include <input/input_manager.h>
#include <keymap/keymap.h>

LOG_MODULE_REGISTER(input_manager, LOG_LEVEL_INF);

#define KBD_MATRIX_NODE DT_NODELABEL(kbd_matrix)
static struct hid_keyboard_report keyboard_report;

#if defined(CONFIG_INPUT)
void input_manager_release_all(void)
{
    int err;

    hid_keyboard_report_clear(&keyboard_report);

    err = hid_flowctrl_submit_keyboard_report(&keyboard_report);
    if (err == 0) {
        err = hid_flowctrl_flush_keyboard();
    }

    if (err != 0 && err != -ENOTCONN) {
        LOG_WRN("keyboard release report failed: %d", err);
    }
}

static void input_manager_event_cb(struct input_event *evt, void *user_data)
{
    ARG_UNUSED(user_data);

    if (evt->type != INPUT_EV_KEY) {
        return;
    }

    if (evt->code == INPUT_KEY_MUTE) {
        if (evt->value == 0) {
            return;
        }

        struct app_config config;
        uint8_t action = APP_ENCODER_ACTION_MUTE;
        int err;

        if (app_config_get(&config) == 0) {
            action = config.encoder_press_action;
        }

        LOG_INF("encoder press action: %u", action);

        err = hid_flowctrl_submit_encoder_action(action);
        if (err != 0 && err != -ENOMSG && err != -ENOTCONN) {
            LOG_WRN("encoder press action submit failed: %d", err);
        }

        return;
    }

    const uint8_t usage_id = keymap_lookup_input_code_usage(evt->code);

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

    err = hid_flowctrl_submit_keyboard_report(&keyboard_report);
    if (err != 0) {
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
