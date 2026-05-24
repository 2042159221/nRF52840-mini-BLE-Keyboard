#ifndef POWER_STATE_H
#define POWER_STATE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define POWER_STATE_LEVEL_HYSTERESIS_SAMPLES 2u

enum power_level_state {
    POWER_LEVEL_STATE_UNKNOWN,
    POWER_LEVEL_STATE_CRITICAL,
    POWER_LEVEL_STATE_LOW,
    POWER_LEVEL_STATE_NORMAL,
    POWER_LEVEL_STATE_FULL,
};

enum power_charging_state {
    POWER_CHARGING_STATE_UNKNOWN,
    POWER_CHARGING_STATE_DISCHARGING,
    POWER_CHARGING_STATE_CHARGING,
    POWER_CHARGING_STATE_FULL,
};

enum power_state_source {
    POWER_STATE_SOURCE_UNKNOWN,
    POWER_STATE_SOURCE_IP5306,
    POWER_STATE_SOURCE_FALLBACK,
    POWER_STATE_SOURCE_DEFAULT,
};

enum power_state_valid_flag {
    POWER_STATE_VALID_VOLTAGE = 1u << 0,
    POWER_STATE_VALID_PERCENT = 1u << 1,
    POWER_STATE_VALID_LEVEL = 1u << 2,
    POWER_STATE_VALID_USB_PRESENT = 1u << 3,
    POWER_STATE_VALID_CHARGING = 1u << 4,
    POWER_STATE_VALID_BATTERY_PRESENT = 1u << 5,
    POWER_STATE_VALID_SOURCE = 1u << 6,
    POWER_STATE_VALID_TIMESTAMP = 1u << 7,
};

struct power_state_snapshot {
    uint32_t timestamp_ms;
    int32_t voltage_mv;
    uint8_t estimated_percent;
    enum power_level_state level_state;
    bool usb_present;
    enum power_charging_state charging_state;
    uint32_t valid_flags;
    bool battery_present;
    enum power_state_source source;
};

struct power_level_hysteresis_state {
    enum power_level_state level_state;
    uint8_t low_sample_count;
    uint8_t normal_sample_count;
    uint8_t critical_sample_count;
    uint8_t critical_recover_sample_count;
    bool initialized;
};

typedef void (*power_state_listener_t)(const struct power_state_snapshot *snapshot,
                       void *user_data);

uint8_t power_state_estimate_percent(int32_t voltage_mv,
                     enum power_charging_state charging_state,
                     uint32_t valid_flags);
void power_level_hysteresis_init(struct power_level_hysteresis_state *state);
enum power_level_state
power_level_hysteresis_update(struct power_level_hysteresis_state *state,
                  int32_t voltage_mv);

int power_state_subscribe(power_state_listener_t listener, void *user_data);
int power_state_get_latest(struct power_state_snapshot *snapshot);
bool power_state_publish_if_changed(const struct power_state_snapshot *snapshot);

#if defined(UNIT_TEST)
void power_state_reset_for_test(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
