#include <assert.h>
#include <stdint.h>

#include <rgb/rgb_keymap.h>
#include <rgb/rgb_types.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>

static void test_rgb_keymap_maps_chain_order(void)
{
    uint8_t index = 0xff;

    assert(!rgb_keymap_lookup(INPUT_KEY_NUMLOCK, &index));
    assert(index == 0xff);

    assert(rgb_keymap_lookup(INPUT_KEY_KPSLASH, &index));
    assert(index == 1);
    assert(rgb_keymap_lookup(INPUT_KEY_KPASTERISK, &index));
    assert(index == 2);
    assert(rgb_keymap_lookup(INPUT_KEY_KPMINUS, &index));
    assert(index == 3);
    assert(rgb_keymap_lookup(INPUT_KEY_KP7, &index));
    assert(index == 4);
    assert(rgb_keymap_lookup(INPUT_KEY_KP8, &index));
    assert(index == 5);
    assert(rgb_keymap_lookup(INPUT_KEY_KP9, &index));
    assert(index == 6);
    assert(rgb_keymap_lookup(INPUT_KEY_KP4, &index));
    assert(index == 7);
    assert(rgb_keymap_lookup(INPUT_KEY_KP5, &index));
    assert(index == 8);
    assert(rgb_keymap_lookup(INPUT_KEY_KP6, &index));
    assert(index == 9);
    assert(rgb_keymap_lookup(INPUT_KEY_KPPLUS, &index));
    assert(index == 10);
    assert(rgb_keymap_lookup(INPUT_KEY_KP1, &index));
    assert(index == 11);
    assert(rgb_keymap_lookup(INPUT_KEY_KP2, &index));
    assert(index == 12);
    assert(rgb_keymap_lookup(INPUT_KEY_KP3, &index));
    assert(index == 13);
    assert(rgb_keymap_lookup(INPUT_KEY_KP0, &index));
    assert(index == 14);
    assert(rgb_keymap_lookup(INPUT_KEY_KPDOT, &index));
    assert(index == 15);
    assert(rgb_keymap_lookup(INPUT_KEY_KPENTER, &index));
    assert(index == 16);
}

static void test_rgb_keymap_excludes_non_reactive_keys(void)
{
    uint8_t index = 7;

    assert(!rgb_keymap_lookup(INPUT_KEY_NUMLOCK, &index));
    assert(index == 7);
    assert(!rgb_keymap_lookup(INPUT_KEY_MUTE, &index));
    assert(index == 7);
    assert(!rgb_keymap_lookup(0xffff, &index));
    assert(index == 7);
    assert(RGB_LED_COUNT == 17);
    assert(RGB_NUM_LOCK_INDEX == 0);
}

int main(void)
{
    test_rgb_keymap_maps_chain_order();
    test_rgb_keymap_excludes_non_reactive_keys();
    return 0;
}
