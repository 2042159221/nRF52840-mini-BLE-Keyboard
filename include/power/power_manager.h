#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <stdbool.h>

#include <power/battery_monitor.h>

#ifdef __cplusplus
extern "C" {
#endif

#define POWER_MANAGER_KEEPALIVE_INTERVAL_MS 8000
#define POWER_MANAGER_KEEPALIVE_PULSE_MS 400
#define POWER_MANAGER_BATTERY_SAMPLE_INTERVAL_MS 5000

static inline bool power_manager_keepalive_timing_valid(void)
{
    return POWER_MANAGER_KEEPALIVE_INTERVAL_MS == 8000 &&
           POWER_MANAGER_KEEPALIVE_PULSE_MS >= 300 &&
           POWER_MANAGER_KEEPALIVE_PULSE_MS <= 500;
}

int power_manager_init(void);
void power_manager_usb_power_present(bool present);
void power_manager_notify_battery_state(enum battery_state state);

#ifdef __cplusplus
}
#endif

#endif
