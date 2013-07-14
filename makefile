all: static-lib example

static-lib:
	clang -c redismq.c
	libtool -static -o redismq.a redismq.o

example: static-lib
	clang -o example example.c redismq.a -lev -lhiredis

clean:
	rm -rf *.dSYM *.a *.o example
