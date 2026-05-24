#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#if !defined(UNIT_TEST)
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#endif

#include <power/ip5306.h>

#if !defined(UNIT_TEST)
LOG_MODULE_REGISTER(ip5306, LOG_LEVEL_INF);

#define IP5306_NODE DT_NODELABEL(ip5306)

static const struct i2c_dt_spec ip5306_i2c = I2C_DT_SPEC_GET(IP5306_NODE);
static bool ip5306_ready;
#endif

#define IP5306_CHARGING_STATUS_REG 0x70
#define IP5306_CHARGE_FULL_STATUS_REG 0x71
#define IP5306_STATUS_CHARGING_MASK ((uint8_t)(1u << 3))
#define IP5306_STATUS_CHARGE_FULL_MASK ((uint8_t)(1u << 3))
#define IP5306_STATUS_VALID_FLAGS \
    (IP5306_STATUS_VALID_USB_PRESENT | IP5306_STATUS_VALID_CHARGE_FULL | \
     IP5306_STATUS_VALID_CHARGING_STATE)

#if defined(UNIT_TEST)
#define IP5306_TESTABLE
static uint8_t ip5306_test_charging_status_reg;
static uint8_t ip5306_test_charge_full_status_reg;
static int ip5306_test_read_error = -ENOTSUP;
#else
#define IP5306_TESTABLE static
#endif

IP5306_TESTABLE int ip5306_decode_status_registers(uint8_t charging_status_reg,
                           uint8_t charge_full_status_reg,
                           struct ip5306_status *status)
{
    const bool charging = (charging_status_reg & IP5306_STATUS_CHARGING_MASK) != 0;
    const bool charge_full =
        (charge_full_status_reg & IP5306_STATUS_CHARGE_FULL_MASK) != 0;

    if (status == NULL) {
        return -EINVAL;
    }

    status->usb_present = charging || charge_full;
    status->charge_full = charge_full;
    if (charge_full) {
        status->charging_state = IP5306_CHARGING_STATE_FULL;
    } else if (charging) {
        status->charging_state = IP5306_CHARGING_STATE_CHARGING;
    } else {
        status->charging_state = IP5306_CHARGING_STATE_DISCHARGING;
    }
    status->valid_flags = IP5306_STATUS_VALID_FLAGS;
    return 0;
}

int ip5306_get_status(struct ip5306_status *status)
{
    uint8_t charging_status_reg;
    uint8_t charge_full_status_reg;
    int err;

    if (status == NULL) {
        return -EINVAL;
    }

    err = ip5306_read_reg(IP5306_CHARGING_STATUS_REG, &charging_status_reg);
    if (err < 0) {
        return err;
    }

    err = ip5306_read_reg(IP5306_CHARGE_FULL_STATUS_REG, &charge_full_status_reg);
    if (err < 0) {
        return err;
    }

    return ip5306_decode_status_registers(charging_status_reg,
                          charge_full_status_reg, status);
}

int ip5306_get_charging_state(enum ip5306_charging_state *state)
{
    struct ip5306_status status;
    int err;

    if (state == NULL) {
        return -EINVAL;
    }

    err = ip5306_get_status(&status);
    if (err < 0) {
        return err;
    }

    *state = status.charging_state;
    return 0;
}

int ip5306_is_usb_present(bool *present)
{
    struct ip5306_status status;
    int err;

    if (present == NULL) {
        return -EINVAL;
    }

    err = ip5306_get_status(&status);
    if (err < 0) {
        return err;
    }

    *present = status.usb_present;
    return 0;
}

int ip5306_is_charge_full(bool *full)
{
    struct ip5306_status status;
    int err;

    if (full == NULL) {
        return -EINVAL;
    }

    err = ip5306_get_status(&status);
    if (err < 0) {
        return err;
    }

    *full = status.charge_full;
    return 0;
}

#if defined(UNIT_TEST)
int ip5306_init(void)
{
    return -ENOTSUP;
}

int ip5306_probe(void)
{
    return -ENOTSUP;
}

int ip5306_read_reg(uint8_t reg, uint8_t *value)
{
    if (value == NULL) {
        return -EINVAL;
    }

    if (ip5306_test_read_error < 0) {
        return ip5306_test_read_error;
    }

    switch (reg) {
    case IP5306_CHARGING_STATUS_REG:
        *value = ip5306_test_charging_status_reg;
        return 0;
    case IP5306_CHARGE_FULL_STATUS_REG:
        *value = ip5306_test_charge_full_status_reg;
        return 0;
    default:
        return -EINVAL;
    }
}

int ip5306_write_reg(uint8_t reg, uint8_t value)
{
    (void)reg;
    (void)value;
    return -ENOTSUP;
}

void ip5306_test_set_status_register_values(uint8_t charging_status_reg,
                        uint8_t charge_full_status_reg)
{
    ip5306_test_charging_status_reg = charging_status_reg;
    ip5306_test_charge_full_status_reg = charge_full_status_reg;
    ip5306_test_read_error = 0;
}

void ip5306_test_set_status_read_error(int err)
{
    ip5306_test_read_error = (err < 0) ? err : -EIO;
}
#else
int ip5306_init(void)
{
    ip5306_ready = false;

    if (!device_is_ready(ip5306_i2c.bus)) {
        LOG_WRN("power: ip5306 i2c bus not ready");
        return -ENODEV;
    }

    ip5306_ready = true;
    return 0;
}

int ip5306_probe(void)
{
    uint8_t value;
    int err;

    if (!ip5306_ready) {
        return -ENODEV;
    }

    err = ip5306_read_reg(IP5306_CHARGING_STATUS_REG, &value);
    if (err < 0) {
        LOG_WRN("power: ip5306 probe failed: %d", err);
        return err;
    }

    LOG_INF("power: ip5306 probe ok");
    return 0;
}

int ip5306_read_reg(uint8_t reg, uint8_t *value)
{
    if (value == NULL) {
        return -EINVAL;
    }

    if (!ip5306_ready) {
        return -ENODEV;
    }

    return i2c_reg_read_byte_dt(&ip5306_i2c, reg, value);
}

int ip5306_write_reg(uint8_t reg, uint8_t value)
{
    if (!ip5306_ready) {
        return -ENODEV;
    }

    return i2c_reg_write_byte_dt(&ip5306_i2c, reg, value);
}
#endif
