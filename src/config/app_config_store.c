#include <errno.h>
#include <stddef.h>
#include <string.h>

#include <config/app_config.h>
#include <config/app_config_store.h>

#if !defined(UNIT_TEST)
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

LOG_MODULE_REGISTER(app_config_store, LOG_LEVEL_INF);
#endif

#define APP_CONFIG_STORE_KEY "app/config/blob"

#if !defined(UNIT_TEST)
static int app_config_store_ensure_ready(void)
{
    return settings_subsys_init();
}
#endif

#if defined(UNIT_TEST)
static uint8_t test_blob[sizeof(struct app_config)];
static size_t test_blob_len;
static bool test_blob_present;
static struct app_config test_saved_config;
static bool test_saved_present;

void app_config_store_reset_for_test(void)
{
    memset(test_blob, 0, sizeof(test_blob));
    test_blob_len = 0;
    test_blob_present = false;
    memset(&test_saved_config, 0, sizeof(test_saved_config));
    test_saved_present = false;
}

void app_config_store_seed_blob_for_test(const void *data, size_t len)
{
    memset(test_blob, 0, sizeof(test_blob));
    test_blob_len = len;
    test_blob_present = true;

    if (data != NULL && len <= sizeof(test_blob)) {
        memcpy(test_blob, data, len);
    }
}

bool app_config_store_saved_for_test(struct app_config *out)
{
    if (!test_saved_present) {
        return false;
    }

    if (out != NULL) {
        *out = test_saved_config;
    }

    return true;
}
#endif

int app_config_store_load(void)
{
#if defined(UNIT_TEST)
    struct app_config stored;

    if (!test_blob_present) {
        return app_config_reset_defaults(false);
    }

    if (test_blob_len != sizeof(stored)) {
        return app_config_reset_defaults(false);
    }

    memcpy(&stored, test_blob, sizeof(stored));
    if (!app_config_crc32_valid(&stored)) {
        return app_config_reset_defaults(false);
    }

    if (app_config_set(&stored, false) != 0) {
        return app_config_reset_defaults(false);
    }

    return 0;
#else
    struct app_config stored;
    ssize_t len;
    int err;

    err = app_config_store_ensure_ready();
    if (err != 0) {
        LOG_WRN("settings init failed: %d", err);
        return app_config_reset_defaults(false);
    }

    len = settings_load_one(APP_CONFIG_STORE_KEY, &stored, sizeof(stored));
    if (len == -ENOENT || len == 0) {
        LOG_INF("no persisted app config, using defaults");
        return app_config_reset_defaults(false);
    }

    if (len < 0) {
        LOG_WRN("app config load failed: %d", (int)len);
        return app_config_reset_defaults(false);
    }

    if ((size_t)len != sizeof(stored)) {
        LOG_WRN("app config size mismatch: %d", (int)len);
        return app_config_reset_defaults(false);
    }

    if (!app_config_crc32_valid(&stored)) {
        LOG_WRN("persisted app config crc mismatch");
        return app_config_reset_defaults(false);
    }

    err = app_config_set(&stored, false);
    if (err != 0) {
        LOG_WRN("persisted app config rejected: %d", err);
        return app_config_reset_defaults(false);
    }

    LOG_INF("app config loaded");
    return 0;
#endif
}

int app_config_store_save(void)
{
#if defined(UNIT_TEST)
    int err = app_config_get(&test_saved_config);

    if (err != 0) {
        return err;
    }

    app_config_update_crc32(&test_saved_config);
    test_saved_present = true;
    return 0;
#else
    struct app_config config;
    int err;

    err = app_config_store_ensure_ready();
    if (err != 0) {
        LOG_WRN("settings init failed: %d", err);
        return err;
    }

    err = app_config_get(&config);
    if (err != 0) {
        return err;
    }

    app_config_update_crc32(&config);
    err = settings_save_one(APP_CONFIG_STORE_KEY, &config, sizeof(config));
    if (err != 0) {
        LOG_WRN("app config save failed: %d", err);
        return err;
    }

    LOG_INF("app config saved");
    return 0;
#endif
}

int app_config_store_factory_reset(void)
{
    int err;

    err = app_config_reset_defaults(true);
    if (err != 0) {
        return err;
    }

    return app_config_store_save();
}
