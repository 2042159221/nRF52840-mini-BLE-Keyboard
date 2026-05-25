#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#ifndef __packed
#define __packed __attribute__((__packed__))
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define APP_CONFIG_VERSION 1u

enum app_rgb_mode {
    APP_RGB_MODE_OFF = 0,
    APP_RGB_MODE_STATIC = 1,
    APP_RGB_MODE_KEY_REACTIVE = 2,
};

enum app_encoder_action {
    APP_ENCODER_ACTION_NONE = 0,
    APP_ENCODER_ACTION_VOLUME_UP = 1,
    APP_ENCODER_ACTION_VOLUME_DOWN = 2,
    APP_ENCODER_ACTION_MUTE = 3,
    APP_ENCODER_ACTION_PLAY_PAUSE = 4,
    APP_ENCODER_ACTION_NEXT_TRACK = 5,
    APP_ENCODER_ACTION_PREV_TRACK = 6,
    APP_ENCODER_ACTION_RGB_MODE_NEXT = 7,
};

enum app_numlock_policy {
    APP_NUMLOCK_FOLLOW_HOST = 0,
    APP_NUMLOCK_ALWAYS_WHITE = 1,
    APP_NUMLOCK_OFF_WHEN_LOW_POWER = 2,
};

enum app_mode_policy {
    APP_MODE_POLICY_FOLLOW_HARDWARE = 0,
    APP_MODE_POLICY_LAST_MODE = 1,
    APP_MODE_POLICY_USB_PREFERRED = 2,
    APP_MODE_POLICY_BLE_PREFERRED = 3,
};

enum app_default_mode {
    APP_DEFAULT_MODE_USB = 0,
    APP_DEFAULT_MODE_BLE = 1,
};

struct app_config {
    uint32_t version;

    bool rgb_enable;
    uint8_t rgb_mode;
    uint8_t rgb_brightness;
    uint8_t rgb_red;
    uint8_t rgb_green;
    uint8_t rgb_blue;
    uint8_t numlock_policy;

    uint8_t encoder_cw_action;
    uint8_t encoder_ccw_action;
    uint8_t encoder_press_action;

    uint8_t mode_policy;
    uint8_t default_mode;

    uint32_t crc32;
} __packed;

typedef void (*app_config_listener_t)(const struct app_config *config,
                      void *user_data);

int app_config_init(void);
int app_config_get_defaults(struct app_config *out);
int app_config_get(struct app_config *out);
int app_config_set(const struct app_config *config, bool notify);
int app_config_reset_defaults(bool notify);
int app_config_validate(const struct app_config *config);
int app_config_subscribe(app_config_listener_t handler, void *user_data);
void app_config_notify_all(void);
uint32_t app_config_compute_crc32(const struct app_config *config);
void app_config_update_crc32(struct app_config *config);
bool app_config_crc32_valid(const struct app_config *config);

#if defined(UNIT_TEST)
void app_config_reset_for_test(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
