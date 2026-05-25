#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <config/app_config.h>
#include <hid/hid_consumer.h>
#include <input/encoder_action.h>
#include <mode/mode_manager.h>
#include <rgb/rgb_types.h>

static uint16_t submitted_usages[8];
static unsigned int submitted_count;
static enum rgb_mode current_rgb_mode;
static bool consumer_ready = true;

void hid_consumer_report_press(struct hid_consumer_report *report, uint16_t usage)
{
    report->usage = usage;
}

void hid_consumer_report_release(struct hid_consumer_report *report)
{
    report->usage = HID_CONSUMER_NONE;
}

enum kb_mode mode_manager_get_mode(void)
{
    return KB_MODE_USB;
}

int transport_send_consumer_report(enum kb_mode mode,
                   const struct hid_consumer_report *report)
{
    assert(mode == KB_MODE_USB);
    assert(report != NULL);
    assert(submitted_count < sizeof(submitted_usages) /
           sizeof(submitted_usages[0]));

    submitted_usages[submitted_count++] = report->usage;
    return 0;
}

bool transport_consumer_ready(enum kb_mode mode)
{
    assert(mode == KB_MODE_USB);
    return consumer_ready;
}

enum rgb_mode rgb_manager_get_mode(void)
{
    return current_rgb_mode;
}

void rgb_manager_set_mode(enum rgb_mode mode)
{
    current_rgb_mode = mode;
}

static void reset_transport_spy(void)
{
    consumer_ready = true;
    submitted_count = 0;
    for (size_t i = 0; i < sizeof(submitted_usages) /
         sizeof(submitted_usages[0]); i++) {
        submitted_usages[i] = HID_CONSUMER_NONE;
    }
}

static void assert_consumer_pulse(uint8_t action, uint16_t usage)
{
    reset_transport_spy();
    assert(encoder_action_trigger(action) == 0);
    assert(submitted_count == 2u);
    assert(submitted_usages[0] == usage);
    assert(submitted_usages[1] == HID_CONSUMER_NONE);
}

static void test_none_action_is_noop(void)
{
    reset_transport_spy();
    assert(encoder_action_trigger(APP_ENCODER_ACTION_NONE) == 0);
    assert(submitted_count == 0u);
}

static void test_consumer_actions_send_press_release_pulse(void)
{
    assert_consumer_pulse(APP_ENCODER_ACTION_VOLUME_UP,
                  HID_CONSUMER_VOLUME_UP);
    assert_consumer_pulse(APP_ENCODER_ACTION_VOLUME_DOWN,
                  HID_CONSUMER_VOLUME_DOWN);
    assert_consumer_pulse(APP_ENCODER_ACTION_MUTE, HID_CONSUMER_MUTE);
    assert_consumer_pulse(APP_ENCODER_ACTION_PLAY_PAUSE,
                  HID_CONSUMER_PLAY_PAUSE);
    assert_consumer_pulse(APP_ENCODER_ACTION_NEXT_TRACK,
                  HID_CONSUMER_NEXT_TRACK);
    assert_consumer_pulse(APP_ENCODER_ACTION_PREV_TRACK,
                  HID_CONSUMER_PREV_TRACK);
}

static void test_invalid_action_is_rejected(void)
{
    reset_transport_spy();
    assert(encoder_action_trigger(99u) == -EINVAL);
    assert(submitted_count == 0u);
}

static void test_transport_not_ready_does_not_send_report(void)
{
    reset_transport_spy();
    consumer_ready = false;

    assert(encoder_action_trigger(APP_ENCODER_ACTION_VOLUME_UP) == -ENOTCONN);
    assert(submitted_count == 0u);
}

static void test_rgb_mode_next_cycles_modes(void)
{
    current_rgb_mode = RGB_MODE_OFF;
    assert(encoder_action_trigger(APP_ENCODER_ACTION_RGB_MODE_NEXT) == 0);
    assert(current_rgb_mode == RGB_MODE_STATIC);

    assert(encoder_action_trigger(APP_ENCODER_ACTION_RGB_MODE_NEXT) == 0);
    assert(current_rgb_mode == RGB_MODE_KEY_REACTIVE);

    assert(encoder_action_trigger(APP_ENCODER_ACTION_RGB_MODE_NEXT) == 0);
    assert(current_rgb_mode == RGB_MODE_OFF);
}

int main(void)
{
    test_none_action_is_noop();
    test_consumer_actions_send_press_release_pulse();
    test_invalid_action_is_rejected();
    test_transport_not_ready_does_not_send_report();
    test_rgb_mode_next_cycles_modes();
    return 0;
}
