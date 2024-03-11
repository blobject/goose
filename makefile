include config.mk
idir = include
pdir = protocols
wpdir = /usr/share/wayland-protocols
prtc = \
	$(wpdir)/stable/xdg-shell/xdg-shell \
	$(wpdir)/unstable/xdg-output/xdg-output-unstable-v1 \
	$(pdir)/wlr-layer-shell-unstable-v1 \
	$(pdir)/wlr-input-inhibitor-unstable-v1
prts = \
	$(wpdir)/unstable/pointer-constraints/pointer-constraints-unstable-v1 \
	$(pdir)/idle \
	$(pdir)/wlr-output-power-management-unstable-v1
deps = \
	gdk-pixbuf-2.0 \
	json-c \
	libevdev \
	libinput \
	pangocairo \
	pixman-1 \
	wayland-client \
	wayland-cursor \
	wayland-egl \
	wayland-protocols \
	wayland-server \
	wlroots \
	xcb \
	xkbcommon
CFLAGS += -std=c17 -I./$(idir) -I./$(bdir) -DWLR_USE_UNSTABLE
CFLAGS += $(foreach p,$(deps),$(shell pkg-config --cflags $(p)))
LDLIBS += $(foreach p,$(deps),$(shell pkg-config --libs $(p)))

.DEFAULT_GOAL = all
.PHONY: all clean prep scan

all: prep goose

prep:
	if test ! -d $(bdir); then mkdir $(bdir); fi

scan:
	$(foreach p,$(prts),$(shell \
		wayland-scanner private-code $(p).xml \
			$(bdir)/$(shell basename $(p))-protocol.c; \
		wayland-scanner server-header $(p).xml \
			$(bdir)/$(shell basename $(p))-protocol.h))
	$(foreach p,$(prtc),$(shell \
		wayland-scanner private-code $(p).xml \
			$(bdir)/$(shell basename $(p))-protocol.c; \
		wayland-scanner server-header $(p).xml \
			$(bdir)/$(shell basename $(p))-protocol.h; \
		wayland-scanner client-header $(p).xml \
			$(bdir)/$(shell basename $(p))-client-protocol.h))

goose: scan goosebump
	$(CC) $(CFLAGS) src/$@.c $(LDLIBS) -o $(bdir)/$@

goosebump:
	$(CC) $(CFLAGS) src/$@.c -shared -o $(bdir)/$@.so

clean:
	if test -d $(bdir); then rm -R $(bdir); fi

