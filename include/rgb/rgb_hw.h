#ifndef RGB_HW_H
#define RGB_HW_H

#include <stdbool.h>
#include <stddef.h>

#include <rgb/rgb_types.h>

#ifdef __cplusplus
extern "C" {
#endif

int rgb_hw_init(void);
bool rgb_hw_is_ready(void);
int rgb_hw_set_power(bool enabled);
int rgb_hw_show(const struct rgb_color *pixels, size_t count);
int rgb_hw_off(void);

#ifdef __cplusplus
}
#endif

#endif
