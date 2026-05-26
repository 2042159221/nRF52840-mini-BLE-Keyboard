#include <errno.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>

#include <display/status_screen.h>
#include <hid/hid_flowctrl.h>
#include <input/encoder_manager.h>

LOG_MODULE_REGISTER(encoder_manager, LOG_LEVEL_INF);

#define ENCODER_NODE DT_NODELABEL(ec11_encoder)

static void encoder_manager_event_cb(struct input_event *evt, void *user_data)
{
    const char *event_name;
    int err;

    ARG_UNUSED(user_data);

    if (evt->type != INPUT_EV_REL || evt->code != INPUT_REL_WHEEL || evt->value == 0) {
        return;
    }

    event_name = evt->value > 0 ? "encoder cw" : "encoder ccw";

    LOG_DBG("%s: %d", event_name, evt->value);

    if (status_screen_handle_encoder_delta(evt->value)) {
        return;
    }

    err = hid_flowctrl_submit_encoder_delta(evt->value);
    if (err != 0 && err != -ENOTCONN) {
        LOG_WRN_RATELIMIT(
            "%s delta submit failed: %d", event_name, err);
    }
}

INPUT_CALLBACK_DEFINE(NULL, encoder_manager_event_cb, NULL);

int encoder_manager_init(void)
{
#if DT_NODE_HAS_STATUS(ENCODER_NODE, okay)
    const struct device *encoder = DEVICE_DT_GET(ENCODER_NODE);

    if (!device_is_ready(encoder)) {
        LOG_ERR("EC11 encoder device is not ready");
        return -ENODEV;
    }

    LOG_INF("EC11 encoder input ready");
    return 0;
#else
    LOG_WRN("EC11 encoder node is disabled or missing");
    return 0;
#endif
}
