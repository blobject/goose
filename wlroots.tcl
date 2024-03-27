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

proc opaquePointerType {type} {
    critcl::argtype $type "
        sscanf(Tcl_GetString(@@), \"($type) 0x%p\", &@A);
    " $type

    critcl::resulttype $type "
        Tcl_SetObjResult(interp, Tcl_ObjPrintf(\"($type) 0x%lx\", (uintptr_t) rv));
        return TCL_OK;
    " $type
}
opaquePointerType void*

critcl::cproc wl_display_create {} void* {
    return wl_display_create();
}

critcl::cproc wl_display_get_event_loop {void* wl_disp} void* {
    return wl_display_get_event_loop(wl_disp);
}

critcl::cproc wlr_backend_autocreate {void* event_loop void* {session "(void*) 0x0"}} void* {
    return wlr_backend_autocreate(event_loop, session);
}

critcl::cproc wlr_renderer_autocreate {void* backend} void* {
    return wlr_renderer_autocreate(backend);
}

critcl::cproc wlr_renderer_init_wl_display {void* renderer void* display} void {
    wlr_renderer_init_wl_display(renderer, display);
}


critcl::msg -nonewline { Building ...}
if {![critcl::load]} {
    error "Building and loading the project failed."
}

package provide wlroots 1
