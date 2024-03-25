#include <assert.h>
#include <libinput.h>
#include <stdlib.h>
#include <stdio.h>
#include <wlr/backend/interface.h>
#include <wlr/backend/session.h>
#include <wlr/util/log.h>
#include "backend/libinput.h"
#include "util/env.h"

static struct wlr_libinput_backend *get_libinput_backend_from_backend(
		struct wlr_backend *wlr_backend) {
	assert(wlr_backend_is_libinput(wlr_backend));
	struct wlr_libinput_backend *backend = wl_container_of(wlr_backend, backend, backend);
	return backend;
}

static int libinput_open_restricted(const char *path,
		int flags, void *_backend) {
	struct wlr_libinput_backend *backend = _backend;
	struct wlr_device *dev = wlr_session_open_file(backend->session, path);
	if (dev == NULL) {
		return -1;
	}
	return dev->fd;
}

static void libinput_close_restricted(int fd, void *_backend) {
	struct wlr_libinput_backend *backend = _backend;

	struct wlr_device *dev;
	bool found = false;
	wl_list_for_each(dev, &backend->session->devices, link) {
		if (dev->fd == fd) {
			found = true;
			break;
		}
	}
	if (found) {
		wlr_session_close_file(backend->session, dev);
	}
}

static const struct libinput_interface libinput_impl = {
	.open_restricted = libinput_open_restricted,
	.close_restricted = libinput_close_restricted
};

static int handle_libinput_readable(int fd, uint32_t mask, void *_backend) {
	struct wlr_libinput_backend *backend = _backend;
	int ret = libinput_dispatch(backend->libinput_context);
	if (ret != 0) {
		wlr_log(WLR_ERROR, "Failed to dispatch libinput: %s", strerror(-ret));
		wlr_backend_destroy(&backend->backend);
		return 0;
	}
	struct libinput_event *event;
	while ((event = libinput_get_event(backend->libinput_context))) {
		handle_libinput_event(backend, event);
		libinput_event_destroy(event);
	}
	return 0;
}

static enum wlr_log_importance libinput_log_priority_to_wlr(
		enum libinput_log_priority priority) {
	switch (priority) {
	case LIBINPUT_LOG_PRIORITY_ERROR:
		return WLR_ERROR;
	case LIBINPUT_LOG_PRIORITY_INFO:
		return WLR_INFO;
	default:
		return WLR_DEBUG;
	}
}

static void log_libinput(struct libinput *libinput_context,
		enum libinput_log_priority priority, const char *fmt, va_list args) {
	enum wlr_log_importance importance = libinput_log_priority_to_wlr(priority);
	static char wlr_fmt[1024];
	snprintf(wlr_fmt, sizeof(wlr_fmt), "[libinput] %s", fmt);
	_wlr_vlog(importance, wlr_fmt, args);
}

static bool backend_start(struct wlr_backend *wlr_backend) {
	struct wlr_libinput_backend *backend =
		get_libinput_backend_from_backend(wlr_backend);
	wlr_log(WLR_DEBUG, "Starting libinput backend");

	backend->libinput_context = libinput_udev_create_context(&libinput_impl,
		backend, backend->session->udev);
	if (!backend->libinput_context) {
		wlr_log(WLR_ERROR, "Failed to create libinput context");
		return false;
	}

	if (libinput_udev_assign_seat(backend->libinput_context,
			backend->session->seat) != 0) {
		wlr_log(WLR_ERROR, "Failed to assign libinput seat");
		return false;
	}

	// TODO: More sophisticated logging
	libinput_log_set_handler(backend->libinput_context, log_libinput);
	libinput_log_set_priority(backend->libinput_context, LIBINPUT_LOG_PRIORITY_ERROR);

	int libinput_fd = libinput_get_fd(backend->libinput_context);

	if (!env_parse_bool("WLR_LIBINPUT_NO_DEVICES") && wl_list_empty(&backend->devices)) {
		handle_libinput_readable(libinput_fd, WL_EVENT_READABLE, backend);
		if (wl_list_empty(&backend->devices)) {
			wlr_log(WLR_ERROR, "libinput initialization failed, no input devices");
			wlr_log(WLR_ERROR, "Set WLR_LIBINPUT_NO_DEVICES=1 to suppress this check");
			return false;
		}
	}

	if (backend->input_event) {
		wl_event_source_remove(backend->input_event);
	}
	backend->input_event = wl_event_loop_add_fd(backend->session->event_loop, libinput_fd,
			WL_EVENT_READABLE, handle_libinput_readable, backend);
	if (!backend->input_event) {
		wlr_log(WLR_ERROR, "Failed to create input event on event loop");
		return false;
	}
	wlr_log(WLR_DEBUG, "libinput successfully initialized");
	return true;
}

static void backend_destroy(struct wlr_backend *wlr_backend) {
	if (!wlr_backend) {
		return;
	}
	struct wlr_libinput_backend *backend =
		get_libinput_backend_from_backend(wlr_backend);

	struct wlr_libinput_input_device *dev, *tmp;
	wl_list_for_each_safe(dev, tmp, &backend->devices, link) {
		destroy_libinput_input_device(dev);
	}

	wlr_backend_finish(wlr_backend);

	wl_list_remove(&backend->session_destroy.link);
	wl_list_remove(&backend->session_signal.link);

	if (backend->input_event) {
		wl_event_source_remove(backend->input_event);
	}
	libinput_unref(backend->libinput_context);
	free(backend);
}

static const struct wlr_backend_impl backend_impl = {
	.start = backend_start,
	.destroy = backend_destroy,
};

bool wlr_backend_is_libinput(struct wlr_backend *b) {
	return b->impl == &backend_impl;
}

static void session_signal(struct wl_listener *listener, void *data) {
	struct wlr_libinput_backend *backend =
		wl_container_of(listener, backend, session_signal);
	struct wlr_session *session = backend->session;

	if (!backend->libinput_context) {
		return;
	}

	if (session->active) {
		libinput_resume(backend->libinput_context);
	} else {
		libinput_suspend(backend->libinput_context);
	}
}

static void handle_session_destroy(struct wl_listener *listener, void *data) {
	struct wlr_libinput_backend *backend =
		wl_container_of(listener, backend, session_destroy);
	backend_destroy(&backend->backend);
}

struct wlr_backend *wlr_libinput_backend_create(struct wlr_session *session) {
	struct wlr_libinput_backend *backend = calloc(1, sizeof(*backend));
	if (!backend) {
		wlr_log(WLR_ERROR, "Allocation failed: %s", strerror(errno));
		return NULL;
	}
	wlr_backend_init(&backend->backend, &backend_impl);

	wl_list_init(&backend->devices);

	backend->session = session;

	backend->session_signal.notify = session_signal;
	wl_signal_add(&session->events.active, &backend->session_signal);

	backend->session_destroy.notify = handle_session_destroy;
	wl_signal_add(&session->events.destroy, &backend->session_destroy);

	return &backend->backend;
}

struct libinput_device *wlr_libinput_get_device_handle(
		struct wlr_input_device *wlr_dev) {
	struct wlr_libinput_input_device *dev = NULL;
	switch (wlr_dev->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		dev = device_from_keyboard(wlr_keyboard_from_input_device(wlr_dev));
		break;
	case WLR_INPUT_DEVICE_POINTER:
		dev = device_from_pointer(wlr_pointer_from_input_device(wlr_dev));
		break;
	case WLR_INPUT_DEVICE_SWITCH:
		dev = device_from_switch(wlr_switch_from_input_device(wlr_dev));
		break;
	case WLR_INPUT_DEVICE_TOUCH:
		dev = device_from_touch(wlr_touch_from_input_device(wlr_dev));
		break;
	case WLR_INPUT_DEVICE_TABLET:
		dev = device_from_tablet(wlr_tablet_from_input_device(wlr_dev));
		break;
	case WLR_INPUT_DEVICE_TABLET_PAD:
		dev = device_from_tablet_pad(wlr_tablet_pad_from_input_device(wlr_dev));
		break;
	}
	return dev->handle;
}

uint32_t usec_to_msec(uint64_t usec) {
	return (uint32_t)(usec / 1000);
}

const char *get_libinput_device_name(struct libinput_device *device) {
	// libinput guarantees that the name is non-NULL, and an empty string if
	// unset. However wlroots uses NULL to indicate that the name is unset.
	const char *name = libinput_device_get_name(device);
	if (name[0] == '\0') {
		return NULL;
	}
	return name;
}
