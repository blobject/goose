# goose

**Status:** pre-alpha

Goose is a tcl'ish wayland compositor.

## try out tinywl (on void)

```
cd wlroots
```

**install python pip:**
```
xbps-install -S python3-pip
```
- Maybe also setup a virtual python environment in a way that you like.

**install meson and ninja:**
```
pip install meson ninja
```

**install wlroots requirements:**
```
xbps-install -S \
  wayland-devel wayland-protocols \
  elogind-devel libseat-devel \
  cairo-devel libgbm-devel libinput-devel libxkbcommon-devel \
  xcb-util-errors-devel xcb-util-renderutil-devel xcb-util-wm-devel \
  libdisplay-info-devel libdrm-devel libglvnd-devel libliftoff-devel Vulkan-Headers
```
- Above packages were missing on my local machine -- there may be more requirements.

**prepare wlroots build:**
```
meson setup --wipe build
```
- Repeat this if the setup outputs `NO` on some (optional) requirements.

**build wlroots:**
```
ninja -C build
```

**run tinywl:**
```
./build/tinywl/tinywl
```
