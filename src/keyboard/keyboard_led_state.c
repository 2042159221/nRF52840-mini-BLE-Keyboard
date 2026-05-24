#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <keyboard/keyboard_led_state.h>

#if !defined(UNIT_TEST)
#include <zephyr/kernel.h>
#endif

#define KEYBOARD_LED_STATE_MAX_LISTENERS 4u

struct keyboard_led_state_subscriber {
    keyboard_led_state_listener_t listener;
    void *user_data;
};

static uint8_t keyboard_led_bits;
static struct keyboard_led_state_subscriber subscribers[KEYBOARD_LED_STATE_MAX_LISTENERS];
static size_t subscriber_count;

#if !defined(UNIT_TEST)
K_MUTEX_DEFINE(keyboard_led_state_lock);
#define KEYBOARD_LED_STATE_LOCK() k_mutex_lock(&keyboard_led_state_lock, K_FOREVER)
#define KEYBOARD_LED_STATE_UNLOCK() k_mutex_unlock(&keyboard_led_state_lock)
#else
#define KEYBOARD_LED_STATE_LOCK() ((void)0)
#define KEYBOARD_LED_STATE_UNLOCK() ((void)0)
#endif

void keyboard_led_state_update(uint8_t led_bits)
{
    struct keyboard_led_state_subscriber local_subscribers[KEYBOARD_LED_STATE_MAX_LISTENERS];
    size_t local_count;
    size_t i;

    KEYBOARD_LED_STATE_LOCK();
    if (keyboard_led_bits == led_bits) {
        KEYBOARD_LED_STATE_UNLOCK();
        return;
    }

    keyboard_led_bits = led_bits;
    local_count = subscriber_count;
    memcpy(local_subscribers, subscribers,
           local_count * sizeof(local_subscribers[0]));
    KEYBOARD_LED_STATE_UNLOCK();

    for (i = 0; i < local_count; i++) {
        local_subscribers[i].listener(led_bits, local_subscribers[i].user_data);
    }
}

uint8_t keyboard_led_state_get(void)
{
    uint8_t led_bits;

    KEYBOARD_LED_STATE_LOCK();
    led_bits = keyboard_led_bits;
    KEYBOARD_LED_STATE_UNLOCK();
    return led_bits;
}

bool keyboard_led_state_num_lock(void)
{
    return (keyboard_led_state_get() & KEYBOARD_LED_NUM_LOCK) != 0;
}

int keyboard_led_state_subscribe(keyboard_led_state_listener_t listener,
                 void *user_data)
{
    size_t i;

    if (listener == NULL) {
        return -EINVAL;
    }

    KEYBOARD_LED_STATE_LOCK();
    for (i = 0; i < subscriber_count; i++) {
        if (subscribers[i].listener == listener &&
            subscribers[i].user_data == user_data) {
            KEYBOARD_LED_STATE_UNLOCK();
            return 0;
        }
    }

    if (subscriber_count >= KEYBOARD_LED_STATE_MAX_LISTENERS) {
        KEYBOARD_LED_STATE_UNLOCK();
        return -ENOMEM;
    }

    subscribers[subscriber_count].listener = listener;
    subscribers[subscriber_count].user_data = user_data;
    subscriber_count++;
    KEYBOARD_LED_STATE_UNLOCK();
    return 0;
}

#if defined(UNIT_TEST)
void keyboard_led_state_reset_for_test(void)
{
    keyboard_led_bits = 0;
    subscriber_count = 0;
    memset(subscribers, 0, sizeof(subscribers));
}
#endif
