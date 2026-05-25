#ifndef ENCODER_ACTION_H
#define ENCODER_ACTION_H

#include <stdint.h>

#include <config/app_config.h>

#ifdef __cplusplus
extern "C" {
#endif

int encoder_action_trigger(uint8_t action);
int encoder_action_trigger_rgb_mode_next(void);

#ifdef __cplusplus
}
#endif

#endif
