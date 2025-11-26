#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rgbw-wad.h"

void app_main(void) {
    // 1. Ініціалізація
    rgb_driver_init();
    printf("WADZIK LED DRIVER RAINBOW SUPER MEGA COOL\n");

    float hue = 0.0;
    
    // 2. Головний цикл
    while (1) {
        float r, g, b;
        
        // Магія перетворення
        hsv_to_rgb(hue, 1.0, 1.0, &r, &g, &b);
        
        // Магія драйвера
        rgb_set_color(r, g, b);

        // Анімація
        hue += 0.2; 
        if (hue >= 360.0) hue = 0.0;

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}