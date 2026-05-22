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
#include <keymap/keymap.h>
#include <mode/mode_manager.h>
#include <transport/transport.h>

LOG_MODULE_REGISTER(input_manager, LOG_LEVEL_INF);

#define KBD_MATRIX_NODE DT_NODELABEL(kbd_matrix)
#define TRANSPORT_NOT_READY_LOG_INTERVAL_MS 3000

static struct hid_keyboard_report keyboard_report;

#if defined(CONFIG_INPUT)
static int send_keyboard_report_on_mode(enum kb_mode mode);

static int send_keyboard_report(void)
{
    return send_keyboard_report_on_mode(mode_manager_get_mode());
}

static int send_keyboard_report_on_mode(enum kb_mode mode)
{
    if (!transport_ready(mode)) {
        LOG_DBG("transport for mode %d is not ready", mode);
        return -ENOTCONN;
    }

    return transport_send_keyboard_report(mode, &keyboard_report);
}

void input_manager_release_all(void)
{
    int err;

    hid_keyboard_report_clear(&keyboard_report);

    err = send_keyboard_report();
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

    err = send_keyboard_report();
    if (err == -ENOTCONN) {
        LOG_WRN_RATELIMIT_RATE(TRANSPORT_NOT_READY_LOG_INTERVAL_MS,
            "keyboard report deferred: mode %d transport is not ready",
            mode_manager_get_mode());
    } else if (err != 0) {
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
