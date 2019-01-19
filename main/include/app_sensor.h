#ifndef _APP_SENSOR_H_
#define _APP_SENSOR_H_

#include "bme280.h"
#if __cplusplus
extern "C" {
#endif

bool setPowerBoostKeepOn(bool en);
void app_sensor_init(struct bme280_dev *dev);
void app_sensor_deinit(struct bme280_dev *dev);

#if __cplusplus
}
#endif

#endif /*_APP_SENSOR_H_*/