#include "bh1750.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define BH1750_ADDR 0x23

static i2c_port_t bh1750_port = I2C_NUM_0; 

void bh1750_init(i2c_port_t port) {
    bh1750_port = port;
    ESP_LOGI("BH1750", "Initialized on port %d", bh1750_port);
}

float bh1750_read(void) {
    uint8_t data[2];
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BH1750_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0x10, true);
    i2c_master_stop(cmd);
    
    i2c_master_cmd_begin(bh1750_port, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    vTaskDelay(pdMS_TO_TICKS(150));

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BH1750_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, data, 2, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(bh1750_port, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_OK) return ((data[0] << 8) | data[1]) / 1.2;
    return -1.0;
}