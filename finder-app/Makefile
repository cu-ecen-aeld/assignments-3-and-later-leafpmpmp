CC=gcc
CFLAGS=-Wall -std=c11

SRC=writer.c
OUTPUT=writer

all: clean $(OUTPUT)

writer: $(SRC)
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -o $(OUTPUT) $(SRC)
clean:
	rm -f $(OUTPUT)
