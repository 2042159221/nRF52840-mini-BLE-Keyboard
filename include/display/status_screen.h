#ifndef DISPLAY_STATUS_SCREEN_H_
#define DISPLAY_STATUS_SCREEN_H_

#include <stdbool.h>
#include <stdint.h>

#include <config/app_config.h>
#include <display/status_screen_model.h>
#include <mode/mode_manager.h>
#include <power/power_state.h>

#ifdef __cplusplus
extern "C" {
#endif

struct status_screen_snapshot {
	enum kb_mode mode;
	struct power_state_snapshot power;
	struct app_config config;
	bool num_lock;
	const char *message;
};

int status_screen_init(void);
bool status_screen_input_route(struct status_screen_model *model, enum status_screen_input input,
			       struct status_screen_event_result *result);

#ifdef CONFIG_LVGL
int status_screen_lvgl_init(const struct status_screen_snapshot *snapshot);
void status_screen_lvgl_update(const struct status_screen_snapshot *snapshot);
#endif

#ifdef __cplusplus
}
#endif

#endif
