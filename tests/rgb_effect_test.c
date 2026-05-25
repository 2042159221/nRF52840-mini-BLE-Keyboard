#include <assert.h>
#include <string.h>

#include <rgb/rgb_effect.h>

static void assert_pixel(const struct rgb_color *pixels, uint8_t index,
             uint8_t red, uint8_t green, uint8_t blue)
{
    assert(pixels[index].red == red);
    assert(pixels[index].green == green);
    assert(pixels[index].blue == blue);
}

static void test_key_reactive_press_hold_and_fade(void)
{
    struct rgb_effect_state effect;
    struct rgb_color pixels[RGB_LED_COUNT];
    struct rgb_policy policy;

    rgb_policy_init(&policy);
    policy.normal_brightness_percent = 40;
    policy.num_brightness_percent = 8;

    rgb_effect_init(&effect);
    rgb_effect_key_pressed(&effect, 4, 100);
    memset(pixels, 0, sizeof(pixels));
    rgb_effect_render(&effect, &policy, false, 120, pixels, RGB_LED_COUNT);
    assert_pixel(pixels, 4, 102, 0, 0);

    rgb_effect_key_released(&effect, 4, 200);
    memset(pixels, 0, sizeof(pixels));
    rgb_effect_render(&effect, &policy, false, 700, pixels, RGB_LED_COUNT);
    assert_pixel(pixels, 4, 51, 0, 0);

    memset(pixels, 0, sizeof(pixels));
    rgb_effect_render(&effect, &policy, false, 1200, pixels, RGB_LED_COUNT);
    assert_pixel(pixels, 4, 0, 0, 0);
}

static void test_repress_during_fade_resets_brightness(void)
{
    struct rgb_effect_state effect;
    struct rgb_color pixels[RGB_LED_COUNT];
    struct rgb_policy policy;

    rgb_policy_init(&policy);
    policy.normal_brightness_percent = 40;

    rgb_effect_init(&effect);
    rgb_effect_key_pressed(&effect, 5, 0);
    rgb_effect_key_released(&effect, 5, 100);
    rgb_effect_key_pressed(&effect, 5, 500);
    memset(pixels, 0, sizeof(pixels));
    rgb_effect_render(&effect, &policy, false, 900, pixels, RGB_LED_COUNT);

    assert_pixel(pixels, 5, 102, 0, 0);
}

static void test_static_off_and_num_overlay(void)
{
    struct rgb_effect_state effect;
    struct rgb_color pixels[RGB_LED_COUNT];
    struct rgb_policy policy;

    rgb_policy_init(&policy);
    policy.normal_brightness_percent = 20;
    policy.num_brightness_percent = 20;

    rgb_effect_init(&effect);
    rgb_effect_set_mode(&effect, RGB_MODE_STATIC);
    rgb_effect_set_color(&effect, (struct rgb_color){ .red = 0, .green = 0, .blue = 200 });
    rgb_effect_render(&effect, &policy, true, 0, pixels, RGB_LED_COUNT);
    assert_pixel(pixels, 1, 0, 0, 40);
    assert_pixel(pixels, RGB_NUM_LOCK_INDEX, 51, 51, 51);

    rgb_effect_set_mode(&effect, RGB_MODE_OFF);
    rgb_effect_render(&effect, &policy, true, 0, pixels, RGB_LED_COUNT);
    assert_pixel(pixels, 1, 0, 0, 0);
    assert_pixel(pixels, RGB_NUM_LOCK_INDEX, 51, 51, 51);

    policy.critical_shutdown = true;
    rgb_effect_render(&effect, &policy, true, 0, pixels, RGB_LED_COUNT);
    assert_pixel(pixels, RGB_NUM_LOCK_INDEX, 0, 0, 0);
}

int main(void)
{
    test_key_reactive_press_hold_and_fade();
    test_repress_during_fade_resets_brightness();
    test_static_off_and_num_overlay();
    return 0;
}
