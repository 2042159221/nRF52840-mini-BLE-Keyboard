#include <transport/transport_ble_security.h>

#if defined(__has_include)
#if __has_include(<zephyr/bluetooth/conn.h>)
#include <zephyr/bluetooth/conn.h>
#define TRANSPORT_BLE_SECURITY_ERR_PIN_OR_KEY_MISSING BT_SECURITY_ERR_PIN_OR_KEY_MISSING
#endif
#endif

#ifndef TRANSPORT_BLE_SECURITY_ERR_PIN_OR_KEY_MISSING
#define TRANSPORT_BLE_SECURITY_ERR_PIN_OR_KEY_MISSING 2
#endif

bool transport_ble_security_error_requires_bond_reset(int security_err)
{
    return security_err == TRANSPORT_BLE_SECURITY_ERR_PIN_OR_KEY_MISSING;
}
