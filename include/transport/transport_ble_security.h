#ifndef TRANSPORT_BLE_SECURITY_H
#define TRANSPORT_BLE_SECURITY_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool transport_ble_security_error_requires_bond_reset(int security_err);

#ifdef __cplusplus
}
#endif

#endif
