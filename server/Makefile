CC = $(CROSS_COMPILE)gcc

CFLAGS  = -Wall -Werror
LDFLAGS = -g -pthread -lrt

TARGET = aesdsocket

all: $(TARGET)

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).c $(LDFLAGS)

clean:
	$(RM) $(TARGET) $(TARGET).o
