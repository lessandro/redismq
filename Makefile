all: example static

static:
	$(CC) -std=c99 -Wall -c redismq.c
	ar rcs redismq.a redismq.o

example:
	$(MAKE) -C example

clean:
	rm -rf *.a *.o
	$(MAKE) -C example clean

.PHONY: all static example clean
