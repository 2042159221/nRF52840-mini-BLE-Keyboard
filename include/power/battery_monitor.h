#ifndef POWER_BATTERY_MONITOR_H
#define POWER_BATTERY_MONITOR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum battery_state {
    BATTERY_STATE_FULL_OR_HIGH,
    BATTERY_STATE_NORMAL,
    BATTERY_STATE_LOW,
    BATTERY_STATE_CRITICAL,
};

struct battery_monitor_sample {
    int32_t raw;
    int32_t adc_mv;
    int32_t vbat_mv;
    enum battery_state state;
};

static inline int32_t battery_monitor_adc_mv_to_vbat_mv(int32_t adc_mv)
{
    return adc_mv * 2;
}

enum battery_state battery_monitor_classify_mv(int32_t vbat_mv);
const char *battery_monitor_state_name(enum battery_state state);

int battery_monitor_init(void);
int battery_monitor_sample_once(struct battery_monitor_sample *sample);

#ifdef __cplusplus
}
#endif

#endif
