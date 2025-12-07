#ifndef DS3231_H
#define DS3231_H

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c.h"

// Адреса I2C для DS3231
#define DS3231_ADDR 0x68

// Структура для часу
typedef struct {
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t day_of_week;
    uint8_t day;
    uint8_t month;
    uint16_t year;
} ds3231_time_t;

// Конфігурація I2C дескриптора
typedef struct {
    i2c_port_t port;
    int sda_pin;
    int scl_pin;
    uint32_t clk_speed;
} ds3231_config_t;

/**
 * @brief Ініціалізація I2C шини для DS3231
 */
esp_err_t ds3231_init(ds3231_config_t *cfg);

/**
 * @brief Встановлення часу
 */
esp_err_t ds3231_set_time(i2c_port_t port, ds3231_time_t *time);

/**
 * @brief Отримання часу
 */
esp_err_t ds3231_get_time(i2c_port_t port, ds3231_time_t *time);

/**
 * @brief Отримання температури
 */
esp_err_t ds3231_get_temp(i2c_port_t port, float *temp);

esp_err_t ds3231_enable_sqw(i2c_port_t port);

#endif // DS3231_H