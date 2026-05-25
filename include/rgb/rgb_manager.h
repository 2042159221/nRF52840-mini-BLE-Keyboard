#ifndef RGB_MANAGER_H
#define RGB_MANAGER_H

#include <stdint.h>

#include <config/app_config.h>
#include <rgb/rgb_types.h>

#ifdef __cplusplus
extern "C" {
#endif

int rgb_manager_init(void);
void rgb_manager_set_mode(enum rgb_mode mode);
enum rgb_mode rgb_manager_get_mode(void);
void rgb_manager_set_color(uint8_t red, uint8_t green, uint8_t blue);
void rgb_manager_apply_config(const struct app_config *config);
void rgb_manager_mark_dirty(void);

#ifdef __cplusplus
}
#endif

#endif
