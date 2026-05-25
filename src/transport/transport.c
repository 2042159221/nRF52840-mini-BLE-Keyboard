#include <errno.h>

#include <zephyr/logging/log.h>

#include <transport/transport.h>

LOG_MODULE_REGISTER(transport, LOG_LEVEL_INF);

int transport_usb_init(void);
int transport_usb_enable(void);
int transport_usb_disable(void);
bool transport_usb_ready(void);
bool transport_usb_consumer_ready(void);
int transport_usb_send_keyboard_report(const struct hid_keyboard_report *report);
int transport_usb_send_consumer_report(const struct hid_consumer_report *report);

int transport_ble_init(void);
int transport_ble_enable(void);
int transport_ble_disable(void);
bool transport_ble_ready(void);
bool transport_ble_consumer_ready(void);
int transport_ble_send_keyboard_report(const struct hid_keyboard_report *report);
int transport_ble_send_consumer_report(const struct hid_consumer_report *report);

int transport_24g_init(void);
int transport_24g_enable(void);
int transport_24g_disable(void);
bool transport_24g_ready(void);
bool transport_24g_consumer_ready(void);
int transport_24g_send_keyboard_report(const struct hid_keyboard_report *report);
int transport_24g_send_consumer_report(const struct hid_consumer_report *report);

int transport_init(void)
{
    int err = transport_usb_init();

    if (err != 0) {
        return err;
    }

    err = transport_ble_init();
    if (err != 0) {
        return err;
    }

    return transport_24g_init();
}

int transport_enable(enum kb_mode mode)
{
    switch (mode) {
    case KB_MODE_USB:
        return transport_usb_enable();
    case KB_MODE_BLE:
        return transport_ble_enable();
    case KB_MODE_24G_RESERVED:
        return transport_24g_enable();
    default:
        return -EINVAL;
    }
}

int transport_disable(enum kb_mode mode)
{
    switch (mode) {
    case KB_MODE_USB:
        return transport_usb_disable();
    case KB_MODE_BLE:
        return transport_ble_disable();
    case KB_MODE_24G_RESERVED:
        return transport_24g_disable();
    default:
        return -EINVAL;
    }
}

bool transport_keyboard_ready(enum kb_mode mode)
{
    switch (mode) {
    case KB_MODE_USB:
        return transport_usb_ready();
    case KB_MODE_BLE:
        return transport_ble_ready();
    case KB_MODE_24G_RESERVED:
        return transport_24g_ready();
    default:
        return false;
    }
}

bool transport_ready(enum kb_mode mode)
{
    return transport_keyboard_ready(mode);
}

bool transport_consumer_ready(enum kb_mode mode)
{
    switch (mode) {
    case KB_MODE_USB:
        return transport_usb_consumer_ready();
    case KB_MODE_BLE:
        return transport_ble_consumer_ready();
    case KB_MODE_24G_RESERVED:
        return transport_24g_consumer_ready();
    default:
        return false;
    }
}

int transport_send_keyboard_report(enum kb_mode mode, const struct hid_keyboard_report *report)
{
    switch (mode) {
    case KB_MODE_USB:
        return transport_usb_send_keyboard_report(report);
    case KB_MODE_BLE:
        return transport_ble_send_keyboard_report(report);
    case KB_MODE_24G_RESERVED:
        return transport_24g_send_keyboard_report(report);
    default:
        return -EINVAL;
    }
}

int transport_send_consumer_report(enum kb_mode mode, const struct hid_consumer_report *report)
{
    switch (mode) {
    case KB_MODE_USB:
        return transport_usb_send_consumer_report(report);
    case KB_MODE_BLE:
        return transport_ble_send_consumer_report(report);
    case KB_MODE_24G_RESERVED:
        return transport_24g_send_consumer_report(report);
    default:
        return -EINVAL;
    }
}
