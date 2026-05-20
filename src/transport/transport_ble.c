#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include <bluetooth/services/hids.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/util.h>

#include <hid/hid_report.h>

LOG_MODULE_REGISTER(transport_ble, LOG_LEVEL_INF);

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

#define BASE_USB_HID_SPEC_VERSION 0x0101
#define INPUT_REP_KEYS_IDX 0
#define OUTPUT_REP_KEYS_IDX 0
#define INPUT_REP_KEYS_REF_ID 0
#define OUTPUT_REP_KEYS_REF_ID 0
#define INPUT_REPORT_KEYS_MAX_LEN sizeof(struct hid_keyboard_report)
#define OUTPUT_REPORT_MAX_LEN 1

BT_HIDS_DEF(hids_obj, OUTPUT_REPORT_MAX_LEN, INPUT_REPORT_KEYS_MAX_LEN);

static bool ble_transport_enabled;
static bool ble_transport_initialized;
static bool ble_transport_advertising;
static struct bt_conn *ble_conn;
static bool ble_in_boot_mode;

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_GAP_APPEARANCE,
              (CONFIG_BT_DEVICE_APPEARANCE >> 0) & 0xff,
              (CONFIG_BT_DEVICE_APPEARANCE >> 8) & 0xff),
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL),
              BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)),
};

static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static void advertising_start(void)
{
    const struct bt_le_adv_param *adv_param = BT_LE_ADV_PARAM(
        BT_LE_ADV_OPT_CONN, BT_GAP_ADV_FAST_INT_MIN_2, BT_GAP_ADV_FAST_INT_MAX_2, NULL);
    int err;

    err = bt_le_adv_start(adv_param, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (err == -EALREADY) {
        ble_transport_advertising = true;
        return;
    }

    if (err != 0) {
        LOG_ERR("BLE advertising start failed: %d", err);
        return;
    }

    ble_transport_advertising = true;
    LOG_INF("BLE advertising started");
}

static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err != 0) {
        LOG_WRN("BLE connection failed: 0x%02x", err);
        return;
    }

    ble_transport_advertising = false;
    ble_conn = bt_conn_ref(conn);
    ble_in_boot_mode = false;
    (void)bt_hids_connected(&hids_obj, conn);
    LOG_INF("BLE connected");
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    (void)bt_hids_disconnected(&hids_obj, conn);

    if (ble_conn != NULL) {
        bt_conn_unref(ble_conn);
        ble_conn = NULL;
    }

    LOG_INF("BLE disconnected: 0x%02x", reason);

    if (ble_transport_enabled) {
        advertising_start();
    }
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

static void hids_pm_evt_handler(enum bt_hids_pm_evt evt, struct bt_conn *conn)
{
    ARG_UNUSED(conn);

    switch (evt) {
    case BT_HIDS_PM_EVT_BOOT_MODE_ENTERED:
        ble_in_boot_mode = true;
        break;
    case BT_HIDS_PM_EVT_REPORT_MODE_ENTERED:
        ble_in_boot_mode = false;
        break;
    default:
        break;
    }
}

static void hids_outp_rep_handler(struct bt_hids_rep *rep,
                  struct bt_conn *conn, bool write)
{
    ARG_UNUSED(rep);
    ARG_UNUSED(conn);
    ARG_UNUSED(write);
}

static void hids_init(void)
{
    struct bt_hids_init_param hids_init_obj = { 0 };
    struct bt_hids_inp_rep *hids_inp_rep;
    struct bt_hids_outp_feat_rep *hids_outp_rep;

    static const uint8_t report_map[] = {
        0x05, 0x01, 0x09, 0x06, 0xA1, 0x01,
        0x05, 0x07, 0x19, 0xe0, 0x29, 0xe7,
        0x15, 0x00, 0x25, 0x01, 0x75, 0x01,
        0x95, 0x08, 0x81, 0x02,
        0x95, 0x01, 0x75, 0x08, 0x81, 0x01,
        0x95, 0x06, 0x75, 0x08, 0x15, 0x00,
        0x25, 0x65, 0x05, 0x07, 0x19, 0x00,
        0x29, 0x65, 0x81, 0x00,
        0x95, 0x05, 0x75, 0x01, 0x05, 0x08,
        0x19, 0x01, 0x29, 0x05, 0x91, 0x02,
        0x95, 0x01, 0x75, 0x03, 0x91, 0x01,
        0xC0
    };

    hids_init_obj.rep_map.data = report_map;
    hids_init_obj.rep_map.size = sizeof(report_map);
    hids_init_obj.info.bcd_hid = BASE_USB_HID_SPEC_VERSION;
    hids_init_obj.info.b_country_code = 0x00;
    hids_init_obj.info.flags = BT_HIDS_REMOTE_WAKE | BT_HIDS_NORMALLY_CONNECTABLE;

    hids_inp_rep = &hids_init_obj.inp_rep_group_init.reports[INPUT_REP_KEYS_IDX];
    hids_inp_rep->size = INPUT_REPORT_KEYS_MAX_LEN;
    hids_inp_rep->id = INPUT_REP_KEYS_REF_ID;
    hids_init_obj.inp_rep_group_init.cnt++;

    hids_outp_rep = &hids_init_obj.outp_rep_group_init.reports[OUTPUT_REP_KEYS_IDX];
    hids_outp_rep->size = OUTPUT_REPORT_MAX_LEN;
    hids_outp_rep->id = OUTPUT_REP_KEYS_REF_ID;
    hids_outp_rep->handler = hids_outp_rep_handler;
    hids_init_obj.outp_rep_group_init.cnt++;

    hids_init_obj.is_kb = true;
    hids_init_obj.pm_evt_handler = hids_pm_evt_handler;

    (void)bt_hids_init(&hids_obj, &hids_init_obj);
}

int transport_ble_init(void)
{
    int err;

    hids_init();

    err = bt_enable(NULL);
    if (err != 0) {
        LOG_ERR("Bluetooth init failed: %d", err);
        return err;
    }

    if (IS_ENABLED(CONFIG_SETTINGS)) {
        settings_load();
    }

    bt_bas_set_battery_level(100);
    ble_transport_initialized = true;
    LOG_INF("BLE HID transport initialized");
    return 0;
}

int transport_ble_enable(void)
{
    if (!ble_transport_initialized) {
        return -ENODEV;
    }

    ble_transport_enabled = true;

    if (ble_conn == NULL && !ble_transport_advertising) {
        advertising_start();
    }

    LOG_INF("BLE HID transport enabled");
    return 0;
}

int transport_ble_disable(void)
{
    ble_transport_enabled = false;

    if (ble_transport_advertising) {
        int err = bt_le_adv_stop();

        if (err != 0 && err != -EALREADY) {
            LOG_WRN("BLE advertising stop failed: %d", err);
        }
        ble_transport_advertising = false;
    }

    return 0;
}

bool transport_ble_ready(void)
{
    return ble_transport_enabled && ble_conn != NULL;
}

int transport_ble_send_keyboard_report(const struct hid_keyboard_report *report)
{
    if (report == NULL) {
        return -EINVAL;
    }

    if (!transport_ble_ready()) {
        return -ENOTCONN;
    }

    if (ble_in_boot_mode) {
        return bt_hids_boot_kb_inp_rep_send(&hids_obj, ble_conn,
                            (const uint8_t *)report, sizeof(*report), NULL);
    }

    return bt_hids_inp_rep_send(&hids_obj, ble_conn, INPUT_REP_KEYS_IDX,
                    (const uint8_t *)report, sizeof(*report), NULL);
}