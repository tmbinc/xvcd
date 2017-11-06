

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <stdbool.h>
#include <winsock.h>

#include "io_ftdi.h"

#pragma comment(lib, "Ws2_32.lib")

static int jtag_state;
static int verbose = 0;

// temporary
static LARGE_INTEGER sQPC_FREQ;



//
// JTAG state machine.
//

enum
{
	test_logic_reset, run_test_idle,

	select_dr_scan, capture_dr, shift_dr,
	exit1_dr, pause_dr, exit2_dr, update_dr,

	select_ir_scan, capture_ir, shift_ir,
	exit1_ir, pause_ir, exit2_ir, update_ir,

	num_states
};

static int jtag_step(int state, int tms)
{
	static const int next_state[num_states][2] =
	{
		[test_logic_reset] = {run_test_idle, test_logic_reset},
		[run_test_idle] = {run_test_idle, select_dr_scan},

		[select_dr_scan] = {capture_dr, select_ir_scan},
		[capture_dr] = {shift_dr, exit1_dr},
		[shift_dr] = {shift_dr, exit1_dr},
		[exit1_dr] = {pause_dr, update_dr},
		[pause_dr] = {pause_dr, exit2_dr},
		[exit2_dr] = {shift_dr, update_dr},
		[update_dr] = {run_test_idle, select_dr_scan},

		[select_ir_scan] = {capture_ir, test_logic_reset},
		[capture_ir] = {shift_ir, exit1_ir},
		[shift_ir] = {shift_ir, exit1_ir},
		[exit1_ir] = {pause_ir, update_ir},
		[pause_ir] = {pause_ir, exit2_ir},
		[exit2_ir] = {shift_ir, update_ir},
		[update_ir] = {run_test_idle, select_dr_scan}
	};

	return next_state[state][tms];
}

static int sread(int fd, void *target, int len) {
   unsigned char *t = target;
   while (len) {
      int r = recv(fd, t, len, 0);
      if (r <= 0)
         return r;
      t += r;
      len -= r;
   }
   return 1;
}

//
// handle_data(fd) handles JTAG shift instructions.
//   To allow multiple programs to access the JTAG chain
//   at the same time, we only allow switching between
//   different clients only when we're in run_test_idle
//   after going test_logic_reset. This ensures that one
//   client can't disrupt the other client's IR or state.
//
int handle_data(int fd)
{
	int i;
	int seen_tlr = 0;
	const char xvcInfo[] = "xvcServer_v1.0:2048\n"; 

	const bool TCP_DONT_DELAY = true;
	if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &TCP_DONT_DELAY, sizeof(TCP_DONT_DELAY)))
	{
		perror("Error disabling Nagle's algorithm");
	}

	LARGE_INTEGER qpc_times[10];
	do
	{
		// Baseline
		int qpc_inx = 0;
		QueryPerformanceCounter(&qpc_times[qpc_inx++]);

		char cmd[16];
		unsigned char buffer[2*2048], result[2*1024];
		memset(cmd, 0, 16);

		if (sread(fd, cmd, 2) != 1)
			return 1;

		if (memcmp(cmd, "ge", 2) == 0) {
			if (sread(fd, cmd, 6) != 1)
				return 1;
			memcpy(result, xvcInfo, strlen(xvcInfo));
			if (send(fd, result, strlen(xvcInfo), 0) != strlen(xvcInfo)) {
				perror("write");
				return 1;
			}
			if (verbose) {
				printf("%u : Received command: 'getinfo'\n", (int)time(NULL));
				printf("\t Replied with %s\n", xvcInfo);
			}
			break;
		} else if (memcmp(cmd, "se", 2) == 0) {
			if (sread(fd, cmd, 9) != 1)
				return 1;
			memcpy(result, cmd + 5, 4);
			if (send(fd, result, 4, 0) != 4) {
				perror("write");
				return 1;
			}
			if (verbose) {
				printf("%u : Received command: 'settck'\n", (int)time(NULL));
				printf("\t Replied with '%.*s'\n\n", 4, cmd + 5);
			}
			break;
		} else if (memcmp(cmd, "sh", 2) == 0) {
			if (sread(fd, cmd, 4) != 1)
				return 1;
			if (verbose) {
				printf("%u : Received command: 'shift'\n", (int)time(NULL));
			}
		} else {

			fprintf(stderr, "invalid cmd '%s'-ignoring\n", cmd);
			return 0;
		}
		
		
		int len;
		if (sread(fd, &len, 4) != 1)
		{
			fprintf(stderr, "reading length failed\n");
			return 1;
		}

		// Read socket complete
		QueryPerformanceCounter(&qpc_times[qpc_inx++]);
		
		int nr_bytes = (len + 7) / 8;
		if (nr_bytes * 2 > sizeof(buffer))
		{
			fprintf(stderr, "buffer size exceeded\n");
			return 1;
		}
		
		if (sread(fd, buffer, nr_bytes * 2) != 1)
		{
			fprintf(stderr, "reading data failed\n");
			return 1;
		}
		
		memset(result, 0, nr_bytes);

		if (verbose)
		{
			printf("#");
			for (i = 0; i < nr_bytes * 2; ++i)
				printf("%02x ", buffer[i]);
			printf("\n");
		}

		//
		// Only allow exiting if the state is rti and the IR
		// has the default value (IDCODE) by going through test_logic_reset.
		// As soon as going through capture_dr or capture_ir no exit is
		// allowed as this will change DR/IR.
		//
		seen_tlr = (seen_tlr || jtag_state == test_logic_reset) && (jtag_state != capture_dr) && (jtag_state != capture_ir);
		
		
		//
		// Due to a weird bug(??) xilinx impacts goes through another "capture_ir"/"capture_dr" cycle after
		// reading IR/DR which unfortunately sets IR to the read-out IR value.
		// Just ignore these transactions.
		//
		
		if ((jtag_state == exit1_ir && len == 5 && buffer[0] == 0x17) || (jtag_state == exit1_dr && len == 4 && buffer[0] == 0x0b))
		{
			if (verbose)
				printf("ignoring bogus jtag state movement in jtag_state %d\n", jtag_state);
		} else
		{
			for (i = 0; i < len; ++i)
			{
				//
				// Do the actual cycle.
				//
				
				int tms = !!(buffer[i/8] & (1<<(i&7)));
				//
				// Track the state.
				//
				jtag_state = jtag_step(jtag_state, tms);
			}
			if (io_scan(buffer, buffer + nr_bytes, result, len) < 0)
			{
				fprintf(stderr, "io_scan failed\n");
				exit(1);
			}
		}

		// USB cycle complete
		QueryPerformanceCounter(&qpc_times[qpc_inx++]);

		if (send(fd, result, nr_bytes, 0) != nr_bytes)
		{
			perror("write");
			return 1;
		}
				
		if (verbose)
		{
			printf("jtag state %d\n", jtag_state);
		}

		// send socket complete complete
		QueryPerformanceCounter(&qpc_times[qpc_inx++]);


#if 0
		// Report
		printf("(%5i) ", len);
		for (int inx = 1; inx < qpc_inx; inx++)
		{
			LONGLONG elap = qpc_times[inx].QuadPart - qpc_times[inx-1].QuadPart;
			elap *= 1000000;
			elap /= sQPC_FREQ.QuadPart;
			printf("%10lld", elap);
		}
		printf("\n");
#endif
	} while (!(seen_tlr && jtag_state == run_test_idle));
	return 0;
}

int main(int argc, char **argv)
{
	int port = 2542;
	int product = -1, vendor = -1;
	struct sockaddr_in address;

	QueryPerformanceFrequency(&sQPC_FREQ);
	
	
	if (io_init(product, vendor))
	{
		fprintf(stderr, "io_init failed\n");
		return 1;
	}
	
	//
	// Listen on port 2542.
	//

	WSADATA wsaData;
	if (WSAStartup(0x202, &wsaData) != 0)
	{
		perror("Winsock startup failed");
		return 1;
	}
	
	
	SOCKET sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	
	if (INVALID_SOCKET == sock_fd)
	{
		wchar_t *s = NULL;
		FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, WSAGetLastError(),
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPWSTR)&s, 0, NULL);
		fprintf(stderr, "%S\n", s);
		LocalFree(s);



		return 1;
	}
	
	const bool REUSE_SOCKET = true;
	setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &REUSE_SOCKET, sizeof(REUSE_SOCKET));

	const bool TCP_DONT_DELAY = true;
	setsockopt(sock_fd, IPPROTO_TCP, TCP_NODELAY, &TCP_DONT_DELAY, sizeof(TCP_DONT_DELAY));
	
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);
	address.sin_family = AF_INET;
	
	if (bind(sock_fd, (struct sockaddr*)&address, sizeof(address)) < 0)
	{
		perror("bind");
		return 1;
	}
	
	if (listen(sock_fd, 0) < 0)
	{
		perror("listen");
		return 1;
	}
	
	fd_set conn;
	SOCKET maxfd = 0;
	
	FD_ZERO(&conn);
	FD_SET(sock_fd, &conn);
	
	maxfd = sock_fd;

	if (verbose)
		printf("waiting for connection on port %d...\n", port);
	
	while (1)
	{
		fd_set read = conn, except = conn;
		int fd;
		
		//
		// Look for work to do.
		//
		
		if (select(maxfd + 1, &read, 0, &except, 0) < 0)
		{
			perror("select");
			break;
		}
		
		for (fd = 0; fd <= maxfd; ++fd)
		{
			if (FD_ISSET(fd, &read))
			{
				//
				// Readable listen socket? Accept connection.
				//
				
				if (fd == sock_fd)
				{
					SOCKET newfd;
					int nsize = sizeof(address);
					
					newfd = accept(sock_fd, (struct sockaddr*)&address, &nsize);
					if (verbose)
						printf("connection accepted - fd %d\n", newfd);
					if (newfd < 0)
					{
						perror("accept");
					} 
					else
					{
						if (newfd > maxfd)
						{
							maxfd = newfd;
						}
						FD_SET(newfd, &conn);
					}

				}
				//
				// Otherwise, do work.
				//
				else if (handle_data(fd))
				{
					//
					// Close connection when required.
					//
					
					if (verbose)
						printf("connection closed - fd %d\n", fd);
					closesocket(fd);
					FD_CLR(fd, &conn);
				}
			}
			//
			// Abort connection?
			//
			else if (FD_ISSET(fd, &except))
			{
				if (verbose)
					printf("connection aborted - fd %d\n", fd);
				closesocket(fd);
				FD_CLR(fd, &conn);
				if (fd == sock_fd)
					break;
			}
		}
	}
	
	//
	// Un-map IOs.
	//
	io_close();
	
	return 0;
}
