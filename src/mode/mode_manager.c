#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/dt-bindings/adc/nrf-saadc.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <input/input_manager.h>
#include <mode/mode_manager.h>
#include <transport/transport.h>

LOG_MODULE_REGISTER(mode_manager, LOG_LEVEL_INF);

#define MODE_ADC_DEVICE DT_NODELABEL(adc)
#define MODE_ADC_CHANNEL_ID 5
#define MODE_ADC_RESOLUTION_BITS 12
#define MODE_ADC_POLL_INTERVAL_MS 150
#define MODE_ADC_SAMPLE_COUNT 4
#define MODE_ADC_STABLE_READS 3
#define MODE_USB_MAX_MV 825
#define MODE_BLE_MIN_MV 2475
#define MODE_MANAGER_MAX_LISTENERS 4

static const struct device *const mode_adc_dev = DEVICE_DT_GET(MODE_ADC_DEVICE);
static const struct adc_channel_cfg mode_adc_channel_cfg = {
    .gain = ADC_GAIN_1_6,
    .reference = ADC_REF_INTERNAL,
    .acquisition_time = ADC_ACQ_TIME_DEFAULT,
    .channel_id = MODE_ADC_CHANNEL_ID,
#if defined(CONFIG_ADC_CONFIGURABLE_INPUTS)
    .differential = 0,
    .input_positive = NRF_SAADC_AIN5,
#endif
};
static uint16_t mode_adc_sample;
static struct adc_sequence mode_adc_sequence = {
    .channels = BIT(MODE_ADC_CHANNEL_ID),
    .buffer = &mode_adc_sample,
    .buffer_size = sizeof(mode_adc_sample),
    .resolution = MODE_ADC_RESOLUTION_BITS,
};
static struct k_work_delayable mode_poll_work;
static enum kb_mode current_mode = KB_MODE_24G_RESERVED;
static enum kb_mode pending_mode = KB_MODE_24G_RESERVED;
static uint8_t pending_mode_reads;
static bool mode_adc_ready;
static bool mode_manager_initialized;

struct mode_listener_entry {
    bool in_use;
    mode_manager_event_handler_t handler;
    void *user_data;
};

static struct mode_listener_entry mode_listeners[MODE_MANAGER_MAX_LISTENERS];

static const char *mode_to_string(enum kb_mode mode)
{
    switch (mode) {
    case KB_MODE_USB:
        return "USB";
    case KB_MODE_BLE:
        return "BLE";
    case KB_MODE_24G_RESERVED:
        return "2.4G_RESERVED";
    default:
        return "UNKNOWN";
    }
}

static enum kb_mode mode_from_voltage_mv(int32_t mv)
{
    if (mv < MODE_USB_MAX_MV) {
        return KB_MODE_USB;
    }

    if (mv >= MODE_BLE_MIN_MV) {
        return KB_MODE_BLE;
    }

    return KB_MODE_24G_RESERVED;
}

static int mode_adc_init(void)
{
    int err;

    if (!device_is_ready(mode_adc_dev)) {
        return -ENODEV;
    }

    err = adc_channel_setup(mode_adc_dev, &mode_adc_channel_cfg);
    if (err < 0) {
        return err;
    }

    mode_adc_ready = true;
    return 0;
}

static int mode_adc_read_mv(int32_t *mv_out)
{
    int err;
    int32_t sum_mv = 0;
    int32_t ref_mv;

    ref_mv = (int32_t)adc_ref_internal(mode_adc_dev);
    if (ref_mv <= 0) {
        return -ENOTSUP;
    }

    for (size_t i = 0; i < MODE_ADC_SAMPLE_COUNT; ++i) {
        int32_t sample_mv;

        err = adc_read(mode_adc_dev, &mode_adc_sequence);
        if (err < 0) {
            return err;
        }

        sample_mv = (int32_t)mode_adc_sample;
        err = adc_raw_to_millivolts(ref_mv, mode_adc_channel_cfg.gain,
                        MODE_ADC_RESOLUTION_BITS, &sample_mv);
        if (err < 0) {
            return err;
        }

        sum_mv += sample_mv;
    }

    *mv_out = sum_mv / MODE_ADC_SAMPLE_COUNT;
    return 0;
}

static void mode_manager_publish_event(enum kb_mode mode,
                   enum kb_mode previous_mode,
                   bool initial)
{
    for (size_t i = 0; i < ARRAY_SIZE(mode_listeners); ++i) {
        if (!mode_listeners[i].in_use || mode_listeners[i].handler == NULL) {
            continue;
        }

        mode_listeners[i].handler(mode, previous_mode, initial,
                      mode_listeners[i].user_data);
    }
}

static int mode_manager_apply_mode(enum kb_mode mode, bool initial)
{
    enum kb_mode previous_mode = current_mode;
    int err;

    if (mode == previous_mode) {
        if (initial) {
            mode_manager_publish_event(mode, previous_mode, true);
        }
        return 0;
    }

    if (previous_mode != KB_MODE_24G_RESERVED) {
        input_manager_release_all();

        err = transport_disable(previous_mode);
        if (err != 0) {
            return err;
        }
    }

    current_mode = mode;

    if (mode == KB_MODE_24G_RESERVED) {
        LOG_WRN("mode selector moved to reserved 2.4G position");
        mode_manager_publish_event(current_mode, previous_mode, initial);
        return 0;
    }

    err = transport_enable(mode);
    if (err != 0) {
        if (previous_mode != KB_MODE_24G_RESERVED) {
            int restore_err = transport_enable(previous_mode);

            if (restore_err != 0) {
                LOG_ERR("failed to restore previous mode %s: %d",
                    mode_to_string(previous_mode), restore_err);
            }
        }

        current_mode = previous_mode;
        return err;
    }

    LOG_INF("keyboard mode changed to %s", mode_to_string(current_mode));
    mode_manager_publish_event(current_mode, previous_mode, initial);
    return 0;
}

static int mode_manager_sync_from_hardware(void)
{
    int32_t mv;
    enum kb_mode mode;
    int err;

    err = mode_adc_read_mv(&mv);
    if (err < 0) {
        return err;
    }

    mode = mode_from_voltage_mv(mv);
    LOG_INF("mode selector boot read %ld mV -> %s", (long)mv, mode_to_string(mode));
    return mode_manager_apply_mode(mode, true);
}

static int mode_manager_update_from_hardware(void)
{
    int32_t mv;
    enum kb_mode mode;
    int err;

    err = mode_adc_read_mv(&mv);
    if (err < 0) {
        return err;
    }

    mode = mode_from_voltage_mv(mv);

    if (mode != pending_mode) {
        pending_mode = mode;
        pending_mode_reads = 1;
        return 0;
    }

    if (pending_mode_reads < MODE_ADC_STABLE_READS) {
        pending_mode_reads++;
        return 0;
    }

    pending_mode_reads = 0;

    if (mode == current_mode) {
        return 0;
    }

    LOG_INF("mode selector stable at %ld mV -> %s", (long)mv, mode_to_string(mode));
    return mode_manager_apply_mode(mode, false);
}

static void mode_poll_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    if (mode_adc_ready) {
        int err = mode_manager_update_from_hardware();

        if (err < 0) {
            LOG_WRN("mode selector ADC poll failed: %d", err);
        }
    }

    (void)k_work_reschedule(&mode_poll_work, K_MSEC(MODE_ADC_POLL_INTERVAL_MS));
}

int mode_manager_init(void)
{
    int err;

    current_mode = KB_MODE_24G_RESERVED;
    pending_mode = KB_MODE_24G_RESERVED;
    pending_mode_reads = 0;
    mode_adc_ready = false;
    mode_manager_initialized = false;

    err = mode_adc_init();
    if (err != 0) {
        LOG_WRN("mode selector ADC unavailable: %d", err);
        err = mode_manager_apply_mode(KB_MODE_USB, true);
        if (err != 0) {
            return err;
        }

        mode_manager_initialized = true;
        LOG_INF("keyboard mode manager initialized in USB mode");
        return 0;
    }

    err = mode_manager_sync_from_hardware();
    if (err != 0) {
        LOG_WRN("initial mode selector read failed: %d, defaulting to USB", err);
        current_mode = KB_MODE_24G_RESERVED;
        err = mode_manager_apply_mode(KB_MODE_USB, true);
        if (err != 0) {
            return err;
        }
    }

    k_work_init_delayable(&mode_poll_work, mode_poll_work_handler);
    (void)k_work_reschedule(&mode_poll_work, K_MSEC(MODE_ADC_POLL_INTERVAL_MS));

    mode_manager_initialized = true;
    LOG_INF("keyboard mode manager initialized in %s mode", mode_to_string(current_mode));
    return 0;
}

enum kb_mode mode_manager_get_mode(void)
{
    return current_mode;
}

int mode_manager_set_mode(enum kb_mode mode)
{
    if (mode > KB_MODE_24G_RESERVED) {
        return -EINVAL;
    }

    pending_mode = mode;
    pending_mode_reads = 0;
    return mode_manager_apply_mode(mode, false);
}

int mode_manager_register_listener(mode_manager_event_handler_t handler, void *user_data)
{
    if (handler == NULL) {
        return -EINVAL;
    }

    for (size_t i = 0; i < ARRAY_SIZE(mode_listeners); ++i) {
        if (mode_listeners[i].in_use) {
            continue;
        }

        mode_listeners[i].in_use = true;
        mode_listeners[i].handler = handler;
        mode_listeners[i].user_data = user_data;

        if (mode_manager_initialized) {
            handler(current_mode, current_mode, true, user_data);
        }

        return 0;
    }

    return -ENOMEM;
}

int mode_manager_unregister_listener(mode_manager_event_handler_t handler, void *user_data)
{
    if (handler == NULL) {
        return -EINVAL;
    }

    for (size_t i = 0; i < ARRAY_SIZE(mode_listeners); ++i) {
        if (!mode_listeners[i].in_use) {
            continue;
        }

        if (mode_listeners[i].handler != handler || mode_listeners[i].user_data != user_data) {
            continue;
        }

        mode_listeners[i].in_use = false;
        mode_listeners[i].handler = NULL;
        mode_listeners[i].user_data = NULL;
        return 0;
    }

    return -ENOENT;
}

enum kb_mode mode_manager_next_supported_mode(enum kb_mode current)
{
    switch (current) {
    case KB_MODE_USB:
        return KB_MODE_BLE;
    case KB_MODE_BLE:
    case KB_MODE_24G_RESERVED:
    default:
        return KB_MODE_USB;
    }
}
