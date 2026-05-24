#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <rgb/rgb_effect.h>

static struct rgb_color rgb_scale_color(struct rgb_color color, uint8_t percent)
{
    struct rgb_color scaled;

    scaled.red = (uint8_t)(((uint16_t)color.red * percent) / 100u);
    scaled.green = (uint8_t)(((uint16_t)color.green * percent) / 100u);
    scaled.blue = (uint8_t)(((uint16_t)color.blue * percent) / 100u);
    return scaled;
}

static bool rgb_index_is_normal_key(uint8_t rgb_index)
{
    return rgb_index < RGB_LED_COUNT && rgb_index != RGB_NUM_LOCK_INDEX;
}

void rgb_effect_init(struct rgb_effect_state *effect)
{
    if (effect == NULL) {
        return;
    }

    memset(effect, 0, sizeof(*effect));
    effect->mode = RGB_MODE_KEY_REACTIVE;
    effect->color.red = 255;
}

void rgb_effect_set_mode(struct rgb_effect_state *effect, enum rgb_mode mode)
{
    if (effect == NULL) {
        return;
    }

    effect->mode = mode;
}

enum rgb_mode rgb_effect_get_mode(const struct rgb_effect_state *effect)
{
    if (effect == NULL) {
        return RGB_MODE_OFF;
    }

    return effect->mode;
}

void rgb_effect_set_color(struct rgb_effect_state *effect, struct rgb_color color)
{
    if (effect == NULL) {
        return;
    }

    effect->color = color;
}

void rgb_effect_key_pressed(struct rgb_effect_state *effect, uint8_t rgb_index,
                uint32_t now_ms)
{
    (void)now_ms;

    if (effect == NULL || !rgb_index_is_normal_key(rgb_index)) {
        return;
    }

    effect->keys[rgb_index].pressed = true;
    effect->keys[rgb_index].fading = false;
}

void rgb_effect_key_released(struct rgb_effect_state *effect, uint8_t rgb_index,
                 uint32_t now_ms)
{
    if (effect == NULL || !rgb_index_is_normal_key(rgb_index)) {
        return;
    }

    if (!effect->keys[rgb_index].pressed) {
        return;
    }

    effect->keys[rgb_index].pressed = false;
    effect->keys[rgb_index].released_at_ms = now_ms;
    effect->keys[rgb_index].fading = true;
}

bool rgb_effect_has_active_pixels(const struct rgb_effect_state *effect,
                  uint32_t now_ms)
{
    size_t i;

    if (effect == NULL) {
        return false;
    }

    if (effect->mode == RGB_MODE_STATIC) {
        return true;
    }

    if (effect->mode != RGB_MODE_KEY_REACTIVE) {
        return false;
    }

    for (i = 1; i < RGB_LED_COUNT; i++) {
        const struct rgb_effect_key_state *key = &effect->keys[i];

        if (key->pressed) {
            return true;
        }

        if (key->fading && (now_ms - key->released_at_ms) < RGB_FADE_DURATION_MS) {
            return true;
        }
    }

    return false;
}

static uint8_t reactive_brightness_percent(const struct rgb_effect_key_state *key,
                       uint8_t max_percent,
                       uint32_t now_ms)
{
    uint32_t elapsed_ms;
    uint32_t remaining_ms;

    if (key->pressed) {
        return max_percent;
    }

    if (!key->fading) {
        return 0;
    }

    elapsed_ms = now_ms - key->released_at_ms;
    if (elapsed_ms >= RGB_FADE_DURATION_MS) {
        return 0;
    }

    remaining_ms = RGB_FADE_DURATION_MS - elapsed_ms;
    return (uint8_t)(((uint32_t)max_percent * remaining_ms) /
             RGB_FADE_DURATION_MS);
}

void rgb_effect_render(struct rgb_effect_state *effect,
               const struct rgb_policy *policy,
               bool num_lock,
               uint32_t now_ms,
               struct rgb_color *pixels,
               size_t pixel_count)
{
    size_t i;

    if (effect == NULL || policy == NULL || pixels == NULL) {
        return;
    }

    memset(pixels, 0, pixel_count * sizeof(pixels[0]));
    if (pixel_count == 0 || policy->critical_shutdown) {
        return;
    }

    for (i = 1; i < pixel_count && i < RGB_LED_COUNT; i++) {
        uint8_t brightness = 0;

        if (effect->mode == RGB_MODE_STATIC) {
            brightness = policy->normal_brightness_percent;
        } else if (effect->mode == RGB_MODE_KEY_REACTIVE) {
            brightness = reactive_brightness_percent(&effect->keys[i],
                                 policy->normal_brightness_percent,
                                 now_ms);
            if (brightness == 0 && effect->keys[i].fading) {
                effect->keys[i].fading = false;
            }
        }

        pixels[i] = rgb_scale_color(effect->color, brightness);
    }

    if (num_lock && RGB_NUM_LOCK_INDEX < pixel_count) {
        const struct rgb_color white = {
            .red = 255,
            .green = 255,
            .blue = 255,
        };

        pixels[RGB_NUM_LOCK_INDEX] =
            rgb_scale_color(white, policy->num_brightness_percent);
    }
}
