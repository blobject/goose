#! /usr/bin/env tclsh
lappend auto_path lib
package require wlroots

set display [wl_display_create]
puts $display
# wlroots 0.18
# https://gitlab.freedesktop.org/wlroots/wlroots/-/commit/d1b39b58432c471c16e09103fd2c7850e3c41950
# set ev [wl_display_get_event_loop $display]
# set backend [wlr_backend_autocreate $ev]
set backend [wlr_backend_autocreate $display]
puts $backend
