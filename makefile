all:
	clang -g -Wall *.c -lhiredis -lev
