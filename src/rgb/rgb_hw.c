#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <rgb/rgb_hw.h>

LOG_MODULE_REGISTER(rgb_hw, LOG_LEVEL_INF);

#define RGB_STRIP_NODE DT_ALIAS(led_strip)
#define RGB_POWER_NODE DT_PATH(zephyr_user)

#if DT_NODE_HAS_PROP(RGB_POWER_NODE, rgb_power_enable_gpios)
#define RGB_HAS_POWER_ENABLE 1
static const struct gpio_dt_spec rgb_power_enable =
    GPIO_DT_SPEC_GET(RGB_POWER_NODE, rgb_power_enable_gpios);
#else
#define RGB_HAS_POWER_ENABLE 0
#endif

static const struct device *const rgb_strip = DEVICE_DT_GET(RGB_STRIP_NODE);
static struct led_rgb rgb_hw_pixels[RGB_LED_COUNT];
static bool rgb_hw_ready;
static bool rgb_power_enabled;

int rgb_hw_set_power(bool enabled)
{
#if RGB_HAS_POWER_ENABLE
    int err;

    if (!gpio_is_ready_dt(&rgb_power_enable)) {
        return -ENODEV;
    }

    err = gpio_pin_set_dt(&rgb_power_enable, enabled ? 1 : 0);
    if (err < 0) {
        return err;
    }

    rgb_power_enabled = enabled;
    if (enabled) {
        k_sleep(K_MSEC(2));
    }
    return 0;
#else
    (void)enabled;
    return -ENODEV;
#endif
}

int rgb_hw_init(void)
{
    int err;

#if RGB_HAS_POWER_ENABLE
    if (!gpio_is_ready_dt(&rgb_power_enable)) {
        LOG_ERR("RGB power enable GPIO is not ready");
        return -ENODEV;
    }

    err = gpio_pin_configure_dt(&rgb_power_enable, GPIO_OUTPUT_INACTIVE);
    if (err < 0) {
        LOG_ERR("RGB power enable GPIO config failed: %d", err);
        return err;
    }
#else
    LOG_ERR("RGB power enable GPIO is missing from zephyr,user");
    return -ENODEV;
#endif

    if (!device_is_ready(rgb_strip)) {
        LOG_ERR("RGB LED strip device is not ready");
        return -ENODEV;
    }

    rgb_hw_ready = true;
    LOG_INF("RGB hardware ready");
    return 0;
}

bool rgb_hw_is_ready(void)
{
    return rgb_hw_ready;
}

int rgb_hw_show(const struct rgb_color *pixels, size_t count)
{
    size_t i;
    int err;

    if (pixels == NULL) {
        return -EINVAL;
    }

    if (!rgb_hw_ready) {
        return -ENODEV;
    }

    if (count > RGB_LED_COUNT) {
        return -ERANGE;
    }

    if (!rgb_power_enabled) {
        err = rgb_hw_set_power(true);
        if (err < 0) {
            return err;
        }
    }

    memset(rgb_hw_pixels, 0, sizeof(rgb_hw_pixels));
    for (i = 0; i < count; i++) {
        rgb_hw_pixels[i].r = pixels[i].red;
        rgb_hw_pixels[i].g = pixels[i].green;
        rgb_hw_pixels[i].b = pixels[i].blue;
    }

    return led_strip_update_rgb(rgb_strip, rgb_hw_pixels, RGB_LED_COUNT);
}

int rgb_hw_off(void)
{
    int err = 0;

    if (rgb_hw_ready) {
        memset(rgb_hw_pixels, 0, sizeof(rgb_hw_pixels));
        err = led_strip_update_rgb(rgb_strip, rgb_hw_pixels, RGB_LED_COUNT);
    }

    (void)rgb_hw_set_power(false);
    return err;
}
