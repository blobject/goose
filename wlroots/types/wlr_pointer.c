#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wlr/interfaces/wlr_pointer.h>
#include <wlr/types/wlr_pointer.h>

#include "interfaces/wlr_input_device.h"

struct wlr_pointer *wlr_pointer_from_input_device(
		struct wlr_input_device *input_device) {
	assert(input_device->type == WLR_INPUT_DEVICE_POINTER);
	return wl_container_of(input_device, (struct wlr_pointer *)NULL, base);
}

void wlr_pointer_init(struct wlr_pointer *pointer,
		const struct wlr_pointer_impl *impl, const char *name) {
	*pointer = (struct wlr_pointer){
		.impl = impl,
	};
	wlr_input_device_init(&pointer->base, WLR_INPUT_DEVICE_POINTER, name);

	wl_signal_init(&pointer->events.motion);
	wl_signal_init(&pointer->events.motion_absolute);
	wl_signal_init(&pointer->events.button);
	wl_signal_init(&pointer->events.axis);
	wl_signal_init(&pointer->events.frame);
	wl_signal_init(&pointer->events.swipe_begin);
	wl_signal_init(&pointer->events.swipe_update);
	wl_signal_init(&pointer->events.swipe_end);
	wl_signal_init(&pointer->events.pinch_begin);
	wl_signal_init(&pointer->events.pinch_update);
	wl_signal_init(&pointer->events.pinch_end);
	wl_signal_init(&pointer->events.hold_begin);
	wl_signal_init(&pointer->events.hold_end);
}

void wlr_pointer_finish(struct wlr_pointer *pointer) {
	wlr_input_device_finish(&pointer->base);

	free(pointer->output_name);
}
