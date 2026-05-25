#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if !defined(UNIT_TEST)
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#endif

#include <config/app_config.h>
#include <hid/hid_flowctrl.h>
#include <input/encoder_action.h>
#include <mode/mode_manager.h>
#include <transport/transport.h>

#define HID_FLOWCTRL_DROP_LOG_INTERVAL_MS 3000
#define FLOWCTRL_ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

#if !defined(UNIT_TEST)
LOG_MODULE_REGISTER(hid_flowctrl, LOG_LEVEL_INF);

static struct k_spinlock flowctrl_lock;
static struct k_work keyboard_work;
static struct k_work_delayable consumer_work;
static bool flowctrl_initialized;
static atomic_t pending_encoder_delta;
#else
static int32_t pending_encoder_delta;
#endif

static struct hid_keyboard_report latest_keyboard_report;
static bool latest_keyboard_report_pending;
static uint8_t consumer_action_fifo[HID_FLOWCTRL_CONSUMER_QUEUE_DEPTH];
static size_t consumer_action_head;
static size_t consumer_action_tail;
static size_t consumer_action_count;

static bool action_requires_consumer_transport(uint8_t action)
{
    switch (action) {
    case APP_ENCODER_ACTION_VOLUME_UP:
    case APP_ENCODER_ACTION_VOLUME_DOWN:
    case APP_ENCODER_ACTION_MUTE:
    case APP_ENCODER_ACTION_PLAY_PAUSE:
    case APP_ENCODER_ACTION_NEXT_TRACK:
    case APP_ENCODER_ACTION_PREV_TRACK:
        return true;
    default:
        return false;
    }
}

#if defined(UNIT_TEST)
typedef unsigned int flowctrl_lock_key_t;

static unsigned int flowctrl_lock_take(void)
{
    return 0U;
}

static void flowctrl_lock_release(unsigned int key)
{
    (void)key;
}

static void flowctrl_schedule_keyboard(void)
{
}

static void flowctrl_schedule_consumer(void)
{
}
#else
typedef k_spinlock_key_t flowctrl_lock_key_t;

static k_spinlock_key_t flowctrl_lock_take(void)
{
    return k_spin_lock(&flowctrl_lock);
}

static void flowctrl_lock_release(k_spinlock_key_t key)
{
    k_spin_unlock(&flowctrl_lock, key);
}

static void flowctrl_schedule_keyboard(void)
{
    if (flowctrl_initialized) {
        (void)k_work_submit(&keyboard_work);
    }
}

static void flowctrl_schedule_consumer(void)
{
    ARG_UNUSED(flowctrl_initialized);
}
#endif

static int32_t pending_delta_get(void)
{
#if defined(UNIT_TEST)
    return pending_encoder_delta;
#else
    return (int32_t)atomic_get(&pending_encoder_delta);
#endif
}

static void pending_delta_add(int32_t delta)
{
#if defined(UNIT_TEST)
    pending_encoder_delta += delta;
#else
    (void)atomic_add(&pending_encoder_delta, (atomic_val_t)delta);
#endif
}

static bool pending_delta_consume_one(int32_t *step)
{
    if (step == NULL) {
        return false;
    }

    while (true) {
        const int32_t current = pending_delta_get();
        const int32_t next_step = current > 0 ? 1 : -1;
        const int32_t next = current - next_step;

        if (current == 0) {
            return false;
        }

#if defined(UNIT_TEST)
        pending_encoder_delta = next;
        *step = next_step;
        return true;
#else
        if (atomic_cas(&pending_encoder_delta, (atomic_val_t)current,
                   (atomic_val_t)next)) {
            *step = next_step;
            return true;
        }
#endif
    }
}

static int consumer_fifo_push(uint8_t action)
{
    flowctrl_lock_key_t key = flowctrl_lock_take();

    if (consumer_action_count >= FLOWCTRL_ARRAY_SIZE(consumer_action_fifo)) {
        flowctrl_lock_release(key);
        return -ENOMSG;
    }

    consumer_action_fifo[consumer_action_tail] = action;
    consumer_action_tail = (consumer_action_tail + 1U) %
                   FLOWCTRL_ARRAY_SIZE(consumer_action_fifo);
    consumer_action_count++;
    flowctrl_lock_release(key);
    return 0;
}

static bool consumer_fifo_pop(uint8_t *action)
{
    flowctrl_lock_key_t key = flowctrl_lock_take();

    if (consumer_action_count == 0U) {
        flowctrl_lock_release(key);
        return false;
    }

    *action = consumer_action_fifo[consumer_action_head];
    consumer_action_head = (consumer_action_head + 1U) %
                   FLOWCTRL_ARRAY_SIZE(consumer_action_fifo);
    consumer_action_count--;
    flowctrl_lock_release(key);
    return true;
}

static bool keyboard_report_take_latest(struct hid_keyboard_report *report)
{
    flowctrl_lock_key_t key = flowctrl_lock_take();

    if (!latest_keyboard_report_pending) {
        flowctrl_lock_release(key);
        return false;
    }

    *report = latest_keyboard_report;
    latest_keyboard_report_pending = false;
    flowctrl_lock_release(key);
    return true;
}

static int process_latest_keyboard_report(void)
{
    struct hid_keyboard_report report;
    enum kb_mode mode;
    int err;

    if (!keyboard_report_take_latest(&report)) {
        return 0;
    }

    mode = mode_manager_get_mode();
    if (!transport_keyboard_ready(mode)) {
#if !defined(UNIT_TEST)
        LOG_WRN_RATELIMIT_RATE(HID_FLOWCTRL_DROP_LOG_INTERVAL_MS,
            "keyboard report dropped: mode %d transport is not ready", mode);
#endif
        return -ENOTCONN;
    }

    err = transport_send_keyboard_report(mode, &report);
    if (err != 0) {
#if !defined(UNIT_TEST)
        LOG_WRN_RATELIMIT_RATE(HID_FLOWCTRL_DROP_LOG_INTERVAL_MS,
            "keyboard report send failed: %d", err);
#endif
    }

    return err;
}

static uint8_t encoder_delta_action(int32_t step)
{
    struct app_config config;

    if (app_config_get(&config) != 0) {
        return step > 0 ? APP_ENCODER_ACTION_VOLUME_UP :
                  APP_ENCODER_ACTION_VOLUME_DOWN;
    }

    return step > 0 ? config.encoder_cw_action : config.encoder_ccw_action;
}

static int trigger_flowctrl_action(uint8_t action)
{
    enum kb_mode mode = mode_manager_get_mode();

    if (action_requires_consumer_transport(action) &&
        !transport_consumer_ready(mode)) {
        return -ENOTCONN;
    }

    return encoder_action_trigger(action);
}

static int process_consumer_round(void)
{
    uint8_t attempts = 0U;
    int last_err = 0;

    while (attempts < HID_FLOWCTRL_CONSUMER_ACTIONS_PER_ROUND) {
        uint8_t action;
        int err;

        if (!consumer_fifo_pop(&action)) {
            break;
        }

        attempts++;
        err = trigger_flowctrl_action(action);
        if (err != 0) {
            last_err = err;
#if !defined(UNIT_TEST)
            LOG_WRN_RATELIMIT_RATE(HID_FLOWCTRL_DROP_LOG_INTERVAL_MS,
                "consumer action %u dropped: %d", action, err);
#endif
            continue;
        }
    }

    while (attempts < HID_FLOWCTRL_CONSUMER_ACTIONS_PER_ROUND) {
        int32_t step;
        int err;

        if (!pending_delta_consume_one(&step)) {
            break;
        }

        attempts++;
        err = trigger_flowctrl_action(encoder_delta_action(step));
        if (err != 0) {
            last_err = err;
#if !defined(UNIT_TEST)
            LOG_WRN_RATELIMIT_RATE(HID_FLOWCTRL_DROP_LOG_INTERVAL_MS,
                "encoder delta dropped: %d", err);
#endif
            continue;
        }
    }

    return last_err;
}

#if !defined(UNIT_TEST)
static void keyboard_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    (void)process_latest_keyboard_report();
}

static void consumer_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    (void)process_consumer_round();
    (void)k_work_reschedule(&consumer_work,
                K_MSEC(HID_FLOWCTRL_WORK_INTERVAL_MS));
}
#endif

int hid_flowctrl_init(void)
{
#if !defined(UNIT_TEST)
    if (flowctrl_initialized) {
        return 0;
    }

    k_work_init(&keyboard_work, keyboard_work_handler);
    k_work_init_delayable(&consumer_work, consumer_work_handler);
    flowctrl_initialized = true;
    (void)k_work_reschedule(&consumer_work,
                K_MSEC(HID_FLOWCTRL_WORK_INTERVAL_MS));
#endif
    return 0;
}

int hid_flowctrl_submit_keyboard_report(const struct hid_keyboard_report *report)
{
    flowctrl_lock_key_t key = flowctrl_lock_take();

    if (report == NULL) {
        flowctrl_lock_release(key);
        return -EINVAL;
    }

    latest_keyboard_report = *report;
    latest_keyboard_report_pending = true;
    flowctrl_lock_release(key);
    flowctrl_schedule_keyboard();
    return 0;
}

int hid_flowctrl_flush_keyboard(void)
{
    return process_latest_keyboard_report();
}

int hid_flowctrl_submit_encoder_action(uint8_t action)
{
    enum kb_mode mode = mode_manager_get_mode();
    int err;

    if (action_requires_consumer_transport(action) &&
        !transport_consumer_ready(mode)) {
#if !defined(UNIT_TEST)
        LOG_WRN_RATELIMIT_RATE(HID_FLOWCTRL_DROP_LOG_INTERVAL_MS,
            "consumer action %u dropped: mode %d transport is not ready",
            action, mode);
#endif
        return -ENOTCONN;
    }

    err = consumer_fifo_push(action);
    if (err == -ENOMSG) {
#if !defined(UNIT_TEST)
        LOG_WRN_RATELIMIT_RATE(HID_FLOWCTRL_DROP_LOG_INTERVAL_MS,
            "consumer action queue full; dropping action %u", action);
#endif
        return err;
    }

    if (err == 0) {
        flowctrl_schedule_consumer();
    }

    return err;
}

int hid_flowctrl_submit_encoder_delta(int32_t delta)
{
    enum kb_mode mode;

    if (delta == 0) {
        return 0;
    }

    mode = mode_manager_get_mode();
    if (!transport_consumer_ready(mode)) {
#if !defined(UNIT_TEST)
        LOG_WRN_RATELIMIT_RATE(HID_FLOWCTRL_DROP_LOG_INTERVAL_MS,
            "encoder delta %ld dropped: mode %d consumer transport is not ready",
            (long)delta, mode);
#endif
        return -ENOTCONN;
    }

    pending_delta_add(delta);
    flowctrl_schedule_consumer();
    return 0;
}

#if defined(UNIT_TEST)
void hid_flowctrl_reset_for_test(void)
{
    memset(&latest_keyboard_report, 0, sizeof(latest_keyboard_report));
    latest_keyboard_report_pending = false;
    memset(consumer_action_fifo, 0, sizeof(consumer_action_fifo));
    consumer_action_head = 0U;
    consumer_action_tail = 0U;
    consumer_action_count = 0U;
    pending_encoder_delta = 0;
}

int hid_flowctrl_process_for_test(void)
{
    int keyboard_err = process_latest_keyboard_report();
    int consumer_err = process_consumer_round();

    return keyboard_err != 0 ? keyboard_err : consumer_err;
}

int32_t hid_flowctrl_pending_encoder_delta_for_test(void)
{
    return pending_delta_get();
}
#endif
