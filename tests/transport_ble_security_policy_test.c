#include <assert.h>

#include <transport/transport_ble_security.h>

#define BT_SECURITY_ERR_SUCCESS 0
#define BT_SECURITY_ERR_AUTH_FAIL 1
#define BT_SECURITY_ERR_PIN_OR_KEY_MISSING 2
#define BT_SECURITY_ERR_AUTH_REQUIREMENT 4

static void test_pin_or_key_missing_requires_bond_reset(void)
{
    assert(transport_ble_security_error_requires_bond_reset(
           BT_SECURITY_ERR_PIN_OR_KEY_MISSING));
}

static void test_other_security_errors_do_not_reset_bonds(void)
{
    assert(!transport_ble_security_error_requires_bond_reset(
           BT_SECURITY_ERR_SUCCESS));
    assert(!transport_ble_security_error_requires_bond_reset(
           BT_SECURITY_ERR_AUTH_FAIL));
    assert(!transport_ble_security_error_requires_bond_reset(
           BT_SECURITY_ERR_AUTH_REQUIREMENT));
}

int main(void)
{
    test_pin_or_key_missing_requires_bond_reset();
    test_other_security_errors_do_not_reset_bonds();
    return 0;
}
