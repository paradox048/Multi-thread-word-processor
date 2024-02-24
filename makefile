CC=gcc
CFLAGS=-Wall -g -pedantic -std=c99 -D_POSIX_C_SOURCE=200809L

A2: A2.o
	$(CC) A2.o -o A2

A2.o: A2.c
	$(CC) $(CFLAGS) -c A2.c 

clean:
	rm *.o *.hist A2