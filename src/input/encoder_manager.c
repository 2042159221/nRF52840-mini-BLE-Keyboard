#include <errno.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>

#include <config/app_config.h>
#include <input/encoder_action.h>
#include <input/encoder_manager.h>

LOG_MODULE_REGISTER(encoder_manager, LOG_LEVEL_INF);

#define ENCODER_NODE DT_NODELABEL(ec11_encoder)
#define TRANSPORT_NOT_READY_LOG_INTERVAL_MS 3000

static void encoder_manager_event_cb(struct input_event *evt, void *user_data)
{
    struct app_config config;
    uint8_t action = APP_ENCODER_ACTION_NONE;
    const char *event_name;
    uint32_t pulses;

    ARG_UNUSED(user_data);

    if (evt->type != INPUT_EV_REL || evt->code != INPUT_REL_WHEEL || evt->value == 0) {
        return;
    }

    event_name = evt->value > 0 ? "encoder cw" : "encoder ccw";
    pulses = evt->value > 0 ? (uint32_t)evt->value : (uint32_t)-evt->value;

    LOG_INF("%s: %d", event_name, evt->value);

    if (app_config_get(&config) == 0) {
        action = evt->value > 0 ? config.encoder_cw_action :
                 config.encoder_ccw_action;
    }

    while (pulses-- > 0U) {
        int err = encoder_action_trigger(action);

        if (err == -ENOTCONN) {
            LOG_WRN_RATELIMIT_RATE(TRANSPORT_NOT_READY_LOG_INTERVAL_MS,
                "%s deferred: transport is not ready", event_name);
        } else if (err != 0) {
            LOG_WRN("%s action failed: %d", event_name, err);
        }
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
