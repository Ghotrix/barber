CFLAGS=-g -c -std=c99

all: barber

main.o: main.c
	gcc $(CFLAGS) main.c

barber: main.o
	gcc -lrt -lpthread main.o -o barber

clean:
	rm -rf *.o barber
