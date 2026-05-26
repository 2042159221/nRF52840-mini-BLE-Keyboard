#ifndef DISPLAY_STATUS_SCREEN_H_
#define DISPLAY_STATUS_SCREEN_H_

#include <display/status_screen_model.h>

#ifdef __cplusplus
extern "C" {
#endif

int status_screen_init(void);
bool status_screen_input_route(struct status_screen_model *model, enum status_screen_input input,
			       struct status_screen_event_result *result);

#ifdef __cplusplus
}
#endif

#endif
