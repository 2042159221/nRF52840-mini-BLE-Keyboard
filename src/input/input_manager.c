#include <errno.h>
#include <stdint.h>

#include <zephyr/autoconf.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#if defined(CONFIG_INPUT)
#include <zephyr/input/input.h>
#endif
#include <zephyr/logging/log.h>

#include <config/app_config.h>
#include <display/status_screen.h>
#include <hid/hid_flowctrl.h>
#include <hid/hid_report.h>
#include <hid/hid_usage.h>
#include <input/input_manager.h>
#include <keymap/keymap.h>

LOG_MODULE_REGISTER(input_manager, LOG_LEVEL_INF);

#define KBD_MATRIX_NODE DT_NODELABEL(kbd_matrix)
#define ENCODER_LONG_PRESS_MS 700

static struct hid_keyboard_report keyboard_report;

#if defined(CONFIG_INPUT)
static struct k_work_delayable encoder_long_press_work;
static struct k_mutex encoder_press_lock;
static bool encoder_press_active;
static bool encoder_long_press_consumed;

static int submit_encoder_press_hid_action(void)
{
    struct app_config config;
    uint8_t action = APP_ENCODER_ACTION_MUTE;
    int err;

    if (app_config_get(&config) == 0) {
        action = config.encoder_press_action;
    }

    LOG_INF("encoder press action: %u", action);

    err = hid_flowctrl_submit_encoder_action(action);
    if (err != 0 && err != -ENOMSG && err != -ENOTCONN) {
        LOG_WRN("encoder press action submit failed: %d", err);
    }

    return err;
}

static void encoder_long_press_work_handler(struct k_work *work)
{
    bool active;
    bool consumed = false;

    ARG_UNUSED(work);

    k_mutex_lock(&encoder_press_lock, K_FOREVER);
    active = encoder_press_active;
    k_mutex_unlock(&encoder_press_lock);

    if (active) {
        consumed = status_screen_handle_encoder_long_press();
    }

    if (consumed) {
        k_mutex_lock(&encoder_press_lock, K_FOREVER);
        if (encoder_press_active) {
            encoder_long_press_consumed = true;
        }
        k_mutex_unlock(&encoder_press_lock);
    }
}

static void handle_encoder_press_event(int32_t value)
{
    bool long_consumed;

    if (value != 0) {
        k_mutex_lock(&encoder_press_lock, K_FOREVER);
        encoder_press_active = true;
        encoder_long_press_consumed = false;
        k_mutex_unlock(&encoder_press_lock);

        (void)k_work_reschedule(&encoder_long_press_work, K_MSEC(ENCODER_LONG_PRESS_MS));
        return;
    }

    k_mutex_lock(&encoder_press_lock, K_FOREVER);
    if (!encoder_press_active) {
        k_mutex_unlock(&encoder_press_lock);
        return;
    }
    encoder_press_active = false;
    long_consumed = encoder_long_press_consumed;
    encoder_long_press_consumed = false;
    k_mutex_unlock(&encoder_press_lock);

    (void)k_work_cancel_delayable(&encoder_long_press_work);

    if (long_consumed || status_screen_handle_encoder_press()) {
        return;
    }

    (void)submit_encoder_press_hid_action();
}

void input_manager_release_all(void)
{
    int err;

    hid_keyboard_report_clear(&keyboard_report);

    err = hid_flowctrl_submit_keyboard_report(&keyboard_report);
    if (err == 0) {
        err = hid_flowctrl_flush_keyboard();
    }

    if (err != 0 && err != -ENOTCONN) {
        LOG_WRN("keyboard release report failed: %d", err);
    }
}

static void input_manager_event_cb(struct input_event *evt, void *user_data)
{
    ARG_UNUSED(user_data);

    if (evt->type != INPUT_EV_KEY) {
        return;
    }

    if (evt->code == INPUT_KEY_MUTE) {
        handle_encoder_press_event(evt->value);
        return;
    }

    const uint8_t usage_id = keymap_lookup_input_code_usage(evt->code);

    if (usage_id == HID_USAGE_KEY_NONE) {
        LOG_DBG("ignored input key code %u", evt->code);
        return;
    }

    int err;

    if (evt->value != 0) {
        err = hid_keyboard_report_add_key(&keyboard_report, usage_id);
    } else {
        err = hid_keyboard_report_remove_key(&keyboard_report, usage_id);
    }

    if (err != 0) {
        LOG_WRN("keyboard report update failed: %d", err);
        return;
    }

    err = hid_flowctrl_submit_keyboard_report(&keyboard_report);
    if (err != 0) {
        LOG_WRN("keyboard report send failed: %d", err);
    }
}

INPUT_CALLBACK_DEFINE(NULL, input_manager_event_cb, NULL);
#endif

int input_manager_init(void)
{
    hid_keyboard_report_clear(&keyboard_report);

#if defined(CONFIG_INPUT)
    k_mutex_init(&encoder_press_lock);
    k_work_init_delayable(&encoder_long_press_work, encoder_long_press_work_handler);
#endif

#if DT_NODE_HAS_STATUS(KBD_MATRIX_NODE, okay)
    const struct device *matrix = DEVICE_DT_GET(KBD_MATRIX_NODE);

    if (!device_is_ready(matrix)) {
        LOG_ERR("keyboard matrix device is not ready");
        return -ENODEV;
    }

    LOG_INF("keyboard matrix input ready");
    return 0;
#else
    LOG_WRN("keyboard matrix node is disabled or missing");
    return 0;
#endif
}
