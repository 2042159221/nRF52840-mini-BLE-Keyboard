#include <errno.h>
#include <stdbool.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <power/battery_monitor.h>
#include <power/ip5306.h>
#include <power/power_manager.h>
#include <power/power_policy.h>
#include <power/power_state.h>

LOG_MODULE_REGISTER(power_manager, LOG_LEVEL_INF);

#define BATTERY_POWER_NODE DT_PATH(zephyr_user)

static const struct gpio_dt_spec ip5306_wakeup =
    GPIO_DT_SPEC_GET(BATTERY_POWER_NODE, wakeup_gpios);
static struct k_work_delayable keepalive_work;
static struct k_work_delayable keepalive_release_work;
static struct k_work_delayable battery_sample_work;
static struct power_policy_state policy_state;
static struct power_level_hysteresis_state level_filter;
static bool power_manager_ready;
static bool usb_power_present;
static bool battery_monitor_available;
static bool ip5306_available;

static enum power_charging_state
power_manager_charging_state_from_ip5306(enum ip5306_charging_state state)
{
    switch (state) {
    case IP5306_CHARGING_STATE_DISCHARGING:
        return POWER_CHARGING_STATE_DISCHARGING;
    case IP5306_CHARGING_STATE_CHARGING:
        return POWER_CHARGING_STATE_CHARGING;
    case IP5306_CHARGING_STATE_FULL:
        return POWER_CHARGING_STATE_FULL;
    default:
        return POWER_CHARGING_STATE_UNKNOWN;
    }
}

static void power_manager_log_policy_action(enum power_policy_action action)
{
    if (action == POWER_POLICY_ACTION_LOW_BATTERY) {
        LOG_WRN("power: low battery policy hook");
    } else if (action == POWER_POLICY_ACTION_CRITICAL_BATTERY) {
        LOG_ERR("power: critical battery policy hook");
    }
}

static void power_manager_policy_listener(const struct power_state_snapshot *snapshot,
                      void *user_data)
{
    struct power_policy_state *state = user_data;
    enum power_policy_action action;

    action = power_policy_update_snapshot(state, snapshot);
    power_manager_log_policy_action(action);
}

static void power_manager_apply_ip5306_status(struct power_state_snapshot *snapshot)
{
    struct ip5306_status status;
    int err;

    snapshot->usb_present = usb_power_present;
    snapshot->valid_flags |= POWER_STATE_VALID_USB_PRESENT;

    if (!ip5306_available) {
        return;
    }

    err = ip5306_get_status(&status);
    if (err < 0) {
        LOG_DBG("power: ip5306 status unavailable: %d", err);
        return;
    }

    if ((status.valid_flags & IP5306_STATUS_VALID_USB_PRESENT) != 0) {
        snapshot->usb_present = usb_power_present || status.usb_present;
        snapshot->valid_flags |= POWER_STATE_VALID_USB_PRESENT;
    }

    if ((status.valid_flags & IP5306_STATUS_VALID_CHARGING_STATE) != 0) {
        snapshot->charging_state =
            power_manager_charging_state_from_ip5306(status.charging_state);
        snapshot->valid_flags |= POWER_STATE_VALID_CHARGING;
    }

    snapshot->source = POWER_STATE_SOURCE_IP5306;
    snapshot->valid_flags |= POWER_STATE_VALID_SOURCE;
}

static void power_manager_publish_sample(const struct battery_monitor_sample *sample)
{
    struct power_state_snapshot snapshot = {0};

    snapshot.timestamp_ms = k_uptime_get_32();
    snapshot.voltage_mv = sample->vbat_mv;
    snapshot.level_state = power_level_hysteresis_update(&level_filter,
                                 sample->vbat_mv);
    snapshot.charging_state = POWER_CHARGING_STATE_UNKNOWN;
    snapshot.battery_present = true;
    snapshot.source = POWER_STATE_SOURCE_FALLBACK;
    snapshot.valid_flags = POWER_STATE_VALID_TIMESTAMP |
                  POWER_STATE_VALID_VOLTAGE |
                  POWER_STATE_VALID_LEVEL |
                  POWER_STATE_VALID_BATTERY_PRESENT |
                  POWER_STATE_VALID_SOURCE;

    power_manager_apply_ip5306_status(&snapshot);

    snapshot.estimated_percent =
        power_state_estimate_percent(snapshot.voltage_mv,
                         snapshot.charging_state,
                         snapshot.valid_flags);
    snapshot.valid_flags |= POWER_STATE_VALID_PERCENT;

    LOG_INF("power: vbat raw=%ld mv=%ld percent=%u",
        (long)sample->raw, (long)sample->vbat_mv, snapshot.estimated_percent);
    LOG_INF("power: battery state=%s",
        battery_monitor_state_name(sample->state));

    (void)power_state_publish_if_changed(&snapshot);
}

static void keepalive_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    if (!gpio_is_ready_dt(&ip5306_wakeup)) {
        return;
    }

    (void)gpio_pin_set_dt(&ip5306_wakeup, 1);
    LOG_INF("power: ip5306 keepalive pulse");
    (void)k_work_reschedule(&keepalive_release_work,
                K_MSEC(POWER_MANAGER_KEEPALIVE_PULSE_MS));
    (void)k_work_reschedule(&keepalive_work,
                K_MSEC(POWER_MANAGER_KEEPALIVE_INTERVAL_MS));
}

static void keepalive_release_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    (void)gpio_pin_set_dt(&ip5306_wakeup, 0);
}

static void battery_sample_work_handler(struct k_work *work)
{
    struct battery_monitor_sample sample;
    int err;

    ARG_UNUSED(work);

    err = battery_monitor_sample_once(&sample);
    if (err == 0) {
        power_manager_publish_sample(&sample);
    } else {
        LOG_WRN("power: battery sample failed: %d", err);
    }

    (void)k_work_reschedule(&battery_sample_work,
                K_MSEC(POWER_MANAGER_BATTERY_SAMPLE_INTERVAL_MS));
}

int power_manager_init(void)
{
    int err;

    power_policy_init(&policy_state);
    power_level_hysteresis_init(&level_filter);
    usb_power_present = false;
    power_manager_ready = false;
    battery_monitor_available = false;
    ip5306_available = false;

    if (!gpio_is_ready_dt(&ip5306_wakeup)) {
        return -ENODEV;
    }

    err = gpio_pin_configure_dt(&ip5306_wakeup, GPIO_OUTPUT_INACTIVE);
    if (err < 0) {
        return err;
    }

    err = ip5306_init();
    if (err == 0) {
        ip5306_available = true;
        (void)ip5306_probe();
    } else {
        LOG_WRN("power: ip5306 init failed: %d", err);
    }

    err = battery_monitor_init();
    if (err < 0) {
        LOG_WRN("power: battery monitor init failed: %d", err);
    } else {
        battery_monitor_available = true;
    }

    err = power_state_subscribe(power_manager_policy_listener, &policy_state);
    if (err < 0) {
        LOG_WRN("power: policy subscriber registration failed: %d", err);
    }

    k_work_init_delayable(&keepalive_work, keepalive_work_handler);
    k_work_init_delayable(&keepalive_release_work, keepalive_release_work_handler);
    k_work_init_delayable(&battery_sample_work, battery_sample_work_handler);
    (void)k_work_reschedule(&keepalive_work, K_NO_WAIT);
    if (battery_monitor_available) {
        (void)k_work_reschedule(&battery_sample_work, K_NO_WAIT);
    }

    power_manager_ready = true;
    LOG_INF("power: init ok");
    return 0;
}

void power_manager_usb_power_present(bool present)
{
    struct power_state_snapshot snapshot = {0};

    if (usb_power_present == present && power_manager_ready) {
        return;
    }

    usb_power_present = present;
    if (!power_manager_ready) {
        return;
    }

    if (power_state_get_latest(&snapshot) < 0) {
        snapshot.charging_state = POWER_CHARGING_STATE_UNKNOWN;
        snapshot.level_state = POWER_LEVEL_STATE_UNKNOWN;
        snapshot.source = POWER_STATE_SOURCE_FALLBACK;
        snapshot.valid_flags = POWER_STATE_VALID_SOURCE;
    }

    snapshot.timestamp_ms = k_uptime_get_32();
    snapshot.usb_present = present;
    snapshot.valid_flags |= POWER_STATE_VALID_TIMESTAMP |
                    POWER_STATE_VALID_USB_PRESENT;

    (void)power_state_publish_if_changed(&snapshot);
    LOG_INF("power: usb power %s", present ? "present" : "removed");
}

void power_manager_notify_battery_state(enum battery_state state)
{
    enum power_policy_action action = power_policy_update(&policy_state, state);

    power_manager_log_policy_action(action);
}
