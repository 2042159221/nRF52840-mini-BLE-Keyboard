#include <errno.h>

#include <zephyr/logging/log.h>

#include <mode/mode_manager.h>
#include <transport/transport.h>

LOG_MODULE_REGISTER(mode_manager, LOG_LEVEL_INF);

static enum kb_mode current_mode = KB_MODE_USB;

int mode_manager_init(void)
{
    current_mode = KB_MODE_USB;
    LOG_INF("keyboard mode manager initialized in USB mode");
    return transport_enable(current_mode);
}

enum kb_mode mode_manager_get_mode(void)
{
    return current_mode;
}

int mode_manager_set_mode(enum kb_mode mode)
{
    if (mode == KB_MODE_24G_RESERVED) {
        LOG_WRN("2.4G mode is reserved and not available in phase one");
        return -ENOTSUP;
    }

    if (mode != KB_MODE_USB && mode != KB_MODE_BLE) {
        return -EINVAL;
    }

    if (mode == current_mode) {
        return 0;
    }

    int err = transport_disable(current_mode);

    if (err != 0) {
        return err;
    }

    err = transport_enable(mode);
    if (err != 0) {
        (void)transport_enable(current_mode);
        return err;
    }

    current_mode = mode;
    LOG_INF("keyboard mode changed to %d", current_mode);
    return 0;
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