#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <input/input_manager.h>
#include <mode/mode_manager.h>
#include <transport/transport.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

int main(void)
{
        int err = transport_init();

        if (err != 0) {
                LOG_ERR("transport init failed: %d", err);
                return 0;
        }

        err = input_manager_init();
        if (err != 0) {
                LOG_ERR("input init failed: %d", err);
                return 0;
        }

        err = mode_manager_init();
        if (err != 0) {
                LOG_ERR("mode manager init failed: %d", err);
                return 0;
        }

        LOG_INF("mini keyboard application started");
        return 0;
}
