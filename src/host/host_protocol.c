#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <pb_decode.h>
#include <pb_encode.h>
#include "proto/device_comm.pb.h"

#include <config/app_config.h>
#include <config/app_config_store.h>
#include <host/host_protocol.h>
#include <mode/mode_manager.h>
#include <power/power_state.h>

#define HOST_PROTOCOL_VENDOR_ID 0x1915u
#define HOST_PROTOCOL_PRODUCT_ID 0x52F0u
#define HOST_PROTOCOL_FW_MAJOR 0u
#define HOST_PROTOCOL_FW_MINOR 1u
#define HOST_PROTOCOL_FW_PATCH 0u

static bool session_ready;

static keyboard_ResponseCode errno_to_response_code(int err)
{
    switch (err) {
    case 0:
        return keyboard_ResponseCode_RESPONSE_CODE_OK;
    case -EINVAL:
        return keyboard_ResponseCode_RESPONSE_CODE_INVALID_PARAM;
    case -ENODEV:
    case -ENODATA:
        return keyboard_ResponseCode_RESPONSE_CODE_NOT_READY;
    default:
        return keyboard_ResponseCode_RESPONSE_CODE_INTERNAL_ERROR;
    }
}

static void app_config_to_proto(const struct app_config *config,
                keyboard_DeviceConfig *proto)
{
    memset(proto, 0, sizeof(*proto));
    proto->version = config->version;
    proto->rgb_enable = config->rgb_enable;
    proto->rgb_mode = config->rgb_mode;
    proto->rgb_brightness = config->rgb_brightness;
    proto->rgb_red = config->rgb_red;
    proto->rgb_green = config->rgb_green;
    proto->rgb_blue = config->rgb_blue;
    proto->numlock_policy = config->numlock_policy;
    proto->encoder_cw_action = config->encoder_cw_action;
    proto->encoder_ccw_action = config->encoder_ccw_action;
    proto->encoder_press_action = config->encoder_press_action;
    proto->mode_policy = config->mode_policy;
    proto->default_mode = config->default_mode;
}

static bool proto_uint8_fields_in_range(const keyboard_DeviceConfig *proto)
{
    return proto->rgb_mode <= UINT8_MAX &&
           proto->rgb_brightness <= UINT8_MAX &&
           proto->rgb_red <= UINT8_MAX &&
           proto->rgb_green <= UINT8_MAX &&
           proto->rgb_blue <= UINT8_MAX &&
           proto->numlock_policy <= UINT8_MAX &&
           proto->encoder_cw_action <= UINT8_MAX &&
           proto->encoder_ccw_action <= UINT8_MAX &&
           proto->encoder_press_action <= UINT8_MAX &&
           proto->mode_policy <= UINT8_MAX &&
           proto->default_mode <= UINT8_MAX;
}

static int proto_to_app_config(const keyboard_DeviceConfig *proto,
                   struct app_config *config)
{
    if (!proto_uint8_fields_in_range(proto)) {
        return -EINVAL;
    }

    memset(config, 0, sizeof(*config));
    config->version = proto->version;
    config->rgb_enable = proto->rgb_enable;
    config->rgb_mode = (uint8_t)proto->rgb_mode;
    config->rgb_brightness = (uint8_t)proto->rgb_brightness;
    config->rgb_red = (uint8_t)proto->rgb_red;
    config->rgb_green = (uint8_t)proto->rgb_green;
    config->rgb_blue = (uint8_t)proto->rgb_blue;
    config->numlock_policy = (uint8_t)proto->numlock_policy;
    config->encoder_cw_action = (uint8_t)proto->encoder_cw_action;
    config->encoder_ccw_action = (uint8_t)proto->encoder_ccw_action;
    config->encoder_press_action = (uint8_t)proto->encoder_press_action;
    config->mode_policy = (uint8_t)proto->mode_policy;
    config->default_mode = (uint8_t)proto->default_mode;
    app_config_update_crc32(config);
    return 0;
}

static void set_response(keyboard_DeviceMessage *rsp, uint32_t reply_to,
             keyboard_ResponseCode code, const char *message)
{
    rsp->reply_to = reply_to;
    rsp->which_body = keyboard_DeviceMessage_response_tag;
    rsp->body.response.code = code;
    memset(rsp->body.response.message, 0,
           sizeof(rsp->body.response.message));
    if (message != NULL) {
        (void)strncpy(rsp->body.response.message, message,
                  sizeof(rsp->body.response.message) - 1u);
    }
}

static void handle_hello(const keyboard_DeviceMessage *req,
             keyboard_DeviceMessage *rsp)
{
    session_ready = true;
    rsp->reply_to = req->msg_id;
    rsp->which_body = keyboard_DeviceMessage_hello_rsp_tag;
    rsp->body.hello_rsp.protocol_version = HOST_PROTOCOL_VERSION;
    rsp->body.hello_rsp.vendor_id = HOST_PROTOCOL_VENDOR_ID;
    rsp->body.hello_rsp.product_id = HOST_PROTOCOL_PRODUCT_ID;
    rsp->body.hello_rsp.firmware_major = HOST_PROTOCOL_FW_MAJOR;
    rsp->body.hello_rsp.firmware_minor = HOST_PROTOCOL_FW_MINOR;
    rsp->body.hello_rsp.firmware_patch = HOST_PROTOCOL_FW_PATCH;
    rsp->body.hello_rsp.capability_flags = HOST_PROTOCOL_CAP_RGB_CONFIG |
                         HOST_PROTOCOL_CAP_ENCODER_CONFIG |
                         HOST_PROTOCOL_CAP_SETTINGS_STORE;
}

static void handle_config_get(const keyboard_DeviceMessage *req,
                  keyboard_DeviceMessage *rsp)
{
    struct app_config config;
    int err;

    err = app_config_get(&config);
    if (err != 0) {
        set_response(rsp, req->msg_id, errno_to_response_code(err),
                 "config not ready");
        return;
    }

    rsp->reply_to = req->msg_id;
    rsp->which_body = keyboard_DeviceMessage_config_get_rsp_tag;
    rsp->body.config_get_rsp.has_config = true;
    app_config_to_proto(&config, &rsp->body.config_get_rsp.config);
}

static void handle_config_set(const keyboard_DeviceMessage *req,
                  keyboard_DeviceMessage *rsp)
{
    const keyboard_ConfigSetReq *set_req = &req->body.config_set_req;
    struct app_config config;
    struct app_config effective;
    int config_err = 0;
    int save_err = 0;

    if (!set_req->has_config) {
        set_response(rsp, req->msg_id,
                 keyboard_ResponseCode_RESPONSE_CODE_INVALID_PARAM,
                 "missing config");
        return;
    }

    config_err = proto_to_app_config(&set_req->config, &config);
    if (config_err == 0) {
        if (set_req->apply || set_req->save) {
            config_err = app_config_set(&config, true);
        } else {
            config_err = app_config_validate(&config);
        }
    }

    if (config_err == 0 && set_req->save) {
        save_err = app_config_store_save();
    }

    rsp->reply_to = req->msg_id;
    rsp->which_body = keyboard_DeviceMessage_config_set_rsp_tag;
    rsp->body.config_set_rsp.code = errno_to_response_code(config_err);
    if (config_err == 0 && save_err != 0) {
        rsp->body.config_set_rsp.code =
            keyboard_ResponseCode_RESPONSE_CODE_STORAGE_ERROR;
    }

    if (app_config_get(&effective) == 0) {
        rsp->body.config_set_rsp.has_effective_config = true;
        app_config_to_proto(&effective,
                    &rsp->body.config_set_rsp.effective_config);
    }
}

static void handle_config_save(const keyboard_DeviceMessage *req,
                   keyboard_DeviceMessage *rsp)
{
    int err = app_config_store_save();

    set_response(rsp, req->msg_id,
             err == 0 ? keyboard_ResponseCode_RESPONSE_CODE_OK :
             keyboard_ResponseCode_RESPONSE_CODE_STORAGE_ERROR,
             err == 0 ? "saved" : "save failed");
}

static void handle_factory_reset(const keyboard_DeviceMessage *req,
                 keyboard_DeviceMessage *rsp)
{
    int err = app_config_store_factory_reset();

    set_response(rsp, req->msg_id,
             err == 0 ? keyboard_ResponseCode_RESPONSE_CODE_OK :
             keyboard_ResponseCode_RESPONSE_CODE_STORAGE_ERROR,
             err == 0 ? "factory reset" : "reset failed");
}

static void handle_status(const keyboard_DeviceMessage *req,
              keyboard_DeviceMessage *rsp)
{
    struct power_state_snapshot snapshot;

    rsp->reply_to = req->msg_id;
    rsp->which_body = keyboard_DeviceMessage_device_status_rsp_tag;
    rsp->body.device_status_rsp.current_mode = (uint32_t)mode_manager_get_mode();

    if (power_state_get_latest(&snapshot) == 0) {
        if ((snapshot.valid_flags & POWER_STATE_VALID_PERCENT) != 0u) {
            rsp->body.device_status_rsp.battery_percent =
                snapshot.estimated_percent;
        }
        if ((snapshot.valid_flags & POWER_STATE_VALID_LEVEL) != 0u) {
            rsp->body.device_status_rsp.power_level =
                (uint32_t)snapshot.level_state;
        }
        if ((snapshot.valid_flags & POWER_STATE_VALID_USB_PRESENT) != 0u) {
            rsp->body.device_status_rsp.usb_present = snapshot.usb_present;
        }
        if ((snapshot.valid_flags & POWER_STATE_VALID_CHARGING) != 0u) {
            rsp->body.device_status_rsp.charging =
                snapshot.charging_state == POWER_CHARGING_STATE_CHARGING;
        }
    }
}

static void dispatch_message(const keyboard_DeviceMessage *req,
                 keyboard_DeviceMessage *rsp)
{
    switch (req->which_body) {
    case keyboard_DeviceMessage_hello_req_tag:
        handle_hello(req, rsp);
        return;
    case keyboard_DeviceMessage_config_get_req_tag:
    case keyboard_DeviceMessage_config_set_req_tag:
    case keyboard_DeviceMessage_config_save_req_tag:
    case keyboard_DeviceMessage_factory_reset_req_tag:
    case keyboard_DeviceMessage_device_status_req_tag:
        if (!session_ready) {
            set_response(rsp, req->msg_id,
                     keyboard_ResponseCode_RESPONSE_CODE_NOT_READY,
                     "hello required");
            return;
        }
        break;
    default:
        set_response(rsp, req->msg_id,
                 keyboard_ResponseCode_RESPONSE_CODE_UNKNOWN_TYPE,
                 "unknown request");
        return;
    }

    switch (req->which_body) {
    case keyboard_DeviceMessage_config_get_req_tag:
        handle_config_get(req, rsp);
        break;
    case keyboard_DeviceMessage_config_set_req_tag:
        handle_config_set(req, rsp);
        break;
    case keyboard_DeviceMessage_config_save_req_tag:
        handle_config_save(req, rsp);
        break;
    case keyboard_DeviceMessage_factory_reset_req_tag:
        handle_factory_reset(req, rsp);
        break;
    case keyboard_DeviceMessage_device_status_req_tag:
        handle_status(req, rsp);
        break;
    default:
        set_response(rsp, req->msg_id,
                 keyboard_ResponseCode_RESPONSE_CODE_UNKNOWN_TYPE,
                 "unknown request");
        break;
    }
}

int host_protocol_handle_payload(const uint8_t *request, size_t request_len,
                 uint8_t *response, size_t response_size,
                 size_t *response_len)
{
    keyboard_DeviceMessage req = keyboard_DeviceMessage_init_zero;
    keyboard_DeviceMessage rsp = keyboard_DeviceMessage_init_zero;
    pb_istream_t input;
    pb_ostream_t output;

    if (request == NULL || response == NULL || response_len == NULL) {
        return -EINVAL;
    }

    input = pb_istream_from_buffer(request, request_len);
    if (!pb_decode(&input, keyboard_DeviceMessage_fields, &req)) {
        set_response(&rsp, 0,
                 keyboard_ResponseCode_RESPONSE_CODE_INVALID_LENGTH,
                 "decode failed");
    } else {
        dispatch_message(&req, &rsp);
    }

    output = pb_ostream_from_buffer(response, response_size);
    if (!pb_encode(&output, keyboard_DeviceMessage_fields, &rsp)) {
        return -EMSGSIZE;
    }

    *response_len = output.bytes_written;
    return 0;
}

void host_protocol_reset_session(void)
{
    session_ready = false;
}
