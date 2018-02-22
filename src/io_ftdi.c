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
#define USE_LIBFTDI1

#ifndef USE_ASYNC
#define FTDI_MAX_WRITESIZE 256
#endif

#define FTDI_BAUDRATE (750000)
struct ftdi_context ftdi;
static int vlevel = 0;

void io_close(void);

// Pass in the selected FTDI device to be opened
//
// vendor: vendor ID of desired device, or -1 to use the default
//
// product: product ID of desired device, or -1 to use the default
//
// frequency: (in Hz) set TCK frequency - settck: command from the
//            client is ignored. Pass in 0 to try to obey settck:
//            commands.
//
// verbosity: 0 means no output, increase from 0 for more and more debugging output
//
int io_init(int vendor, int product, unsigned long frequency, int verbosity)
{
	int res;
	if (product < 0)
		product = 0x6010;
	if (vendor < 0)
		vendor = 0x0403;
	
	// Save verbosity level
	vlevel = verbosity;

	res = ftdi_init(&ftdi);
	
	if (res < 0)
	{
		fprintf(stderr, "ftdi_init: %d (%s)\n", res, ftdi_get_error_string(&ftdi));
		return 1;
	}
	
	res = ftdi_usb_open(&ftdi, vendor, product);
	
	if (res < 0)
	{
		fprintf(stderr, "ftdi_usb_open(0x%04x, 0x%04x): %d (%s)\n", vendor, product, res, ftdi_get_error_string(&ftdi));
		ftdi_deinit(&ftdi);
		return 1;
	}
	
	ftdi_set_bitmode(&ftdi, 0xFF, BITMODE_CBUS);
	res = ftdi_set_bitmode(&ftdi, IO_OUTPUT, BITMODE_SYNCBB);
	
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

	res = ftdi_set_baudrate(&ftdi, FTDI_BAUDRATE);

	if (res < 0)
	{
		fprintf(stderr, "ftdi_set_baudrate %d (%s)\n", res, ftdi_get_error_string(&ftdi));
		io_close();
		return 1;
	}
	
	return 0;
}

// period - desired JTAG TCK period in ns
//
// return: if error, return an error code < 0
//         if success, return the actual period in ns
//
int io_set_period(unsigned int period)
{
	int baudrate;
	int actPeriod;
	int res;

	// Not completely sure this is the proper conversion rate between
	// baudrate and period but believe it to be close.
	baudrate =  25000000 / period;

	res = ftdi_set_baudrate(&ftdi, baudrate);

	if (res < 0)
	{
		fprintf(stderr, "ftdi_set_baudrate %d (%s)\n", res, ftdi_get_error_string(&ftdi));
		return -1;
	}


	actPeriod = 25000000 / baudrate;

	return  actPeriod;
}


int io_scan(const unsigned char *TMS, const unsigned char *TDI, unsigned char *TDO, int bits)
{
	unsigned char buffer[2*16384];
	int i, res; 
#ifndef USE_ASYNC
#error no async
	int r, t;
#else 
	void *vres;
#endif
	
	if (bits > sizeof(buffer)/2)
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
		buffer[i * 2 + 0] = v;
		buffer[i * 2 + 1] = v | PORT_TCK;
	}

#ifndef USE_ASYNC
	r = 0;
	
	while (r < bits * 2)
	{
		t = bits * 2 - r;
		if (t > FTDI_MAX_WRITESIZE)
			t = FTDI_MAX_WRITESIZE;
		
		if (vlevel > 2) printf("writing %d bytes\n", t);
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
				return -1;
			}
			
			i += res;
		}
		
		r += t;
	}
#else
#ifdef USE_LIBFTDI1
	vres = ftdi_write_data_submit(&ftdi, buffer, bits * 2);
	if (!vres)
	{
		fprintf(stderr, "ftdi_write_data_submit (%s)\n", ftdi_get_error_string(&ftdi));
		return -1;
	}
#else
	res = ftdi_write_data_async(&ftdi, buffer, bits * 2);
	if (res < 0)
	{
		fprintf(stderr, "ftdi_write_data_async %d (%s)\n", res, ftdi_get_error_string(&ftdi));
		return -1;
	}
#endif

	i = 0;
	
	while (i < bits * 2)
	{
		res = ftdi_read_data(&ftdi, buffer + i, bits * 2 - i);

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
		if (buffer[i * 2 + 1] & PORT_TDO)
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
