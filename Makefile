PREFIX=/usr/local
DESTDIR=

CC=gcc
XUAKE_OBJFILES=xdg-shell-protocol.o xuake.o readpng.o ft.o term.o xwayland.o \
    signals.o xkw_battery.o xkw_clock.o  selection.o widgets.o lua.o key.o \
    xuake-control.o output.o input.o decoration.o surface.o
XKTERM_OBJFILES=xkterm.o xdg-shell-protocol.o term.o xkw_battery.o xkw_clock.o ft.o \
    xktlua.o
XK_OBJFILES=xk.o xuake-control.o

WAYLAND_PROTOCOLS=$(shell pkg-config --variable=pkgdatadir wayland-protocols)
WAYLAND_SCANNER=$(shell pkg-config --variable=wayland_scanner wayland-scanner)

WAYLAND_GENSRC=xdg-shell-protocol.h xdg-shell-protocol.c xdg-shell-client-protocol.h \
    xuake-control-server.h xuake-control-client.h xuake-control.c

CFLAGS=-ggdb -Werror -I. \
    -DWLR_USE_UNSTABLE \
    $(shell pkg-config --cflags wlroots) \
    $(shell pkg-config --cflags wayland-server) \
    $(shell pkg-config --cflags pixman-1) \
    $(shell pkg-config --cflags libpng) \
    $(shell pkg-config --cflags zlib) \
    $(shell pkg-config --cflags xkbcommon) \
    $(shell pkg-config --cflags libtsm) \
    $(shell pkg-config --cflags xcb) \
    $(shell pkg-config --cflags lua)

XUAKE_LIBS=\
    -lutil -lm \
    $(shell pkg-config --libs wlroots) \
    $(shell pkg-config --libs wayland-server) \
    $(shell pkg-config --libs pixman-1) \
    $(shell pkg-config --libs libpng) \
    $(shell pkg-config --libs zlib) \
    $(shell pkg-config --libs xkbcommon) \
    $(shell pkg-config --libs libtsm) \
    $(shell pkg-config --libs xcb) \
    $(shell pkg-config --libs lua)

XKTERM_LIBS= \
    -lrt -lm -lutil \
    $(shell pkg-config --libs wlroots) \
    $(shell pkg-config --libs wayland-server) \
    $(shell pkg-config --libs wayland-client) \
    $(shell pkg-config --libs wayland-egl) \
    $(shell pkg-config --libs egl) \
    $(shell pkg-config --libs gl) \
    $(shell pkg-config --libs glesv2) \
    $(shell pkg-config --libs libpng) \
    $(shell pkg-config --libs zlib) \
    $(shell pkg-config --libs libtsm) \
    $(shell pkg-config --libs xkbcommon) \
    $(shell pkg-config --libs lua)

XK_LIBS= \
    $(shell pkg-config --libs wayland-client)
    

# wayland-scanner is a tool which generates C headers and rigging for Wayland
# protocols, which are specified in XML. wlroots requires you to rig these up
# to your build system yourself and provide them in the include path.
xdg-shell-protocol.h:
	$(WAYLAND_SCANNER) server-header $(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@

xdg-shell-protocol.c: xdg-shell-protocol.h
	$(WAYLAND_SCANNER) private-code $(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@

xdg-shell-client-protocol.h:
	$(WAYLAND_SCANNER) client-header $(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@

xuake-control-server.h: xuake_control_v1.xml
	$(WAYLAND_SCANNER) server-header xuake_control_v1.xml $@

xuake-control-client.h: xuake_control_v1.xml
	$(WAYLAND_SCANNER) client-header xuake_control_v1.xml $@

xuake-control.c: xuake_control_v1.xml
	$(WAYLAND_SCANNER) private-code xuake_control_v1.xml $@

xktlua.o: lua.c
	$(CC) $(CFLAGS) -DXKTERM_BUILD -c -o $@ $<

xuake: xuake-control-server.h $(XUAKE_OBJFILES)
	$(CC) $(CFLAGS) -o $@ $(XUAKE_OBJFILES) $(XUAKE_LIBS)

xkterm: xdg-shell-client-protocol.h $(XKTERM_OBJFILES)
	$(CC) $(CFLAGS) -o $@ $(XKTERM_OBJFILES) $(XKTERM_LIBS) 

xk: xuake-control-client.h $(XK_OBJFILES)
	$(CC) $(CFLAGS) -o $@ $(XK_OBJFILES) $(XK_LIBS)

all: xuake xkterm xk

install: xuake xkterm xk
	cp $< $(DESTDIR)$(PREFIX)/bin/$<
	chown root.root $(DESTDIR)$(PREFIX)/bin/$<
	chmod 4755 $(DESTDIR)$(PREFIX)/bin/$<
	cp xkterm $(DESTDIR)$(PREFIX)/bin/xkterm
	chown root.root $(DESTDIR)$(PREFIX)/bin/xkterm
	cp xk $(DESTDIR)$(PREFIX)/bin/xk

clean:
	rm -f xuake xk xkterm $(WAYLAND_GENSRC) $(XUAKE_OBJFILES) $(XKTERM_OBJFILES) $(XK_OBJFILES)

.DEFAULT_GOAL=all
.PHONY: clean
