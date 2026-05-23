#include <assert.h>
#include <stdint.h>

#include <power/battery_monitor.h>

static void test_vbat_conversion_applies_enabled_divider_ratio(void)
{
    assert(battery_monitor_adc_mv_to_vbat_mv(1800) == 3600);
    assert(battery_monitor_adc_mv_to_vbat_mv(2049) == 4098);
}

static void test_battery_state_boundaries_are_explicit(void)
{
    assert(battery_monitor_classify_mv(4200) == BATTERY_STATE_FULL_OR_HIGH);
    assert(battery_monitor_classify_mv(4100) == BATTERY_STATE_FULL_OR_HIGH);
    assert(battery_monitor_classify_mv(4099) == BATTERY_STATE_NORMAL);
    assert(battery_monitor_classify_mv(3800) == BATTERY_STATE_NORMAL);
    assert(battery_monitor_classify_mv(3799) == BATTERY_STATE_LOW);
    assert(battery_monitor_classify_mv(3600) == BATTERY_STATE_LOW);
    assert(battery_monitor_classify_mv(3599) == BATTERY_STATE_LOW);
    assert(battery_monitor_classify_mv(3500) == BATTERY_STATE_LOW);
    assert(battery_monitor_classify_mv(3499) == BATTERY_STATE_CRITICAL);
}

int main(void)
{
    test_vbat_conversion_applies_enabled_divider_ratio();
    test_battery_state_boundaries_are_explicit();
    return 0;
}
