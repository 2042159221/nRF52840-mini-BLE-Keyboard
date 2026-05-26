#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <config/app_config.h>
#include <config/app_config_store.h>
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
static struct status_screen_model status_model;
static struct k_work status_init_work;
static struct k_work status_refresh_work;
static struct k_work status_action_work;
static bool status_model_initialized;
static bool status_screen_ready;
static bool status_workq_started;

K_MUTEX_DEFINE(status_snapshot_lock);
K_MSGQ_DEFINE(status_action_msgq, sizeof(struct status_screen_event_result), 4, 4);

#ifdef CONFIG_LVGL
#define STATUS_SCREEN_WORKQ_STACK_SIZE 6144
#define STATUS_SCREEN_WORKQ_PRIORITY K_PRIO_PREEMPT(8)

K_THREAD_STACK_DEFINE(status_workq_stack, STATUS_SCREEN_WORKQ_STACK_SIZE);
static struct k_work_q status_workq;
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
	status_screen_model_init_with_brightness(&snapshot->ui, snapshot->config.rgb_brightness);
	snapshot->mode = KB_MODE_24G_RESERVED;
	snapshot->power.estimated_percent = 100;
	snapshot->power.level_state = POWER_LEVEL_STATE_UNKNOWN;
	snapshot->power.charging_state = POWER_CHARGING_STATE_UNKNOWN;
	snapshot->power.valid_flags = POWER_STATE_VALID_PERCENT;
	snapshot->message = mode_message(snapshot->mode);
}

static void status_model_init_from_config(const struct app_config *config)
{
	int brightness = 50;

	if (config != NULL) {
		brightness = config->rgb_brightness;
	}

	status_screen_model_init_with_brightness(&status_model, brightness);
	status_model_initialized = true;
}

static void status_model_ensure_initialized(void)
{
	struct app_config config;

	if (status_model_initialized) {
		return;
	}

	if (app_config_get(&config) == 0) {
		status_model_init_from_config(&config);
	} else {
		status_model_init_from_config(NULL);
	}
}

static void status_snapshot_sync_ui_locked(void)
{
	status_model_ensure_initialized();
	status_snapshot.ui = status_model;
}

static void status_schedule_refresh(void)
{
	if (!status_screen_ready) {
		return;
	}

#ifdef CONFIG_LVGL
	(void)k_work_submit_to_queue(&status_workq, &status_refresh_work);
#endif
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
	status_model_ensure_initialized();
	status_screen_model_set_rgb_brightness(&status_model, config->rgb_brightness);
	status_snapshot_sync_ui_locked();
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
	status_model_init_from_config(&status_snapshot.config);
	status_snapshot_sync_ui_locked();
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

static void status_handle_model_action(const struct status_screen_event_result *result)
{
	struct app_config config;
	int err;

	if (result == NULL) {
		return;
	}

	switch (result->action) {
	case STATUS_SCREEN_ACTION_APPLY_RGB_BRIGHTNESS:
		err = app_config_get(&config);
		if (err != 0) {
			LOG_WRN("status screen config read failed: %d", err);
			return;
		}

		config.rgb_brightness = (uint8_t)result->value;
		err = app_config_set(&config, true);
		if (err != 0) {
			LOG_WRN("status screen brightness apply failed: %d", err);
			return;
		}

		err = app_config_store_save();
		if (err != 0) {
			LOG_WRN("status screen brightness save failed: %d", err);
		}
		return;
	case STATUS_SCREEN_ACTION_FACTORY_RESET_CONFIRMED:
		err = app_config_store_factory_reset();
		if (err != 0) {
			LOG_WRN("status screen factory reset failed: %d", err);
		}
		return;
	default:
		return;
	}
}

static void status_action_work_handler(struct k_work *work)
{
	struct status_screen_event_result result;

	ARG_UNUSED(work);

	while (k_msgq_get(&status_action_msgq, &result, K_NO_WAIT) == 0) {
		status_handle_model_action(&result);
	}
}

static void status_queue_model_action(const struct status_screen_event_result *result)
{
	int err;

	if (result == NULL) {
		return;
	}

	if (result->action != STATUS_SCREEN_ACTION_APPLY_RGB_BRIGHTNESS &&
	    result->action != STATUS_SCREEN_ACTION_FACTORY_RESET_CONFIRMED) {
		return;
	}

	err = k_msgq_put(&status_action_msgq, result, K_NO_WAIT);
	if (err != 0) {
		LOG_WRN("status screen action queue full: %d", err);
		return;
	}

#ifdef CONFIG_LVGL
	if (status_workq_started) {
		(void)k_work_submit_to_queue(&status_workq, &status_action_work);
	}
#else
	k_work_submit(&status_action_work);
#endif
}

static bool status_screen_handle_model_input(enum status_screen_input input)
{
	struct status_screen_event_result result;
	bool refresh;

	if (!status_screen_ready) {
		return false;
	}

	k_mutex_lock(&status_snapshot_lock, K_FOREVER);
	status_model_ensure_initialized();
	result = status_screen_model_handle_input(&status_model, input);
	status_snapshot_sync_ui_locked();
	refresh = result.consumed ||
		  (result.action != STATUS_SCREEN_ACTION_NONE &&
		   result.action != STATUS_SCREEN_ACTION_HID_PASSTHROUGH);
	k_mutex_unlock(&status_snapshot_lock);

	status_queue_model_action(&result);

	if (refresh) {
		status_schedule_refresh();
	}

	return result.consumed;
}

bool status_screen_handle_encoder_delta(int32_t delta)
{
	if (delta > 0) {
		return status_screen_handle_model_input(STATUS_SCREEN_INPUT_ROTATE_CW);
	}

	if (delta < 0) {
		return status_screen_handle_model_input(STATUS_SCREEN_INPUT_ROTATE_CCW);
	}

	return false;
}

bool status_screen_handle_encoder_press(void)
{
	return status_screen_handle_model_input(STATUS_SCREEN_INPUT_PRESS);
}

bool status_screen_handle_encoder_long_press(void)
{
	return status_screen_handle_model_input(STATUS_SCREEN_INPUT_LONG_PRESS);
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

	if (!status_screen_ready) {
		return;
	}

	wait_ms = lv_timer_handler();
	(void)k_work_reschedule_for_queue(&status_workq, &status_lvgl_timer_work,
					   K_MSEC(lvgl_timer_delay(wait_ms)));
}

static void status_init_work_handler(struct k_work *work)
{
	struct status_screen_snapshot snapshot;
	int err;

	ARG_UNUSED(work);

	k_mutex_lock(&status_snapshot_lock, K_FOREVER);
	snapshot = status_snapshot;
	k_mutex_unlock(&status_snapshot_lock);

	lv_lock();
	err = status_screen_lvgl_init(&snapshot);
	lv_unlock();
	if (err != 0) {
		LOG_WRN("status screen LVGL init failed: %d", err);
		return;
	}

	err = display_blanking_off(display_dev);
	if (err != 0 && err != -ENOSYS) {
		LOG_WRN("display blanking off failed: %d", err);
	}

	status_screen_ready = true;
	status_schedule_refresh();

#ifndef CONFIG_LV_Z_RUN_LVGL_ON_WORKQUEUE
	(void)k_work_reschedule_for_queue(&status_workq, &status_lvgl_timer_work, K_MSEC(10));
#endif

	LOG_INF("status screen initialized");
}
#endif

int status_screen_init(void)
{
#ifndef CONFIG_LVGL
	LOG_INF("status screen disabled: LVGL is not enabled");
	return 0;
#else
	int err;

	if (!device_is_ready(display_dev)) {
		return -ENODEV;
	}

	if (!status_workq_started) {
		k_work_queue_start(&status_workq, status_workq_stack,
				   K_THREAD_STACK_SIZEOF(status_workq_stack),
				   STATUS_SCREEN_WORKQ_PRIORITY, NULL);
		status_workq_started = true;
	}

	k_work_init(&status_init_work, status_init_work_handler);
	k_work_init(&status_refresh_work, status_refresh_work_handler);
	k_work_init(&status_action_work, status_action_work_handler);
	k_work_init_delayable(&status_lvgl_timer_work, status_lvgl_timer_work_handler);
	status_load_initial_snapshot();
	status_register_listeners();
	err = k_work_submit_to_queue(&status_workq, &status_init_work);
	if (err < 0) {
		return err;
	}

	LOG_INF("status screen init queued");
	return 0;
#endif
}
