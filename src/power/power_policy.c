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
    state->level_state = POWER_LEVEL_STATE_UNKNOWN;
    state->has_level_state = false;
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
    state->level_state = (battery_state == BATTERY_STATE_CRITICAL) ?
                 POWER_LEVEL_STATE_CRITICAL :
                 (battery_state == BATTERY_STATE_LOW) ?
                 POWER_LEVEL_STATE_LOW :
                 (battery_state == BATTERY_STATE_FULL_OR_HIGH) ?
                 POWER_LEVEL_STATE_FULL :
                 POWER_LEVEL_STATE_NORMAL;
    state->has_level_state = true;

    if (battery_state == BATTERY_STATE_CRITICAL) {
        return POWER_POLICY_ACTION_CRITICAL_BATTERY;
    }

    if (battery_state == BATTERY_STATE_LOW) {
        return POWER_POLICY_ACTION_LOW_BATTERY;
    }

    return POWER_POLICY_ACTION_NONE;
}

enum power_policy_action
power_policy_update_snapshot(struct power_policy_state *state,
                 const struct power_state_snapshot *snapshot)
{
    if (state == NULL || snapshot == NULL ||
        (snapshot->valid_flags & POWER_STATE_VALID_LEVEL) == 0) {
        return POWER_POLICY_ACTION_NONE;
    }

    if (state->has_level_state && state->level_state == snapshot->level_state) {
        return POWER_POLICY_ACTION_NONE;
    }

    state->level_state = snapshot->level_state;
    state->has_level_state = true;

    switch (snapshot->level_state) {
    case POWER_LEVEL_STATE_CRITICAL:
        state->battery_state = BATTERY_STATE_CRITICAL;
        state->has_battery_state = true;
        return POWER_POLICY_ACTION_CRITICAL_BATTERY;
    case POWER_LEVEL_STATE_LOW:
        state->battery_state = BATTERY_STATE_LOW;
        state->has_battery_state = true;
        return POWER_POLICY_ACTION_LOW_BATTERY;
    case POWER_LEVEL_STATE_FULL:
        state->battery_state = BATTERY_STATE_FULL_OR_HIGH;
        state->has_battery_state = true;
        return POWER_POLICY_ACTION_NONE;
    case POWER_LEVEL_STATE_NORMAL:
        state->battery_state = BATTERY_STATE_NORMAL;
        state->has_battery_state = true;
        return POWER_POLICY_ACTION_NONE;
    default:
        return POWER_POLICY_ACTION_NONE;
    }
}
