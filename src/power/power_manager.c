#include <errno.h>
#include <stdbool.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <power/battery_monitor.h>
#include <power/ip5306.h>
#include <power/power_manager.h>
#include <power/power_policy.h>

LOG_MODULE_REGISTER(power_manager, LOG_LEVEL_INF);

#define BATTERY_POWER_NODE DT_PATH(zephyr_user)

static const struct gpio_dt_spec ip5306_wakeup =
    GPIO_DT_SPEC_GET(BATTERY_POWER_NODE, wakeup_gpios);
static struct k_work_delayable keepalive_work;
static struct k_work_delayable keepalive_release_work;
static struct k_work_delayable battery_sample_work;
static struct power_policy_state policy_state;
static bool power_manager_ready;
static bool usb_power_present;

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
        LOG_INF("power: vbat raw=%ld mv=%ld", (long)sample.raw, (long)sample.vbat_mv);
        power_manager_notify_battery_state(sample.state);
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
    usb_power_present = false;
    power_manager_ready = false;

    if (!gpio_is_ready_dt(&ip5306_wakeup)) {
        return -ENODEV;
    }

    err = gpio_pin_configure_dt(&ip5306_wakeup, GPIO_OUTPUT_INACTIVE);
    if (err < 0) {
        return err;
    }

    err = ip5306_init();
    if (err == 0) {
        (void)ip5306_probe();
    } else {
        LOG_WRN("power: ip5306 init failed: %d", err);
    }

    err = battery_monitor_init();
    if (err < 0) {
        LOG_WRN("power: battery monitor init failed: %d", err);
    }

    k_work_init_delayable(&keepalive_work, keepalive_work_handler);
    k_work_init_delayable(&keepalive_release_work, keepalive_release_work_handler);
    k_work_init_delayable(&battery_sample_work, battery_sample_work_handler);
    (void)k_work_reschedule(&keepalive_work, K_NO_WAIT);
    if (err == 0) {
        (void)k_work_reschedule(&battery_sample_work, K_NO_WAIT);
    }

    power_manager_ready = true;
    LOG_INF("power: init ok");
    return 0;
}

void power_manager_usb_power_present(bool present)
{
    if (!power_manager_ready || usb_power_present == present) {
        return;
    }

    usb_power_present = present;
    LOG_INF("power: usb power %s", present ? "present" : "removed");
}

void power_manager_notify_battery_state(enum battery_state state)
{
    bool state_changed = power_policy_state_would_change(&policy_state, state);
    enum power_policy_action action = power_policy_update(&policy_state, state);

    if (state_changed) {
        LOG_INF("power: battery state=%s", battery_monitor_state_name(state));
    }

    if (action == POWER_POLICY_ACTION_LOW_BATTERY) {
        LOG_WRN("power: low battery policy hook");
    } else if (action == POWER_POLICY_ACTION_CRITICAL_BATTERY) {
        LOG_ERR("power: critical battery policy hook");
    }
}
