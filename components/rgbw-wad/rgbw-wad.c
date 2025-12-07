#include <math.h>
#include "driver/ledc.h"
#include "rgbw-wad.h"

#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_TIMER LEDC_TIMER_1
#define LEDC_DUTY_RES LEDC_TIMER_8_BIT
#define LEDC_FREQUENCY 5000
#define MAX_DUTY 255

static uint32_t get_gamma_duty(float value_0_to_1) {
    uint32_t duty = (uint32_t)(value_0_to_1 * MAX_DUTY);
    if (duty > MAX_DUTY) duty = MAX_DUTY;
    return duty;
}

void rgb_driver_init(void) {
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_MODE, .timer_num = LEDC_TIMER,
        .duty_resolution = LEDC_DUTY_RES, .freq_hz = LEDC_FREQUENCY, .clk_cfg = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    int pins[4] = {PIN_R, PIN_G, PIN_B, PIN_W};
    for(int i=0; i<4; i++){
        ledc_channel_config_t c = {
            .speed_mode = LEDC_MODE, .channel = (ledc_channel_t)i,
            .timer_sel = LEDC_TIMER, .intr_type = LEDC_INTR_DISABLE,
            .gpio_num = pins[i], .duty = 0, .hpoint = 0
        };
        ESP_ERROR_CHECK(ledc_channel_config(&c));
    }
}

void rgb_set_color(float r, float g, float b, float w) {
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, get_gamma_duty(r)); ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_0);
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, get_gamma_duty(g)); ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_1);
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_2, get_gamma_duty(b)); ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_2);
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_3, get_gamma_duty(w)); ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_3);
}