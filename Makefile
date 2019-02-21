CC = /usr/local/musl/bin/musl-gcc
CFLAGS = -Wall -O2 -static -Wl,--build-id=sha1 -lnanomsg -lxjr -L/usr/local/musl/lib -L/usr/local/musl/lib64 -DNN_STATIC_LIB -I/usr/local/musl/include
LIB = -lpthread -L/usr/local/musl/lib -L/usr/local/musl/lib64 /usr/local/musl/lib64/libnanomsg.a /usr/local/musl/lib/libxjr.a
HFILES =
CFILES = nanodist.c

all: norch

nanodist: $(CFILES) $(HFILES)
	$(CC) $(CFLAGS) -o norch $(CFILES) $(LIB)

clean:
	rm -f norch
