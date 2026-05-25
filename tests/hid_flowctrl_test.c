#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <config/app_config.h>
#include <hid/hid_flowctrl.h>
#include <hid/hid_report.h>
#include <mode/mode_manager.h>

#define TEST_ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

static enum kb_mode current_mode;
static bool keyboard_ready;
static bool consumer_ready;
static struct app_config current_config;

static struct hid_keyboard_report sent_keyboard_reports[4];
static size_t sent_keyboard_report_count;
static uint8_t triggered_actions[8];
static size_t triggered_action_count;

enum kb_mode mode_manager_get_mode(void)
{
    return current_mode;
}

bool transport_keyboard_ready(enum kb_mode mode)
{
    assert(mode == current_mode);
    return keyboard_ready;
}

bool transport_consumer_ready(enum kb_mode mode)
{
    assert(mode == current_mode);
    return consumer_ready;
}

int transport_send_keyboard_report(enum kb_mode mode,
                   const struct hid_keyboard_report *report)
{
    assert(mode == current_mode);
    assert(report != NULL);
    assert(sent_keyboard_report_count < TEST_ARRAY_SIZE(sent_keyboard_reports));

    sent_keyboard_reports[sent_keyboard_report_count++] = *report;
    return 0;
}

int encoder_action_trigger(uint8_t action)
{
    assert(triggered_action_count < TEST_ARRAY_SIZE(triggered_actions));
    triggered_actions[triggered_action_count++] = action;
    return 0;
}

int app_config_get(struct app_config *out)
{
    assert(out != NULL);
    *out = current_config;
    return 0;
}

static void reset_state(void)
{
    memset(&current_config, 0, sizeof(current_config));
    current_config.encoder_cw_action = APP_ENCODER_ACTION_VOLUME_UP;
    current_config.encoder_ccw_action = APP_ENCODER_ACTION_VOLUME_DOWN;
    current_config.encoder_press_action = APP_ENCODER_ACTION_MUTE;

    current_mode = KB_MODE_USB;
    keyboard_ready = true;
    consumer_ready = true;
    sent_keyboard_report_count = 0;
    triggered_action_count = 0;
    memset(sent_keyboard_reports, 0, sizeof(sent_keyboard_reports));
    memset(triggered_actions, 0, sizeof(triggered_actions));
    hid_flowctrl_reset_for_test();
}

static void test_keyboard_report_uses_latest_overwrite(void)
{
    struct hid_keyboard_report first = {0};
    struct hid_keyboard_report second = {0};

    reset_state();
    first.keys[0] = 0x04;
    second.keys[0] = 0x05;

    assert(hid_flowctrl_submit_keyboard_report(&first) == 0);
    assert(hid_flowctrl_submit_keyboard_report(&second) == 0);
    assert(sent_keyboard_report_count == 0u);

    assert(hid_flowctrl_process_for_test() == 0);
    assert(sent_keyboard_report_count == 1u);
    assert(sent_keyboard_reports[0].keys[0] == 0x05);
}

static void test_ble_not_ready_drops_encoder_delta_before_queue(void)
{
    reset_state();
    current_mode = KB_MODE_BLE;
    consumer_ready = false;

    assert(hid_flowctrl_submit_encoder_delta(9) == -ENOTCONN);
    assert(hid_flowctrl_pending_encoder_delta_for_test() == 0);

    assert(hid_flowctrl_process_for_test() == 0);
    assert(triggered_action_count == 0u);
}

static void test_encoder_delta_is_coalesced_and_rate_limited(void)
{
    reset_state();

    assert(hid_flowctrl_submit_encoder_delta(5) == 0);
    assert(hid_flowctrl_pending_encoder_delta_for_test() == 5);

    assert(hid_flowctrl_process_for_test() == 0);
    assert(triggered_action_count == 2u);
    assert(triggered_actions[0] == APP_ENCODER_ACTION_VOLUME_UP);
    assert(triggered_actions[1] == APP_ENCODER_ACTION_VOLUME_UP);
    assert(hid_flowctrl_pending_encoder_delta_for_test() == 3);

    assert(hid_flowctrl_process_for_test() == 0);
    assert(triggered_action_count == 4u);
    assert(hid_flowctrl_pending_encoder_delta_for_test() == 1);

    assert(hid_flowctrl_process_for_test() == 0);
    assert(triggered_action_count == 5u);
    assert(hid_flowctrl_pending_encoder_delta_for_test() == 0);
}

static void test_consumer_action_fifo_drops_when_full(void)
{
    reset_state();

    for (size_t i = 0; i < HID_FLOWCTRL_CONSUMER_QUEUE_DEPTH; ++i) {
        assert(hid_flowctrl_submit_encoder_action(APP_ENCODER_ACTION_MUTE) == 0);
    }

    assert(hid_flowctrl_submit_encoder_action(APP_ENCODER_ACTION_MUTE) == -ENOMSG);

    assert(hid_flowctrl_process_for_test() == 0);
    assert(triggered_action_count == HID_FLOWCTRL_CONSUMER_ACTIONS_PER_ROUND);
}

int main(void)
{
    test_keyboard_report_uses_latest_overwrite();
    test_ble_not_ready_drops_encoder_delta_before_queue();
    test_encoder_delta_is_coalesced_and_rate_limited();
    test_consumer_action_fifo_drops_when_full();
    return 0;
}
