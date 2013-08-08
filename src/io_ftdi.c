#include <stdio.h>
#include <ftdi.h>
#include <string.h>

#include "io_ftdi.h"

#define PORT_TCK            0x01
#define PORT_TDI            0x02
#define PORT_TDO            0x04
#define PORT_TMS            0x08
#define IO_OUTPUT (PORT_TCK|PORT_TDI|PORT_TMS)

#define USE_ASYNC

#ifndef USE_ASYNC
#define FTDI_MAX_WRITESIZE 256
#endif

struct ftdi_context ftdi;

void io_close(void);

int io_init(void)
{
	int res;
	
	res = ftdi_init(&ftdi);
	
	if (res < 0)
	{
		fprintf(stderr, "ftdi_init: %d (%s)\n", res, ftdi_get_error_string(&ftdi));
		return 1;
	}

	res = ftdi_set_interface(&ftdi, INTERFACE_B);

	if (res < 0) 
	{
		fprintf(stderr, "ftdi_set_interface: %d (%s)\n", res, ftdi_get_error_string(&ftdi));
		return 1;
	}
	
	res = ftdi_usb_open(&ftdi, 0x0403, 0x6010);
	
	if (res < 0)
	{
		fprintf(stderr, "ftdi_usb_open: %d (%s)\n", res, ftdi_get_error_string(&ftdi));
		ftdi_deinit(&ftdi);
		return 1;
	}
	
	res = ftdi_set_bitmode(&ftdi, IO_OUTPUT, BITMODE_MPSSE);
	
	if (res < 0)
	{
		fprintf(stderr, "ftdi_set_bitmode: %d (%s)\n", res, ftdi_get_error_string(&ftdi));
		io_close();
		return 1;
	}
	
	res = ftdi_usb_purge_buffers(&ftdi);
	
	if (res < 0)
	{
		fprintf(stderr, "ftdi_usb_purge_buffers %d (%s)\n", res, ftdi_get_error_string(&ftdi));
		io_close();
		return 1;
	}
	
	res = ftdi_set_baudrate(&ftdi, 750000);
	
	if (res < 0)
	{
		fprintf(stderr, "ftdi_set_baudrate %d (%s)\n", res, ftdi_get_error_string(&ftdi));
		io_close();
		return 1;
	}
	
	return 0;
}

int io_scan(const unsigned char *TMS, const unsigned char *TDI, unsigned char *TDO, int bits)
{
#define B 7
#define RB 1
	unsigned char buffer[8192 * B];
	int i, res; 
#ifndef USE_ASYNC
#error no async
	int r, t;
#endif
	
	if (bits * B > sizeof(buffer))
	{
		fprintf(stderr, "FATAL: out of buffer space for %d bits\n", bits);
		return -1;
	}

	for (i = 0; i < bits; ++i)
	{
		unsigned char v = 0;
		if (TMS[i/8] & (1<<(i&7)))
			v |= PORT_TMS;
		if (TDI[i/8] & (1<<(i&7)))
			v |= PORT_TDI;
		buffer[i * B + 0] = 0x80;
		buffer[i * B + 1] = v;
		buffer[i * B + 2] = IO_OUTPUT;
		buffer[i * B + 3] = 0x81;
		buffer[i * B + 4] = 0x80;
		buffer[i * B + 5] = v | PORT_TCK;
		buffer[i * B + 6] = IO_OUTPUT;
	}

#ifndef USE_ASYNC
	r = 0;
#error broken
	while (r < bits * B)
	{
		t = bits * B - r;
		if (t > FTDI_MAX_WRITESIZE)
			t = FTDI_MAX_WRITESIZE;
		
		printf("writing %d bytes\n", t);
		res = ftdi_write_data(&ftdi, buffer + r, t);

		if (res != t)
		{
			fprintf(stderr, "ftdi_write_data %d (%s)\n", res, ftdi_get_error_string(&ftdi));
			return -1;
		}
		
		i = 0;
		
		while (i < t)
		{
			res = ftdi_read_data(&ftdi, buffer + r + i, t - i);

			if (res < 0)
			{
				fprintf(stderr, "ftdi_read_data %d (%s)\n", res, ftdi_get_error_string(&ftdi));
				return -1
			}
			
			i += res;
		}
		
		r += t;
	}
#else
	res = ftdi_write_data_async(&ftdi, buffer, bits * B);
	if (res < 0)
	{
		fprintf(stderr, "ftdi_write_data_async %d (%s)\n", res, ftdi_get_error_string(&ftdi));
		return -1;
	}

	i = 0;
	
	while (i < bits * RB)
	{
		res = ftdi_read_data(&ftdi, buffer + i, bits * RB - i);

		if (res < 0)
		{
			fprintf(stderr, "ftdi_read_data %d (%s)\n", res, ftdi_get_error_string(&ftdi));
			return -1;
		}
		
		i += res;
	}
#endif

	memset(TDO, 0, (bits + 7) / 8);
	
	for (i = 0; i < bits; ++i)
	{
		if (buffer[i * RB] & PORT_TDO)
		{
			TDO[i/8] |= 1 << (i&7);
		}
	}

	return 0;
}

void io_close(void)
{
	ftdi_usb_close(&ftdi);
	ftdi_deinit(&ftdi);
}