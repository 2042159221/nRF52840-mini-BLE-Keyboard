#include <assert.h>

#include <hid/hid_consumer.h>

static void test_consumer_report_press_and_release(void)
{
    struct hid_consumer_report report = {0};

    hid_consumer_report_press(&report, HID_CONSUMER_VOLUME_UP);
    assert(report.usage == HID_CONSUMER_VOLUME_UP);

    hid_consumer_report_release(&report);
    assert(report.usage == HID_CONSUMER_NONE);
}

static void test_consumer_report_ignores_null_report(void)
{
    hid_consumer_report_press(NULL, HID_CONSUMER_MUTE);
    hid_consumer_report_release(NULL);
}

int main(void)
{
    test_consumer_report_press_and_release();
    test_consumer_report_ignores_null_report();
    return 0;
}
