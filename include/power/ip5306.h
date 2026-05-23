#ifndef POWER_IP5306_H
#define POWER_IP5306_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IP5306_I2C_ADDR 0x75

int ip5306_init(void);
int ip5306_probe(void);
int ip5306_read_reg(uint8_t reg, uint8_t *value);
int ip5306_write_reg(uint8_t reg, uint8_t value);

#ifdef __cplusplus
}
#endif

#endif
