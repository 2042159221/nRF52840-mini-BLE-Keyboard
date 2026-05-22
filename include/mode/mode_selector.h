#ifndef MODE_SELECTOR_H
#define MODE_SELECTOR_H

#include <stdint.h>

#include <mode/mode_manager.h>

#ifdef __cplusplus
extern "C" {
#endif

enum kb_mode mode_selector_classify_mv(int32_t mv, enum kb_mode current_mode);

#ifdef __cplusplus
}
#endif

#endif
