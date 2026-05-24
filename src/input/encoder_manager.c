#include <errno.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>

#include <hid/hid_consumer.h>
#include <input/encoder_manager.h>
#include <mode/mode_manager.h>
#include <transport/transport.h>

LOG_MODULE_REGISTER(encoder_manager, LOG_LEVEL_INF);

#define ENCODER_NODE DT_NODELABEL(ec11_encoder)
#define TRANSPORT_NOT_READY_LOG_INTERVAL_MS 3000

static int send_consumer_report_on_mode(enum kb_mode mode, uint16_t usage)
{
    struct hid_consumer_report report;
    int err;

    hid_consumer_report_press(&report, usage);
    err = transport_send_consumer_report(mode, &report);
    if (err != 0) {
        return err;
    }

    hid_consumer_report_release(&report);
    return transport_send_consumer_report(mode, &report);
}

static void send_consumer_pulse(uint16_t usage, const char *event_name)
{
    const enum kb_mode mode = mode_manager_get_mode();
    int err = send_consumer_report_on_mode(mode, usage);

    if (err == -ENOTCONN) {
        LOG_WRN_RATELIMIT_RATE(TRANSPORT_NOT_READY_LOG_INTERVAL_MS,
            "%s deferred: mode %d transport is not ready", event_name, mode);
    } else if (err != 0) {
        LOG_WRN("%s send failed: %d", event_name, err);
    }
}

static void encoder_manager_event_cb(struct input_event *evt, void *user_data)
{
    ARG_UNUSED(user_data);

    if (evt->type != INPUT_EV_REL || evt->code != INPUT_REL_WHEEL || evt->value == 0) {
        return;
    }

    const uint16_t usage = evt->value > 0 ? HID_CONSUMER_VOLUME_UP :
                         HID_CONSUMER_VOLUME_DOWN;
    const char *event_name = evt->value > 0 ? "encoder cw" : "encoder ccw";
    uint32_t pulses = evt->value > 0 ? (uint32_t)evt->value : (uint32_t)-evt->value;

    LOG_INF("%s: %d", event_name, evt->value);

    while (pulses-- > 0U) {
        send_consumer_pulse(usage, event_name);
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
