#include <errno.h>
#include <stdbool.h>

#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <hid/hid_report.h>

LOG_MODULE_REGISTER(transport_24g_stub, LOG_LEVEL_INF);

int transport_24g_init(void)
{
    LOG_INF("2.4G transport reserved");
    return 0;
}

int transport_24g_enable(void)
{
    LOG_WRN("2.4G transport is reserved and disabled");
    return -ENOTSUP;
}

int transport_24g_disable(void)
{
    return 0;
}

bool transport_24g_ready(void)
{
    return false;
}

int transport_24g_send_keyboard_report(const struct hid_keyboard_report *report)
{
    ARG_UNUSED(report);
    return -ENOTSUP;
}