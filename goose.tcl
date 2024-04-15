#! /usr/bin/env tclsh

package require Ffidl

ffidl::callout wl_display_create {} pointer [ffidl::symbol libwlroots.so.12 wl_display_create]

wl_display_create
