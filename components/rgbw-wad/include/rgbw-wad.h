#ifndef RGB_DRIVER_H
#define RGB_DRIVER_H

#include <stdint.h>


#define PIN_R 27
#define PIN_G 12
#define PIN_B 14
#define PIN_W 13


#define MAX_DUTY 1000 


// Ініціалізація драйвера (PWM, таймери)
void rgb_driver_init(void);

// Встановити колір (0.0 - 1.0 для кожного каналу)
void rgb_set_color(float r, float g, float b);

// Допоміжна функція: перетворити HSV в RGB
void hsv_to_rgb(float h, float s, float v, float *r, float *g, float *b);

#endif