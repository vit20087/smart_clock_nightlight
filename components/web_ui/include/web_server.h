#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <stdbool.h>
void web_init_system(void);
void web_get_target_color(int *r, int *g, int *b, int *w);
int web_get_brightness(void);
int web_get_mode(void);
int web_get_speed(void);


bool web_is_auto_mode(void); 


void web_update_lux_display(float lux); 

#endif