CFLAGS=-Werror -Wall -Os -lftdi

all: xvcd

xvcd: io_ftdi.o xvcd.o
	$(CC) -o $@ $^ $(CFLAGS)
