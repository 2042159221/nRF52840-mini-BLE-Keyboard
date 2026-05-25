#ifndef HID_FLOWCTRL_H
#define HID_FLOWCTRL_H

#include <stdint.h>

#include <hid/hid_report.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HID_FLOWCTRL_CONSUMER_QUEUE_DEPTH 8U
#define HID_FLOWCTRL_CONSUMER_ACTIONS_PER_ROUND 2U
#define HID_FLOWCTRL_WORK_INTERVAL_MS 25U

int hid_flowctrl_init(void);
int hid_flowctrl_submit_keyboard_report(const struct hid_keyboard_report *report);
int hid_flowctrl_flush_keyboard(void);
int hid_flowctrl_submit_encoder_action(uint8_t action);
int hid_flowctrl_submit_encoder_delta(int32_t delta);

#if defined(UNIT_TEST)
void hid_flowctrl_reset_for_test(void);
int hid_flowctrl_process_for_test(void);
int32_t hid_flowctrl_pending_encoder_delta_for_test(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
