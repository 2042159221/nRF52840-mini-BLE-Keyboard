#include <assert.h>
#include <stdint.h>

#include <power/power_state.h>
#include <transport/ble_bas_adapter.h>

static unsigned int bas_set_count;
static uint8_t bas_last_level;

int bt_bas_set_battery_level(uint8_t level)
{
    bas_set_count++;
    bas_last_level = level;
    return 0;
}

static void reset_test_state(void)
{
    power_state_reset_for_test();
    ble_bas_adapter_reset_for_test();
    bas_set_count = 0;
    bas_last_level = 0;
}

static struct power_state_snapshot bas_snapshot(uint8_t percent,
                         enum power_level_state level,
                         enum power_charging_state charging)
{
    struct power_state_snapshot snapshot = { 0 };

    snapshot.timestamp_ms = 1;
    snapshot.estimated_percent = percent;
    snapshot.level_state = level;
    snapshot.charging_state = charging;
    snapshot.battery_present = true;
    snapshot.source = POWER_STATE_SOURCE_FALLBACK;
    snapshot.valid_flags = POWER_STATE_VALID_PERCENT | POWER_STATE_VALID_LEVEL |
                   POWER_STATE_VALID_CHARGING |
                   POWER_STATE_VALID_BATTERY_PRESENT |
                   POWER_STATE_VALID_SOURCE |
                   POWER_STATE_VALID_TIMESTAMP;
    return snapshot;
}

static void test_init_sets_existing_latest_percent_immediately(void)
{
    struct power_state_snapshot snapshot =
        bas_snapshot(62, POWER_LEVEL_STATE_NORMAL, POWER_CHARGING_STATE_DISCHARGING);

    reset_test_state();
    assert(power_state_publish_if_changed(&snapshot));
    assert(ble_bas_adapter_init() == 0);
    assert(bas_set_count == 1);
    assert(bas_last_level == 62);
}

static void test_adapter_filters_duplicate_and_small_percent_changes(void)
{
    struct power_state_snapshot snapshot =
        bas_snapshot(60, POWER_LEVEL_STATE_NORMAL, POWER_CHARGING_STATE_DISCHARGING);

    reset_test_state();
    assert(ble_bas_adapter_init() == 0);

    assert(power_state_publish_if_changed(&snapshot));
    assert(bas_set_count == 1);
    assert(bas_last_level == 60);

    snapshot.timestamp_ms++;
    assert(!power_state_publish_if_changed(&snapshot));
    assert(bas_set_count == 1);

    snapshot.estimated_percent = 63;
    assert(power_state_publish_if_changed(&snapshot));
    assert(bas_set_count == 1);

    snapshot.estimated_percent = 65;
    assert(power_state_publish_if_changed(&snapshot));
    assert(bas_set_count == 2);
    assert(bas_last_level == 65);
}

static void test_adapter_forces_low_and_critical_level_updates(void)
{
    struct power_state_snapshot snapshot =
        bas_snapshot(60, POWER_LEVEL_STATE_NORMAL, POWER_CHARGING_STATE_DISCHARGING);

    reset_test_state();
    assert(ble_bas_adapter_init() == 0);

    assert(power_state_publish_if_changed(&snapshot));
    assert(bas_set_count == 1);

    snapshot.estimated_percent = 62;
    snapshot.level_state = POWER_LEVEL_STATE_LOW;
    assert(power_state_publish_if_changed(&snapshot));
    assert(bas_set_count == 2);
    assert(bas_last_level == 62);

    snapshot.estimated_percent = 63;
    snapshot.level_state = POWER_LEVEL_STATE_CRITICAL;
    assert(power_state_publish_if_changed(&snapshot));
    assert(bas_set_count == 3);
    assert(bas_last_level == 63);
}

static void test_adapter_forces_full_charge_to_one_hundred(void)
{
    struct power_state_snapshot snapshot =
        bas_snapshot(75, POWER_LEVEL_STATE_NORMAL, POWER_CHARGING_STATE_CHARGING);

    reset_test_state();
    assert(ble_bas_adapter_init() == 0);

    assert(power_state_publish_if_changed(&snapshot));
    assert(bas_set_count == 1);
    assert(bas_last_level == 75);

    snapshot.estimated_percent = 76;
    snapshot.charging_state = POWER_CHARGING_STATE_FULL;
    assert(power_state_publish_if_changed(&snapshot));
    assert(bas_set_count == 2);
    assert(bas_last_level == 100);
}

int main(void)
{
    test_init_sets_existing_latest_percent_immediately();
    test_adapter_filters_duplicate_and_small_percent_changes();
    test_adapter_forces_low_and_critical_level_updates();
    test_adapter_forces_full_charge_to_one_hundred();
    return 0;
}
