#ifndef TRANSPORT_H
#define TRANSPORT_H

#include <stdbool.h>

#include <hid/hid_consumer.h>
#include <hid/hid_report.h>
#include <mode/mode_manager.h>

#ifdef __cplusplus
extern "C" {
#endif

int transport_init(void);
int transport_enable(enum kb_mode mode);
int transport_disable(enum kb_mode mode);
bool transport_ready(enum kb_mode mode);
bool transport_consumer_ready(enum kb_mode mode);
int transport_send_keyboard_report(enum kb_mode mode, const struct hid_keyboard_report *report);
int transport_send_consumer_report(enum kb_mode mode, const struct hid_consumer_report *report);

#ifdef __cplusplus
}
#endif

#endif
