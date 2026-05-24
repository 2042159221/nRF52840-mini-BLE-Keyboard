#include <assert.h>

#include <power/battery_monitor.h>
#include <power/power_policy.h>
#include <power/power_manager.h>
#include <power/power_state.h>

static void test_policy_maps_normal_without_actions(void)
{
    struct power_policy_state state;

    power_policy_init(&state);

    assert(power_policy_update(&state, BATTERY_STATE_NORMAL) == POWER_POLICY_ACTION_NONE);
    assert(state.battery_state == BATTERY_STATE_NORMAL);
}

static void test_policy_maps_low_and_critical_without_shutdown(void)
{
    struct power_policy_state state;

    power_policy_init(&state);

    assert(power_policy_update(&state, BATTERY_STATE_LOW) == POWER_POLICY_ACTION_LOW_BATTERY);
    assert(state.battery_state == BATTERY_STATE_LOW);
    assert(power_policy_update(&state, BATTERY_STATE_CRITICAL) ==
           POWER_POLICY_ACTION_CRITICAL_BATTERY);
    assert(state.battery_state == BATTERY_STATE_CRITICAL);
}

static void test_policy_repeated_state_is_noop(void)
{
    struct power_policy_state state;

    power_policy_init(&state);

    assert(power_policy_update(&state, BATTERY_STATE_LOW) == POWER_POLICY_ACTION_LOW_BATTERY);
    assert(power_policy_update(&state, BATTERY_STATE_LOW) == POWER_POLICY_ACTION_NONE);
}

static void test_policy_reports_state_change_without_action(void)
{
    struct power_policy_state state;

    power_policy_init(&state);

    assert(power_policy_state_would_change(&state, BATTERY_STATE_NORMAL));
    assert(power_policy_update(&state, BATTERY_STATE_NORMAL) == POWER_POLICY_ACTION_NONE);
    assert(!power_policy_state_would_change(&state, BATTERY_STATE_NORMAL));
    assert(power_policy_state_would_change(&state, BATTERY_STATE_FULL_OR_HIGH));
}

static void test_policy_consumes_power_state_snapshots(void)
{
    struct power_policy_state state;
    struct power_state_snapshot snapshot = { 0 };

    power_policy_init(&state);

    snapshot.level_state = POWER_LEVEL_STATE_LOW;
    snapshot.valid_flags = POWER_STATE_VALID_LEVEL;
    assert(power_policy_update_snapshot(&state, &snapshot) ==
           POWER_POLICY_ACTION_LOW_BATTERY);
    assert(power_policy_update_snapshot(&state, &snapshot) == POWER_POLICY_ACTION_NONE);

    snapshot.level_state = POWER_LEVEL_STATE_CRITICAL;
    assert(power_policy_update_snapshot(&state, &snapshot) ==
           POWER_POLICY_ACTION_CRITICAL_BATTERY);

    snapshot.valid_flags = 0;
    assert(power_policy_update_snapshot(&state, &snapshot) == POWER_POLICY_ACTION_NONE);
}

static void test_keepalive_timing_matches_ip5306_policy(void)
{
    assert(power_manager_keepalive_timing_valid());
}

int main(void)
{
    test_policy_maps_normal_without_actions();
    test_policy_maps_low_and_critical_without_shutdown();
    test_policy_repeated_state_is_noop();
    test_policy_reports_state_change_without_action();
    test_policy_consumes_power_state_snapshots();
    test_keepalive_timing_matches_ip5306_policy();
    return 0;
}
