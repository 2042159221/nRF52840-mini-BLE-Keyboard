#include <stddef.h>

#include <hid/hid_consumer.h>

void hid_consumer_report_press(struct hid_consumer_report *report, uint16_t usage)
{
    if (report == NULL) {
        return;
    }

    report->usage = usage;
}

void hid_consumer_report_release(struct hid_consumer_report *report)
{
    if (report == NULL) {
        return;
    }

    report->usage = HID_CONSUMER_NONE;
}
