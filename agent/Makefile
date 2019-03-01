CC = /usr/local/musl/bin/musl-gcc
CFLAGS = -g -Wall -O2 -static -Wl,--build-id=sha1 -lnanomsg -lxjr -L/usr/local/musl/lib -L/usr/local/musl/lib64 -DNN_STATIC_LIB -I/usr/local/musl/include
LIB = -lpthread -L/usr/local/musl/lib -L/usr/local/musl/lib64 /usr/local/musl/lib64/libnanomsg.a /usr/local/musl/lib/libxjr.a
HFILES = item_cmd.h runner.h returner.h output.h misc.h broadcast_handler.h config.h
CFILES = norch.c item_cmd.c runner.c returner.c output.c misc.c broadcast_handler.c config.c

all: norch test_cmd

norch: $(CFILES) $(HFILES)
	$(CC) $(CFLAGS) -o norch $(CFILES) $(LIB)

test_cmd: test_cmd.c item_cmd.c item_cmd.h
	$(CC) $(CFLAGS) -o test_cmd test_cmd.c item_cmd.c $(LIB)

clean:
	rm -f norch
