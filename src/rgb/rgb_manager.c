#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <zephyr/autoconf.h>
#include <zephyr/kernel.h>
#if defined(CONFIG_INPUT)
#include <zephyr/input/input.h>
#endif
#include <zephyr/logging/log.h>

#include <keyboard/keyboard_led_state.h>
#include <power/power_state.h>
#include <rgb/rgb_effect.h>
#include <rgb/rgb_hw.h>
#include <rgb/rgb_keymap.h>
#include <rgb/rgb_manager.h>
#include <rgb/rgb_policy.h>

LOG_MODULE_REGISTER(rgb_manager, LOG_LEVEL_INF);

static struct rgb_effect_state rgb_effect;
static struct rgb_policy rgb_policy;
static struct rgb_color rgb_frame[RGB_LED_COUNT];
static struct k_work_delayable rgb_refresh_work;
static bool rgb_initialized;
static bool rgb_hw_available;

static void rgb_manager_schedule(uint32_t delay_ms)
{
    if (!rgb_initialized) {
        return;
    }

    (void)k_work_reschedule(&rgb_refresh_work, K_MSEC(delay_ms));
}

void rgb_manager_mark_dirty(void)
{
    rgb_manager_schedule(0);
}

static void rgb_keyboard_led_listener(uint8_t led_bits, void *user_data)
{
    ARG_UNUSED(led_bits);
    ARG_UNUSED(user_data);

    rgb_manager_mark_dirty();
}

static void rgb_power_state_listener(const struct power_state_snapshot *snapshot,
                     void *user_data)
{
    ARG_UNUSED(user_data);

    rgb_policy_update(&rgb_policy, snapshot);
    rgb_manager_mark_dirty();
}

static void rgb_refresh_work_handler(struct k_work *work)
{
    uint32_t now_ms;
    bool active;
    int err;

    ARG_UNUSED(work);

    if (!rgb_hw_available) {
        return;
    }

    if (rgb_policy.critical_shutdown) {
        (void)rgb_hw_off();
        return;
    }

    now_ms = k_uptime_get_32();
    rgb_effect_render(&rgb_effect, &rgb_policy, keyboard_led_state_num_lock(),
              now_ms, rgb_frame, RGB_LED_COUNT);

    err = rgb_hw_show(rgb_frame, RGB_LED_COUNT);
    if (err < 0) {
        LOG_WRN("RGB frame update failed: %d", err);
    }

    active = rgb_effect_has_active_pixels(&rgb_effect, now_ms);
    if (active) {
        rgb_manager_schedule(rgb_policy.refresh_period_ms);
    }
}

#if defined(CONFIG_INPUT)
static void rgb_input_event_cb(struct input_event *evt, void *user_data)
{
    uint8_t rgb_index;
    uint32_t now_ms;

    ARG_UNUSED(user_data);

    if (evt->type != INPUT_EV_KEY) {
        return;
    }

    if (!rgb_keymap_lookup(evt->code, &rgb_index)) {
        return;
    }

    now_ms = k_uptime_get_32();
    if (evt->value != 0) {
        rgb_effect_key_pressed(&rgb_effect, rgb_index, now_ms);
    } else {
        rgb_effect_key_released(&rgb_effect, rgb_index, now_ms);
    }

    rgb_manager_mark_dirty();
}

INPUT_CALLBACK_DEFINE(NULL, rgb_input_event_cb, NULL);
#endif

int rgb_manager_init(void)
{
    struct power_state_snapshot snapshot;
    int err;

    rgb_effect_init(&rgb_effect);
    rgb_policy_init(&rgb_policy);
    k_work_init_delayable(&rgb_refresh_work, rgb_refresh_work_handler);

    err = power_state_get_latest(&snapshot);
    if (err == 0) {
        rgb_policy_update(&rgb_policy, &snapshot);
    }

    err = power_state_subscribe(rgb_power_state_listener, NULL);
    if (err < 0) {
        LOG_WRN("RGB power-state subscription failed: %d", err);
    }

    err = keyboard_led_state_subscribe(rgb_keyboard_led_listener, NULL);
    if (err < 0) {
        LOG_WRN("RGB keyboard LED subscription failed: %d", err);
    }

    err = rgb_hw_init();
    if (err < 0) {
        LOG_WRN("RGB hardware unavailable: %d", err);
        rgb_hw_available = false;
    } else {
        rgb_hw_available = true;
    }

    rgb_initialized = true;
    rgb_manager_mark_dirty();
    LOG_INF("RGB manager initialized");
    return 0;
}

void rgb_manager_set_mode(enum rgb_mode mode)
{
    rgb_effect_set_mode(&rgb_effect, mode);
    rgb_manager_mark_dirty();
}

enum rgb_mode rgb_manager_get_mode(void)
{
    return rgb_effect_get_mode(&rgb_effect);
}

void rgb_manager_set_color(uint8_t red, uint8_t green, uint8_t blue)
{
    const struct rgb_color color = {
        .red = red,
        .green = green,
        .blue = blue,
    };

    rgb_effect_set_color(&rgb_effect, color);
    rgb_manager_mark_dirty();
}
