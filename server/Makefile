CC = gcc
CFLAGS = -Wall -Werror
LDFLAGS=
CROSS_COMPILE =

EXE = aesdsocket
SOURCES=aesdsocket.c
OBJECTS=$(SOURCES:.c=.o) # replace .c to .o of all the sources file

all: $(EXE) $(OBJECTS)

$(EXE): $(OBJECTS)
	$(CROSS_COMPILE)$(CC) $(LDFLAGS) $(OBJECTS) -o $@

$(OBJECTS): $(SOURCES)
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(EXE) $(OBJECTS)
