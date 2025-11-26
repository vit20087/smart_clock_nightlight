#include <stdio.h>
#include <math.h>
#include "driver/ledc.h"
#include "rgbw-wad.h" // Підключаємо наш хедер

// Внутрішні налаштування PWM
#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_DUTY_RES           LEDC_TIMER_10_BIT 
#define LEDC_FREQUENCY          10000

// --- ПРИВАТНА ФУНКЦІЯ (Користувачу її знати не треба) ---
// Гамма-корекція
static uint32_t get_gamma_duty(float value_0_to_1) {
    float corrected = pow(value_0_to_1, 2.2); 
    uint32_t duty = (uint32_t)(corrected * MAX_DUTY);
    if (duty > MAX_DUTY) duty = MAX_DUTY;
    return duty;
}

// --- ПУБЛІЧНІ ФУНКЦІЇ ---

void rgb_driver_init(void) {
    // Налаштування таймера
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_MODE, .timer_num = LEDC_TIMER,
        .duty_resolution = LEDC_DUTY_RES, .freq_hz = LEDC_FREQUENCY, .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    // Налаштування каналів
    int pins[4] = {PIN_R, PIN_G, PIN_B, PIN_W};
    for(int i=0; i<4; i++){
        ledc_channel_config_t ledc_channel = {
            .speed_mode = LEDC_MODE, .channel = (ledc_channel_t)i,
            .timer_sel = LEDC_TIMER, .intr_type = LEDC_INTR_DISABLE,
            .gpio_num = pins[i], .duty = 0, .hpoint = 0
        };
        ledc_channel_config(&ledc_channel);
    }
}

void rgb_set_color(float r, float g, float b) {
    // R
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, get_gamma_duty(r));
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_0);
    // G
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, get_gamma_duty(g));
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_1);
    // B
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_2, get_gamma_duty(b));
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_2);
    
    // W (Білий) - поки вимкнений, або можеш додати логіку
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_3, 0); 
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_3);
}

void hsv_to_rgb(float h, float s, float v, float *r, float *g, float *b) {
    float c = v * s;
    float x = c * (1 - fabs(fmod(h / 60.0, 2) - 1));
    float m = v - c;
    float r_temp, g_temp, b_temp;

    if (h >= 0 && h < 60) { r_temp = c; g_temp = x; b_temp = 0; }
    else if (h >= 60 && h < 120) { r_temp = x; g_temp = c; b_temp = 0; }
    else if (h >= 120 && h < 180) { r_temp = 0; g_temp = c; b_temp = x; }
    else if (h >= 180 && h < 240) { r_temp = 0; g_temp = x; b_temp = c; }
    else if (h >= 240 && h < 300) { r_temp = x; g_temp = 0; b_temp = c; }
    else { r_temp = c; g_temp = 0; b_temp = x; }

    *r = r_temp + m;
    *g = g_temp + m;
    *b = b_temp + m;
}