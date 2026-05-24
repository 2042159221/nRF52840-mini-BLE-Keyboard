#include <assert.h>

#include <mode/mode_selector.h>

static void test_measured_switch_positions_are_classified(void)
{
    assert(mode_selector_classify_mv(0, KB_MODE_24G_RESERVED) == KB_MODE_USB);
    assert(mode_selector_classify_mv(100, KB_MODE_24G_RESERVED) == KB_MODE_USB);
    assert(mode_selector_classify_mv(1021, KB_MODE_24G_RESERVED) == KB_MODE_24G_RESERVED);
    assert(mode_selector_classify_mv(1027, KB_MODE_24G_RESERVED) == KB_MODE_24G_RESERVED);
    assert(mode_selector_classify_mv(1500, KB_MODE_24G_RESERVED) == KB_MODE_24G_RESERVED);
    assert(mode_selector_classify_mv(1885, KB_MODE_24G_RESERVED) == KB_MODE_BLE);
    assert(mode_selector_classify_mv(3304, KB_MODE_24G_RESERVED) == KB_MODE_BLE);
}

static void test_mode_selector_hysteresis_holds_near_boundaries(void)
{
    assert(mode_selector_classify_mv(900, KB_MODE_USB) == KB_MODE_USB);
    assert(mode_selector_classify_mv(760, KB_MODE_24G_RESERVED) == KB_MODE_24G_RESERVED);
    assert(mode_selector_classify_mv(1750, KB_MODE_24G_RESERVED) == KB_MODE_24G_RESERVED);
    assert(mode_selector_classify_mv(1650, KB_MODE_BLE) == KB_MODE_BLE);
}

static void test_reserved_position_exits_ble_mode(void)
{
    assert(mode_selector_classify_mv(1500, KB_MODE_BLE) == KB_MODE_24G_RESERVED);
}

int main(void)
{
    test_measured_switch_positions_are_classified();
    test_mode_selector_hysteresis_holds_near_boundaries();
    test_reserved_position_exits_ble_mode();
    return 0;
}
