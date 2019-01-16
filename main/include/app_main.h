#include "app_camera.h"
#include "app_httpserver.h"
#include "app_wifi.h"
#include "app_speech_srcif.h"

#define VERSION "0.9.0"

#define GPIO_LED_RED    21
#define GPIO_LED_WHITE  22
#define GPIO_BUTTON     0//34//15
#define I2C_SCL_PIN_NUM 22          /*!< gpio number for I2C master clock */
#define I2C_SDA_PIN_NUM 21          /*!< gpio number for I2C master data  */


#define IIS_SCLK    14//26  
#define IIS_LCLK    32  //ws
#define IIS_DSIN    -1
#define IIS_DOUT    33

#define TFT_MISO -1
#define TFT_MOSI 19
#define TFT_SCLK 21
#define TFT_CS 12 // Chip select control pin
#define TFT_DC 15 // Data Command control pin
#define TFT_BK  2
#define TFT_RST GPIO_NUM_MAX // Set TFT_RST to -1 if display RESET is connected to ESP32 board RST
#define TFT_WITDH   240
#define TFT_HEIGHT  240
typedef enum
{
    WAIT_FOR_WAKEUP,
    WAIT_FOR_CONNECT,
    START_DETECT,
    START_RECOGNITION,
    START_ENROLL,
    START_DELETE,

} en_fsm_state;

extern en_fsm_state g_state;
extern int g_is_enrolling;
extern int g_is_deleting;

