#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include "nvs_flash.h"
#include "esp_log.h"
#include "rgbw-wad.h"
#include "bh1750.h"
#include "web_server.h"

static const char *TAG = "LED_SYSTEM";

// Допоміжна для веселки
void hsv2rgb(float h, float s, float v, int* r, int* g, int* b) {
    float c = v * s; float x = c * (1 - fabs(fmod(h / 60.0, 2) - 1)); float m = v - c;
    float rt, gt, bt;
    if(h<60){rt=c;gt=x;bt=0;} else if(h<120){rt=x;gt=c;bt=0;} else if(h<180){rt=0;gt=c;bt=x;}
    else if(h<240){rt=0;gt=x;bt=c;} else if(h<300){rt=x;gt=0;bt=c;} else{rt=c;gt=0;bt=x;}
    *r=(rt+m)*255; *g=(gt+m)*255; *b=(bt+m)*255;
}

// --- ТАСКА ---
void master_task(void *pvParameter) {
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
                
                target_br = 1.0 - (smooth_lux / 10.0);
                
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

void app_main(void) {
    // Ініціалізація
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    rgb_driver_init();
    bh1750_init(); // I2C
    web_init_system(); // Wi-Fi

    // Запуск
    xTaskCreate(master_task, "master", 4096, NULL, 5, NULL);
    
    printf("MEGACOOL SMOOTH SYSTEM STARTED!\n");
}