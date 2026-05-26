#include <display/status_screen.h>

bool status_screen_input_route(struct status_screen_model *model, enum status_screen_input input,
			       struct status_screen_event_result *result)
{
	struct status_screen_event_result event_result =
		status_screen_model_handle_input(model, input);

	if (result != 0) {
		*result = event_result;
	}

	return event_result.consumed;
}
