
#include "app_sd.h"
#include "board_def.h"
#include "sdmmc_cmd.h"
#define TAG "[SD]"

bool app_sd_init()
{
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    sdspi_slot_config_t slot_config = SDSPI_SLOT_CONFIG_DEFAULT();
    slot_config.gpio_miso = (gpio_num_t)SD_MISO;
    slot_config.gpio_mosi = (gpio_num_t)SD_MOSI;
    slot_config.gpio_sck  = (gpio_num_t)SD_CLK;
    slot_config.gpio_cs   = (gpio_num_t)SD_CS;


    gpio_set_pull_mode((gpio_num_t)SD_MISO, GPIO_PULLUP_ONLY);   
    gpio_set_pull_mode((gpio_num_t)SD_MOSI, GPIO_PULLUP_ONLY);    
    gpio_set_pull_mode((gpio_num_t)SD_CLK, GPIO_PULLUP_ONLY);   
    gpio_set_pull_mode((gpio_num_t)SD_CS, GPIO_PULLUP_ONLY);   


    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_card_t *card;
    esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                     "If you want the card to be formatted, set format_if_mount_failed = true.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return false;
    }

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);

    return true;
}