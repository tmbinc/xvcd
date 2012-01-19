#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "gpio.h"

static int jtag_state;
static int verbose;

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

static int sread(int fd, void *target, int len)
{
	unsigned char *t = target;
	while (len)
	{
		int r = read(fd, t, len);
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

	do
	{
		char cmd[16];
		unsigned char buffer[2048], result[1024];
		
		if (sread(fd, cmd, 6) != 1)
			return 1;
		
		if (memcmp(cmd, "shift:", 6))
		{
			cmd[6] = 0;
			fprintf(stderr, "invalid cmd '%s'\n", cmd);
			return 1;
		}
		
		int len;
		if (sread(fd, &len, 4) != 1)
		{
			fprintf(stderr, "reading length failed\n");
			return 1;
		}
		
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
			for (i = 0; i < len; ++i)
			{
				//
				// Do the actual cycle.
				//
				
				int tms = !!(buffer[i/8] & (1<<(i&7)));
				int tdi = !!(buffer[nr_bytes + i/8] & (1<<(i&7)));
				result[i / 8] |= gpio_get(GPIO_TDO) << (i&7);
				gpio_set(GPIO_TMS, tms);
				gpio_set(GPIO_TDI, tdi);
				gpio_set(GPIO_TCK, 1);
				gpio_set(GPIO_TCK, 0);
				
				//
				// Track the state.
				//
				jtag_state = jtag_step(jtag_state, tms);
			}

		if (write(fd, result, nr_bytes) != nr_bytes)
		{
			perror("write");
			return 1;
		}
		
		if (verbose)
		{
			printf("jtag state %d\n", jtag_state);
		}
	} while (!(seen_tlr && jtag_state == run_test_idle));
	return 0;
}

int main(int argc, char **argv)
{
	int i;
	int s;
	int c;
	struct sockaddr_in address;
	
	opterr = 0;
	
	while ((c = getopt(argc, argv, "v")) != -1)
		switch (c)
		{
		case 'v':
			verbose = 1;
			break;
		case '?':
			fprintf(stderr, "usage: %s [-v]\n", *argv);
			return 1;
		}
	
	//
	// Initialize GPIOs (mapping them into the process, 
	// re-setting alternate functions, making input/outputs).
	//
	
	gpio_init();
	
	//
	// Listen on port 2542.
	//
	
	s = socket(AF_INET, SOCK_STREAM, 0);
	
	if (s < 0)
	{
		perror("socket");
		return 1;
	}
	
	i = 1;
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &i, sizeof i);
	
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(2542);
	address.sin_family = AF_INET;
	
	if (bind(s, (struct sockaddr*)&address, sizeof(address)) < 0)
	{
		perror("bind");
		return 1;
	}
	
	if (listen(s, 0) < 0)
	{
		perror("listen");
		return 1;
	}
	
	fd_set conn;
	int maxfd = 0;
	
	FD_ZERO(&conn);
	FD_SET(s, &conn);
	
	maxfd = s;
	
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
				
				if (fd == s)
				{
					int newfd;
					socklen_t nsize = sizeof(address);
					
					newfd = accept(s, (struct sockaddr*)&address, &nsize);
					if (verbose)
						printf("connection accepted - fd %d\n", newfd);
					if (newfd < 0)
					{
						perror("accept");
					} else
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
					close(fd);
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
				close(fd);
				FD_CLR(fd, &conn);
				if (fd == s)
					break;
			}
		}
	}
	
	//
	// Un-map IOs.
	//
	
	gpio_close();
	
	return 0;
}
