#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <config/app_config.h>

#if !defined(UNIT_TEST)
#include <zephyr/kernel.h>
#include <zephyr/sys/crc.h>
#endif

#define APP_CONFIG_MAX_LISTENERS 8u
#define APP_CONFIG_CRC_OFFSET offsetof(struct app_config, crc32)

struct app_config_subscriber {
    app_config_listener_t handler;
    void *user_data;
};

static struct app_config current_config;
static struct app_config_subscriber subscribers[APP_CONFIG_MAX_LISTENERS];
static size_t subscriber_count;
static bool app_config_initialized;

#if !defined(UNIT_TEST)
K_MUTEX_DEFINE(app_config_lock);
#define APP_CONFIG_LOCK() k_mutex_lock(&app_config_lock, K_FOREVER)
#define APP_CONFIG_UNLOCK() k_mutex_unlock(&app_config_lock)
#else
#define APP_CONFIG_LOCK() ((void)0)
#define APP_CONFIG_UNLOCK() ((void)0)
#endif

static bool valid_rgb_mode(uint8_t mode)
{
    return mode <= APP_RGB_MODE_KEY_REACTIVE;
}

static bool valid_numlock_policy(uint8_t policy)
{
    return policy <= APP_NUMLOCK_OFF_WHEN_LOW_POWER;
}

static bool valid_encoder_action(uint8_t action)
{
    return action <= APP_ENCODER_ACTION_RGB_MODE_NEXT;
}

static bool valid_mode_policy(uint8_t policy)
{
    return policy <= APP_MODE_POLICY_BLE_PREFERRED;
}

static bool valid_default_mode(uint8_t mode)
{
    return mode <= APP_DEFAULT_MODE_BLE;
}

#if defined(UNIT_TEST)
static uint32_t app_config_crc32_update(uint32_t crc, const uint8_t *data,
                    size_t len)
{
    size_t i;

    crc = ~crc;
    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if ((crc & 1u) != 0u) {
                crc = (crc >> 1) ^ 0xedb88320u;
            } else {
                crc >>= 1;
            }
        }
    }

    return ~crc;
}
#else
static uint32_t app_config_crc32_update(uint32_t crc, const uint8_t *data,
                    size_t len)
{
    return crc32_ieee_update(crc, data, len);
}
#endif

uint32_t app_config_compute_crc32(const struct app_config *config)
{
    struct app_config copy;

    if (config == NULL) {
        return 0;
    }

    copy = *config;
    copy.crc32 = 0;
    return app_config_crc32_update(0, (const uint8_t *)&copy,
                       APP_CONFIG_CRC_OFFSET);
}

void app_config_update_crc32(struct app_config *config)
{
    if (config == NULL) {
        return;
    }

    config->crc32 = app_config_compute_crc32(config);
}

bool app_config_crc32_valid(const struct app_config *config)
{
    if (config == NULL) {
        return false;
    }

    return config->crc32 == app_config_compute_crc32(config);
}

int app_config_get_defaults(struct app_config *out)
{
    if (out == NULL) {
        return -EINVAL;
    }

    memset(out, 0, sizeof(*out));
    out->version = APP_CONFIG_VERSION;
    out->rgb_enable = true;
    out->rgb_mode = APP_RGB_MODE_KEY_REACTIVE;
    out->rgb_brightness = 30;
    out->rgb_red = 255;
    out->rgb_green = 80;
    out->rgb_blue = 20;
    out->numlock_policy = APP_NUMLOCK_ALWAYS_WHITE;
    out->encoder_cw_action = APP_ENCODER_ACTION_VOLUME_UP;
    out->encoder_ccw_action = APP_ENCODER_ACTION_VOLUME_DOWN;
    out->encoder_press_action = APP_ENCODER_ACTION_MUTE;
    out->mode_policy = APP_MODE_POLICY_FOLLOW_HARDWARE;
    out->default_mode = APP_DEFAULT_MODE_USB;
    app_config_update_crc32(out);
    return 0;
}

int app_config_validate(const struct app_config *config)
{
    if (config == NULL) {
        return -EINVAL;
    }

    if (config->version != APP_CONFIG_VERSION) {
        return -EINVAL;
    }

    if (!valid_rgb_mode(config->rgb_mode)) {
        return -EINVAL;
    }

    if (config->rgb_brightness > 100u) {
        return -EINVAL;
    }

    if (!valid_numlock_policy(config->numlock_policy)) {
        return -EINVAL;
    }

    if (!valid_encoder_action(config->encoder_cw_action) ||
        !valid_encoder_action(config->encoder_ccw_action) ||
        !valid_encoder_action(config->encoder_press_action)) {
        return -EINVAL;
    }

    if (!valid_mode_policy(config->mode_policy)) {
        return -EINVAL;
    }

    if (!valid_default_mode(config->default_mode)) {
        return -EINVAL;
    }

    return 0;
}

int app_config_init(void)
{
    APP_CONFIG_LOCK();
    (void)app_config_get_defaults(&current_config);
    app_config_initialized = true;
    APP_CONFIG_UNLOCK();
    return 0;
}

int app_config_get(struct app_config *out)
{
    if (out == NULL) {
        return -EINVAL;
    }

    APP_CONFIG_LOCK();
    if (!app_config_initialized) {
        (void)app_config_get_defaults(&current_config);
        app_config_initialized = true;
    }
    *out = current_config;
    APP_CONFIG_UNLOCK();
    return 0;
}

static void notify_subscribers(const struct app_config *config)
{
    struct app_config_subscriber local_subscribers[APP_CONFIG_MAX_LISTENERS];
    size_t local_count;
    size_t i;

    APP_CONFIG_LOCK();
    local_count = subscriber_count;
    memcpy(local_subscribers, subscribers,
           local_count * sizeof(local_subscribers[0]));
    APP_CONFIG_UNLOCK();

    for (i = 0; i < local_count; i++) {
        local_subscribers[i].handler(config, local_subscribers[i].user_data);
    }
}

int app_config_set(const struct app_config *config, bool notify)
{
    struct app_config next;
    int err;

    if (config == NULL) {
        return -EINVAL;
    }

    next = *config;
    err = app_config_validate(&next);
    if (err != 0) {
        return err;
    }

    app_config_update_crc32(&next);
    APP_CONFIG_LOCK();
    current_config = next;
    app_config_initialized = true;
    APP_CONFIG_UNLOCK();

    if (notify) {
        notify_subscribers(&next);
    }

    return 0;
}

int app_config_reset_defaults(bool notify)
{
    struct app_config defaults;

    (void)app_config_get_defaults(&defaults);
    return app_config_set(&defaults, notify);
}

int app_config_subscribe(app_config_listener_t handler, void *user_data)
{
    size_t i;

    if (handler == NULL) {
        return -EINVAL;
    }

    APP_CONFIG_LOCK();
    for (i = 0; i < subscriber_count; i++) {
        if (subscribers[i].handler == handler &&
            subscribers[i].user_data == user_data) {
            APP_CONFIG_UNLOCK();
            return 0;
        }
    }

    if (subscriber_count >= APP_CONFIG_MAX_LISTENERS) {
        APP_CONFIG_UNLOCK();
        return -ENOMEM;
    }

    subscribers[subscriber_count].handler = handler;
    subscribers[subscriber_count].user_data = user_data;
    subscriber_count++;
    APP_CONFIG_UNLOCK();
    return 0;
}

void app_config_notify_all(void)
{
    struct app_config config;

    if (app_config_get(&config) == 0) {
        notify_subscribers(&config);
    }
}

#if defined(UNIT_TEST)
void app_config_reset_for_test(void)
{
    memset(&current_config, 0, sizeof(current_config));
    memset(subscribers, 0, sizeof(subscribers));
    subscriber_count = 0;
    app_config_initialized = false;
}
#endif
