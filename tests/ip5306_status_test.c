#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <power/ip5306.h>

static void test_status_decode_maps_charging_full_and_discharge(void)
{
    struct ip5306_status status;

    assert(ip5306_decode_status_registers(0x08, 0x00, &status) == 0);
    assert(status.usb_present);
    assert(!status.charge_full);
    assert(status.charging_state == IP5306_CHARGING_STATE_CHARGING);
    assert((status.valid_flags & IP5306_STATUS_VALID_CHARGING_STATE) != 0);

    assert(ip5306_decode_status_registers(0x08, 0x08, &status) == 0);
    assert(status.usb_present);
    assert(status.charge_full);
    assert(status.charging_state == IP5306_CHARGING_STATE_FULL);

    assert(ip5306_decode_status_registers(0x00, 0x00, &status) == 0);
    assert(!status.usb_present);
    assert(!status.charge_full);
    assert(status.charging_state == IP5306_CHARGING_STATE_DISCHARGING);
}

static void test_status_accessors_use_read_registers(void)
{
    enum ip5306_charging_state charging_state;
    bool present = false;
    bool full = true;

    ip5306_test_set_status_register_values(0x08, 0x00);

    assert(ip5306_get_charging_state(&charging_state) == 0);
    assert(charging_state == IP5306_CHARGING_STATE_CHARGING);
    assert(ip5306_is_usb_present(&present) == 0);
    assert(present);
    assert(ip5306_is_charge_full(&full) == 0);
    assert(!full);
}

static void test_status_read_errors_are_reported(void)
{
    struct ip5306_status status;

    ip5306_test_set_status_read_error(-EIO);

    assert(ip5306_get_status(&status) == -EIO);
}

int main(void)
{
    test_status_decode_maps_charging_full_and_discharge();
    test_status_accessors_use_read_registers();
    test_status_read_errors_are_reported();
    return 0;
}
