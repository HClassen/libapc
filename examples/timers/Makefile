CC= gcc
CFLAGS= -W -std=c99
PT= -pthread
LIB= -L../../ -lapc

timers: timers.c
	$(CC) $(CFLAGS) -o timers timers.c $(PT) $(LIB)

clean:
	rm ./timers