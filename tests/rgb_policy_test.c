#include <assert.h>

#include <power/power_state.h>
#include <rgb/rgb_policy.h>

static struct power_state_snapshot snapshot(enum power_level_state level, bool usb_present)
{
    struct power_state_snapshot state = { 0 };

    state.level_state = level;
    state.usb_present = usb_present;
    state.valid_flags = POWER_STATE_VALID_LEVEL | POWER_STATE_VALID_USB_PRESENT;
    return state;
}

static void test_policy_defaults_to_battery_normal(void)
{
    struct rgb_policy policy;

    rgb_policy_init(&policy);

    assert(policy.normal_brightness_percent == 25);
    assert(policy.num_brightness_percent == 15);
    assert(policy.refresh_period_ms == 33);
    assert(!policy.critical_shutdown);
}

static void test_policy_uses_usb_brightness_when_present(void)
{
    struct rgb_policy policy;
    struct power_state_snapshot state = snapshot(POWER_LEVEL_STATE_NORMAL, true);

    rgb_policy_init(&policy);
    rgb_policy_update(&policy, &state);

    assert(policy.normal_brightness_percent == 40);
    assert(policy.num_brightness_percent == 20);
    assert(policy.refresh_period_ms == 20);
    assert(!policy.critical_shutdown);
}

static void test_policy_lowers_brightness_on_low_battery(void)
{
    struct rgb_policy policy;
    struct power_state_snapshot state = snapshot(POWER_LEVEL_STATE_LOW, false);

    rgb_policy_init(&policy);
    rgb_policy_update(&policy, &state);

    assert(policy.normal_brightness_percent == 10);
    assert(policy.num_brightness_percent == 8);
    assert(policy.refresh_period_ms == 50);
    assert(!policy.critical_shutdown);
}

static void test_policy_turns_off_on_critical(void)
{
    struct rgb_policy policy;
    struct power_state_snapshot state = snapshot(POWER_LEVEL_STATE_CRITICAL, true);

    rgb_policy_init(&policy);
    rgb_policy_update(&policy, &state);

    assert(policy.normal_brightness_percent == 0);
    assert(policy.num_brightness_percent == 0);
    assert(policy.refresh_period_ms == 50);
    assert(policy.critical_shutdown);
}

int main(void)
{
    test_policy_defaults_to_battery_normal();
    test_policy_uses_usb_brightness_when_present();
    test_policy_lowers_brightness_on_low_battery();
    test_policy_turns_off_on_critical();
    return 0;
}
