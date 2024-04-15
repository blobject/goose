#! /usr/bin/env tclsh

package require Ffidl

proc wlroots {function args_types ret_type} {
	ffidl::callout $function $args_types $ret_type [ffidl::symbol libwlroots.so.12 $function]
} 

wlroots wl_display_create {} pointer
wlroots wlr_backend_autocreate {pointer} pointer
wlroots wlr_renderer_autocreate {pointer} pointer
wlroots wlr_renderer_init_wl_display {pointer pointer} int

set display [wl_display_create]
set backend [wlr_backend_autocreate $display]
set renderer [wlr_renderer_autocreate $backend]
puts [wlr_renderer_init_wl_display $renderer $display]
