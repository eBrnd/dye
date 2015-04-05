CC = clang -Wall -Werror -g

SOURCES = $(wildcard *.c)
BINARIES = $(patsubst %.c, %, $(SOURCES))

.PHONY: all clean

all: $(BINARIES)

%: %.c
	$(CC) -o $@ $<

clean:
	-rm $(BINARIES)
