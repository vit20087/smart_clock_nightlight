#include "ds3231.h"
#include "esp_log.h"

#define TAG "DS3231"
#define I2C_TIMEOUT_MS 1000
#define DS3231_REG_CONTROL  0x0E

// Допоміжні функції BCD
static uint8_t dec2bcd(uint8_t val) {
    return ((val / 10) << 4) + (val % 10);
}

static uint8_t bcd2dec(uint8_t val) {
    return ((val >> 4) * 10) + (val & 0x0F);
}

esp_err_t ds3231_init(ds3231_config_t *cfg) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = cfg->sda_pin,
        .scl_io_num = cfg->scl_pin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = cfg->clk_speed,
    };
    
    esp_err_t err = i2c_param_config(cfg->port, &conf);
    if (err != ESP_OK) return err;
    
    return i2c_driver_install(cfg->port, conf.mode, 0, 0, 0);
}

esp_err_t ds3231_set_time(i2c_port_t port, ds3231_time_t *time) {
    uint8_t data[8]; // 1 байт адреси регістра + 7 байт даних
    
    data[0] = 0x00; // Початковий регістр (Секунди)
    data[1] = dec2bcd(time->seconds);
    data[2] = dec2bcd(time->minutes);
    data[3] = dec2bcd(time->hours); // Припускаємо 24-годинний формат
    data[4] = dec2bcd(time->day_of_week);
    data[5] = dec2bcd(time->day);
    data[6] = dec2bcd(time->month);
    data[7] = dec2bcd(time->year - 2000);

    return i2c_master_write_to_device(port, DS3231_ADDR, data, 8, I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
}

esp_err_t ds3231_get_time(i2c_port_t port, ds3231_time_t *time) {
    uint8_t reg = 0x00;
    uint8_t data[7];
    
    // Записуємо адресу регістра (0x00) і читаємо 7 байт
    esp_err_t err = i2c_master_write_read_device(port, DS3231_ADDR, &reg, 1, data, 7, I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
    
    if (err == ESP_OK) {
        time->seconds = bcd2dec(data[0]);
        time->minutes = bcd2dec(data[1]);
        // Маскуємо 0x3F, щоб прибрати біти режиму 12/24
        time->hours   = bcd2dec(data[2] & 0x3F); 
        time->day_of_week = bcd2dec(data[3]);
        time->day     = bcd2dec(data[4]);
        time->month   = bcd2dec(data[5] & 0x1F); // Маскуємо біт століття, якщо він є
        time->year    = bcd2dec(data[6]) + 2000;
    }
    return err;
}

esp_err_t ds3231_get_temp(i2c_port_t port, float *temp) {
    uint8_t reg = 0x11; // Регістр температури (MSB)
    uint8_t data[2];
    
    esp_err_t err = i2c_master_write_read_device(port, DS3231_ADDR, &reg, 1, data, 2, I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
    
    if (err == ESP_OK) {
        int16_t temp_raw = (int16_t)((data[0] << 8) | data[1]);
        *temp = (float)(temp_raw >> 6) * 0.25f;
    }
    return err;
}

esp_err_t ds3231_enable_sqw(i2c_port_t port) {
    uint8_t reg = DS3231_REG_CONTROL;
    uint8_t ctrl_reg_val;

    // 1. Спочатку читаємо поточне значення регістра, щоб не збити інші налаштування
    // (наприклад, bit 7 - робота осцилятора)
    esp_err_t err = i2c_master_write_read_device(port, DS3231_ADDR, &reg, 1, &ctrl_reg_val, 1, I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
    if (err != ESP_OK) return err;

    // 2. Модифікуємо біти для 1Hz SQW
    // INTCN (bit 2) = 0  -> Вмикає Square Wave
    // RS2 (bit 4)   = 0  -> Частота 1Hz
    // RS1 (bit 3)   = 0  -> Частота 1Hz
    
    // Використовуємо побітове "І" з інверсією маски, щоб обнулити потрібні біти
    // Маска 0b00011100 = 0x1C (це біти RS2, RS1, INTCN)
    ctrl_reg_val &= ~0x1C; 

    // Щоб SQW працював навіть на батарейці (коли VCC відключено),
    // розкоментуй рядок нижче (BBSQW bit 6):
    // ctrl_reg_val |= 0x40; 

    // 3. Записуємо нове значення назад
    uint8_t data[2] = {DS3231_REG_CONTROL, ctrl_reg_val};
    return i2c_master_write_to_device(port, DS3231_ADDR, data, 2, I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
}