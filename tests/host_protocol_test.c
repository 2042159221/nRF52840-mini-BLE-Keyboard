#include <assert.h>
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

enum kb_mode mode_manager_get_mode(void)
{
    return KB_MODE_USB;
}

int power_state_get_latest(struct power_state_snapshot *snapshot)
{
    if (snapshot == NULL) {
        return -22;
    }

    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->estimated_percent = 95;
    snapshot->level_state = POWER_LEVEL_STATE_NORMAL;
    snapshot->usb_present = true;
    snapshot->valid_flags = POWER_STATE_VALID_PERCENT |
                POWER_STATE_VALID_LEVEL |
                POWER_STATE_VALID_USB_PRESENT;
    return 0;
}

static void encode_message(const keyboard_DeviceMessage *message,
               uint8_t *buffer, size_t buffer_size, size_t *len)
{
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, buffer_size);

    assert(pb_encode(&stream, keyboard_DeviceMessage_fields, message));
    *len = stream.bytes_written;
}

static keyboard_DeviceMessage decode_response(const uint8_t *buffer, size_t len)
{
    keyboard_DeviceMessage response = keyboard_DeviceMessage_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(buffer, len);

    assert(pb_decode(&stream, keyboard_DeviceMessage_fields, &response));
    return response;
}

static keyboard_DeviceMessage request(const keyboard_DeviceMessage *message)
{
    uint8_t request_payload[128];
    uint8_t response_payload[128];
    size_t request_len;
    size_t response_len;

    encode_message(message, request_payload, sizeof(request_payload),
               &request_len);
    assert(host_protocol_handle_payload(request_payload, request_len,
                        response_payload,
                        sizeof(response_payload),
                        &response_len) == 0);
    return decode_response(response_payload, response_len);
}

static void send_hello(void)
{
    keyboard_DeviceMessage hello = keyboard_DeviceMessage_init_zero;
    keyboard_DeviceMessage response;

    hello.msg_id = 1;
    hello.which_body = keyboard_DeviceMessage_hello_req_tag;
    hello.body.hello_req.protocol_version = 1;
    response = request(&hello);

    assert(response.reply_to == 1);
    assert(response.which_body == keyboard_DeviceMessage_hello_rsp_tag);
    assert(response.body.hello_rsp.protocol_version == HOST_PROTOCOL_VERSION);
}

static keyboard_DeviceConfig valid_proto_config(void)
{
    struct app_config config;
    keyboard_DeviceConfig proto = keyboard_DeviceConfig_init_zero;

    assert(app_config_get_defaults(&config) == 0);
    proto.version = config.version;
    proto.rgb_enable = config.rgb_enable;
    proto.rgb_mode = config.rgb_mode;
    proto.rgb_brightness = config.rgb_brightness;
    proto.rgb_red = config.rgb_red;
    proto.rgb_green = config.rgb_green;
    proto.rgb_blue = config.rgb_blue;
    proto.numlock_policy = config.numlock_policy;
    proto.encoder_cw_action = config.encoder_cw_action;
    proto.encoder_ccw_action = config.encoder_ccw_action;
    proto.encoder_press_action = config.encoder_press_action;
    proto.mode_policy = config.mode_policy;
    proto.default_mode = config.default_mode;
    return proto;
}

static void reset_protocol_state(void)
{
    app_config_reset_for_test();
    app_config_store_reset_for_test();
    assert(app_config_init() == 0);
    host_protocol_reset_session();
}

static void test_get_requires_hello(void)
{
    keyboard_DeviceMessage get = keyboard_DeviceMessage_init_zero;
    keyboard_DeviceMessage response;

    reset_protocol_state();
    get.msg_id = 2;
    get.which_body = keyboard_DeviceMessage_config_get_req_tag;
    response = request(&get);

    assert(response.reply_to == 2);
    assert(response.which_body == keyboard_DeviceMessage_response_tag);
    assert(response.body.response.code ==
           keyboard_ResponseCode_RESPONSE_CODE_NOT_READY);
}

static void test_get_returns_default_config_after_hello(void)
{
    keyboard_DeviceMessage get = keyboard_DeviceMessage_init_zero;
    keyboard_DeviceMessage response;

    reset_protocol_state();
    send_hello();

    get.msg_id = 3;
    get.which_body = keyboard_DeviceMessage_config_get_req_tag;
    response = request(&get);

    assert(response.reply_to == 3);
    assert(response.which_body == keyboard_DeviceMessage_config_get_rsp_tag);
    assert(response.body.config_get_rsp.has_config);
    assert(response.body.config_get_rsp.config.rgb_brightness == 30);
    assert(response.body.config_get_rsp.config.encoder_press_action ==
           APP_ENCODER_ACTION_MUTE);
}

static void test_set_rejects_invalid_config(void)
{
    keyboard_DeviceMessage set = keyboard_DeviceMessage_init_zero;
    keyboard_DeviceMessage response;

    reset_protocol_state();
    send_hello();

    set.msg_id = 4;
    set.which_body = keyboard_DeviceMessage_config_set_req_tag;
    set.body.config_set_req.has_config = true;
    set.body.config_set_req.config = valid_proto_config();
    set.body.config_set_req.config.rgb_brightness = 101;
    set.body.config_set_req.apply = true;
    set.body.config_set_req.save = true;

    response = request(&set);
    assert(response.reply_to == 4);
    assert(response.which_body == keyboard_DeviceMessage_config_set_rsp_tag);
    assert(response.body.config_set_rsp.code ==
           keyboard_ResponseCode_RESPONSE_CODE_INVALID_PARAM);
    assert(!app_config_store_saved_for_test(NULL));
}

static void test_set_rejects_uint32_values_that_would_truncate(void)
{
    keyboard_DeviceMessage set = keyboard_DeviceMessage_init_zero;
    keyboard_DeviceMessage response;

    reset_protocol_state();
    send_hello();

    set.msg_id = 14;
    set.which_body = keyboard_DeviceMessage_config_set_req_tag;
    set.body.config_set_req.has_config = true;
    set.body.config_set_req.config = valid_proto_config();
    set.body.config_set_req.config.rgb_brightness = 257;
    set.body.config_set_req.apply = true;
    set.body.config_set_req.save = true;

    response = request(&set);
    assert(response.reply_to == 14);
    assert(response.which_body == keyboard_DeviceMessage_config_set_rsp_tag);
    assert(response.body.config_set_rsp.code ==
           keyboard_ResponseCode_RESPONSE_CODE_INVALID_PARAM);
    assert(!app_config_store_saved_for_test(NULL));
}

static void test_set_apply_and_save_updates_runtime_and_store(void)
{
    keyboard_DeviceMessage set = keyboard_DeviceMessage_init_zero;
    keyboard_DeviceMessage response;
    struct app_config saved;

    reset_protocol_state();
    send_hello();

    set.msg_id = 5;
    set.which_body = keyboard_DeviceMessage_config_set_req_tag;
    set.body.config_set_req.has_config = true;
    set.body.config_set_req.config = valid_proto_config();
    set.body.config_set_req.config.rgb_brightness = 67;
    set.body.config_set_req.apply = true;
    set.body.config_set_req.save = true;

    response = request(&set);
    assert(response.reply_to == 5);
    assert(response.which_body == keyboard_DeviceMessage_config_set_rsp_tag);
    assert(response.body.config_set_rsp.code ==
           keyboard_ResponseCode_RESPONSE_CODE_OK);
    assert(response.body.config_set_rsp.has_effective_config);
    assert(response.body.config_set_rsp.effective_config.rgb_brightness == 67);
    assert(app_config_store_saved_for_test(&saved));
    assert(saved.rgb_brightness == 67);
}

static void test_factory_reset_restores_defaults_and_saves(void)
{
    keyboard_DeviceMessage reset = keyboard_DeviceMessage_init_zero;
    keyboard_DeviceMessage response;
    struct app_config changed = { 0 };
    struct app_config saved;

    reset_protocol_state();
    send_hello();

    assert(app_config_get_defaults(&changed) == 0);
    changed.rgb_brightness = 88;
    assert(app_config_set(&changed, false) == 0);
    app_config_store_reset_for_test();

    reset.msg_id = 6;
    reset.which_body = keyboard_DeviceMessage_factory_reset_req_tag;
    response = request(&reset);

    assert(response.reply_to == 6);
    assert(response.which_body == keyboard_DeviceMessage_response_tag);
    assert(response.body.response.code ==
           keyboard_ResponseCode_RESPONSE_CODE_OK);
    assert(app_config_store_saved_for_test(&saved));
    assert(saved.rgb_brightness == 30);
}

static void test_bad_payload_returns_invalid_length_response(void)
{
    const uint8_t bad_payload[] = { 0xff, 0xff, 0xff };
    uint8_t response_payload[128];
    size_t response_len;
    keyboard_DeviceMessage response;

    reset_protocol_state();
    assert(host_protocol_handle_payload(bad_payload, sizeof(bad_payload),
                        response_payload,
                        sizeof(response_payload),
                        &response_len) == 0);
    response = decode_response(response_payload, response_len);
    assert(response.which_body == keyboard_DeviceMessage_response_tag);
    assert(response.body.response.code ==
           keyboard_ResponseCode_RESPONSE_CODE_INVALID_LENGTH);
}

int main(void)
{
    test_get_requires_hello();
    test_get_returns_default_config_after_hello();
    test_set_rejects_invalid_config();
    test_set_rejects_uint32_values_that_would_truncate();
    test_set_apply_and_save_updates_runtime_and_store();
    test_factory_reset_restores_defaults_and_saves();
    test_bad_payload_returns_invalid_length_response();
    return 0;
}
