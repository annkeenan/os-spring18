C=gcc
CFLAGS=-Wall -g
OBJECTS=main.o page_table.o disk.o program.o

all: virtmem

virtmem: $(OBJECTS)
	$(C) $^ $(CFLAGS) -o $@

%.o: %.c
	$(C) $(CFLAGS) -c $<

.PHONY: clean
clean:
	rm -f *.o virtmem
