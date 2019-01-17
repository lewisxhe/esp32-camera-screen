#ifndef _APP_SD_H_
#define _APP_SD_H_

#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"

#if __cplusplus
extern "C" {
#endif

bool app_sd_init();



#if __cplusplus
}
#endif
#endif /*_APP_SD_H_*/