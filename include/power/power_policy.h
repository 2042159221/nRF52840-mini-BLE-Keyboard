#ifndef POWER_POLICY_H
#define POWER_POLICY_H

#include <stdbool.h>

#include <power/battery_monitor.h>

#ifdef __cplusplus
extern "C" {
#endif

enum power_policy_action {
    POWER_POLICY_ACTION_NONE,
    POWER_POLICY_ACTION_LOW_BATTERY,
    POWER_POLICY_ACTION_CRITICAL_BATTERY,
};

struct power_policy_state {
    enum battery_state battery_state;
    bool has_battery_state;
};

void power_policy_init(struct power_policy_state *state);
bool power_policy_state_would_change(const struct power_policy_state *state,
                     enum battery_state battery_state);
enum power_policy_action power_policy_update(struct power_policy_state *state,
                         enum battery_state battery_state);

#ifdef __cplusplus
}
#endif

#endif
