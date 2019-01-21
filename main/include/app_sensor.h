#ifndef _APP_SENSOR_H_
#define _APP_SENSOR_H_

#include "bme280.h"
#if __cplusplus
extern "C" {
#endif
#define ERROR_CHECK(err) do{if(err!=ESP_OK)ESP_LOGE(TAG, "[%d]Error : 0x%x", __LINE__,err);}while(0)

void hal_i2c_init();

bool setPowerBoostKeepOn(bool en);
void app_sensor_init(struct bme280_dev *dev);
void app_sensor_deinit(struct bme280_dev *dev);


uint8_t csccb_probe();
uint8_t csccb_write(uint8_t addr, uint8_t reg, uint8_t data);
uint8_t csccb_read(uint8_t addr, uint8_t reg);

#if __cplusplus
}
#endif

#endif /*_APP_SENSOR_H_*/