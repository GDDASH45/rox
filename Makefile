CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -std=c11
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

.PHONY: all clean install uninstall
all: rox

rox: src/rox.c
	@$(CC) $(CFLAGS) -o $@ $<

install: rox
	install -Dm755 rox $(DESTDIR)$(BINDIR)/rox

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/rox

clean:
	rm -f rox
