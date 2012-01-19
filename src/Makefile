CFLAGS=-Werror -Wall -Os

all: xvcd

xvcd: gpio.o xvcd.o
	$(CC) -o $@ $^ $(CFLAGS)
