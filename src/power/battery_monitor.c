#include <errno.h>
#include <stdint.h>

#include <power/battery_monitor.h>

#if !defined(UNIT_TEST)
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/dt-bindings/adc/nrf-saadc.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(battery_monitor, LOG_LEVEL_INF);

#define BATTERY_ADC_DEVICE DT_NODELABEL(adc)
#define BATTERY_ADC_CHANNEL_ID 7
#define BATTERY_ADC_RESOLUTION_BITS 12
#define BATTERY_ADC_SETTLE_MS 10
#define BATTERY_POWER_NODE DT_PATH(zephyr_user)

static const struct device *const battery_adc_dev = DEVICE_DT_GET(BATTERY_ADC_DEVICE);
static const struct gpio_dt_spec battery_adc_enable =
    GPIO_DT_SPEC_GET(BATTERY_POWER_NODE, battery_enable_gpios);
static const struct adc_channel_cfg battery_adc_channel_cfg = {
    .gain = ADC_GAIN_1_6,
    .reference = ADC_REF_INTERNAL,
    .acquisition_time = ADC_ACQ_TIME_DEFAULT,
    .channel_id = BATTERY_ADC_CHANNEL_ID,
#if defined(CONFIG_ADC_CONFIGURABLE_INPUTS)
    .differential = 0,
    .input_positive = NRF_SAADC_AIN7,
#endif
};
static uint16_t battery_adc_raw;
static struct adc_sequence battery_adc_sequence = {
    .channels = BIT(BATTERY_ADC_CHANNEL_ID),
    .buffer = &battery_adc_raw,
    .buffer_size = sizeof(battery_adc_raw),
    .resolution = BATTERY_ADC_RESOLUTION_BITS,
};
static bool battery_monitor_ready;
#endif

enum battery_state battery_monitor_classify_mv(int32_t vbat_mv)
{
    if (vbat_mv >= 4100) {
        return BATTERY_STATE_FULL_OR_HIGH;
    }

    if (vbat_mv >= 3800) {
        return BATTERY_STATE_NORMAL;
    }

    if (vbat_mv >= 3500) {
        return BATTERY_STATE_LOW;
    }

    return BATTERY_STATE_CRITICAL;
}

const char *battery_monitor_state_name(enum battery_state state)
{
    switch (state) {
    case BATTERY_STATE_FULL_OR_HIGH:
        return "FULL_OR_HIGH";
    case BATTERY_STATE_NORMAL:
        return "NORMAL";
    case BATTERY_STATE_LOW:
        return "LOW";
    case BATTERY_STATE_CRITICAL:
        return "CRITICAL";
    default:
        return "UNKNOWN";
    }
}

#if defined(UNIT_TEST)
int battery_monitor_init(void)
{
    return -ENOTSUP;
}

int battery_monitor_sample_once(struct battery_monitor_sample *sample)
{
    (void)sample;
    return -ENOTSUP;
}
#else
int battery_monitor_init(void)
{
    int err;

    battery_monitor_ready = false;

    if (!gpio_is_ready_dt(&battery_adc_enable)) {
        return -ENODEV;
    }

    err = gpio_pin_configure_dt(&battery_adc_enable, GPIO_OUTPUT_INACTIVE);
    if (err < 0) {
        return err;
    }

    if (!device_is_ready(battery_adc_dev)) {
        return -ENODEV;
    }

    err = adc_channel_setup(battery_adc_dev, &battery_adc_channel_cfg);
    if (err < 0) {
        (void)gpio_pin_set_dt(&battery_adc_enable, 0);
        return err;
    }

    battery_monitor_ready = true;
    return 0;
}

int battery_monitor_sample_once(struct battery_monitor_sample *sample)
{
    int err;
    int32_t sample_mv;
    int32_t ref_mv;

    if (sample == NULL) {
        return -EINVAL;
    }

    if (!battery_monitor_ready) {
        return -ENODEV;
    }

    err = gpio_pin_set_dt(&battery_adc_enable, 1);
    if (err < 0) {
        return err;
    }

    k_sleep(K_MSEC(BATTERY_ADC_SETTLE_MS));

    err = adc_read(battery_adc_dev, &battery_adc_sequence);
    (void)gpio_pin_set_dt(&battery_adc_enable, 0);
    if (err < 0) {
        return err;
    }

    ref_mv = (int32_t)adc_ref_internal(battery_adc_dev);
    if (ref_mv <= 0) {
        return -ENOTSUP;
    }

    sample_mv = (int32_t)battery_adc_raw;
    err = adc_raw_to_millivolts(ref_mv, battery_adc_channel_cfg.gain,
                    BATTERY_ADC_RESOLUTION_BITS, &sample_mv);
    if (err < 0) {
        return err;
    }

    sample->raw = (int32_t)battery_adc_raw;
    sample->adc_mv = sample_mv;
    sample->vbat_mv = battery_monitor_adc_mv_to_vbat_mv(sample_mv);
    sample->state = battery_monitor_classify_mv(sample->vbat_mv);
    return 0;
}
#endif
