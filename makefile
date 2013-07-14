all: static-lib example

static-lib:
	$(CC) -c redismq.c
	libtool -static -o redismq.a redismq.o

example: static-lib
	$(CC) -o example example.c redismq.a -lev -lhiredis

clean:
	rm -rf *.dSYM *.a *.o example
