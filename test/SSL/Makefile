# Makefile for testcase

SRC := main.c
TARGET = main.o

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -c -o $@ $<

all: $(TARGET)

clean:
	rm -rf $(TARGET)

.PHONY: all clean
