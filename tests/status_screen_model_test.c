#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <display/status_screen_model.h>

static void test_home_rotation_passes_through(void)
{
	struct status_screen_model model;
	struct status_screen_event_result result;

	status_screen_model_init(&model);

	result = status_screen_model_handle_input(&model, STATUS_SCREEN_INPUT_ROTATE_CW);

	assert(!result.consumed);
	assert(result.action == STATUS_SCREEN_ACTION_HID_PASSTHROUGH);
	assert(!status_screen_model_ui_active(&model));
	assert(status_screen_model_current_page(&model) == STATUS_SCREEN_PAGE_HOME);
}

static void test_long_press_enters_settings(void)
{
	struct status_screen_model model;
	struct status_screen_event_result result;

	status_screen_model_init(&model);

	result = status_screen_model_handle_input(&model, STATUS_SCREEN_INPUT_LONG_PRESS);

	assert(result.consumed);
	assert(result.action == STATUS_SCREEN_ACTION_ENTER_SETTINGS);
	assert(status_screen_model_ui_active(&model));
	assert(status_screen_model_current_page(&model) == STATUS_SCREEN_PAGE_SETTINGS);
	assert(status_screen_model_visible_exit(&model));
}

static void test_settings_focus_and_exit(void)
{
	struct status_screen_model model;
	struct status_screen_event_result result;

	status_screen_model_init(&model);
	status_screen_model_handle_input(&model, STATUS_SCREEN_INPUT_LONG_PRESS);

	assert(strcmp(status_screen_model_focused_label(&model), "Mode") == 0);
	result = status_screen_model_handle_input(&model, STATUS_SCREEN_INPUT_ROTATE_CCW);

	assert(result.consumed);
	assert(strcmp(status_screen_model_focused_label(&model), "Exit") == 0);
	result = status_screen_model_handle_input(&model, STATUS_SCREEN_INPUT_PRESS);

	assert(result.consumed);
	assert(result.action == STATUS_SCREEN_ACTION_EXIT_HOME);
	assert(!status_screen_model_ui_active(&model));
	assert(status_screen_model_current_page(&model) == STATUS_SCREEN_PAGE_HOME);
}

static void test_edit_apply_produces_action(void)
{
	struct status_screen_model model;
	struct status_screen_event_result result;

	status_screen_model_init(&model);
	status_screen_model_handle_input(&model, STATUS_SCREEN_INPUT_LONG_PRESS);
	status_screen_model_handle_input(&model, STATUS_SCREEN_INPUT_ROTATE_CW);
	result = status_screen_model_handle_input(&model, STATUS_SCREEN_INPUT_PRESS);

	assert(result.consumed);
	assert(status_screen_model_current_page(&model) == STATUS_SCREEN_PAGE_EDIT);
	assert(strcmp(status_screen_model_edit_label(&model), "RGB Brightness") == 0);

	result = status_screen_model_handle_input(&model, STATUS_SCREEN_INPUT_ROTATE_CW);
	assert(result.consumed);
	assert(status_screen_model_edit_value(&model) == 60);

	result = status_screen_model_handle_input(&model, STATUS_SCREEN_INPUT_PRESS);
	assert(result.consumed);
	assert(result.action == STATUS_SCREEN_ACTION_APPLY_RGB_BRIGHTNESS);
	assert(result.value == 60);
	assert(status_screen_model_current_page(&model) == STATUS_SCREEN_PAGE_SETTINGS);
}

static void test_edit_cancel_does_not_apply(void)
{
	struct status_screen_model model;
	struct status_screen_event_result result;

	status_screen_model_init(&model);
	status_screen_model_handle_input(&model, STATUS_SCREEN_INPUT_LONG_PRESS);
	status_screen_model_handle_input(&model, STATUS_SCREEN_INPUT_ROTATE_CW);
	status_screen_model_handle_input(&model, STATUS_SCREEN_INPUT_PRESS);
	status_screen_model_handle_input(&model, STATUS_SCREEN_INPUT_ROTATE_CW);

	result = status_screen_model_handle_input(&model, STATUS_SCREEN_INPUT_LONG_PRESS);

	assert(result.consumed);
	assert(result.action == STATUS_SCREEN_ACTION_CANCEL_EDIT);
	assert(status_screen_model_edit_value(&model) == 50);
	assert(status_screen_model_current_page(&model) == STATUS_SCREEN_PAGE_SETTINGS);
}

static void test_confirm_cancel_and_confirm(void)
{
	struct status_screen_model model;
	struct status_screen_event_result result;

	status_screen_model_init(&model);
	status_screen_model_handle_input(&model, STATUS_SCREEN_INPUT_LONG_PRESS);
	status_screen_model_handle_input(&model, STATUS_SCREEN_INPUT_ROTATE_CW);
	status_screen_model_handle_input(&model, STATUS_SCREEN_INPUT_ROTATE_CW);

	result = status_screen_model_handle_input(&model, STATUS_SCREEN_INPUT_PRESS);
	assert(result.consumed);
	assert(status_screen_model_current_page(&model) == STATUS_SCREEN_PAGE_CONFIRM);
	assert(!status_screen_model_confirm_selected(&model));

	result = status_screen_model_handle_input(&model, STATUS_SCREEN_INPUT_PRESS);
	assert(result.consumed);
	assert(result.action == STATUS_SCREEN_ACTION_CANCEL_CONFIRM);
	assert(status_screen_model_current_page(&model) == STATUS_SCREEN_PAGE_SETTINGS);

	result = status_screen_model_handle_input(&model, STATUS_SCREEN_INPUT_PRESS);
	assert(result.consumed);
	assert(status_screen_model_current_page(&model) == STATUS_SCREEN_PAGE_CONFIRM);
	status_screen_model_handle_input(&model, STATUS_SCREEN_INPUT_ROTATE_CW);
	assert(status_screen_model_confirm_selected(&model));

	result = status_screen_model_handle_input(&model, STATUS_SCREEN_INPUT_PRESS);
	assert(result.consumed);
	assert(result.action == STATUS_SCREEN_ACTION_FACTORY_RESET_CONFIRMED);
	assert(status_screen_model_current_page(&model) == STATUS_SCREEN_PAGE_SETTINGS);
}

int main(void)
{
	test_home_rotation_passes_through();
	test_long_press_enters_settings();
	test_settings_focus_and_exit();
	test_edit_apply_produces_action();
	test_edit_cancel_does_not_apply();
	test_confirm_cancel_and_confirm();

	puts("status screen model tests passed");
	return 0;
}
