#ifndef BH1750_H
#define BH1750_H

#include "driver/i2c.h"

void bh1750_init(i2c_port_t port);
float bh1750_read(void);

#endif