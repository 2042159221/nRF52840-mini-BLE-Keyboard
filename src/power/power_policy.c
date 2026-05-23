#include <stdbool.h>
#include <stddef.h>

#include <power/power_policy.h>

void power_policy_init(struct power_policy_state *state)
{
    if (state == NULL) {
        return;
    }

    state->battery_state = BATTERY_STATE_NORMAL;
    state->has_battery_state = false;
}

bool power_policy_state_would_change(const struct power_policy_state *state,
                     enum battery_state battery_state)
{
    if (state == NULL) {
        return false;
    }

    return !state->has_battery_state || state->battery_state != battery_state;
}

enum power_policy_action power_policy_update(struct power_policy_state *state,
                         enum battery_state battery_state)
{
    if (state == NULL) {
        return POWER_POLICY_ACTION_NONE;
    }

    if (!power_policy_state_would_change(state, battery_state)) {
        return POWER_POLICY_ACTION_NONE;
    }

    state->battery_state = battery_state;
    state->has_battery_state = true;

    if (battery_state == BATTERY_STATE_CRITICAL) {
        return POWER_POLICY_ACTION_CRITICAL_BATTERY;
    }

    if (battery_state == BATTERY_STATE_LOW) {
        return POWER_POLICY_ACTION_LOW_BATTERY;
    }

    return POWER_POLICY_ACTION_NONE;
}
