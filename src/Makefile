#
# Makefile for building xvcd
#
#
# Default options: USE_ASYNC and USE_LIBFTDI1
#
#
# libftdi1 is a newer version of the libftdi library and is the
# default library to build against. However, if you want to build
# against the older libftdi library, run make with the USE_LIBFTDI=0
# argument, like:
#
#    make USE_LIBFTDI=0
#
# If want to build with the synchronous ftdi_write function, which may
# make JTAG accesses really, really slow, use the USE_SYNC=0 argument:
#
#    make USE_SYNC=0
#
# Both options can be combined, if desired:
#
#    make USE_SYNC=0 USE_LIBFTDI=0
#
#

O ?= .
OBJ ?= $(O)/obj

BUILD_FLAGS=-Werror -Wall -Os


# Want USE_ASYNC to be the default
ifdef USE_SYNC
   SYNC=-UUSE_ASYNC
else
   SYNC=-DUSE_ASYNC
endif

# Want libftdi1 to be the default
ifdef USE_LIBFTDI
   CFLAGS=$(BUILD_FLAGS) $(SYNC) -UUSE_LIBFTDI1
   LDFLAGS=$(BUILD_FLAGS) -lftdi
else
   CFLAGS=$(BUILD_FLAGS) $(SYNC) -DUSE_LIBFTDI1
   LDFLAGS=$(BUILD_FLAGS) -lftdi1
endif

all: $(O) $(O)/xvcd

.PHONY: $(O) $(OBJ)

$(O):
	mkdir -p $(O)

$(OBJ):
	mkdir -p $(OBJ)

$(O)/xvcd: $(OBJ)/io_ftdi.o $(OBJ)/xvcd.o
	$(CC) -o $@ $^ $(LDFLAGS)

$(OBJ)/%.o: %.c $(OBJ) io_ftdi.h
	$(CC) -c $< -o $@ $(CFLAGS)
