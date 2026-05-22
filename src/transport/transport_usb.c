#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <zephyr/usb/class/usbd_hid.h>
#include <zephyr/usb/usbd.h>

#include <hid/hid_report.h>

LOG_MODULE_REGISTER(transport_usb, LOG_LEVEL_INF);

#define PRO_USB_VID 0x1915
#define PRO_USB_PID 0x52F0
#define PRO_USB_MAX_POWER 50

static const uint8_t hid_report_desc[] = HID_KEYBOARD_REPORT_DESC();

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
static struct usbd_context *usb_context;
static const struct device *hid_device;

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
            (void)usbd_enable(usbd_ctx);
        } else if (msg->type == USBD_MSG_VBUS_REMOVED) {
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

    err = usbd_enable(usb_context);
    if (err != 0 && err != -EALREADY) {
        usb_transport_enabled = false;
        LOG_WRN("USB enable failed: %d", err);
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
    if (report == NULL) {
        return -EINVAL;
    }

    if (!transport_usb_ready()) {
        return -ENOTCONN;
    }

    return hid_device_submit_report(hid_device, sizeof(*report), (const uint8_t *)report);
}
