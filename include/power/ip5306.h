#ifndef POWER_IP5306_H
#define POWER_IP5306_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IP5306_I2C_ADDR 0x75

enum ip5306_charging_state {
    IP5306_CHARGING_STATE_UNKNOWN,
    IP5306_CHARGING_STATE_DISCHARGING,
    IP5306_CHARGING_STATE_CHARGING,
    IP5306_CHARGING_STATE_FULL,
};

enum ip5306_status_valid_flag {
    IP5306_STATUS_VALID_USB_PRESENT = 1u << 0,
    IP5306_STATUS_VALID_CHARGE_FULL = 1u << 1,
    IP5306_STATUS_VALID_CHARGING_STATE = 1u << 2,
};

struct ip5306_status {
    bool usb_present;
    bool charge_full;
    enum ip5306_charging_state charging_state;
    uint32_t valid_flags;
};

int ip5306_init(void);
int ip5306_probe(void);
int ip5306_read_reg(uint8_t reg, uint8_t *value);
int ip5306_write_reg(uint8_t reg, uint8_t value);
int ip5306_get_status(struct ip5306_status *status);
int ip5306_get_charging_state(enum ip5306_charging_state *state);
int ip5306_is_usb_present(bool *present);
int ip5306_is_charge_full(bool *full);

#if defined(UNIT_TEST)
int ip5306_decode_status_registers(uint8_t charging_status_reg,
                   uint8_t charge_full_status_reg,
                   struct ip5306_status *status);
void ip5306_test_set_status_register_values(uint8_t charging_status_reg,
                        uint8_t charge_full_status_reg);
void ip5306_test_set_status_read_error(int err);
#endif

#ifdef __cplusplus
}
#endif

#endif
