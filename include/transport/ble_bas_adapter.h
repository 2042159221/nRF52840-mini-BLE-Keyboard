#ifndef TRANSPORT_BLE_BAS_ADAPTER_H
#define TRANSPORT_BLE_BAS_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

int ble_bas_adapter_init(void);

#if defined(UNIT_TEST)
void ble_bas_adapter_reset_for_test(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
