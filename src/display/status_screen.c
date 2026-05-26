#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <config/app_config.h>
#include <display/status_screen.h>
#include <keyboard/keyboard_led_state.h>
#include <mode/mode_manager.h>
#include <power/power_state.h>

#ifdef CONFIG_LVGL
#include <lvgl.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#endif

LOG_MODULE_REGISTER(status_screen, LOG_LEVEL_INF);

static struct status_screen_snapshot status_snapshot;
static struct k_work status_refresh_work;
static bool status_screen_ready;

K_MUTEX_DEFINE(status_snapshot_lock);

#ifdef CONFIG_LVGL
static const struct device *const display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
static struct k_work_delayable status_lvgl_timer_work;
#endif

static const char *mode_message(enum kb_mode mode)
{
	switch (mode) {
	case KB_MODE_USB:
		return "USB Ready";
	case KB_MODE_BLE:
		return "BLE Active";
	case KB_MODE_24G_RESERVED:
	default:
		return "2.4G Reserved";
	}
}

static void status_snapshot_defaults(struct status_screen_snapshot *snapshot)
{
	memset(snapshot, 0, sizeof(*snapshot));
	(void)app_config_get_defaults(&snapshot->config);
	snapshot->mode = KB_MODE_24G_RESERVED;
	snapshot->power.estimated_percent = 100;
	snapshot->power.level_state = POWER_LEVEL_STATE_UNKNOWN;
	snapshot->power.charging_state = POWER_CHARGING_STATE_UNKNOWN;
	snapshot->power.valid_flags = POWER_STATE_VALID_PERCENT;
	snapshot->message = mode_message(snapshot->mode);
}

static void status_schedule_refresh(void)
{
	if (!status_screen_ready) {
		return;
	}

	k_work_submit(&status_refresh_work);
}

static void status_mode_listener(enum kb_mode mode, enum kb_mode previous_mode, bool initial,
				 void *user_data)
{
	ARG_UNUSED(previous_mode);
	ARG_UNUSED(initial);
	ARG_UNUSED(user_data);

	k_mutex_lock(&status_snapshot_lock, K_FOREVER);
	status_snapshot.mode = mode;
	status_snapshot.message = mode_message(mode);
	k_mutex_unlock(&status_snapshot_lock);

	status_schedule_refresh();
}

static void status_power_listener(const struct power_state_snapshot *snapshot, void *user_data)
{
	ARG_UNUSED(user_data);

	if (snapshot == NULL) {
		return;
	}

	k_mutex_lock(&status_snapshot_lock, K_FOREVER);
	status_snapshot.power = *snapshot;
	k_mutex_unlock(&status_snapshot_lock);

	status_schedule_refresh();
}

static void status_led_listener(uint8_t led_bits, void *user_data)
{
	ARG_UNUSED(user_data);

	k_mutex_lock(&status_snapshot_lock, K_FOREVER);
	status_snapshot.num_lock = (led_bits & KEYBOARD_LED_NUM_LOCK) != 0U;
	k_mutex_unlock(&status_snapshot_lock);

	status_schedule_refresh();
}

static void status_config_listener(const struct app_config *config, void *user_data)
{
	ARG_UNUSED(user_data);

	if (config == NULL) {
		return;
	}

	k_mutex_lock(&status_snapshot_lock, K_FOREVER);
	status_snapshot.config = *config;
	k_mutex_unlock(&status_snapshot_lock);

	status_schedule_refresh();
}

static void status_load_initial_snapshot(void)
{
	struct power_state_snapshot power;
	struct app_config config;
	int err;

	k_mutex_lock(&status_snapshot_lock, K_FOREVER);
	status_snapshot_defaults(&status_snapshot);
	status_snapshot.mode = mode_manager_get_mode();
	status_snapshot.message = mode_message(status_snapshot.mode);
	status_snapshot.num_lock = keyboard_led_state_num_lock();
	if (app_config_get(&config) == 0) {
		status_snapshot.config = config;
	}
	err = power_state_get_latest(&power);
	if (err == 0) {
		status_snapshot.power = power;
	}
	k_mutex_unlock(&status_snapshot_lock);
}

static void status_refresh_work_handler(struct k_work *work)
{
	struct status_screen_snapshot snapshot;

	ARG_UNUSED(work);

#ifdef CONFIG_LVGL
	k_mutex_lock(&status_snapshot_lock, K_FOREVER);
	snapshot = status_snapshot;
	k_mutex_unlock(&status_snapshot_lock);

	lv_lock();
	status_screen_lvgl_update(&snapshot);
	lv_unlock();

	(void)lv_timer_handler();
#else
	ARG_UNUSED(snapshot);
#endif
}

static void status_register_listeners(void)
{
	int err;

	err = mode_manager_register_listener(status_mode_listener, NULL);
	if (err != 0) {
		LOG_WRN("status screen mode subscription failed: %d", err);
	}

	err = power_state_subscribe(status_power_listener, NULL);
	if (err != 0) {
		LOG_WRN("status screen power subscription failed: %d", err);
	}

	err = keyboard_led_state_subscribe(status_led_listener, NULL);
	if (err != 0) {
		LOG_WRN("status screen keyboard LED subscription failed: %d", err);
	}

	err = app_config_subscribe(status_config_listener, NULL);
	if (err != 0) {
		LOG_WRN("status screen config subscription failed: %d", err);
	}
}

#ifdef CONFIG_LVGL
static uint32_t lvgl_timer_delay(uint32_t suggested_ms)
{
	if (suggested_ms < 5U) {
		return 5U;
	}

	if (suggested_ms > 50U) {
		return 50U;
	}

	return suggested_ms;
}

static void status_lvgl_timer_work_handler(struct k_work *work)
{
	uint32_t wait_ms;

	ARG_UNUSED(work);

	wait_ms = lv_timer_handler();
	(void)k_work_reschedule(&status_lvgl_timer_work,
				 K_MSEC(lvgl_timer_delay(wait_ms)));
}
#endif

int status_screen_init(void)
{
#ifndef CONFIG_LVGL
	LOG_INF("status screen disabled: LVGL is not enabled");
	return 0;
#else
	struct status_screen_snapshot snapshot;
	int err;

	if (!device_is_ready(display_dev)) {
		return -ENODEV;
	}

	k_work_init(&status_refresh_work, status_refresh_work_handler);
	k_work_init_delayable(&status_lvgl_timer_work, status_lvgl_timer_work_handler);
	status_load_initial_snapshot();

	k_mutex_lock(&status_snapshot_lock, K_FOREVER);
	snapshot = status_snapshot;
	k_mutex_unlock(&status_snapshot_lock);

	lv_lock();
	err = status_screen_lvgl_init(&snapshot);
	lv_unlock();
	if (err != 0) {
		return err;
	}

	err = display_blanking_off(display_dev);
	if (err != 0 && err != -ENOSYS) {
		LOG_WRN("display blanking off failed: %d", err);
	}

	status_screen_ready = true;
	status_register_listeners();
	status_schedule_refresh();

#ifndef CONFIG_LV_Z_RUN_LVGL_ON_WORKQUEUE
	(void)k_work_reschedule(&status_lvgl_timer_work, K_MSEC(10));
#endif

	LOG_INF("status screen initialized");
	return 0;
#endif
}
