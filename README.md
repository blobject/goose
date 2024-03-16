# goose

**Status:** pre-alpha

Goose is a tcl'ish wayland compositor.

## build

note1: trying out tinywl, copied src from tinywl repo, used wayland-scanner to generate protocol header and source.

note2: broken due to deprecated renderer creation (see "CUSTOM" in tinywl.c)

on void:
- install wayland-devel, wayland-protocols, wlroots0.15-devel
- `CC=clang make && ./tinywl`
