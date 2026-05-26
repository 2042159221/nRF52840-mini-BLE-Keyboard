#ifndef DISPLAY_STATUS_SCREEN_MODEL_H_
#define DISPLAY_STATUS_SCREEN_MODEL_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum status_screen_page {
	STATUS_SCREEN_PAGE_HOME,
	STATUS_SCREEN_PAGE_SETTINGS,
	STATUS_SCREEN_PAGE_EDIT,
	STATUS_SCREEN_PAGE_CONFIRM,
};

enum status_screen_input {
	STATUS_SCREEN_INPUT_ROTATE_CW,
	STATUS_SCREEN_INPUT_ROTATE_CCW,
	STATUS_SCREEN_INPUT_PRESS,
	STATUS_SCREEN_INPUT_LONG_PRESS,
};

enum status_screen_action {
	STATUS_SCREEN_ACTION_NONE,
	STATUS_SCREEN_ACTION_HID_PASSTHROUGH,
	STATUS_SCREEN_ACTION_ENTER_SETTINGS,
	STATUS_SCREEN_ACTION_EXIT_HOME,
	STATUS_SCREEN_ACTION_APPLY_RGB_BRIGHTNESS,
	STATUS_SCREEN_ACTION_CANCEL_EDIT,
	STATUS_SCREEN_ACTION_CANCEL_CONFIRM,
	STATUS_SCREEN_ACTION_FACTORY_RESET_CONFIRMED,
};

struct status_screen_event_result {
	bool consumed;
	enum status_screen_action action;
	int value;
};

struct status_screen_model {
	enum status_screen_page page;
	unsigned int settings_focus;
	int rgb_brightness;
	int edit_candidate;
	bool confirm_selected;
};

void status_screen_model_init(struct status_screen_model *model);
void status_screen_model_init_with_brightness(struct status_screen_model *model, int brightness);
void status_screen_model_set_rgb_brightness(struct status_screen_model *model, int brightness);
struct status_screen_event_result status_screen_model_handle_input(
	struct status_screen_model *model, enum status_screen_input input);
bool status_screen_model_ui_active(const struct status_screen_model *model);
enum status_screen_page status_screen_model_current_page(const struct status_screen_model *model);
unsigned int status_screen_model_settings_focus(const struct status_screen_model *model);
const char *status_screen_model_focused_label(const struct status_screen_model *model);
bool status_screen_model_visible_exit(const struct status_screen_model *model);
const char *status_screen_model_edit_label(const struct status_screen_model *model);
int status_screen_model_edit_value(const struct status_screen_model *model);
bool status_screen_model_confirm_selected(const struct status_screen_model *model);

#ifdef __cplusplus
}
#endif

#endif
