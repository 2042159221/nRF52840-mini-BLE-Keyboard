#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#if !defined(UNIT_TEST)
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#endif

#include <hid/hid_consumer.h>
#include <input/encoder_action.h>
#include <mode/mode_manager.h>
#include <rgb/rgb_manager.h>
#include <transport/transport.h>

#define ENCODER_ACTION_QUEUE_DEPTH 16
#define ENCODER_ACTION_THREAD_STACK_SIZE 1024
#define ENCODER_ACTION_THREAD_PRIORITY 5
#define TRANSPORT_NOT_READY_LOG_INTERVAL_MS 3000

#if !defined(UNIT_TEST)
LOG_MODULE_REGISTER(encoder_action, LOG_LEVEL_INF);

K_MSGQ_DEFINE(encoder_action_msgq, sizeof(uint8_t), ENCODER_ACTION_QUEUE_DEPTH, 1);
K_THREAD_STACK_DEFINE(encoder_action_stack, ENCODER_ACTION_THREAD_STACK_SIZE);

static struct k_thread encoder_action_thread_data;
static bool encoder_action_thread_started;
#endif

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

#if !defined(UNIT_TEST)
static void encoder_action_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (true) {
        uint8_t action;
        int err;

        err = k_msgq_get(&encoder_action_msgq, &action, K_FOREVER);
        if (err != 0) {
            continue;
        }

        err = encoder_action_trigger(action);
        if (err == -ENOTCONN) {
            LOG_WRN_RATELIMIT_RATE(TRANSPORT_NOT_READY_LOG_INTERVAL_MS,
                "encoder action skipped: mode %d consumer transport is not ready",
                mode_manager_get_mode());
        } else if (err != 0) {
            LOG_WRN("encoder action %u failed: %d", action, err);
        }
    }
}

static void encoder_action_thread_start_once(void)
{
    if (encoder_action_thread_started) {
        return;
    }

    k_thread_create(&encoder_action_thread_data, encoder_action_stack,
            K_THREAD_STACK_SIZEOF(encoder_action_stack),
            encoder_action_thread, NULL, NULL, NULL,
            ENCODER_ACTION_THREAD_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&encoder_action_thread_data, "encoder_action");
    encoder_action_thread_started = true;
}

int encoder_action_init(void)
{
    encoder_action_thread_start_once();
    return 0;
}

int encoder_action_submit(uint8_t action)
{
    encoder_action_thread_start_once();
    return k_msgq_put(&encoder_action_msgq, &action, K_NO_WAIT);
}
#else
int encoder_action_init(void)
{
    return 0;
}

int encoder_action_submit(uint8_t action)
{
    return encoder_action_trigger(action);
}
#endif
