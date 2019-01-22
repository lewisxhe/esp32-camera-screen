#include "pcf8574.h"
#include "app_sensor.h"
#include "esp_log.h"

#define PCF8574_ADDRESS 0x3F

static uint8_t _pinMask;

void pcf8574_set(uint8_t i)
{
    if (i2c_write(PCF8574_ADDRESS, i) != 0) {
        ESP_LOGI("PCF", "PCF8574 Fail");
    }
}




