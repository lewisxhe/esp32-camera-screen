#include "app_sensor.h"
#include <stdio.h>
#include "driver/i2c.h"
#include "esp_log.h"
#include "board_def.h"

#define TAG "[SENSOR]"

#define I2C_SCL_PIN_NUM     I2C_SCL          /*!< gpio number for I2C master clock */
#define I2C_SDA_PIN_NUM     I2C_SDA          /*!< gpio number for I2C master data  */

#define I2C_MASTER_NUM I2C_NUM_1    /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DISABLE 0 /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0 /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ 100000   /*!< I2C master clock frequency */

#define IP5306_ADDR         0X75
#define IP5306_REG_SYS_CTL0 0x00

int8_t user_i2c_read(uint8_t dev_id, uint8_t reg_addr, uint8_t *reg_data, uint16_t len);
int8_t user_i2c_write(uint8_t dev_id, uint8_t reg_addr, uint8_t *reg_data, uint16_t len);


int i2c_write(uint8_t dev_id, uint8_t data)
{
    esp_err_t err; /* Return 0 for Success, non-zero for failure */
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, dev_id << 1 | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, data, true);
    // i2c_master_write(cmd, reg_data, len, true);
    i2c_master_stop(cmd);
    err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return err;
}

// I (229) camera: Detected camera at address=0x30
// I (240) camera: Camera PID=0x26 VER=0x42 MIDL=0x7f MIDH=0xa2

esp_err_t csccb_read_buff(uint8_t dev_id, uint8_t reg_addr, uint8_t *reg_data, uint16_t len)
{
    esp_err_t err = 0; /* Return 0 for Success, non-zero for failure */
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    err = i2c_master_start(cmd);
    i2c_master_write_byte(cmd, dev_id << 1 | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);

    err = i2c_master_start(cmd);

    err = i2c_master_write_byte(cmd, dev_id << 1 | I2C_MASTER_READ, true);
    ERROR_CHECK(err);

    // I2C_MASTER_ACK
    // I2C_MASTER_NACK
    // I2C_MASTER_LAST_NACK
    err = i2c_master_read(cmd, reg_data, len, I2C_MASTER_LAST_NACK);
    ERROR_CHECK(err);

    err = i2c_master_stop(cmd);
    ERROR_CHECK(err);

    err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    ERROR_CHECK(err);

    i2c_cmd_link_delete(cmd);
    return err;
}

int csccb_write_buf(uint8_t dev_id, uint8_t *data, uint8_t len)
{
    int ret; /* Return 0 for Success, non-zero for failure */
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, dev_id << 1 | I2C_MASTER_WRITE, true);

    for (int i = 0; i < len; ++i) {
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, data[i], true);
        i2c_master_stop(cmd);
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

uint8_t csccb_read(uint8_t addr, uint8_t reg)
{
    uint8_t data = 0;
    esp_err_t err =  csccb_read_buff(addr, reg, &data, 1);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "sccb_read fail");
    }
    ESP_LOGI(TAG, "[0x%x]->Read : 0x%x", addr, data);
    return data;
}

uint8_t csccb_write(uint8_t addr, uint8_t reg, uint8_t data)
{
    uint8_t buff[2];
    buff[0] = reg;
    buff[1] = data;
    // esp_err_t err =  csccb_write_buf(addr, buff, 2);
    // if (err != ESP_OK) {
    //     ESP_LOGE(TAG, "csccb_write err : 0x%x", err);
    // }

    int ret; /* Return 0 for Success, non-zero for failure */
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, addr << 1 | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write(cmd, &data, 1, true);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

uint8_t csccb_probe()
{
    uint8_t addr = 0, reg = 0;

    for (uint8_t i = 0; i < 127; ++i) {
        if (i2c_write(i, reg) == ESP_OK) {
            addr = i;
            break;
        }
        if (i != 126) {
            vTaskDelay(20 / portTICK_PERIOD_MS);
        }
    }
    ESP_LOGE(TAG, "Found addr :0x%x", addr);
    return addr;
}



void user_delay_ms(uint32_t period)
{
    vTaskDelay(period / portTICK_PERIOD_MS);
}


int8_t user_i2c_read(uint8_t dev_id, uint8_t reg_addr, uint8_t *reg_data, uint16_t len)
{
    int ret = 0; /* Return 0 for Success, non-zero for failure */
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, dev_id << 1 | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, dev_id << 1 | I2C_MASTER_READ, true);
    i2c_master_read(cmd, reg_data, len, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

int8_t user_i2c_write(uint8_t dev_id, uint8_t reg_addr, uint8_t *reg_data, uint16_t len)
{
    int ret; /* Return 0 for Success, non-zero for failure */
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, dev_id << 1 | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_write(cmd, reg_data, len, true);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

void print_sensor_data(struct bme280_data *comp_data)
{
#ifdef BME280_FLOAT_ENABLE
    ESP_LOGI(TAG, "Temperature:%0.2f, Pressure:%0.2f, Humidity:%0.2f\r\n", comp_data->temperature, comp_data->pressure, comp_data->humidity);
#else
    ESP_LOGI(TAG, "Temperature: %d, Pressure:%d, Humidity:%d\r\n", comp_data->temperature, comp_data->pressure, comp_data->humidity);
#endif
}

int8_t stream_sensor_data_normal_mode(struct bme280_dev *dev)
{
    int8_t rslt;
    uint8_t settings_sel;

    /* Recommended mode of operation: Indoor navigation */
    dev->settings.osr_h = BME280_OVERSAMPLING_1X;
    dev->settings.osr_p = BME280_OVERSAMPLING_16X;
    dev->settings.osr_t = BME280_OVERSAMPLING_2X;
    dev->settings.filter = BME280_FILTER_COEFF_16;
    dev->settings.standby_time = BME280_STANDBY_TIME_62_5_MS;

    settings_sel = BME280_OSR_PRESS_SEL;
    settings_sel |= BME280_OSR_TEMP_SEL;
    settings_sel |= BME280_OSR_HUM_SEL;
    settings_sel |= BME280_STANDBY_SEL;
    settings_sel |= BME280_FILTER_SEL;
    rslt = bme280_set_sensor_settings(settings_sel, dev);
    rslt = bme280_set_sensor_mode(BME280_NORMAL_MODE, dev);

    return rslt;
}


void app_sensor_deinit(struct bme280_dev *dev)
{
    bme280_set_sensor_mode(BME280_SLEEP_MODE, dev);
    esp_err_t err =  i2c_driver_delete((i2c_port_t)I2C_MASTER_NUM);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Remove i2c driver fail");
    }
}


void hal_i2c_init()
{
    i2c_port_t  i2c_master_port = I2C_MASTER_NUM;

    i2c_config_t conf;

    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = (gpio_num_t)I2C_SDA_PIN_NUM;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = (gpio_num_t)I2C_SCL_PIN_NUM;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;

    i2c_param_config(i2c_master_port, &conf);

    esp_err_t  err = i2c_driver_install(i2c_master_port, conf.mode,
                                        I2C_MASTER_RX_BUF_DISABLE,
                                        I2C_MASTER_TX_BUF_DISABLE, 0);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "hal_i2c_init fail");
    }
}

void app_sensor_init(struct bme280_dev *dev)
{
    int8_t rslt = BME280_OK;

    dev->dev_id = BME280_I2C_ADDR_SEC;
    dev->intf = BME280_I2C_INTF;
    dev->read = user_i2c_read;
    dev->write = user_i2c_write;
    dev->delay_ms = user_delay_ms;

    rslt = bme280_init(dev);
    if (rslt != BME280_OK) {
        ESP_LOGI(TAG, "BME280 INIT FAIL rslt:%d\n", rslt);
        return;
    }
    stream_sensor_data_normal_mode(dev);
}


bool setPowerBoostKeepOn(bool en)
{
    uint8_t val  = en ? 0x37 : 0x35;
    return user_i2c_write(IP5306_ADDR, IP5306_REG_SYS_CTL0, &val, 1) == ESP_OK;
}
