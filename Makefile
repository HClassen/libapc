CC= gcc
CFLAGS= -W -Wextra -Wall -std=c99
POSIX= -D_POSIX_C_SOURCE=200809L#-D_POSIX_C_SOURCE=200112L
PT= -pthread
DEBUG= -DDEBUG -g
FILES= src/apc.c src/core.c src/net.c src/fd.c src/fs.c src/timer.c src/tcp.c src/udp.c src/threadpool.c src/heap.c src/reactor/reactor.c

main: $(FILES)
	$(CC) $(CFLAGS) -o apc main.c $(PT) $(FILES) $(POSIX)

debug: $(FILES)
	$(CC) $(CFLAGS) -o apc main.c $(PT) $(FILES) $(POSIX) $(DEBUG)

lib: libprep libcomb libclean

libprep: $(FILES)
	$(CC) $(CFLAGS) -c $(PT) $(FILES) $(POSIX)

libcomb: 
	# gcc -shared -o libapc.so  *.o  -lm
	ar -cvq libapc.a *.o

libclean:
	rm *.o

clean:
	rm apc