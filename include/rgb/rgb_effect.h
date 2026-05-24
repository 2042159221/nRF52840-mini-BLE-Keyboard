#ifndef RGB_EFFECT_H
#define RGB_EFFECT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <rgb/rgb_policy.h>
#include <rgb/rgb_types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct rgb_effect_key_state {
    bool pressed;
    uint32_t released_at_ms;
    bool fading;
};

struct rgb_effect_state {
    enum rgb_mode mode;
    struct rgb_color color;
    struct rgb_effect_key_state keys[RGB_LED_COUNT];
};

void rgb_effect_init(struct rgb_effect_state *effect);
void rgb_effect_set_mode(struct rgb_effect_state *effect, enum rgb_mode mode);
enum rgb_mode rgb_effect_get_mode(const struct rgb_effect_state *effect);
void rgb_effect_set_color(struct rgb_effect_state *effect, struct rgb_color color);
void rgb_effect_key_pressed(struct rgb_effect_state *effect, uint8_t rgb_index,
                uint32_t now_ms);
void rgb_effect_key_released(struct rgb_effect_state *effect, uint8_t rgb_index,
                 uint32_t now_ms);
bool rgb_effect_has_active_pixels(const struct rgb_effect_state *effect,
                  uint32_t now_ms);
void rgb_effect_render(struct rgb_effect_state *effect,
               const struct rgb_policy *policy,
               bool num_lock,
               uint32_t now_ms,
               struct rgb_color *pixels,
               size_t pixel_count);

#ifdef __cplusplus
}
#endif

#endif
