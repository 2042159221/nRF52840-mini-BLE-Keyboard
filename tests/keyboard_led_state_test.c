#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <keyboard/keyboard_led_state.h>

static unsigned int listener_calls;
static uint8_t listener_last_bits;

static void listener(uint8_t led_bits, void *user_data)
{
    bool *called = user_data;

    listener_calls++;
    listener_last_bits = led_bits;
    if (called != NULL) {
        *called = true;
    }
}

static void test_num_lock_bit_is_parsed(void)
{
    keyboard_led_state_reset_for_test();

    keyboard_led_state_update(KEYBOARD_LED_NUM_LOCK);
    assert(keyboard_led_state_get() == KEYBOARD_LED_NUM_LOCK);
    assert(keyboard_led_state_num_lock());

    keyboard_led_state_update(0);
    assert(keyboard_led_state_get() == 0);
    assert(!keyboard_led_state_num_lock());
}

static void test_listener_only_runs_on_change(void)
{
    bool called = false;

    keyboard_led_state_reset_for_test();
    listener_calls = 0;
    listener_last_bits = 0xff;

    assert(keyboard_led_state_subscribe(listener, &called) == 0);
    keyboard_led_state_update(KEYBOARD_LED_NUM_LOCK);
    assert(called);
    assert(listener_calls == 1);
    assert(listener_last_bits == KEYBOARD_LED_NUM_LOCK);

    called = false;
    keyboard_led_state_update(KEYBOARD_LED_NUM_LOCK);
    assert(!called);
    assert(listener_calls == 1);

    keyboard_led_state_update(0);
    assert(listener_calls == 2);
    assert(listener_last_bits == 0);
}

static void test_listener_validation_and_deduplication(void)
{
    bool called = false;

    keyboard_led_state_reset_for_test();
    listener_calls = 0;

    assert(keyboard_led_state_subscribe(NULL, NULL) == -EINVAL);
    assert(keyboard_led_state_subscribe(listener, &called) == 0);
    assert(keyboard_led_state_subscribe(listener, &called) == 0);
    keyboard_led_state_update(KEYBOARD_LED_NUM_LOCK);
    assert(listener_calls == 1);
}

static void test_hid_output_report_parses_payload_without_report_id(void)
{
    const uint8_t led_off[] = { 0 };

    keyboard_led_state_reset_for_test();
    keyboard_led_state_update(KEYBOARD_LED_NUM_LOCK);

    assert(keyboard_led_state_update_from_hid_output_report(
               1, 1, led_off, sizeof(led_off)));
    assert(!keyboard_led_state_num_lock());
}

static void test_hid_output_report_skips_report_id_prefix_when_present(void)
{
    const uint8_t report_id_then_led_off[] = { 1, 0 };

    keyboard_led_state_reset_for_test();
    keyboard_led_state_update(KEYBOARD_LED_NUM_LOCK);

    assert(keyboard_led_state_update_from_hid_output_report(
               1, 1, report_id_then_led_off,
               sizeof(report_id_then_led_off)));
    assert(!keyboard_led_state_num_lock());
}

static void test_hid_output_report_ignores_other_report_ids(void)
{
    const uint8_t led_on[] = { KEYBOARD_LED_NUM_LOCK };

    keyboard_led_state_reset_for_test();

    assert(!keyboard_led_state_update_from_hid_output_report(
        2, 1, led_on, sizeof(led_on)));
    assert(!keyboard_led_state_num_lock());
}

int main(void)
{
    test_num_lock_bit_is_parsed();
    test_listener_only_runs_on_change();
    test_listener_validation_and_deduplication();
    test_hid_output_report_parses_payload_without_report_id();
    test_hid_output_report_skips_report_id_prefix_when_present();
    test_hid_output_report_ignores_other_report_ids();
    return 0;
}
