#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "ds3231.h"

static const char *TAG = "APP";

// I2C Config
#define I2C_MASTER_SCL_IO           22
#define I2C_MASTER_SDA_IO           21
#define I2C_MASTER_NUM              0
#define I2C_MASTER_FREQ_HZ          100000

#define GPIO_INPUT_SQW      4

static QueueHandle_t gpio_evt_queue = NULL;

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

void app_main(void)
{
    gpio_config_t io_conf = {};
    io_conf.pull_up_en = 1;
    io_conf.intr_type = GPIO_INTR_NEGEDGE; 
    io_conf.pin_bit_mask = (1ULL << GPIO_INPUT_SQW);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 0; 
    io_conf.pull_down_en = 0;
    gpio_config(&io_conf);

    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    gpio_install_isr_service(0);
    gpio_isr_handler_add(GPIO_INPUT_SQW, gpio_isr_handler, (void*) GPIO_INPUT_SQW);

    ds3231_config_t i2c_cfg = {
        .port = I2C_MASTER_NUM,
        .sda_pin = I2C_MASTER_SDA_IO,
        .scl_pin = I2C_MASTER_SCL_IO,
        .clk_speed = I2C_MASTER_FREQ_HZ
    };

    ESP_ERROR_CHECK(ds3231_init(&i2c_cfg));
    ESP_LOGI(TAG, "DS3231 Initialized");

	ESP_ERROR_CHECK(ds3231_enable_sqw(I2C_MASTER_NUM));
    ESP_LOGI(TAG, "SQW enabled at 1Hz");
    
    ds3231_time_t now;
    float temp;
    uint32_t io_num;

    ESP_LOGI(TAG, "Waiting for DS3231 interrupts...");
	/* ds3231_time_t set_t = {

.seconds = 0, .minutes = 4, .hours = 1,

.day_of_week = 2, .day = 18, .month = 11, .year = 2025

};

ESP_ERROR_CHECK(ds3231_set_time(I2C_MASTER_NUM, &set_t));

ESP_LOGI(TAG, "Time set successfully");*/
    while (1) {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
                        
            if (ds3231_get_time(I2C_MASTER_NUM, &now) == ESP_OK) {
                ESP_LOGI(TAG, "Time: %02d:%02d:%02d, Date: %02d/%02d/%d",
					now.hours, now.minutes, now.seconds,
					now.day, now.month, now.year);
            } else {
                ESP_LOGE(TAG, "Failed to read time");
            }

            if (now.seconds == 0) {
                 if (ds3231_get_temp(I2C_MASTER_NUM, &temp) == ESP_OK) {
                    ESP_LOGI(TAG, "Temp: %.2f C", temp);
                }
            }
        }
    }
}