#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include <bluetooth/services/hids.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/hci_types.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

#include <hid/hid_consumer.h>
#include <hid/hid_report.h>
#include <keyboard/keyboard_led_state.h>
#include <transport/ble_bas_adapter.h>
#include <transport/transport_ble_security.h>

LOG_MODULE_REGISTER(transport_ble, LOG_LEVEL_INF);

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

#define BASE_USB_HID_SPEC_VERSION 0x0101
#define INPUT_REP_KEYS_IDX 0
#define INPUT_REP_CONSUMER_IDX 1
#define OUTPUT_REP_KEYS_IDX 0
#define INPUT_REP_KEYS_REF_ID 1
#define INPUT_REP_CONSUMER_REF_ID 2
#define OUTPUT_REP_KEYS_REF_ID INPUT_REP_KEYS_REF_ID
#define INPUT_REPORT_KEYS_MAX_LEN sizeof(struct hid_keyboard_report)
#define INPUT_REPORT_CONSUMER_MAX_LEN sizeof(struct hid_consumer_report)
#define OUTPUT_REPORT_MAX_LEN 1

BT_HIDS_DEF(hids_obj, OUTPUT_REPORT_MAX_LEN, INPUT_REPORT_KEYS_MAX_LEN,
        INPUT_REPORT_CONSUMER_MAX_LEN);

static bool ble_transport_enabled;
static bool ble_transport_initialized;
static bool ble_transport_advertising;
static struct bt_conn *ble_conn;
static bool ble_in_boot_mode;
static bool ble_keyboard_report_notify_enabled;
static bool ble_consumer_report_notify_enabled;
static bool ble_boot_report_notify_enabled;
static int ble_advertising_restart_delay_ms =
    TRANSPORT_BLE_SECURITY_RECONNECT_DELAY_MS;
static struct k_work_delayable ble_advertising_restart_work;

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

static void ble_advertising_restart_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    if (ble_transport_enabled && ble_conn == NULL && !ble_transport_advertising) {
        advertising_start();
    }
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
    ble_keyboard_report_notify_enabled = false;
    ble_consumer_report_notify_enabled = false;
    ble_boot_report_notify_enabled = false;
    (void)bt_hids_connected(&hids_obj, conn);

    if (bt_conn_set_security(ble_conn, BT_SECURITY_L2) != 0) {
        LOG_WRN("BLE security request failed");
    }

    LOG_INF("BLE connected");
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    (void)bt_hids_disconnected(&hids_obj, conn);

    if (ble_conn != NULL) {
        bt_conn_unref(ble_conn);
        ble_conn = NULL;
    }

    ble_keyboard_report_notify_enabled = false;
    ble_consumer_report_notify_enabled = false;
    ble_boot_report_notify_enabled = false;

    LOG_INF("BLE disconnected: 0x%02x", reason);

    if (ble_transport_enabled) {
        (void)k_work_reschedule(&ble_advertising_restart_work,
                    K_MSEC(ble_advertising_restart_delay_ms));
        ble_advertising_restart_delay_ms =
            transport_ble_security_reconnect_delay_ms(BT_SECURITY_ERR_SUCCESS);
    }
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
                 enum bt_security_err err)
{
    int unpair_err;
    int disconnect_err;

    if (conn != ble_conn) {
        return;
    }

    if (err != BT_SECURITY_ERR_SUCCESS) {
        LOG_WRN("BLE security failed: %s (%d)",
            bt_security_err_to_str(err), err);

        if (transport_ble_security_error_requires_bond_reset(err)) {
            ble_advertising_restart_delay_ms =
                transport_ble_security_reconnect_delay_ms(err);
            unpair_err = bt_unpair(BT_ID_DEFAULT, bt_conn_get_dst(conn));
            if (unpair_err != 0) {
                LOG_WRN("BLE stale bond cleanup failed: %d", unpair_err);
            } else {
                LOG_WRN("BLE stale bond removed; forget this keyboard on the host");
                LOG_WRN("BLE advertising retry delayed %d ms after stale bond",
                    ble_advertising_restart_delay_ms);
            }
        }

        if (transport_ble_security_error_requires_disconnect(err)) {
            disconnect_err = bt_conn_disconnect(conn, BT_HCI_ERR_AUTH_FAIL);
            if (disconnect_err != 0) {
                LOG_WRN("BLE disconnect after security failure failed: %d",
                    disconnect_err);
            } else {
                LOG_WRN("BLE disconnect requested after security failure");
            }
        }

        return;
    }

    LOG_INF("BLE security level: %u", level);
}

static void pairing_complete(struct bt_conn *conn, bool bonded)
{
    ARG_UNUSED(conn);

    LOG_INF("BLE pairing completed, bonded: %d", bonded);
}

static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
    int unpair_err;
    int disconnect_err;

    LOG_WRN("BLE pairing failed: %s (%d)",
        bt_security_err_to_str(reason), reason);

    if (!transport_ble_security_error_requires_bond_reset(reason)) {
        goto disconnect;
    }

    unpair_err = bt_unpair(BT_ID_DEFAULT, bt_conn_get_dst(conn));
    if (unpair_err != 0) {
        LOG_WRN("BLE bond cleanup after pairing failure failed: %d", unpair_err);
    }

disconnect:
    if (transport_ble_security_error_requires_disconnect(reason)) {
        disconnect_err = bt_conn_disconnect(conn, BT_HCI_ERR_AUTH_FAIL);
        if (disconnect_err != 0) {
            LOG_WRN("BLE disconnect after pairing failure failed: %d",
                disconnect_err);
        } else {
            LOG_WRN("BLE disconnect requested after pairing failure");
        }
    }
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
    .security_changed = security_changed,
};

static struct bt_conn_auth_info_cb conn_auth_info_callbacks = {
    .pairing_complete = pairing_complete,
    .pairing_failed = pairing_failed,
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
    ARG_UNUSED(conn);
    ARG_UNUSED(write);

    if (rep == NULL || rep->data == NULL || rep->size == 0U) {
        return;
    }

    if (rep->id == OUTPUT_REP_KEYS_REF_ID || rep->id == 0U) {
        keyboard_led_state_update(rep->data[0]);
    }
}

static void hids_input_notif_handler(uint8_t report_id, enum bt_hids_notify_evt evt)
{
    const bool enabled = (evt == BT_HIDS_CCCD_EVT_NOTIFY_ENABLED);

    switch (report_id) {
    case INPUT_REP_KEYS_REF_ID:
        ble_keyboard_report_notify_enabled = enabled;
        LOG_INF("BLE HID keyboard notifications %s", enabled ? "enabled" : "disabled");
        break;
    case INPUT_REP_CONSUMER_REF_ID:
        ble_consumer_report_notify_enabled = enabled;
        LOG_INF("BLE HID consumer notifications %s", enabled ? "enabled" : "disabled");
        break;
    default:
        LOG_WRN("BLE HID notification event for unknown report id %u", report_id);
        break;
    }
}

static void hids_boot_notif_handler(enum bt_hids_notify_evt evt)
{
    ble_boot_report_notify_enabled = (evt == BT_HIDS_CCCD_EVT_NOTIFY_ENABLED);
    LOG_INF("BLE HID boot notifications %s",
        ble_boot_report_notify_enabled ? "enabled" : "disabled");
}

static bool ble_conn_ready_for_report(void)
{
    if (!ble_transport_enabled || ble_conn == NULL) {
        return false;
    }

    if (bt_conn_get_security(ble_conn) < BT_SECURITY_L2) {
        return false;
    }

    return true;
}

static bool ble_conn_ready_for_keyboard_report(void)
{
    if (!ble_conn_ready_for_report()) {
        return false;
    }

    return ble_in_boot_mode ? ble_boot_report_notify_enabled :
                  ble_keyboard_report_notify_enabled;
}

static bool ble_conn_ready_for_consumer_report(void)
{
    if (!ble_conn_ready_for_report() || ble_in_boot_mode) {
        return false;
    }

    return ble_consumer_report_notify_enabled;
}

static struct bt_hids_init_param hids_init_obj;

static void hids_init(void)
{
    struct bt_hids_inp_rep *hids_inp_rep;
    struct bt_hids_outp_feat_rep *hids_outp_rep;

    memset(&hids_init_obj, 0, sizeof(hids_init_obj));

    static const uint8_t report_map[] = {
        0x05, 0x01, 0x09, 0x06, 0xA1, 0x01,
        0x85, INPUT_REP_KEYS_REF_ID,
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
        0xC0,
        0x05, 0x0c, 0x09, 0x01, 0xa1, 0x01,
        0x85, INPUT_REP_CONSUMER_REF_ID,
        0x15, 0x00, 0x26, 0xff, 0x03,
        0x19, 0x00, 0x2a, 0xff, 0x03,
        0x75, 0x10, 0x95, 0x01, 0x81, 0x00,
        0xc0
    };

    hids_init_obj.rep_map.data = report_map;
    hids_init_obj.rep_map.size = sizeof(report_map);
    hids_init_obj.info.bcd_hid = BASE_USB_HID_SPEC_VERSION;
    hids_init_obj.info.b_country_code = 0x00;
    hids_init_obj.info.flags = BT_HIDS_REMOTE_WAKE | BT_HIDS_NORMALLY_CONNECTABLE;

    hids_inp_rep = &hids_init_obj.inp_rep_group_init.reports[INPUT_REP_KEYS_IDX];
    hids_inp_rep->size = INPUT_REPORT_KEYS_MAX_LEN;
    hids_inp_rep->id = INPUT_REP_KEYS_REF_ID;
    hids_inp_rep->handler_ext = hids_input_notif_handler;
    hids_init_obj.inp_rep_group_init.cnt++;

    hids_inp_rep = &hids_init_obj.inp_rep_group_init.reports[INPUT_REP_CONSUMER_IDX];
    hids_inp_rep->size = INPUT_REPORT_CONSUMER_MAX_LEN;
    hids_inp_rep->id = INPUT_REP_CONSUMER_REF_ID;
    hids_inp_rep->handler_ext = hids_input_notif_handler;
    hids_init_obj.inp_rep_group_init.cnt++;

    hids_outp_rep = &hids_init_obj.outp_rep_group_init.reports[OUTPUT_REP_KEYS_IDX];
    hids_outp_rep->size = OUTPUT_REPORT_MAX_LEN;
    hids_outp_rep->id = OUTPUT_REP_KEYS_REF_ID;
    hids_outp_rep->handler = hids_outp_rep_handler;
    hids_init_obj.outp_rep_group_init.cnt++;

    hids_init_obj.is_kb = true;
    hids_init_obj.pm_evt_handler = hids_pm_evt_handler;
    hids_init_obj.boot_kb_notif_handler = hids_boot_notif_handler;
    hids_init_obj.boot_kb_outp_rep_handler = hids_outp_rep_handler;

    (void)bt_hids_init(&hids_obj, &hids_init_obj);
}

int transport_ble_init(void)
{
    int err;

    hids_init();
    k_work_init_delayable(&ble_advertising_restart_work, ble_advertising_restart_work_handler);

    err = bt_enable(NULL);
    if (err != 0) {
        LOG_ERR("Bluetooth init failed: %d", err);
        return err;
    }

    err = bt_conn_auth_info_cb_register(&conn_auth_info_callbacks);
    if (err != 0) {
        LOG_ERR("Bluetooth auth info callback registration failed: %d", err);
        return err;
    }

    if (IS_ENABLED(CONFIG_SETTINGS)) {
        err = settings_load();
        if (err != 0) {
            LOG_WRN("Bluetooth settings load failed: %d", err);
        } else {
            LOG_INF("Bluetooth settings loaded");
        }
    }

    err = ble_bas_adapter_init();
    if (err != 0) {
        LOG_ERR("BLE BAS adapter init failed: %d", err);
        return err;
    }

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
    int err;

    ble_transport_enabled = false;
    (void)k_work_cancel_delayable(&ble_advertising_restart_work);

    if (ble_transport_advertising) {
        err = bt_le_adv_stop();

        if (err != 0 && err != -EALREADY) {
            LOG_WRN("BLE advertising stop failed: %d", err);
        }
        ble_transport_advertising = false;
    }

    if (ble_conn != NULL) {
        err = bt_conn_disconnect(ble_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        if (err != 0) {
            LOG_WRN("BLE disconnect on transport disable failed: %d", err);
        } else {
            LOG_INF("BLE disconnect requested for transport disable");
        }
    }

    ble_keyboard_report_notify_enabled = false;
    ble_consumer_report_notify_enabled = false;
    ble_boot_report_notify_enabled = false;

    return 0;
}

bool transport_ble_ready(void)
{
    return ble_conn_ready_for_keyboard_report();
}

bool transport_ble_consumer_ready(void)
{
    return ble_conn_ready_for_consumer_report();
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

int transport_ble_send_consumer_report(const struct hid_consumer_report *report)
{
    uint8_t consumer_report[INPUT_REPORT_CONSUMER_MAX_LEN];

    if (report == NULL) {
        return -EINVAL;
    }

    if (ble_in_boot_mode) {
        return -ENOTSUP;
    }

    if (!ble_conn_ready_for_consumer_report()) {
        return -ENOTCONN;
    }

    sys_put_le16(report->usage, consumer_report);

    return bt_hids_inp_rep_send(&hids_obj, ble_conn, INPUT_REP_CONSUMER_IDX,
                    consumer_report, sizeof(consumer_report), NULL);
}
