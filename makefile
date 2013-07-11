all:
	clang -Wall *.c -lhiredis -lev
