#ifndef APP_CONFIG_STORE_H
#define APP_CONFIG_STORE_H

#include <stdbool.h>
#include <stddef.h>

#include <config/app_config.h>

#ifdef __cplusplus
extern "C" {
#endif

int app_config_store_load(void);
int app_config_store_save(void);
int app_config_store_factory_reset(void);

#if defined(UNIT_TEST)
void app_config_store_reset_for_test(void);
void app_config_store_seed_blob_for_test(const void *data, size_t len);
bool app_config_store_saved_for_test(struct app_config *out);
#endif

#ifdef __cplusplus
}
#endif

#endif
