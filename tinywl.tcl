#! /usr/bin/env tclsh

# wlroots-0.17

lappend auto_path lib
package require wlroots

proc show {varName} {
    upvar 1 $varName varValue
    puts "${varName}: $varValue"
}

set display [wl_display_create]
show display
# wlroots 0.18
# https://gitlab.freedesktop.org/wlroots/wlroots/-/commit/d1b39b58432c471c16e09103fd2c7850e3c41950
# set ev [wl_display_get_event_loop $display]
# set backend [wlr_backend_autocreate $ev]
set backend [wlr_backend_autocreate $display]
show backend
set renderer [wlr_renderer_autocreate $backend]
show renderer
wlr_renderer_init_wl_display $renderer $display
