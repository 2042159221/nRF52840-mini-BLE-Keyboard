#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>
#include <zephyr/usb/class/usbd_hid.h>
#include <zephyr/usb/usbd.h>

#include <hid/hid_consumer.h>
#include <hid/hid_report.h>
#include <power/power_manager.h>

LOG_MODULE_REGISTER(transport_usb, LOG_LEVEL_INF);

#define PRO_USB_VID 0x1915
#define PRO_USB_PID 0x52F0
#define PRO_USB_MAX_POWER 50

#define USB_REPORT_ID_KEYBOARD 1
#define USB_REPORT_ID_CONSUMER 2
#define USB_KEYBOARD_REPORT_LEN (1 + sizeof(struct hid_keyboard_report))
#define USB_CONSUMER_REPORT_LEN (1 + sizeof(uint16_t))

static const uint8_t hid_report_desc[] = {
    0x05, 0x01,        /* Usage Page (Generic Desktop) */
    0x09, 0x06,        /* Usage (Keyboard) */
    0xA1, 0x01,        /* Collection (Application) */
    0x85, USB_REPORT_ID_KEYBOARD,
    0x05, 0x07,        /* Usage Page (Keyboard/Keypad) */
    0x19, 0xE0,        /* Usage Minimum (Keyboard LeftControl) */
    0x29, 0xE7,        /* Usage Maximum (Keyboard Right GUI) */
    0x15, 0x00,        /* Logical Minimum (0) */
    0x25, 0x01,        /* Logical Maximum (1) */
    0x75, 0x01,        /* Report Size (1) */
    0x95, 0x08,        /* Report Count (8) */
    0x81, 0x02,        /* Input (Data, Variable, Absolute) */
    0x95, 0x01,        /* Report Count (1) */
    0x75, 0x08,        /* Report Size (8) */
    0x81, 0x03,        /* Input (Constant, Variable, Absolute) */
    0x95, 0x05,        /* Report Count (5) */
    0x75, 0x01,        /* Report Size (1) */
    0x05, 0x08,        /* Usage Page (LEDs) */
    0x19, 0x01,        /* Usage Minimum (Num Lock) */
    0x29, 0x05,        /* Usage Maximum (Kana) */
    0x91, 0x02,        /* Output (Data, Variable, Absolute) */
    0x95, 0x01,        /* Report Count (1) */
    0x75, 0x03,        /* Report Size (3) */
    0x91, 0x03,        /* Output (Constant, Variable, Absolute) */
    0x95, 0x06,        /* Report Count (6) */
    0x75, 0x08,        /* Report Size (8) */
    0x15, 0x00,        /* Logical Minimum (0) */
    0x25, 0x65,        /* Logical Maximum (101) */
    0x05, 0x07,        /* Usage Page (Keyboard/Keypad) */
    0x19, 0x00,        /* Usage Minimum (Reserved) */
    0x29, 0x65,        /* Usage Maximum (Keyboard Application) */
    0x81, 0x00,        /* Input (Data, Array, Absolute) */
    0xC0,              /* End Collection */

    0x05, 0x0C,        /* Usage Page (Consumer) */
    0x09, 0x01,        /* Usage (Consumer Control) */
    0xA1, 0x01,        /* Collection (Application) */
    0x85, USB_REPORT_ID_CONSUMER,
    0x15, 0x00,        /* Logical Minimum (0) */
    0x26, 0xFF, 0x03,  /* Logical Maximum (0x03FF) */
    0x19, 0x00,        /* Usage Minimum (Unassigned) */
    0x2A, 0xFF, 0x03,  /* Usage Maximum (0x03FF) */
    0x75, 0x10,        /* Report Size (16) */
    0x95, 0x01,        /* Report Count (1) */
    0x81, 0x00,        /* Input (Data, Array, Absolute) */
    0xC0,              /* End Collection */
};

USBD_DEVICE_DEFINE(pro_keyboard_usbd,
           DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
           PRO_USB_VID, PRO_USB_PID);

USBD_DESC_LANG_DEFINE(pro_keyboard_lang);
USBD_DESC_MANUFACTURER_DEFINE(pro_keyboard_mfr, "mingDev");
USBD_DESC_PRODUCT_DEFINE(pro_keyboard_product, "PRO BLE Mini Keyboard");
USBD_DESC_CONFIG_DEFINE(pro_keyboard_cfg_desc, "Keyboard Configuration");
USBD_CONFIGURATION_DEFINE(pro_keyboard_fs_config, 0, PRO_USB_MAX_POWER,
              &pro_keyboard_cfg_desc);

static bool usb_transport_enabled;
static bool usb_transport_ready;
static bool usb_transport_initialized;
static bool usb_vbus_present;
static struct usbd_context *usb_context;
static const struct device *hid_device;

static int usb_enable_device_if_allowed(void)
{
    int err;

    if (usb_context == NULL || !usb_transport_enabled) {
        return 0;
    }

    if (usbd_can_detect_vbus(usb_context) && !usb_vbus_present) {
        return 0;
    }

    err = usbd_enable(usb_context);
    if (err != 0 && err != -EALREADY) {
        usb_transport_enabled = false;
        LOG_WRN("USB enable failed: %d", err);
        return err;
    }

    return 0;
}

static void usb_iface_ready(const struct device *dev, const bool ready)
{
    LOG_INF("USB HID interface %s is %s", dev->name, ready ? "ready" : "not ready");
    usb_transport_ready = ready;
}

static int usb_get_report(const struct device *dev,
              const uint8_t type, const uint8_t id, const uint16_t len,
              uint8_t *const buf)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(type);
    ARG_UNUSED(id);
    ARG_UNUSED(len);
    ARG_UNUSED(buf);

    return -ENOTSUP;
}

static int usb_set_report(const struct device *dev,
              const uint8_t type, const uint8_t id, const uint16_t len,
              const uint8_t *const buf)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(type);
    ARG_UNUSED(id);
    ARG_UNUSED(len);
    ARG_UNUSED(buf);

    return 0;
}

static void usb_set_idle(const struct device *dev, const uint8_t id, const uint32_t duration)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(id);
    ARG_UNUSED(duration);
}

static uint32_t usb_get_idle(const struct device *dev, const uint8_t id)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(id);

    return 0;
}

static void usb_set_protocol(const struct device *dev, const uint8_t proto)
{
    ARG_UNUSED(dev);
    LOG_INF("USB HID protocol changed to %s", proto == 0U ? "boot" : "report");
}

static const struct hid_device_ops usb_hid_ops = {
    .iface_ready = usb_iface_ready,
    .get_report = usb_get_report,
    .set_report = usb_set_report,
    .set_idle = usb_set_idle,
    .get_idle = usb_get_idle,
    .set_protocol = usb_set_protocol,
};

static void usb_msg_cb(struct usbd_context *const usbd_ctx, const struct usbd_msg *const msg)
{
    LOG_DBG("USBD message: %s", usbd_msg_type_string(msg->type));

    if (usbd_can_detect_vbus(usbd_ctx)) {
        if (msg->type == USBD_MSG_VBUS_READY) {
            usb_vbus_present = true;
            power_manager_usb_power_present(true);
            (void)usb_enable_device_if_allowed();
        } else if (msg->type == USBD_MSG_VBUS_REMOVED) {
            usb_vbus_present = false;
            power_manager_usb_power_present(false);
            usb_transport_ready = false;
            (void)usbd_disable(usbd_ctx);
        }
    }
}

static int usb_setup_device(void)
{
    int err;

    err = usbd_add_descriptor(&pro_keyboard_usbd, &pro_keyboard_lang);
    if (err != 0) {
        return err;
    }

    err = usbd_add_descriptor(&pro_keyboard_usbd, &pro_keyboard_mfr);
    if (err != 0) {
        return err;
    }

    err = usbd_add_descriptor(&pro_keyboard_usbd, &pro_keyboard_product);
    if (err != 0) {
        return err;
    }

    err = usbd_add_configuration(&pro_keyboard_usbd, USBD_SPEED_FS, &pro_keyboard_fs_config);
    if (err != 0) {
        return err;
    }

    err = usbd_register_all_classes(&pro_keyboard_usbd, USBD_SPEED_FS, 1, NULL);
    if (err != 0) {
        return err;
    }

    err = usbd_msg_register_cb(&pro_keyboard_usbd, usb_msg_cb);
    if (err != 0) {
        return err;
    }

    err = usbd_init(&pro_keyboard_usbd);
    if (err != 0) {
        return err;
    }

    usb_context = &pro_keyboard_usbd;
    return 0;
}

int transport_usb_init(void)
{
    int err;

    hid_device = DEVICE_DT_GET_ONE(zephyr_hid_device);
    if (!device_is_ready(hid_device)) {
        LOG_ERR("USB HID device is not ready");
        return -ENODEV;
    }

    err = hid_device_register(hid_device, hid_report_desc, sizeof(hid_report_desc),
                  &usb_hid_ops);
    if (err != 0) {
        LOG_ERR("USB HID registration failed: %d", err);
        return err;
    }

    err = usb_setup_device();
    if (err != 0) {
        LOG_ERR("USB device setup failed: %d", err);
        return err;
    }

    usb_transport_initialized = true;
    LOG_INF("USB HID transport initialized");
    return 0;
}

int transport_usb_enable(void)
{
    int err;

    if (usb_context == NULL) {
        return -ENODEV;
    }

    if (!usb_transport_initialized) {
        err = usb_setup_device();
        if (err != 0) {
            return err;
        }

        usb_transport_initialized = true;
    }

    usb_transport_enabled = true;

    err = usb_enable_device_if_allowed();
    if (err != 0) {
        return err;
    }

    LOG_INF("USB HID transport enabled");
    return 0;
}

int transport_usb_disable(void)
{
    int err;

    usb_transport_enabled = false;
    usb_transport_ready = false;

    if (usb_context == NULL || !usb_transport_initialized) {
        return 0;
    }

    err = usbd_disable(usb_context);
    if (err != 0 && err != -EALREADY) {
        LOG_WRN("USB disable failed: %d", err);
    }

    return 0;
}

bool transport_usb_ready(void)
{
    const bool ready = usb_transport_enabled && usb_transport_ready;

    if (usb_transport_enabled && !usb_transport_ready) {
        LOG_DBG("USB HID transport is enabled but the host interface is not ready");
    }

    return ready;
}

int transport_usb_send_keyboard_report(const struct hid_keyboard_report *report)
{
    uint8_t usb_report[USB_KEYBOARD_REPORT_LEN];

    if (report == NULL) {
        return -EINVAL;
    }

    if (!transport_usb_ready()) {
        return -ENOTCONN;
    }

    usb_report[0] = USB_REPORT_ID_KEYBOARD;
    memcpy(&usb_report[1], report, sizeof(*report));

    return hid_device_submit_report(hid_device, sizeof(usb_report), usb_report);
}

int transport_usb_send_consumer_report(const struct hid_consumer_report *report)
{
    uint8_t usb_report[USB_CONSUMER_REPORT_LEN];

    if (report == NULL) {
        return -EINVAL;
    }

    if (!transport_usb_ready()) {
        return -ENOTCONN;
    }

    usb_report[0] = USB_REPORT_ID_CONSUMER;
    sys_put_le16(report->usage, &usb_report[1]);

    return hid_device_submit_report(hid_device, sizeof(usb_report), usb_report);
}
