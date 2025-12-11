#ifndef RGBW_WAD_H
#define RGBW_WAD_H
#include <stdint.h>

#define PIN_R 27
#define PIN_G 12
#define PIN_B 14
#define PIN_W 13

void rgb_driver_init(void);
void rgb_set_color(float r, float g, float b, float w);
#endif