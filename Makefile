PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
PROTO ?= /usr/share/wayland-protocols/staging/ext-data-control/ext-data-control-v1.xml
CFLAGS ?= -O2
CFLAGS += -Wall -Wextra -Wno-unused-parameter
GEN = wl-copy-multi/ext-data-control-v1-client-protocol.h wl-copy-multi/ext-data-control-v1-protocol.c

all: wl-copy-multi/wl-copy-multi

wl-copy-multi/wl-copy-multi: wl-copy-multi/wl-copy-multi.c $(GEN)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Iwl-copy-multi -o $@ wl-copy-multi/wl-copy-multi.c \
		wl-copy-multi/ext-data-control-v1-protocol.c $(LDFLAGS) $$(pkg-config --cflags --libs wayland-client)

wl-copy-multi/ext-data-control-v1-client-protocol.h: $(PROTO)
	wayland-scanner client-header $< $@

wl-copy-multi/ext-data-control-v1-protocol.c: $(PROTO)
	wayland-scanner private-code $< $@

install: all
	install -Dm755 bin/md-paste-rich $(DESTDIR)$(BINDIR)/md-paste-rich
	install -Dm755 bin/md-to-slack $(DESTDIR)$(BINDIR)/md-to-slack
	install -Dm755 wl-copy-multi/wl-copy-multi $(DESTDIR)$(BINDIR)/wl-copy-multi

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/md-paste-rich $(DESTDIR)$(BINDIR)/md-to-slack $(DESTDIR)$(BINDIR)/wl-copy-multi

clean:
	rm -f wl-copy-multi/wl-copy-multi $(GEN)

.PHONY: all install uninstall clean
