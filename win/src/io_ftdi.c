#include "io_ftdi.h"

#include <stdio.h>
#include <string.h>

#define FTD2XX_STATIC
#include "../libFTDI/ftd2xx.h"

// Configuration
#define USE_ASYNC
//#define BAUD_RATE (750000 * 1)
#define BAUD_RATE (3000000)

#define PORT_TCK            0x01
#define PORT_TDI            0x02
#define PORT_TDO            0x04
#define PORT_TMS            0x08
#define IO_OUTPUT (PORT_TCK|PORT_TDI|PORT_TMS)

#define CHK_STAT(xx) CheckFTStatus(xx, __LINE__)

#define BITMODE_CBUS 0x20	// FT232R, FT232H only
#define BITMODE_SYNCBB 0x04	// FT232, FT2232, and others


#ifndef USE_ASYNC
#define FTDI_MAX_WRITESIZE 256
#endif

//////////////////////////////////////
static FT_HANDLE sFTDI_fd;
//////////////////////////////////////

static void CheckFTStatus(FT_STATUS stat, size_t line_no)
{
	if (stat != 0)
	{
		fprintf(stderr, "FTDI Error %d (line %u)\n", (int) stat, line_no);
	}
}



void io_close(void)
{
	FT_Close(sFTDI_fd);
}


int io_init(int product, int vendor)
{
	int dev_index = 0; // zzqq

	CHK_STAT(FT_Open(dev_index, &sFTDI_fd));
	CHK_STAT(FT_SetBitMode(sFTDI_fd, IO_OUTPUT, BITMODE_SYNCBB));
	CHK_STAT(FT_Purge(sFTDI_fd, FT_PURGE_RX | FT_PURGE_TX));
	CHK_STAT(FT_SetBaudRate(sFTDI_fd, BAUD_RATE));

	return 0;
}

int io_scan(const unsigned char *TMS, const unsigned char *TDI, unsigned char *TDO, int bits)
{
	unsigned char buffer[2*16384];
	int res = 0;
#ifndef USE_ASYNC
#error no async
	int r, t;
#endif

	if (bits > sizeof(buffer)/2)
	{
		fprintf(stderr, "FATAL: out of buffer space for %d bits\n", bits);
		return -1;
	}

	for (int i = 0; i < bits; ++i)
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

		printf("writing %d bytes\n", t);
		res = ftdi_write_data(sFTDI_fd, buffer + r, t);

		if (res != t)
		{
			fprintf(stderr, "ftdi_write_data %d (%s)\n", res, ftdi_get_error_string(sFTDI_fd));
			return -1;
		}

		i = 0;

		while (i < t)
		{
			res = ftdi_read_data(sFTDI_fd, buffer + r + i, t - i);

			if (res < 0)
			{
				fprintf(stderr, "ftdi_read_data %d (%s)\n", res, ftdi_get_error_string(sFTDI_fd));
				return -1
			}

			i += res;
		}

		r += t;
	}
#else
	DWORD bytes_written;
	const DWORD bytes_to_write = bits * 2;
	CHK_STAT(FT_Write(sFTDI_fd, buffer, bytes_to_write, &bytes_written));

	if (bytes_to_write != bytes_written)
	{
		fprintf(stderr, "Didn't write enough bytes. #written=%d expected %d\n", bytes_written, bytes_to_write);
	}


	DWORD total_bytes_read = 0;
	while (total_bytes_read < bytes_to_write)
	{
		DWORD bytes_read = 0;
		CHK_STAT(FT_Read(sFTDI_fd, buffer + total_bytes_read, bytes_to_write - total_bytes_read, &bytes_read));
		total_bytes_read += bytes_read;
	}
#endif

	memset(TDO, 0, (bits + 7) / 8);

	for (int i = 0; i < bits; ++i)
	{
		if (buffer[i * 2 + 1] & PORT_TDO)
		{
			TDO[i/8] |= 1 << (i&7);
		}
	}

	return 0;
}

