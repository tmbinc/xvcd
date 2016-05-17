O ?= .
OBJ ?= $(O)/obj
CFLAGS=-Werror -Wall -Os -g -lwiringPi -lwiringPiDev -lpthread
LDFLAGS=

all: $(O) $(O)/xvcd

.PHONY: $(O) $(OBJ)

$(O):
	mkdir -p $(O)

$(OBJ):
	mkdir -p $(OBJ)

$(O)/xvcd: $(OBJ)/gpio.o $(OBJ)/xvcd.o
	$(CC) -o $@ $^ $(CFLAGS)

$(OBJ)/%.o: %.c $(OBJ)
	$(CC) -c $< -o $@ $(CFLAGS)
