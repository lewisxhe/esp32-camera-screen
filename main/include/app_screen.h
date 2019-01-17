#ifndef _APP_SCREEN_H_
#define _APP_SCREEN_H_

#include "iot_lcd.h"

#if __cplusplus
extern "C" {
#endif

#define JPG_IMAGE_LINE_BUF_SIZE 512

#define CENTER -9003
#define RIGHT  -9004
#define BOTTOM -9004

void app_lcd_init(bool isInitBus);

int TFT_bmp_image(int x, int y, uint8_t scale, char *fname, uint8_t *imgbuf, int size);

#if 0
void TFT_jpg_image(int x,
                   int y,
                   uint8_t scale,
                   uint8_t *buf,
                   uint32_t size);

#else
void TFT_jpg_image(int x,
                   int y,
                   uint8_t scale,
                   int s,
                   char *filename,
                   uint8_t *buf,
                   int size);
#endif
// void TFT_jpg_image(int x,
//                    int y,
//                    uint8_t scale,
//                    uint8_t *buf,
//                    uint32_t size);

#if __cplusplus
}
#endif
#endif /*_APP_SCREEN_H_*/