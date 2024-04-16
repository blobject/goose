#! /usr/bin/env tclsh

package require Ffidl

# I don't know where to find wl_signal_add...
# ffidl::symbol libwayland-server.so.0.22.0  wl_signal_add

proc wlroots {function args_types ret_type} {
	ffidl::callout $function $args_types $ret_type [ffidl::symbol libwlroots.so.12 $function]
} 

::ffidl::typedef bool uint8

wlroots wl_display_create {} pointer
wlroots wlr_backend_autocreate {pointer} pointer
wlroots wlr_renderer_autocreate {pointer} pointer
wlroots wlr_renderer_init_wl_display {pointer pointer} bool
wlroots wlr_allocator_autocreate {pointer pointer} pointer
wlroots wlr_compositor_create {pointer uint32 pointer} pointer
wlroots wlr_subcompositor_create {pointer} pointer
wlroots wlr_data_device_manager_create {pointer} pointer
wlroots wlr_output_layout_create {} pointer

set display [wl_display_create]
set backend [wlr_backend_autocreate $display]
set renderer [wlr_renderer_autocreate $backend]
wlr_renderer_init_wl_display $renderer $display

set allocator [wlr_allocator_autocreate $backend $renderer]

wlr_compositor_create $display [set version 5] $renderer
wlr_subcompositor_create $display
wlr_data_device_manager_create $display

set output_layout [wlr_output_layout_create]
