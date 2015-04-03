CC = clang

SOURCES = $(wildcard *.c)
BINARIES = $(patsubst %.c, %, $(SOURCES))

.PHONY: all clean

all: $(BINARIES)

%: %.c
	$(CC) -o $@ $<

clean:
	-rm $(BINARIES)
