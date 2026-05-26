#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <config/app_config.h>
#include <config/app_config_store.h>
#include <display/status_screen.h>
#include <hid/hid_flowctrl.h>
#include <host/host_comm.h>
#include <input/encoder_manager.h>
#include <input/input_manager.h>
#include <mode/mode_manager.h>
#include <power/power_manager.h>
#include <rgb/rgb_manager.h>
#include <transport/transport.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

int main(void)
{
        int err = power_manager_init();

        if (err != 0) {
                LOG_ERR("power manager init failed: %d", err);
        }

        err = app_config_init();
        if (err != 0) {
                LOG_ERR("app config init failed: %d", err);
                return 0;
        }

        err = app_config_store_load();
        if (err != 0) {
                LOG_WRN("app config load failed: %d", err);
        }

        err = transport_init();
        if (err != 0) {
                LOG_ERR("transport init failed: %d", err);
                return 0;
        }

        err = host_comm_init();
        if (err != 0) {
                LOG_WRN("host config channel init failed: %d", err);
        }

        err = hid_flowctrl_init();
        if (err != 0) {
                LOG_ERR("HID flow control init failed: %d", err);
                return 0;
        }

        err = input_manager_init();
        if (err != 0) {
                LOG_ERR("input init failed: %d", err);
                return 0;
        }

        err = encoder_manager_init();
        if (err != 0) {
                LOG_ERR("encoder init failed: %d", err);
                return 0;
        }

        err = rgb_manager_init();
        if (err != 0) {
                LOG_WRN("RGB init failed: %d", err);
        }

        err = mode_manager_init();
        if (err != 0) {
                LOG_ERR("mode manager init failed: %d", err);
                return 0;
        }

        err = status_screen_init();
        if (err != 0) {
                LOG_WRN("status screen init failed: %d", err);
        }

        app_config_notify_all();

        LOG_INF("mini keyboard application started");
        return 0;
}
