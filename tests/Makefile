CC=gcc
CFLAGS=-Wall -Wextra -DENABLE_ASSERTS -O2
test:
	$(CC) $(CFLAGS) test.c ../srpmalloc.c -o test
	./test
.PHONY: test
