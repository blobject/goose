# goose

**Status:** pre-alpha

Goose is a tcl'ish wayland compositor.

## build (subject to change)

- Checkout `critcl` branch.
- Follow the README on `sandbox` branch. To summarise:
  - Install pip, meson, ninja.
  - Install wlroots dev dependencies.
  - Copy the entire wlroots.git to `.`
  - Build wlroots (run tinywl to test).
- Copy `libwlroots.so.13` which should have been built in `wlroots/build/` to a recognised place.
  - For example, copy it to `/usr/local/lib/`.
  - Create a symlink called `libwlroots.so`.
  - Edit `/etc/ld.so.conf` (or under `.d/`) if `/usr/local/lib/` is not among the loader path.
- Install critcl.
- Back in repo dir, run `critcl -pkg -I ./wlroots/include -I /usr/include/pixman-1 wlroots.tcl`
- `./tinywl.tcl`
