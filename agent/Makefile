# For non-static
#CC = gcc
#CFLAGS = -g -Wall -O2 -Wl,--build-id=sha1
#LIB = -lpthread -lnanomsg -lxjr

# For static
CC = /usr/local/musl/bin/musl-gcc
CFLAGS = -g -Wall -O2 -static -Wl,--build-id=sha1 -DNN_STATIC_LIB -I/usr/local/musl/include
LIB = -L/usr/local/musl/lib -L/usr/local/musl/lib64 -lpthread -lnanomsg -lxjr

HFILES = item_cmd.h runner.h returner.h output.h misc.h broadcast_handler.h config.h item_file.h item_service.h subreg.h line_matcher.h
CFILES = norch.c item_cmd.c runner.c returner.c output.c misc.c broadcast_handler.c config.c item_file.c item_service.c subreg.c line_matcher.c

all: norch test_cmd test_file test_service

norch: $(CFILES) $(HFILES)
	$(CC) $(CFLAGS) -o norch $(CFILES) $(LIB)

test_cmd: test_cmd.c item_cmd.c item_cmd.h
	$(CC) $(CFLAGS) -o test_cmd test_cmd.c item_cmd.c $(LIB)

test_file: test_file.c item_file.c item_file.h
	$(CC) $(CFLAGS) -o test_file test_file.c item_file.c $(LIB)

test_service: test_service.c item_cmd.c item_cmd.h item_service.c item_service.h subreg.h subreg.c line_matcher.c line_matcher.h misc.c misc.h
	$(CC) $(CFLAGS) -o test_service test_service.c item_service.c item_cmd.c subreg.c line_matcher.c misc.c $(LIB)

clean:
	rm -f norch test_cmd test_file
