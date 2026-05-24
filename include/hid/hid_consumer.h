#ifndef HID_CONSUMER_H
#define HID_CONSUMER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HID_CONSUMER_NONE        0x0000
#define HID_CONSUMER_MUTE        0x00E2
#define HID_CONSUMER_VOLUME_UP   0x00E9
#define HID_CONSUMER_VOLUME_DOWN 0x00EA

struct hid_consumer_report {
    uint16_t usage;
};

void hid_consumer_report_press(struct hid_consumer_report *report, uint16_t usage);
void hid_consumer_report_release(struct hid_consumer_report *report);

#ifdef __cplusplus
}
#endif

#endif
