C=gcc
CFLAGS=-Wall -g
OBJECTS=disk.o fs.o shell.o

all: simplefs

simplefs: $(OBJECTS)
	$(C) $^ $(CFLAGS) -lm -o $@

%.o: %.c
	$(C) $(CFLAGS) -c $<

.PHONY: clean
clean:
	rm -f *.o simplefs
