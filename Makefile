CC = g++
CFLAGS = -Wall -O2 -Wl,-Bstatic -lnanomsg -Wl,-Bdynamic -DNN_STATIC_LIB
#CFLAGS = -Wall -O2 -static -DNN_STATIC_LIB
# LIB = -lpthread
LIB = /usr/lib64/libnanomsg.a -lpthread
#-lpthread -lanl
HFILES =
CFILES = nanodist.c

all: nanodist

nanodist: $(CFILES) $(HFILES)
	$(CC) $(CFLAGS) -o nanodist $(CFILES) $(LIB)

clean:
	rm -f nanodist
