#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#ifdef __cplusplus
extern "C"{
#endif

extern EventGroupHandle_t g_wifi_event_group;

void app_wifi_init();

#ifdef __cplusplus
}
#endif