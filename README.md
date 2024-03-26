# goose

**Status:** pre-alpha

Goose is a tcl'ish wayland compositor.

## build (WIP)

- Checkout `critcl` branch.
- Follow the README on `sandbox` branch. To summarise:
  - Install pip, meson, ninja (in whatever sane way python env is managed locally)
  - Install wlroots dev dependencies.
  - Copy the entire wlroots.git to `.`
  - Build wlroots (and run tinywl to test).
- Copy `wlroots/build/libwlroots.so.13` to a recognised place.
  - For example, copy it to `/usr/local/lib/`.
  - Create a symlink there called `libwlroots.so`.
  - Edit `/etc/ld.so.conf` (or under `.d/`) if `/usr/local/lib/` is not among the loader path.
  - `sudo ldconfig`
- Install critcl.
- Back in repo dir, `critcl -pkg -I ./wlroots/include -I /usr/include/pixman-1 wlroots.tcl`
- `./tinywl.tcl`
