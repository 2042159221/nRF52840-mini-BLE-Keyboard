#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

#include <power/ip5306.h>

LOG_MODULE_REGISTER(ip5306, LOG_LEVEL_INF);

#define IP5306_NODE DT_NODELABEL(ip5306)

static const struct i2c_dt_spec ip5306_i2c = I2C_DT_SPEC_GET(IP5306_NODE);
static bool ip5306_ready;

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

    err = ip5306_read_reg(0x70, &value);
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
