#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#if !defined(UNIT_TEST)
#include <zephyr/bluetooth/services/bas.h>
#endif

#include <power/power_state.h>
#include <transport/ble_bas_adapter.h>

#if defined(UNIT_TEST)
int bt_bas_set_battery_level(uint8_t level);
#endif

static bool initialized;
static bool has_last_level;
static uint8_t last_bas_level;
static enum power_level_state last_power_level = POWER_LEVEL_STATE_UNKNOWN;
static enum power_charging_state last_charging_state = POWER_CHARGING_STATE_UNKNOWN;

static uint8_t clamp_percent(uint8_t percent)
{
    return (percent > 100u) ? 100u : percent;
}

static bool should_force_level_update(enum power_level_state level)
{
    return (level == POWER_LEVEL_STATE_LOW || level == POWER_LEVEL_STATE_CRITICAL) &&
           level != last_power_level;
}

static bool ble_bas_adapter_apply_snapshot(const struct power_state_snapshot *snapshot)
{
    uint8_t target_level;
    bool force_update = false;
    bool has_target;

    if (snapshot == NULL) {
        return false;
    }

    has_target = (snapshot->valid_flags & POWER_STATE_VALID_PERCENT) != 0;
    if ((snapshot->valid_flags & POWER_STATE_VALID_CHARGING) != 0 &&
        snapshot->charging_state == POWER_CHARGING_STATE_FULL) {
        target_level = 100;
        has_target = true;
        force_update = (last_charging_state != POWER_CHARGING_STATE_FULL);
    } else {
        target_level = clamp_percent(snapshot->estimated_percent);
    }

    if (!has_target) {
        return false;
    }

    if ((snapshot->valid_flags & POWER_STATE_VALID_LEVEL) != 0 &&
        should_force_level_update(snapshot->level_state)) {
        force_update = true;
    }

    if (has_last_level && !force_update) {
        const int diff = abs((int)target_level - (int)last_bas_level);

        if (diff < 5) {
            last_power_level = snapshot->level_state;
            last_charging_state = snapshot->charging_state;
            return false;
        }
    }

    if (bt_bas_set_battery_level(target_level) != 0) {
        return false;
    }

    has_last_level = true;
    last_bas_level = target_level;
    last_power_level = snapshot->level_state;
    last_charging_state = snapshot->charging_state;
    return true;
}

static void power_state_listener(const struct power_state_snapshot *snapshot,
                 void *user_data)
{
    (void)user_data;

    (void)ble_bas_adapter_apply_snapshot(snapshot);
}

int ble_bas_adapter_init(void)
{
    struct power_state_snapshot snapshot;
    int err;

    if (!initialized) {
        err = power_state_subscribe(power_state_listener, NULL);
        if (err < 0 && err != -EALREADY) {
            return err;
        }
        initialized = true;
    }

    if (power_state_get_latest(&snapshot) == 0) {
        (void)ble_bas_adapter_apply_snapshot(&snapshot);
    }

    return 0;
}

#if defined(UNIT_TEST)
void ble_bas_adapter_reset_for_test(void)
{
    initialized = false;
    has_last_level = false;
    last_bas_level = 0;
    last_power_level = POWER_LEVEL_STATE_UNKNOWN;
    last_charging_state = POWER_CHARGING_STATE_UNKNOWN;
}
#endif
