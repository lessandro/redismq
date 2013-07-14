all: static-lib example

static-lib: redismq.c redismq.h
	$(CC) -Wall -c redismq.c
	libtool -static -o redismq.a redismq.o

example: static-lib example.c
	$(CC) -Wall -o example example.c redismq.a -lev -lhiredis

clean:
	rm -rf *.dSYM *.a *.o example
