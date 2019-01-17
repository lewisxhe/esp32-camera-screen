

#define SPI_MISO    22
#define SPI_MOSI    19
#define SPI_SCLK    21

#define SD_CS       0
#define SD_MOSI     SPI_MOSI
#define SD_MISO     SPI_MISO
#define SD_CLK      SPI_SCLK

#define TFT_MISO    SPI_MISO
#define TFT_MOSI    SPI_MOSI
#define TFT_SCLK    SPI_SCLK
#define TFT_CS      12      // Chip select control pin
#define TFT_DC      15      // Data Command control pin
#define TFT_BK      2       // TFT backlight  pin
#define TFT_RST     GPIO_NUM_MAX    //No use


#define TFT_WITDH   240
#define TFT_HEIGHT  240

#define I2C_SDA     18
#define I2C_SCL     23

