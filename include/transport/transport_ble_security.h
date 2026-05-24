#ifndef TRANSPORT_BLE_SECURITY_H
#define TRANSPORT_BLE_SECURITY_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TRANSPORT_BLE_SECURITY_RECONNECT_DELAY_MS 250
#define TRANSPORT_BLE_SECURITY_BOND_RESET_DELAY_MS 10000

bool transport_ble_security_error_requires_bond_reset(int security_err);
int transport_ble_security_reconnect_delay_ms(int security_err);

#ifdef __cplusplus
}
#endif

#endif
