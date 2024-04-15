#! /usr/bin/env tclsh

package require Ffidl

ffidl::callout wl_display_create {} pointer [ffidl::symbol libwlroots.so.12 wl_display_create]
ffidl::callout wlr_backend_autocreate {pointer} pointer [ffidl::symbol libwlroots.so.12 wlr_backend_autocreate]
ffidl::callout wlr_renderer_autocreate {pointer} pointer [ffidl::symbol libwlroots.so.12 wlr_renderer_autocreate]
ffidl::callout wlr_renderer_init_wl_display {pointer pointer} int [ffidl::symbol libwlroots.so.12 wlr_renderer_init_wl_display]

set display [wl_display_create]
set backend [wlr_backend_autocreate $display]
set renderer [wlr_renderer_autocreate $backend]
puts [wlr_renderer_init_wl_display $renderer $display]
