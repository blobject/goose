#! /usr/bin/env tcl

package require critcl
if {![critcl::compiling]} {
    error "Unable to build project, no proper compiler found."
}

critcl::tcl 8.6
critcl::cflags -DWLR_USE_UNSTABLE
critcl::cheaders   -I/usr/include
critcl::clibraries -L/usr/lib/x86_64-linux-gnu
critcl::clibraries -lwayland-server
critcl::clibraries -lwlroots
critcl::include wayland-server-core.h
critcl::include wlr/backend.h

critcl::argtype ulong {unsigned long}

critcl::cproc wl_display_create {} {unsigned long} {
    return (unsigned long) wl_display_create();
}

critcl::cproc wl_display_get_event_loop {ulong wl_disp} {unsigned long} {
    return (unsigned long) wl_display_get_event_loop(wl_disp);
}

critcl::cproc wlr_backend_autocreate {ulong event_loop ulong session} {unsigned long} {
    return (unsigned long) wlr_backend_autocreate(event_loop, NULL);
}

critcl::msg -nonewline { Building ...}
if {![critcl::load]} {
    error "Building and loading the project failed."c
}

package provide wlroots 1
