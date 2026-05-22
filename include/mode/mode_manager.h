#ifndef MODE_MANAGER_H
#define MODE_MANAGER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum kb_mode {
    KB_MODE_USB = 0,
    KB_MODE_BLE,
    KB_MODE_24G_RESERVED,
};

typedef void (*mode_manager_event_handler_t)(enum kb_mode mode,
                         enum kb_mode previous_mode,
                         bool initial,
                         void *user_data);

int mode_manager_init(void);
enum kb_mode mode_manager_get_mode(void);
int mode_manager_set_mode(enum kb_mode mode);
int mode_manager_register_listener(mode_manager_event_handler_t handler, void *user_data);
int mode_manager_unregister_listener(mode_manager_event_handler_t handler, void *user_data);
enum kb_mode mode_manager_next_supported_mode(enum kb_mode current);

#ifdef __cplusplus
}
#endif

#endif