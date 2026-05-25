#ifndef RGB_POLICY_H
#define RGB_POLICY_H

#include <stdbool.h>
#include <stdint.h>

#include <power/power_state.h>

#ifdef __cplusplus
extern "C" {
#endif

struct rgb_policy {
    uint8_t normal_brightness_percent;
    uint8_t num_brightness_percent;
    uint8_t user_brightness_percent;
    uint8_t power_limit_percent;
    uint8_t num_power_limit_percent;
    uint32_t refresh_period_ms;
    bool critical_shutdown;
};

void rgb_policy_init(struct rgb_policy *policy);
void rgb_policy_set_user_brightness(struct rgb_policy *policy,
                    uint8_t brightness_percent);
void rgb_policy_update(struct rgb_policy *policy,
               const struct power_state_snapshot *snapshot);

#ifdef __cplusplus
}
#endif

#endif
