#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include "gpio.h"

volatile void * ioremap(unsigned long physaddr, unsigned size)
{
	static int axs_mem_fd = -1;
	unsigned long page_addr, ofs_addr, reg, pgmask;
	void* reg_mem = NULL;

	/*
	 * looks like mmap wants aligned addresses?
	 */
	pgmask = getpagesize()-1;
	page_addr = physaddr & ~pgmask;
	ofs_addr  = physaddr & pgmask;

	/*
	 * Don't forget O_SYNC, esp. if address is in RAM region.
	 * Note: if you do know you'll access in Read Only mode,
	 *	pass O_RDONLY to open, and PROT_READ only to mmap
	 */
	if (axs_mem_fd == -1) {
		axs_mem_fd = open("/dev/mem", O_RDWR|O_SYNC);
		if (axs_mem_fd < 0) {
				perror("AXS: can't open /dev/mem");
				return NULL;
		}
	}

	/* memory map */
	reg_mem = mmap(
		(caddr_t)reg_mem,
		size+ofs_addr,
		PROT_READ|PROT_WRITE,
		MAP_SHARED,
		axs_mem_fd,
		page_addr
	);
	if (reg_mem == MAP_FAILED) {
		perror("AXS: mmap error");
		close(axs_mem_fd);
		return NULL;
	}

	reg = (unsigned long )reg_mem + ofs_addr;
	return (volatile void *)reg;
}

int iounmap(volatile void *start, size_t length)
{
	unsigned long ofs_addr;
	ofs_addr = (unsigned long)start & (getpagesize()-1);

	/* do some cleanup when you're done with it */
	return munmap((unsigned char*)start-ofs_addr, length+ofs_addr);
}

volatile unsigned long *gpio;
volatile unsigned long *mfpr;

void gpio_init(void)
{
	//
	// Map GPIO MMIO space.
	//
	
	gpio = ioremap(0xD4019000, 0x1000);
	mfpr = ioremap(0xD401E000, 0x1000);
	
	//
	// Initialize GPIOs.
	//
	mfpr[0x1d8/4]= 0x880;
	mfpr[0xd8/4]= 0x880;
	mfpr[0xdc/4]= 0x880;

	gpio_output(GPIO_TDI, 1);
	gpio_output(GPIO_TMS, 1);
	gpio_output(GPIO_TCK, 1);
	gpio_output(GPIO_TDO, 0);
}

void gpio_close(void)
{
	iounmap(mfpr, 0x1000);
	iounmap(gpio, 0x1000);
}
