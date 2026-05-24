#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <power/power_state.h>

#if !defined(UNIT_TEST)
#include <zephyr/kernel.h>
#endif

#define POWER_STATE_MAX_SUBSCRIBERS 6u

struct power_state_subscriber {
    power_state_listener_t listener;
    void *user_data;
};

static struct power_state_snapshot latest_snapshot;
static bool has_latest_snapshot;
static struct power_state_subscriber subscribers[POWER_STATE_MAX_SUBSCRIBERS];
static size_t subscriber_count;

#if !defined(UNIT_TEST)
K_MUTEX_DEFINE(power_state_lock);

#define POWER_STATE_LOCK() k_mutex_lock(&power_state_lock, K_FOREVER)
#define POWER_STATE_UNLOCK() k_mutex_unlock(&power_state_lock)
#else
#define POWER_STATE_LOCK() ((void)0)
#define POWER_STATE_UNLOCK() ((void)0)
#endif

uint8_t power_state_estimate_percent(int32_t voltage_mv,
                     enum power_charging_state charging_state,
                     uint32_t valid_flags)
{
    if ((valid_flags & POWER_STATE_VALID_CHARGING) != 0 &&
        charging_state == POWER_CHARGING_STATE_FULL) {
        return 100;
    }

    if (voltage_mv >= 4100) {
        return 95;
    }

    if (voltage_mv >= 3800) {
        return 60;
    }

    if (voltage_mv >= 3500) {
        return 15;
    }

    return 5;
}

void power_level_hysteresis_init(struct power_level_hysteresis_state *state)
{
    if (state == NULL) {
        return;
    }

    memset(state, 0, sizeof(*state));
    state->level_state = POWER_LEVEL_STATE_UNKNOWN;
}

static void reset_hysteresis_counts(struct power_level_hysteresis_state *state)
{
    state->low_sample_count = 0;
    state->normal_sample_count = 0;
    state->critical_sample_count = 0;
    state->critical_recover_sample_count = 0;
}

static enum power_level_state initial_level_from_mv(int32_t voltage_mv)
{
    if (voltage_mv >= 4100) {
        return POWER_LEVEL_STATE_FULL;
    }

    if (voltage_mv >= 3500) {
        return POWER_LEVEL_STATE_NORMAL;
    }

    if (voltage_mv >= 3300) {
        return POWER_LEVEL_STATE_LOW;
    }

    return POWER_LEVEL_STATE_CRITICAL;
}

enum power_level_state
power_level_hysteresis_update(struct power_level_hysteresis_state *state,
                  int32_t voltage_mv)
{
    if (state == NULL) {
        return POWER_LEVEL_STATE_UNKNOWN;
    }

    if (!state->initialized) {
        state->initialized = true;
        state->level_state = initial_level_from_mv(voltage_mv);
        reset_hysteresis_counts(state);
        return state->level_state;
    }

    switch (state->level_state) {
    case POWER_LEVEL_STATE_FULL:
        if (voltage_mv < 4100 && voltage_mv >= 3500) {
            state->level_state = POWER_LEVEL_STATE_NORMAL;
            reset_hysteresis_counts(state);
            break;
        }
        /* fall through */
    case POWER_LEVEL_STATE_NORMAL:
        if (voltage_mv >= 4100) {
            state->level_state = POWER_LEVEL_STATE_FULL;
            reset_hysteresis_counts(state);
        } else if (voltage_mv < 3500) {
            state->low_sample_count++;
            if (state->low_sample_count >= POWER_STATE_LEVEL_HYSTERESIS_SAMPLES) {
                state->level_state = POWER_LEVEL_STATE_LOW;
                reset_hysteresis_counts(state);
            }
        } else {
            reset_hysteresis_counts(state);
        }
        break;
    case POWER_LEVEL_STATE_LOW:
        if (voltage_mv < 3300) {
            state->critical_sample_count++;
            state->normal_sample_count = 0;
            if (state->critical_sample_count >= POWER_STATE_LEVEL_HYSTERESIS_SAMPLES) {
                state->level_state = POWER_LEVEL_STATE_CRITICAL;
                reset_hysteresis_counts(state);
            }
        } else if (voltage_mv > 3600) {
            state->normal_sample_count++;
            state->critical_sample_count = 0;
            if (state->normal_sample_count >= POWER_STATE_LEVEL_HYSTERESIS_SAMPLES) {
                state->level_state = (voltage_mv >= 4100) ?
                             POWER_LEVEL_STATE_FULL :
                             POWER_LEVEL_STATE_NORMAL;
                reset_hysteresis_counts(state);
            }
        } else {
            reset_hysteresis_counts(state);
        }
        break;
    case POWER_LEVEL_STATE_CRITICAL:
        if (voltage_mv > 3450) {
            state->critical_recover_sample_count++;
            if (state->critical_recover_sample_count >=
                POWER_STATE_LEVEL_HYSTERESIS_SAMPLES) {
                state->level_state = POWER_LEVEL_STATE_LOW;
                reset_hysteresis_counts(state);
            }
        } else {
            reset_hysteresis_counts(state);
        }
        break;
    default:
        state->level_state = initial_level_from_mv(voltage_mv);
        reset_hysteresis_counts(state);
        break;
    }

    return state->level_state;
}

static bool snapshots_differ_for_publish(const struct power_state_snapshot *previous,
                     const struct power_state_snapshot *next)
{
    return previous->voltage_mv != next->voltage_mv ||
           previous->estimated_percent != next->estimated_percent ||
           previous->level_state != next->level_state ||
           previous->usb_present != next->usb_present ||
           previous->charging_state != next->charging_state ||
           previous->valid_flags != next->valid_flags ||
           previous->battery_present != next->battery_present ||
           previous->source != next->source;
}

int power_state_subscribe(power_state_listener_t listener, void *user_data)
{
    size_t i;

    if (listener == NULL) {
        return -EINVAL;
    }

    POWER_STATE_LOCK();
    for (i = 0; i < subscriber_count; i++) {
        if (subscribers[i].listener == listener &&
            subscribers[i].user_data == user_data) {
            POWER_STATE_UNLOCK();
            return 0;
        }
    }

    if (subscriber_count >= POWER_STATE_MAX_SUBSCRIBERS) {
        POWER_STATE_UNLOCK();
        return -ENOMEM;
    }

    subscribers[subscriber_count].listener = listener;
    subscribers[subscriber_count].user_data = user_data;
    subscriber_count++;
    POWER_STATE_UNLOCK();
    return 0;
}

int power_state_get_latest(struct power_state_snapshot *snapshot)
{
    if (snapshot == NULL) {
        return -EINVAL;
    }

    POWER_STATE_LOCK();
    if (!has_latest_snapshot) {
        POWER_STATE_UNLOCK();
        return -ENODATA;
    }

    *snapshot = latest_snapshot;
    POWER_STATE_UNLOCK();
    return 0;
}

bool power_state_publish_if_changed(const struct power_state_snapshot *snapshot)
{
    struct power_state_subscriber local_subscribers[POWER_STATE_MAX_SUBSCRIBERS];
    size_t local_count = 0;
    size_t i;
    bool changed;

    if (snapshot == NULL) {
        return false;
    }

    POWER_STATE_LOCK();
    changed = !has_latest_snapshot ||
          snapshots_differ_for_publish(&latest_snapshot, snapshot);
    latest_snapshot = *snapshot;
    has_latest_snapshot = true;
    if (changed) {
        local_count = subscriber_count;
        memcpy(local_subscribers, subscribers,
               subscriber_count * sizeof(subscribers[0]));
    }
    POWER_STATE_UNLOCK();

    if (!changed) {
        return false;
    }

    for (i = 0; i < local_count; i++) {
        local_subscribers[i].listener(snapshot, local_subscribers[i].user_data);
    }

    return true;
}

#if defined(UNIT_TEST)
void power_state_reset_for_test(void)
{
    memset(&latest_snapshot, 0, sizeof(latest_snapshot));
    has_latest_snapshot = false;
    memset(subscribers, 0, sizeof(subscribers));
    subscriber_count = 0;
}
#endif
