CC = /usr/local/musl/bin/musl-gcc
CFLAGS = -Wall -O2 -static -Wl,--build-id=sha1 -lnanomsg -lxjr -L/usr/local/musl/lib -L/usr/local/musl/lib64 -DNN_STATIC_LIB -I/usr/local/musl/include
#CFLAGS = -Wall -O2 -static -DNN_STATIC_LIB
# LIB = -lpthread
LIB = -lpthread -L/usr/local/musl/lib -L/usr/local/musl/lib64 /usr/local/musl/lib64/libnanomsg.a /usr/local/musl/lib/libxjr.a
#-lpthread -lanl
HFILES =
CFILES = nanodist.c

all: nanodist

nanodist: $(CFILES) $(HFILES)
	$(CC) $(CFLAGS) -o nanodist $(CFILES) $(LIB)

clean:
	rm -f nanodist
