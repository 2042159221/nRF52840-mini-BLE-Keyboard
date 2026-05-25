#include <errno.h>
#include <stdint.h>

#include <hid/hid_consumer.h>
#include <input/encoder_action.h>
#include <mode/mode_manager.h>
#include <rgb/rgb_manager.h>
#include <transport/transport.h>

static int send_consumer_pulse(uint16_t usage)
{
    struct hid_consumer_report report;
    enum kb_mode mode = mode_manager_get_mode();
    int err;

    if (!transport_consumer_ready(mode)) {
        return -ENOTCONN;
    }

    hid_consumer_report_press(&report, usage);
    err = transport_send_consumer_report(mode, &report);
    if (err != 0) {
        return err;
    }

    hid_consumer_report_release(&report);
    return transport_send_consumer_report(mode, &report);
}

int encoder_action_trigger_rgb_mode_next(void)
{
    enum rgb_mode mode = rgb_manager_get_mode();

    switch (mode) {
    case RGB_MODE_OFF:
        rgb_manager_set_mode(RGB_MODE_STATIC);
        return 0;
    case RGB_MODE_STATIC:
        rgb_manager_set_mode(RGB_MODE_KEY_REACTIVE);
        return 0;
    case RGB_MODE_KEY_REACTIVE:
    default:
        rgb_manager_set_mode(RGB_MODE_OFF);
        return 0;
    }
}

int encoder_action_trigger(uint8_t action)
{
    switch (action) {
    case APP_ENCODER_ACTION_NONE:
        return 0;
    case APP_ENCODER_ACTION_VOLUME_UP:
        return send_consumer_pulse(HID_CONSUMER_VOLUME_UP);
    case APP_ENCODER_ACTION_VOLUME_DOWN:
        return send_consumer_pulse(HID_CONSUMER_VOLUME_DOWN);
    case APP_ENCODER_ACTION_MUTE:
        return send_consumer_pulse(HID_CONSUMER_MUTE);
    case APP_ENCODER_ACTION_PLAY_PAUSE:
        return send_consumer_pulse(HID_CONSUMER_PLAY_PAUSE);
    case APP_ENCODER_ACTION_NEXT_TRACK:
        return send_consumer_pulse(HID_CONSUMER_NEXT_TRACK);
    case APP_ENCODER_ACTION_PREV_TRACK:
        return send_consumer_pulse(HID_CONSUMER_PREV_TRACK);
    case APP_ENCODER_ACTION_RGB_MODE_NEXT:
        return encoder_action_trigger_rgb_mode_next();
    default:
        return -EINVAL;
    }
}
