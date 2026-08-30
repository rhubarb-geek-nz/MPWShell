# Copyright (c) 2026 Roger Brown.
# Licensed under the MIT License.

SRC=main.c window.c messages.c ../src/mpwtools.c ../src/mbcs.c
CFLAGS=-Wall -Werror
MPWSHELL_VERSION=0.9.4
MPWSHELL_ABOUT="MPW Shell, $(MPWSHELL_VERSION)\nCopyright \xC2\xA9 2026 Roger Brown"
PREFIX=/usr/local

all: mpwshell

mpwshell: $(SRC) ../src/mpwtools.h ../src/mbcs.h mpwshell.h mpwshell.xpm Makefile
	$(CC) $(CFLAGS) $(SRC) -o $@ -I../src -lXm -lXt -lX11 -lXpm -DMPWSHELL_ABOUT=\"$(MPWSHELL_ABOUT)\"

clean:
	rm -rf mpwshell
	rm -rf mpwshell*.deb mpwshell*.rpm

test: mpwshell
	./mpwshell

dist: mpwshell
	./package.sh $(MPWSHELL_VERSION)

install: mpwshell
	install -d "$(DESTDIR)$(PREFIX)/bin"
	install mpwshell "$(DESTDIR)$(PREFIX)/bin/mpwshell"
