#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <power/power_state.h>
#include <rgb/rgb_policy.h>

#define RGB_POLICY_USB_NORMAL_BRIGHTNESS_PERCENT 40u
#define RGB_POLICY_USB_NUM_BRIGHTNESS_PERCENT 20u
#define RGB_POLICY_BATTERY_NORMAL_BRIGHTNESS_PERCENT 25u
#define RGB_POLICY_BATTERY_NUM_BRIGHTNESS_PERCENT 15u
#define RGB_POLICY_LOW_NORMAL_BRIGHTNESS_PERCENT 10u
#define RGB_POLICY_LOW_NUM_BRIGHTNESS_PERCENT 8u
#define RGB_POLICY_FAST_REFRESH_MS 20u
#define RGB_POLICY_BATTERY_REFRESH_MS 33u
#define RGB_POLICY_LOW_REFRESH_MS 50u

void rgb_policy_init(struct rgb_policy *policy)
{
    if (policy == NULL) {
        return;
    }

    policy->normal_brightness_percent =
        RGB_POLICY_BATTERY_NORMAL_BRIGHTNESS_PERCENT;
    policy->num_brightness_percent =
        RGB_POLICY_BATTERY_NUM_BRIGHTNESS_PERCENT;
    policy->refresh_period_ms = RGB_POLICY_BATTERY_REFRESH_MS;
    policy->critical_shutdown = false;
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
        policy->normal_brightness_percent = 0;
        policy->num_brightness_percent = 0;
        policy->refresh_period_ms = RGB_POLICY_LOW_REFRESH_MS;
        policy->critical_shutdown = true;
        return;
    }

    policy->critical_shutdown = false;

    if (level_valid && level == POWER_LEVEL_STATE_LOW) {
        policy->normal_brightness_percent =
            RGB_POLICY_LOW_NORMAL_BRIGHTNESS_PERCENT;
        policy->num_brightness_percent =
            RGB_POLICY_LOW_NUM_BRIGHTNESS_PERCENT;
        policy->refresh_period_ms = RGB_POLICY_LOW_REFRESH_MS;
        return;
    }

    if (usb_present) {
        policy->normal_brightness_percent =
            RGB_POLICY_USB_NORMAL_BRIGHTNESS_PERCENT;
        policy->num_brightness_percent =
            RGB_POLICY_USB_NUM_BRIGHTNESS_PERCENT;
        policy->refresh_period_ms = RGB_POLICY_FAST_REFRESH_MS;
        return;
    }

    policy->normal_brightness_percent =
        RGB_POLICY_BATTERY_NORMAL_BRIGHTNESS_PERCENT;
    policy->num_brightness_percent =
        RGB_POLICY_BATTERY_NUM_BRIGHTNESS_PERCENT;
    policy->refresh_period_ms = RGB_POLICY_BATTERY_REFRESH_MS;
}
