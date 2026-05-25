#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <power/power_state.h>
#include <rgb/rgb_policy.h>

#define RGB_POLICY_DEFAULT_USER_BRIGHTNESS_PERCENT 30u
#define RGB_POLICY_USB_NORMAL_LIMIT_PERCENT 100u
#define RGB_POLICY_USB_NUM_LIMIT_PERCENT 20u
#define RGB_POLICY_BATTERY_NORMAL_LIMIT_PERCENT 50u
#define RGB_POLICY_BATTERY_NUM_LIMIT_PERCENT 15u
#define RGB_POLICY_LOW_NORMAL_LIMIT_PERCENT 20u
#define RGB_POLICY_LOW_NUM_LIMIT_PERCENT 8u
#define RGB_POLICY_FAST_REFRESH_MS 20u
#define RGB_POLICY_BATTERY_REFRESH_MS 33u
#define RGB_POLICY_LOW_REFRESH_MS 50u

static uint8_t min_percent(uint8_t a, uint8_t b)
{
    return a < b ? a : b;
}

static void rgb_policy_apply_effective_brightness(struct rgb_policy *policy)
{
    policy->normal_brightness_percent =
        min_percent(policy->user_brightness_percent,
                policy->power_limit_percent);
    policy->num_brightness_percent = policy->num_power_limit_percent;
}

void rgb_policy_init(struct rgb_policy *policy)
{
    if (policy == NULL) {
        return;
    }

    policy->user_brightness_percent = RGB_POLICY_DEFAULT_USER_BRIGHTNESS_PERCENT;
    policy->power_limit_percent = RGB_POLICY_BATTERY_NORMAL_LIMIT_PERCENT;
    policy->num_power_limit_percent = RGB_POLICY_BATTERY_NUM_LIMIT_PERCENT;
    rgb_policy_apply_effective_brightness(policy);
    policy->refresh_period_ms = RGB_POLICY_BATTERY_REFRESH_MS;
    policy->critical_shutdown = false;
}

void rgb_policy_set_user_brightness(struct rgb_policy *policy,
                    uint8_t brightness_percent)
{
    if (policy == NULL) {
        return;
    }

    if (brightness_percent > 100u) {
        brightness_percent = 100u;
    }

    policy->user_brightness_percent = brightness_percent;
    rgb_policy_apply_effective_brightness(policy);
}

void rgb_policy_update(struct rgb_policy *policy,
               const struct power_state_snapshot *snapshot)
{
    bool usb_present = false;
    bool level_valid = false;
    enum power_level_state level = POWER_LEVEL_STATE_UNKNOWN;

    if (policy == NULL) {
        return;
    }

    if (snapshot == NULL) {
        rgb_policy_init(policy);
        return;
    }

    if ((snapshot->valid_flags & POWER_STATE_VALID_USB_PRESENT) != 0) {
        usb_present = snapshot->usb_present;
    }

    if ((snapshot->valid_flags & POWER_STATE_VALID_LEVEL) != 0) {
        level_valid = true;
        level = snapshot->level_state;
    }

    if (level_valid && level == POWER_LEVEL_STATE_CRITICAL) {
        policy->power_limit_percent = 0;
        policy->num_power_limit_percent = 0;
        policy->normal_brightness_percent = 0;
        policy->num_brightness_percent = 0;
        policy->refresh_period_ms = RGB_POLICY_LOW_REFRESH_MS;
        policy->critical_shutdown = true;
        return;
    }

    policy->critical_shutdown = false;

    if (level_valid && level == POWER_LEVEL_STATE_LOW) {
        policy->power_limit_percent = RGB_POLICY_LOW_NORMAL_LIMIT_PERCENT;
        policy->num_power_limit_percent = RGB_POLICY_LOW_NUM_LIMIT_PERCENT;
        rgb_policy_apply_effective_brightness(policy);
        policy->refresh_period_ms = RGB_POLICY_LOW_REFRESH_MS;
        return;
    }

    if (usb_present) {
        policy->power_limit_percent = RGB_POLICY_USB_NORMAL_LIMIT_PERCENT;
        policy->num_power_limit_percent = RGB_POLICY_USB_NUM_LIMIT_PERCENT;
        rgb_policy_apply_effective_brightness(policy);
        policy->refresh_period_ms = RGB_POLICY_FAST_REFRESH_MS;
        return;
    }

    policy->power_limit_percent = RGB_POLICY_BATTERY_NORMAL_LIMIT_PERCENT;
    policy->num_power_limit_percent = RGB_POLICY_BATTERY_NUM_LIMIT_PERCENT;
    rgb_policy_apply_effective_brightness(policy);
    policy->refresh_period_ms = RGB_POLICY_BATTERY_REFRESH_MS;
}
