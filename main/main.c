#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "ds3231.h"
#include "u8g2.h"
#include "u8g2_esp32_hal.h"
#include <math.h>
#include "nvs_flash.h"
#include "rgbw-wad.h"
#include "bh1750.h"
#include "web_server.h" 
 
static const char *TAG = "CLOCK";

#define I2C_MASTER_SCL_IO           22
#define I2C_MASTER_SDA_IO           21
#define I2C_MASTER_NUM       I2C_NUM_1 
#define I2C_MASTER_FREQ_HZ          400000
#define GPIO_INPUT_SQW      4
#define GPIO_INPUT_RADAR    19
#define SCREEN_TIMEOUT_SEC  30

static QueueHandle_t gpio_evt_queue = NULL;

void hsv2rgb(float h, float s, float v, int* r, int* g, int* b) {
    float c = v * s; float x = c * (1 - fabs(fmod(h / 60.0, 2) - 1)); float m = v - c;
    float rt, gt, bt;
    if(h<60){rt=c;gt=x;bt=0;} else if(h<120){rt=x;gt=c;bt=0;} else if(h<180){rt=0;gt=c;bt=x;}
    else if(h<240){rt=0;gt=x;bt=c;} else if(h<300){rt=x;gt=0;bt=c;} else{rt=c;gt=0;bt=x;}
    *r=(rt+m)*255; *g=(gt+m)*255; *b=(bt+m)*255;
}

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

void clock_task(void *pvParameters) {
	// Змінні задачі (живуть в її стеку)
	gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_NEGEDGE; 
    io_conf.pin_bit_mask = (1ULL << GPIO_INPUT_SQW);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1; 
    io_conf.pull_down_en = 0;
    gpio_config(&io_conf);
    
    gpio_config_t radar_conf = {};
    radar_conf.intr_type = GPIO_INTR_POSEDGE;
    radar_conf.pin_bit_mask = (1ULL << GPIO_INPUT_RADAR);
    radar_conf.mode = GPIO_MODE_INPUT;
    radar_conf.pull_up_en = 0;
    radar_conf.pull_down_en = 1;
    gpio_config(&radar_conf);

    gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    gpio_isr_handler_add(GPIO_INPUT_SQW, gpio_isr_handler, (void*) GPIO_INPUT_SQW);
	gpio_isr_handler_add(GPIO_INPUT_RADAR, gpio_isr_handler, (void*) GPIO_INPUT_RADAR);

	u8g2_esp32_hal_t u8g2_esp32_hal = U8G2_ESP32_HAL_DEFAULT;
    u8g2_esp32_hal.bus.i2c.sda = I2C_MASTER_SDA_IO;
    u8g2_esp32_hal.bus.i2c.scl = I2C_MASTER_SCL_IO;
    u8g2_esp32_hal_init(u8g2_esp32_hal);

    u8g2_t u8g2; 
    u8g2_Setup_sh1106_i2c_128x64_noname_f(&u8g2, U8G2_R0, u8g2_esp32_i2c_byte_cb, u8g2_esp32_gpio_and_delay_cb);
    u8g2_SetI2CAddress(&u8g2, 0x3C * 2); 
    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0); 
    
    ESP_LOGI(TAG, "Enabling SQW on I2C Port %d...", I2C_MASTER_NUM);
    
    vTaskDelay(pdMS_TO_TICKS(100));

    esp_err_t err = ds3231_enable_sqw(I2C_MASTER_NUM);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "SQW enabled at 1Hz");
    } else {
        ESP_LOGE(TAG, "Failed to enable SQW: %s", esp_err_to_name(err));
    }

    ds3231_time_t now;
    float temp;
    uint32_t io_num;
    char time_str[32]; 
    char date_str[32];
    static float current_temp = 0.0;
    char temp_str[32] = "--.- C";
	
    int screen_timer = SCREEN_TIMEOUT_SEC;
    bool is_screen_on = true;
    bool need_redraw = true;
    
    while (1) {
		if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            if (io_num == GPIO_INPUT_SQW) {
				if (ds3231_get_time(I2C_MASTER_NUM, &now) == ESP_OK) {
                ESP_LOGI(TAG, "Time: %02d:%02d:%02d, Date: %02d/%02d/%d",
					now.hours, now.minutes, now.seconds,
					now.day, now.month, now.year);
					 sprintf(time_str, "%02d:%02d:%02d", now.hours, now.minutes, now.seconds);
            sprintf(date_str, "%02d.%02d.%04d", now.day, now.month, now.year);
				
            } else {
                ESP_LOGE(TAG, "Failed to read time");
            }

            if (now.seconds % 30  == 0) {
                 if (ds3231_get_temp(I2C_MASTER_NUM, &temp) == ESP_OK) {
                    ESP_LOGI(TAG, "Temp: %.2f C", temp);
                    current_temp = temp;
					sprintf(temp_str, "%.1f C", current_temp);
                }
            }
            
            if (gpio_get_level(GPIO_INPUT_RADAR) == 1) {
				screen_timer = SCREEN_TIMEOUT_SEC;
				ESP_LOGI(TAG, "Motion Detected by polling");
                if (!is_screen_on) {
                    ESP_LOGI(TAG, "Motion Detected by polling, screen_off! Waking up...");
                    u8g2_SetPowerSave(&u8g2, 0);
                    is_screen_on = true;                    
                }
			} else {
				if (is_screen_on) {
                        screen_timer--;
                        ESP_LOGI(TAG, "Timeout -> Sleep %d", screen_timer); 
                        if (screen_timer <= 0) {
                            ESP_LOGI(TAG, "Timeout -> Sleep");
                            u8g2_SetPowerSave(&u8g2, 1);
                            is_screen_on = false;
                        }
                    }
			}
                   
                need_redraw = is_screen_on;
                
        } else if (io_num == GPIO_INPUT_RADAR) {
                screen_timer = SCREEN_TIMEOUT_SEC;
                if (!is_screen_on) {
                    ESP_LOGI(TAG, "Motion Detected! Waking up...");
                    u8g2_SetPowerSave(&u8g2, 0);
                    is_screen_on = true;
                    
                    ds3231_get_time(I2C_MASTER_NUM, &now);
            sprintf(time_str, "%02d:%02d:%02d", now.hours, now.minutes, now.seconds);
            sprintf(date_str, "%02d.%02d.%04d", now.day, now.month, now.year);
                }
                need_redraw = true;

            }
			}       
			



		if (is_screen_on && need_redraw) {
		u8g2_ClearBuffer(&u8g2); 
        u8g2_DrawFrame(&u8g2, 0, 0, 128, 64);
        
        u8g2_SetFont(&u8g2, u8g2_font_6x10_tr); 
        int w_date = u8g2_GetStrWidth(&u8g2, date_str);
        u8g2_DrawStr(&u8g2, (128 - w_date) / 2, 11, date_str);
        u8g2_DrawHLine(&u8g2, 20, 13, 88); 

        u8g2_SetFont(&u8g2, u8g2_font_logisoso24_tr); 
        int w_time = u8g2_GetStrWidth(&u8g2, time_str);
        u8g2_DrawStr(&u8g2, (128 - w_time) / 2, 42, time_str);

        u8g2_SetFont(&u8g2, u8g2_font_ncenB12_tr); 
        int w_temp = u8g2_GetStrWidth(&u8g2, temp_str);
        u8g2_DrawStr(&u8g2, (128 - w_temp) / 2, 60, temp_str);

        u8g2_SendBuffer(&u8g2);
        need_redraw = false; 
		}
        // Обов'язкова затримка, щоб віддати час іншим
    }
    // Сюди код ніколи не має дійти. Якщо треба вийти — видали задачу:
    vTaskDelete(NULL);
}

void led_task(void *pvParameter) {
    float hue = 0;
    int breath = 0, dir = 1;
    
    // Поточна реальна яскравість
    float current_br = 1.0; 
    // Відфільтроване значення люксів
    float smooth_lux = -1.0; 

    int t_r, t_g, t_b, t_w; 

    while(1) {
        // 1. Отримуємо дані з Вебу
        web_get_target_color(&t_r, &t_g, &t_b, &t_w);
        bool is_auto = web_is_auto_mode();
        int mode = web_get_mode();
        int speed = web_get_speed();

        float target_br = 1.0; 

        if (is_auto) {
            float raw_lux = bh1750_read();
            
            if (raw_lux >= 0) {
                // Формула: Нове = 90% старого + 10% нового
                if (smooth_lux < 0) smooth_lux = raw_lux;
                else smooth_lux = (smooth_lux * 0.9) + (raw_lux * 0.1);

                web_update_lux_display(smooth_lux);
                
                target_br = 1.0 - (smooth_lux / 232.0);
                
                 if (target_br < 0.1) target_br = 0.0; 
                 if (target_br > 1.0) target_br = 1.0;
            }
        } else {
            // Ручний режим
            target_br = web_get_brightness() / 100.0;
        }

        float diff = target_br - current_br;
        
        if (fabs(diff) > 0.001) {
            current_br += diff * 0.05;
        } else {
            current_br = target_br;
        }

        // 4.ефекти
        int r=0, g=0, b=0, w=0;

        if (mode == 0) { // Static
            r = t_r; g = t_g; b = t_b; w = t_w;
        }
        else if (mode == 1) { // Rainbow
            hsv2rgb(hue, 1.0, 1.0, &r, &g, &b);
            hue += 0.5; if(hue>=360) hue=0;
            w = 0;
        }
        else if (mode == 2) { // Strobe
            static int st=0; st=!st;
            if(!st) { r=t_r; g=t_g; b=t_b; w=t_w; }
        }
        else if (mode == 3) { // Breath
            float f = breath/255.0;
            r=t_r*f; g=t_g*f; b=t_b*f; w=t_w*f;
            breath += 5*dir;
            if(breath>=255 || breath<=0) dir*=-1;
        }

        // 5. Накладання згладженої яскравості
        r = (int)(r * current_br);
        g = (int)(g * current_br);
        b = (int)(b * current_br);
        w = (int)(w * current_br);

        // 6. Вивід
        rgb_set_color(r/255.0, g/255.0, b/255.0, w/255.0);
        vTaskDelay(pdMS_TO_TICKS(mode==2 ? speed : 20));
    }
}

void app_main(void)
{
	gpio_evt_queue = xQueueCreate(20, sizeof(uint32_t));
	esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    rgb_driver_init();
    bh1750_init(I2C_MASTER_NUM); // I2C
    web_init_system(); // Wi-Fi

xTaskCreate(
		clock_task,          // Вказівник на функцію
        "clock_task",   // Ім'я (для налагодження)
        4096,             // Розмір стека (у байтах/словах). Для простих задач 2048-4096 ок.
        NULL,             // Параметри, які передаємо в задачу (зазвичай NULL)
        5,                // Пріоритет (0 - найнижчий, configMAX_PRIORITIES - найвищий)
        NULL              // Хендл (вказівник), якщо треба керувати задачею ззовні
    );
xTaskCreate(led_task, "led_task", 4096, NULL, 5, NULL);
    
}