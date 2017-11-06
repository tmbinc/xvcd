#include "io_ftdi.h"

#include <stdio.h>
#include <string.h>

#define FTD2XX_STATIC
#include "../libFTDI/ftd2xx.h"

// Configuration
#define BAUD_RATE (3000000 / 16) // In bitbang mode, clock rate is 16x baud rate

#define PORT_TCK            0x01
#define PORT_TDI            0x02
#define PORT_TDO            0x04
#define PORT_TMS            0x08
#define IO_OUTPUT (PORT_TCK|PORT_TDI|PORT_TMS)

#define CHK_STAT(xx) CheckFTStatus(xx, __LINE__)


//////////////////////////////////////
static FT_HANDLE sFTDI_fd;
//////////////////////////////////////

// temporary
static LARGE_INTEGER sQPC_FREQ;



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
	CHK_STAT(FT_SetBitMode(sFTDI_fd, IO_OUTPUT, FT_BITMODE_SYNC_BITBANG));
	CHK_STAT(FT_Purge(sFTDI_fd, FT_PURGE_RX | FT_PURGE_TX));
	CHK_STAT(FT_SetBaudRate(sFTDI_fd, BAUD_RATE));

	// For performance
	CHK_STAT(FT_SetLatencyTimer(sFTDI_fd, 2));
	CHK_STAT(FT_SetUSBParameters(sFTDI_fd, 1024*16, 1024*16)); // 64 bytes to 64K, in 64 byte increments, default to 4K

	QueryPerformanceFrequency(&sQPC_FREQ);

	return 0;
}

int io_scan(const unsigned char *TMS, const unsigned char *TDI, unsigned char *TDO, int bits)
{
	LARGE_INTEGER qpc_times[10];
	int qpc_inx = 0;
	
	// Baseline
	QueryPerformanceCounter(&qpc_times[qpc_inx++]);

	unsigned char buffer[2*16384];
	int res = 0;

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

	// conv done
	QueryPerformanceCounter(&qpc_times[qpc_inx++]);

	DWORD bytes_written;
	const DWORD bytes_to_write = bits * 2;
	CHK_STAT(FT_Write(sFTDI_fd, buffer, bytes_to_write, &bytes_written));
	if (bytes_to_write != bytes_written)
	{
		fprintf(stderr, "Didn't write enough bytes. #written=%d expected %d\n", bytes_written, bytes_to_write);
	}

	// write done
	QueryPerformanceCounter(&qpc_times[qpc_inx++]);


	DWORD total_bytes_read = 0;
	while (total_bytes_read < bytes_to_write)
	{
		DWORD bytes_read = 0;
		CHK_STAT(FT_Read(sFTDI_fd, buffer + total_bytes_read, bytes_to_write - total_bytes_read, &bytes_read));
		total_bytes_read += bytes_read;
	}

	// read done
	QueryPerformanceCounter(&qpc_times[qpc_inx++]);


	memset(TDO, 0, (bits + 7) / 8);
	for (int i = 0; i < bits; ++i)
	{
		if (buffer[i * 2 + 1] & PORT_TDO)
		{
			TDO[i/8] |= 1 << (i&7);
		}
	}

	// done done
	QueryPerformanceCounter(&qpc_times[qpc_inx++]);

	// Report
#if 0
	printf("(%5i) ", bits);
	for (int inx = 1; inx < qpc_inx; inx++)
	{
		LONGLONG elap = qpc_times[inx].QuadPart - qpc_times[inx - 1].QuadPart;
		elap *= 1000000;
		elap /= sQPC_FREQ.QuadPart;
		printf("%10lld", elap);
	}
	printf("\n");
#endif

	return 0;
}

