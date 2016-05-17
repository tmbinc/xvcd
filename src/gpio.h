#ifndef __GPIO_H
#define __GPIO_H

#define GPIO_TDI 0
#define GPIO_TCK 2
#define GPIO_TMS 3
#define GPIO_TDO 4

void gpio_init(void);
void gpio_close(void);

#include "gpio_inline.h"

#endif
