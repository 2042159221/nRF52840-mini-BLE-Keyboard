#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <power/power_state.h>

static unsigned int listener_call_count;
static struct power_state_snapshot listener_last_snapshot;

static void test_listener(const struct power_state_snapshot *snapshot, void *user_data)
{
    bool *called = user_data;

    listener_call_count++;
    listener_last_snapshot = *snapshot;
    if (called != NULL) {
        *called = true;
    }
}

static struct power_state_snapshot percent_snapshot(uint8_t percent,
                            enum power_level_state level)
{
    struct power_state_snapshot snapshot = { 0 };

    snapshot.timestamp_ms = 1;
    snapshot.estimated_percent = percent;
    snapshot.level_state = level;
    snapshot.charging_state = POWER_CHARGING_STATE_DISCHARGING;
    snapshot.battery_present = true;
    snapshot.source = POWER_STATE_SOURCE_FALLBACK;
    snapshot.valid_flags = POWER_STATE_VALID_PERCENT | POWER_STATE_VALID_LEVEL |
                   POWER_STATE_VALID_CHARGING |
                   POWER_STATE_VALID_BATTERY_PRESENT |
                   POWER_STATE_VALID_SOURCE;
    return snapshot;
}

static void test_percent_estimation_boundaries_are_product_semantics(void)
{
    assert(power_state_estimate_percent(4200, POWER_CHARGING_STATE_DISCHARGING, 0) == 95);
    assert(power_state_estimate_percent(4100, POWER_CHARGING_STATE_DISCHARGING, 0) == 95);
    assert(power_state_estimate_percent(4099, POWER_CHARGING_STATE_DISCHARGING, 0) == 60);
    assert(power_state_estimate_percent(3800, POWER_CHARGING_STATE_DISCHARGING, 0) == 60);
    assert(power_state_estimate_percent(3799, POWER_CHARGING_STATE_DISCHARGING, 0) == 15);
    assert(power_state_estimate_percent(3500, POWER_CHARGING_STATE_DISCHARGING, 0) == 15);
    assert(power_state_estimate_percent(3499, POWER_CHARGING_STATE_DISCHARGING, 0) == 5);
    assert(power_state_estimate_percent(3200, POWER_CHARGING_STATE_FULL,
                        POWER_STATE_VALID_CHARGING) == 100);
}

static void test_latest_snapshot_subscription_and_change_filtering(void)
{
    struct power_state_snapshot snapshot = percent_snapshot(60, POWER_LEVEL_STATE_NORMAL);
    struct power_state_snapshot latest;
    bool called = false;

    power_state_reset_for_test();
    listener_call_count = 0;

    assert(power_state_get_latest(&latest) == -ENODATA);
    assert(power_state_subscribe(test_listener, &called) == 0);

    assert(power_state_publish_if_changed(&snapshot));
    assert(called);
    assert(listener_call_count == 1);
    assert(listener_last_snapshot.estimated_percent == 60);
    assert(power_state_get_latest(&latest) == 0);
    assert(latest.estimated_percent == 60);

    called = false;
    snapshot.timestamp_ms++;
    assert(!power_state_publish_if_changed(&snapshot));
    assert(!called);
    assert(listener_call_count == 1);
    assert(power_state_get_latest(&latest) == 0);
    assert(latest.timestamp_ms == snapshot.timestamp_ms);

    snapshot.estimated_percent = 65;
    assert(power_state_publish_if_changed(&snapshot));
    assert(listener_call_count == 2);
    assert(listener_last_snapshot.estimated_percent == 65);
}

static void test_partial_validity_is_preserved(void)
{
    struct power_state_snapshot snapshot = { 0 };
    struct power_state_snapshot latest;

    power_state_reset_for_test();
    listener_call_count = 0;
    assert(power_state_subscribe(test_listener, NULL) == 0);

    snapshot.timestamp_ms = 10;
    snapshot.voltage_mv = 3890;
    snapshot.valid_flags = POWER_STATE_VALID_VOLTAGE;

    assert(power_state_publish_if_changed(&snapshot));
    assert(listener_call_count == 1);
    assert(power_state_get_latest(&latest) == 0);
    assert(latest.valid_flags == POWER_STATE_VALID_VOLTAGE);
    assert(latest.voltage_mv == 3890);
}

static void test_subscribe_rejects_null_and_deduplicates_listener(void)
{
    struct power_state_snapshot snapshot = percent_snapshot(60, POWER_LEVEL_STATE_NORMAL);
    bool called = false;

    power_state_reset_for_test();
    listener_call_count = 0;

    assert(power_state_subscribe(NULL, NULL) == -EINVAL);
    assert(power_state_subscribe(test_listener, &called) == 0);
    assert(power_state_subscribe(test_listener, &called) == 0);

    assert(power_state_publish_if_changed(&snapshot));
    assert(called);
    assert(listener_call_count == 1);
}

static void test_level_hysteresis_debounces_low_and_critical_edges(void)
{
    struct power_level_hysteresis_state state;

    power_level_hysteresis_init(&state);

    assert(power_level_hysteresis_update(&state, 3700) == POWER_LEVEL_STATE_NORMAL);
    assert(power_level_hysteresis_update(&state, 3499) == POWER_LEVEL_STATE_NORMAL);
    assert(power_level_hysteresis_update(&state, 3499) == POWER_LEVEL_STATE_LOW);
    assert(power_level_hysteresis_update(&state, 3299) == POWER_LEVEL_STATE_LOW);
    assert(power_level_hysteresis_update(&state, 3299) == POWER_LEVEL_STATE_CRITICAL);
    assert(power_level_hysteresis_update(&state, 3460) == POWER_LEVEL_STATE_CRITICAL);
    assert(power_level_hysteresis_update(&state, 3460) == POWER_LEVEL_STATE_LOW);
    assert(power_level_hysteresis_update(&state, 3610) == POWER_LEVEL_STATE_LOW);
    assert(power_level_hysteresis_update(&state, 3610) == POWER_LEVEL_STATE_NORMAL);
}

int main(void)
{
    test_percent_estimation_boundaries_are_product_semantics();
    test_latest_snapshot_subscription_and_change_filtering();
    test_partial_validity_is_preserved();
    test_subscribe_rejects_null_and_deduplicates_listener();
    test_level_hysteresis_debounces_low_and_critical_edges();
    return 0;
}
