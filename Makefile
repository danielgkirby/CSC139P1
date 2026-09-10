CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror -g

.PHONY: all clean

all: proclab proclab-worker

proclab: proclab.c
	$(CC) $(CFLAGS) -o $@ $<

proclab-worker: proclab-worker.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f proclab proclab-worker
