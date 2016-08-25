# Makefile for testcase

SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)

%o: %c
	$(CC) $(CFLAGS) -c -o $@ $<

all: $(OBJS)

clean:
	rm -rf $(OBJS)

.PHONY: all clean
