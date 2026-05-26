#include <display/status_screen_model.h>

#include <stddef.h>

enum settings_item {
	SETTINGS_ITEM_MODE,
	SETTINGS_ITEM_RGB_BRIGHTNESS,
	SETTINGS_ITEM_FACTORY_RESET,
	SETTINGS_ITEM_EXIT,
	SETTINGS_ITEM_COUNT,
};

static const char *const settings_labels[SETTINGS_ITEM_COUNT] = {
	[SETTINGS_ITEM_MODE] = "Mode",
	[SETTINGS_ITEM_RGB_BRIGHTNESS] = "RGB Brightness",
	[SETTINGS_ITEM_FACTORY_RESET] = "Factory Reset",
	[SETTINGS_ITEM_EXIT] = "Exit",
};

static struct status_screen_event_result result(bool consumed, enum status_screen_action action,
						int value)
{
	struct status_screen_event_result r = {
		.consumed = consumed,
		.action = action,
		.value = value,
	};

	return r;
}

static unsigned int wrap_focus(unsigned int focus, enum status_screen_input input)
{
	if (input == STATUS_SCREEN_INPUT_ROTATE_CW) {
		return (focus + 1U) % SETTINGS_ITEM_COUNT;
	}

	return (focus + SETTINGS_ITEM_COUNT - 1U) % SETTINGS_ITEM_COUNT;
}

static int clamp_brightness(int value)
{
	if (value < 0) {
		return 0;
	}
	if (value > 100) {
		return 100;
	}

	return value;
}

void status_screen_model_init(struct status_screen_model *model)
{
	if (model == NULL) {
		return;
	}

	model->page = STATUS_SCREEN_PAGE_HOME;
	model->settings_focus = SETTINGS_ITEM_MODE;
	model->rgb_brightness = 50;
	model->edit_candidate = 50;
	model->confirm_selected = false;
}

static struct status_screen_event_result handle_home(struct status_screen_model *model,
						     enum status_screen_input input)
{
	if (input == STATUS_SCREEN_INPUT_LONG_PRESS) {
		model->page = STATUS_SCREEN_PAGE_SETTINGS;
		model->settings_focus = SETTINGS_ITEM_MODE;
		return result(true, STATUS_SCREEN_ACTION_ENTER_SETTINGS, 0);
	}

	if (input == STATUS_SCREEN_INPUT_ROTATE_CW ||
	    input == STATUS_SCREEN_INPUT_ROTATE_CCW) {
		return result(false, STATUS_SCREEN_ACTION_HID_PASSTHROUGH, 0);
	}

	return result(false, STATUS_SCREEN_ACTION_NONE, 0);
}

static struct status_screen_event_result handle_settings(struct status_screen_model *model,
							 enum status_screen_input input)
{
	if (input == STATUS_SCREEN_INPUT_LONG_PRESS) {
		model->page = STATUS_SCREEN_PAGE_HOME;
		return result(true, STATUS_SCREEN_ACTION_EXIT_HOME, 0);
	}

	if (input == STATUS_SCREEN_INPUT_ROTATE_CW ||
	    input == STATUS_SCREEN_INPUT_ROTATE_CCW) {
		model->settings_focus = wrap_focus(model->settings_focus, input);
		return result(true, STATUS_SCREEN_ACTION_NONE, 0);
	}

	if (input != STATUS_SCREEN_INPUT_PRESS) {
		return result(true, STATUS_SCREEN_ACTION_NONE, 0);
	}

	switch (model->settings_focus) {
	case SETTINGS_ITEM_RGB_BRIGHTNESS:
		model->page = STATUS_SCREEN_PAGE_EDIT;
		model->edit_candidate = model->rgb_brightness;
		return result(true, STATUS_SCREEN_ACTION_NONE, 0);
	case SETTINGS_ITEM_FACTORY_RESET:
		model->page = STATUS_SCREEN_PAGE_CONFIRM;
		model->confirm_selected = false;
		return result(true, STATUS_SCREEN_ACTION_NONE, 0);
	case SETTINGS_ITEM_EXIT:
		model->page = STATUS_SCREEN_PAGE_HOME;
		return result(true, STATUS_SCREEN_ACTION_EXIT_HOME, 0);
	case SETTINGS_ITEM_MODE:
	default:
		return result(true, STATUS_SCREEN_ACTION_NONE, 0);
	}
}

static struct status_screen_event_result handle_edit(struct status_screen_model *model,
						     enum status_screen_input input)
{
	if (input == STATUS_SCREEN_INPUT_LONG_PRESS) {
		model->edit_candidate = model->rgb_brightness;
		model->page = STATUS_SCREEN_PAGE_SETTINGS;
		return result(true, STATUS_SCREEN_ACTION_CANCEL_EDIT, 0);
	}

	if (input == STATUS_SCREEN_INPUT_ROTATE_CW) {
		model->edit_candidate = clamp_brightness(model->edit_candidate + 10);
		return result(true, STATUS_SCREEN_ACTION_NONE, model->edit_candidate);
	}

	if (input == STATUS_SCREEN_INPUT_ROTATE_CCW) {
		model->edit_candidate = clamp_brightness(model->edit_candidate - 10);
		return result(true, STATUS_SCREEN_ACTION_NONE, model->edit_candidate);
	}

	if (input == STATUS_SCREEN_INPUT_PRESS) {
		model->rgb_brightness = model->edit_candidate;
		model->page = STATUS_SCREEN_PAGE_SETTINGS;
		return result(true, STATUS_SCREEN_ACTION_APPLY_RGB_BRIGHTNESS,
			      model->rgb_brightness);
	}

	return result(true, STATUS_SCREEN_ACTION_NONE, 0);
}

static struct status_screen_event_result handle_confirm(struct status_screen_model *model,
							enum status_screen_input input)
{
	if (input == STATUS_SCREEN_INPUT_ROTATE_CW ||
	    input == STATUS_SCREEN_INPUT_ROTATE_CCW) {
		model->confirm_selected = !model->confirm_selected;
		return result(true, STATUS_SCREEN_ACTION_NONE, 0);
	}

	if (input == STATUS_SCREEN_INPUT_LONG_PRESS) {
		model->confirm_selected = false;
		model->page = STATUS_SCREEN_PAGE_SETTINGS;
		return result(true, STATUS_SCREEN_ACTION_CANCEL_CONFIRM, 0);
	}

	if (input == STATUS_SCREEN_INPUT_PRESS) {
		bool confirmed = model->confirm_selected;

		model->confirm_selected = false;
		model->page = STATUS_SCREEN_PAGE_SETTINGS;
		return result(true,
			      confirmed ? STATUS_SCREEN_ACTION_FACTORY_RESET_CONFIRMED :
					  STATUS_SCREEN_ACTION_CANCEL_CONFIRM,
			      0);
	}

	return result(true, STATUS_SCREEN_ACTION_NONE, 0);
}

struct status_screen_event_result status_screen_model_handle_input(
	struct status_screen_model *model, enum status_screen_input input)
{
	if (model == NULL) {
		return result(false, STATUS_SCREEN_ACTION_NONE, 0);
	}

	switch (model->page) {
	case STATUS_SCREEN_PAGE_HOME:
		return handle_home(model, input);
	case STATUS_SCREEN_PAGE_SETTINGS:
		return handle_settings(model, input);
	case STATUS_SCREEN_PAGE_EDIT:
		return handle_edit(model, input);
	case STATUS_SCREEN_PAGE_CONFIRM:
		return handle_confirm(model, input);
	default:
		status_screen_model_init(model);
		return result(false, STATUS_SCREEN_ACTION_NONE, 0);
	}
}

bool status_screen_model_ui_active(const struct status_screen_model *model)
{
	return model != NULL && model->page != STATUS_SCREEN_PAGE_HOME;
}

enum status_screen_page status_screen_model_current_page(const struct status_screen_model *model)
{
	return model == NULL ? STATUS_SCREEN_PAGE_HOME : model->page;
}

const char *status_screen_model_focused_label(const struct status_screen_model *model)
{
	if (model == NULL || model->settings_focus >= SETTINGS_ITEM_COUNT) {
		return "";
	}

	return settings_labels[model->settings_focus];
}

bool status_screen_model_visible_exit(const struct status_screen_model *model)
{
	return model != NULL && model->page != STATUS_SCREEN_PAGE_HOME;
}

const char *status_screen_model_edit_label(const struct status_screen_model *model)
{
	if (model == NULL || model->page != STATUS_SCREEN_PAGE_EDIT) {
		return "";
	}

	return "RGB Brightness";
}

int status_screen_model_edit_value(const struct status_screen_model *model)
{
	if (model == NULL) {
		return 0;
	}

	return model->page == STATUS_SCREEN_PAGE_EDIT ? model->edit_candidate :
						       model->rgb_brightness;
}

bool status_screen_model_confirm_selected(const struct status_screen_model *model)
{
	return model != NULL && model->confirm_selected;
}
