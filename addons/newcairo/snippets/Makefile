#
# trial/Makefile
#
# Small C snippets for testing different Cairo features actually work.
#

CAIRO_CFLAGS=$(shell pkg-config --cflags cairo)
CAIRO_LIBS=$(shell pkg-config --libs cairo)

FT2_CFLAGS=$(shell pkg-config --cflags freetype2)
FT2_LIBS=$(shell pkg-config --libs freetype2)

C_FILES=$(wildcard *.c)
TARGETS=$(C_FILES:.c=)

all: hello

# Compile a.c -> ./a, run and open results
#
%: %.c
	gcc -o $@.exe --std=c99 $(CAIRO_CFLAGS) $(FT2_CFLAGS) $(CAIRO_LIBS) $(FT2_LIBS) $<
	time ./$@.exe
	open out.png

clean:
	-rm out.png $(TARGETS)

.phony: $(TARGETS)
