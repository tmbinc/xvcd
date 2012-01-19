#ifndef __GPIO_H
#define __GPIO_H

#define GPIO_TDI 0
#define GPIO_TDO 1
#define GPIO_TCK 2
#define GPIO_TMS 3

static const int gpio_table[] = {104, 35, 118, 36};

void gpio_init(void);
void gpio_close(void);

#include "gpio_inline.h"

#endif
