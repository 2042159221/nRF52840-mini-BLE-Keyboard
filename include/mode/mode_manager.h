#ifndef MODE_MANAGER_H
#define MODE_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

enum kb_mode {
    KB_MODE_USB = 0,
    KB_MODE_BLE,
    KB_MODE_24G_RESERVED,
};

int mode_manager_init(void);
enum kb_mode mode_manager_get_mode(void);
int mode_manager_set_mode(enum kb_mode mode);
enum kb_mode mode_manager_next_supported_mode(enum kb_mode current);

#ifdef __cplusplus
}
#endif

#endif