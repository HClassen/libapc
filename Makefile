CC= gcc
CFLAGS= -W -Wextra -Wall -std=c99
POSIX= -D_POSIX_C_SOURCE=200809L
PT= -pthread
DEBUG= -DDEBUG -g

CORE= src/apc.c src/core.c
COMMON= src/common/fd.c src/common/threadpool.c src/common/heap.c
NET= src/network/net.c src/network/tcp.c src/network/udp.c
FILES= $(CORE) $(COMMON) $(NET) src/file/fs.c src/timer/timer.c src/reactor/reactor.c

main: $(FILES)
	$(CC) $(CFLAGS) -o apc main.c $(PT) $(FILES)

debug: $(FILES)
	$(CC) $(CFLAGS) -o apc main.c $(PT) $(FILES) $(DEBUG)

lib: libprep libcomb libclean

libprep: $(FILES)
	$(CC) $(CFLAGS) -c $(PT) $(FILES) $(DEBUG) $(POSIX)

libcomb: 
	# gcc -shared -o libapc.so  *.o  -lm
	ar -cvq libapc.a *.o

libclean:
	rm *.o

clean:
	rm apc