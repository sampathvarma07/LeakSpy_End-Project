
CC = gcc
CFLAGS = -Wall -Wextra -g -O0 -fno-omit-frame-pointer
SHARED_FLAGS = -fPIC -shared

all: leakspy leakspy.so leakspy-symbolize tests

leakspy: src/leakspy.c
	$(CC) $(CFLAGS) $< -o $@

leakspy.so: src/interpose.c
	$(CC) $(CFLAGS) $(SHARED_FLAGS) $< -o $@ -ldl

leakspy-symbolize: cli/leakspy-symbolize.c
	$(CC) $(CFLAGS) $< -o $@

tests: tests/test_leak tests/test_normal tests/test_calloc tests/test_realloc tests/test_realloc_fail tests/test_calloc_overflow tests/test_zero_alloc

tests/test_leak: tests/test_leak.c
	$(CC) $(CFLAGS) -no-pie $< -o $@

tests/test_normal: tests/test_normal.c
	$(CC) $(CFLAGS) -no-pie $< -o $@

tests/test_calloc: tests/test_calloc.c
	$(CC) $(CFLAGS) -no-pie $< -o $@

tests/test_realloc: tests/test_realloc.c
	$(CC) $(CFLAGS) -no-pie $< -o $@

tests/test_realloc_fail: tests/test_realloc_fail.c
	$(CC) $(CFLAGS) -no-pie $< -o $@

tests/test_calloc_overflow: tests/test_calloc_overflow.c
	$(CC) $(CFLAGS) -no-pie $< -o $@

tests/test_zero_alloc: tests/test_zero_alloc.c
	$(CC) $(CFLAGS) -no-pie $< -o $@

clean:
	rm -f leakspy leakspy.so leakspy-symbolize
	rm -f tests/test_leak tests/test_normal
	rm -f tests/test_calloc tests/test_realloc tests/test_realloc_fail tests/test_calloc_overflow tests/test_zero_alloc

.PHONY: all tests clean
