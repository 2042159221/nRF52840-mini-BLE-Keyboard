#ifndef KEYBOARD_LED_STATE_H
#define KEYBOARD_LED_STATE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KEYBOARD_LED_NUM_LOCK 0x01u

typedef void (*keyboard_led_state_listener_t)(uint8_t led_bits, void *user_data);

void keyboard_led_state_update(uint8_t led_bits);
uint8_t keyboard_led_state_get(void);
bool keyboard_led_state_num_lock(void);
int keyboard_led_state_subscribe(keyboard_led_state_listener_t listener,
                 void *user_data);

#if defined(UNIT_TEST)
void keyboard_led_state_reset_for_test(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
