#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <config/app_config.h>
#include <config/app_config_store.h>

static unsigned int listener_call_count;
static struct app_config listener_last_config;

static void listener(const struct app_config *config, void *user_data)
{
    bool *called = user_data;

    listener_call_count++;
    listener_last_config = *config;
    if (called != NULL) {
        *called = true;
    }
}

static struct app_config valid_config(void)
{
    struct app_config config;

    assert(app_config_get_defaults(&config) == 0);
    return config;
}

static void test_defaults_match_current_product_behavior(void)
{
    struct app_config config;

    assert(app_config_get_defaults(&config) == 0);
    assert(config.version == APP_CONFIG_VERSION);
    assert(config.rgb_enable);
    assert(config.rgb_mode == APP_RGB_MODE_KEY_REACTIVE);
    assert(config.rgb_brightness == 30);
    assert(config.rgb_red == 255);
    assert(config.rgb_green == 80);
    assert(config.rgb_blue == 20);
    assert(config.numlock_policy == APP_NUMLOCK_ALWAYS_WHITE);
    assert(config.encoder_cw_action == APP_ENCODER_ACTION_VOLUME_UP);
    assert(config.encoder_ccw_action == APP_ENCODER_ACTION_VOLUME_DOWN);
    assert(config.encoder_press_action == APP_ENCODER_ACTION_MUTE);
    assert(config.mode_policy == APP_MODE_POLICY_FOLLOW_HARDWARE);
    assert(config.default_mode == APP_DEFAULT_MODE_USB);
    assert(app_config_crc32_valid(&config));
}

static void test_validation_rejects_invalid_fields(void)
{
    struct app_config config = valid_config();

    config.rgb_brightness = 101;
    assert(app_config_validate(&config) == -EINVAL);

    config = valid_config();
    config.rgb_mode = 99;
    assert(app_config_validate(&config) == -EINVAL);

    config = valid_config();
    config.numlock_policy = 99;
    assert(app_config_validate(&config) == -EINVAL);

    config = valid_config();
    config.encoder_cw_action = 99;
    assert(app_config_validate(&config) == -EINVAL);

    config = valid_config();
    config.encoder_ccw_action = 99;
    assert(app_config_validate(&config) == -EINVAL);

    config = valid_config();
    config.encoder_press_action = 99;
    assert(app_config_validate(&config) == -EINVAL);

    config = valid_config();
    config.mode_policy = 99;
    assert(app_config_validate(&config) == -EINVAL);

    config = valid_config();
    config.default_mode = 99;
    assert(app_config_validate(&config) == -EINVAL);
}

static void test_crc_detects_mutation(void)
{
    struct app_config config = valid_config();

    assert(app_config_crc32_valid(&config));
    config.rgb_red = 1;
    assert(!app_config_crc32_valid(&config));
}

static void test_set_updates_runtime_and_notifies_when_requested(void)
{
    struct app_config config = valid_config();
    struct app_config latest;
    bool called = false;

    app_config_reset_for_test();
    listener_call_count = 0;
    assert(app_config_init() == 0);
    assert(app_config_subscribe(listener, &called) == 0);

    config.rgb_brightness = 72;
    assert(app_config_set(&config, true) == 0);
    assert(called);
    assert(listener_call_count == 1);
    assert(listener_last_config.rgb_brightness == 72);
    assert(app_config_get(&latest) == 0);
    assert(latest.rgb_brightness == 72);
    assert(app_config_crc32_valid(&latest));
}

static void test_set_can_update_without_notify(void)
{
    struct app_config config = valid_config();
    bool called = false;

    app_config_reset_for_test();
    listener_call_count = 0;
    assert(app_config_init() == 0);
    assert(app_config_subscribe(listener, &called) == 0);

    config.rgb_brightness = 11;
    assert(app_config_set(&config, false) == 0);
    assert(!called);
    assert(listener_call_count == 0);
}

static void test_subscribe_rejects_null_and_deduplicates(void)
{
    struct app_config config = valid_config();
    bool called = false;

    app_config_reset_for_test();
    listener_call_count = 0;
    assert(app_config_init() == 0);
    assert(app_config_subscribe(NULL, NULL) == -EINVAL);
    assert(app_config_subscribe(listener, &called) == 0);
    assert(app_config_subscribe(listener, &called) == 0);

    assert(app_config_set(&config, true) == 0);
    assert(called);
    assert(listener_call_count == 1);
}

static void test_reset_defaults_notifies_current_defaults(void)
{
    bool called = false;

    app_config_reset_for_test();
    listener_call_count = 0;
    assert(app_config_init() == 0);
    assert(app_config_subscribe(listener, &called) == 0);

    assert(app_config_reset_defaults(true) == 0);
    assert(called);
    assert(listener_call_count == 1);
    assert(listener_last_config.rgb_mode == APP_RGB_MODE_KEY_REACTIVE);
}

static void test_store_load_accepts_valid_blob(void)
{
    struct app_config stored = valid_config();
    struct app_config current;

    app_config_reset_for_test();
    app_config_store_reset_for_test();
    assert(app_config_init() == 0);

    stored.rgb_brightness = 64;
    app_config_update_crc32(&stored);
    app_config_store_seed_blob_for_test(&stored, sizeof(stored));

    assert(app_config_store_load() == 0);
    assert(app_config_get(&current) == 0);
    assert(current.rgb_brightness == 64);
    assert(app_config_crc32_valid(&current));
}

static void test_store_load_rejects_bad_length(void)
{
    struct app_config stored = valid_config();
    struct app_config current;

    app_config_reset_for_test();
    app_config_store_reset_for_test();
    assert(app_config_init() == 0);

    stored.rgb_brightness = 64;
    app_config_update_crc32(&stored);
    app_config_store_seed_blob_for_test(&stored, sizeof(stored) - 1u);

    assert(app_config_store_load() == 0);
    assert(app_config_get(&current) == 0);
    assert(current.rgb_brightness == 30);
}

static void test_store_load_rejects_bad_crc(void)
{
    struct app_config stored = valid_config();
    struct app_config current;

    app_config_reset_for_test();
    app_config_store_reset_for_test();
    assert(app_config_init() == 0);

    stored.rgb_brightness = 64;
    app_config_update_crc32(&stored);
    stored.rgb_green ^= 0x01u;
    app_config_store_seed_blob_for_test(&stored, sizeof(stored));

    assert(app_config_store_load() == 0);
    assert(app_config_get(&current) == 0);
    assert(current.rgb_brightness == 30);
}

static void test_store_load_rejects_invalid_fields(void)
{
    struct app_config stored = valid_config();
    struct app_config current;

    app_config_reset_for_test();
    app_config_store_reset_for_test();
    assert(app_config_init() == 0);

    stored.rgb_brightness = 101;
    app_config_update_crc32(&stored);
    app_config_store_seed_blob_for_test(&stored, sizeof(stored));

    assert(app_config_store_load() == 0);
    assert(app_config_get(&current) == 0);
    assert(current.rgb_brightness == 30);
}

static void test_store_save_is_explicit(void)
{
    struct app_config config = valid_config();
    struct app_config saved;

    app_config_reset_for_test();
    app_config_store_reset_for_test();
    assert(app_config_init() == 0);

    config.rgb_brightness = 55;
    assert(app_config_set(&config, false) == 0);
    assert(!app_config_store_saved_for_test(NULL));

    assert(app_config_store_save() == 0);
    assert(app_config_store_saved_for_test(&saved));
    assert(saved.rgb_brightness == 55);
    assert(app_config_crc32_valid(&saved));
}

int main(void)
{
    test_defaults_match_current_product_behavior();
    test_validation_rejects_invalid_fields();
    test_crc_detects_mutation();
    test_set_updates_runtime_and_notifies_when_requested();
    test_set_can_update_without_notify();
    test_subscribe_rejects_null_and_deduplicates();
    test_reset_defaults_notifies_current_defaults();
    test_store_load_accepts_valid_blob();
    test_store_load_rejects_bad_length();
    test_store_load_rejects_bad_crc();
    test_store_load_rejects_invalid_fields();
    test_store_save_is_explicit();
    return 0;
}
