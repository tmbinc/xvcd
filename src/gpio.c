#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <wiringPi.h>

#include "gpio.h"

void gpio_init(void)
{
    wiringPiSetup();

    gpio_output(GPIO_TDI, 1);
    gpio_output(GPIO_TMS, 1);
    gpio_output(GPIO_TCK, 1);
    gpio_output(GPIO_TDO, 0);
}

void gpio_close(void)
{
    return;
}

