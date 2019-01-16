#ifndef _APP_SCREEN_H_
#define _APP_SCREEN_H_

#include "iot_lcd.h"
#if __cplusplus
extern "C" {
#endif

#define CENTER -9003
#define RIGHT  -9004
#define BOTTOM -9004

void app_lcd_init();
int TFT_bmp_image(int x, int y, uint8_t scale, char *fname, uint8_t *imgbuf, int size);

void TFT_jpg_image(int x,
                   int y,
                   uint8_t scale,
                   uint8_t *buf,
                   uint32_t size);

void TFT_jpg_image(int x,
                   int y,
                   uint8_t scale,
                   uint8_t *buf,
                   uint32_t size);

#if __cplusplus
}
#endif
#endif /*_APP_SCREEN_H_*/