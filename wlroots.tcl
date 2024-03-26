#! /usr/bin/env tcl

package require critcl
if {![critcl::compiling]} {
    error "Unable to build project, no proper compiler found."
}

critcl::tcl 8.6

critcl::cheaders   -I/usr/include
critcl::clibraries -L/usr/lib/x86_64-linux-gnu
critcl::clibraries -lwayland-server
critcl::include wayland-server-core.h

critcl::cproc wl_display_create {} void {
    struct wl_display *wl_display;
    wl_display = wl_display_create();
    printf("hello world\n");
}

critcl::msg -nonewline { Building ...}
if {![critcl::load]} {
    error "Building and loading the project failed."
}

package provide wlroots 1
