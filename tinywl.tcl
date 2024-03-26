#! /usr/bin/env tclsh
lappend auto_path lib
package require wlroots

set display [wl_display_create]
puts $display
set ev [wl_display_get_event_loop $display]
puts $ev
set backend [wlr_backend_autocreate $ev 0]
puts $backend
