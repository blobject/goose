#include <assert.h>
#include <stdlib.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/util/transform.h>
#include "types/wlr_scene.h"

static void handle_scene_buffer_outputs_update(
		struct wl_listener *listener, void *data) {
	struct wlr_scene_surface *surface =
		wl_container_of(listener, surface, outputs_update);

	if (surface->buffer->primary_output == NULL) {
		return;
	}
	double scale = surface->buffer->primary_output->output->scale;
	wlr_fractional_scale_v1_notify_scale(surface->surface, scale);
	wlr_surface_set_preferred_buffer_scale(surface->surface, ceil(scale));
}

static void handle_scene_buffer_output_enter(
		struct wl_listener *listener, void *data) {
	struct wlr_scene_surface *surface =
		wl_container_of(listener, surface, output_enter);
	struct wlr_scene_output *output = data;

	wlr_surface_send_enter(surface->surface, output->output);
}

static void handle_scene_buffer_output_leave(
		struct wl_listener *listener, void *data) {
	struct wlr_scene_surface *surface =
		wl_container_of(listener, surface, output_leave);
	struct wlr_scene_output *output = data;

	wlr_surface_send_leave(surface->surface, output->output);
}

static void handle_scene_buffer_output_sample(
		struct wl_listener *listener, void *data) {
	struct wlr_scene_surface *surface =
		wl_container_of(listener, surface, output_sample);
	const struct wlr_scene_output_sample_event *event = data;
	struct wlr_scene_output *scene_output = event->output;
	if (surface->buffer->primary_output != scene_output) {
		return;
	}

	if (event->direct_scanout) {
		wlr_presentation_surface_scanned_out_on_output(surface->surface, scene_output->output);
	} else {
		wlr_presentation_surface_textured_on_output(surface->surface, scene_output->output);
	}
}

static void handle_scene_buffer_frame_done(
		struct wl_listener *listener, void *data) {
	struct wlr_scene_surface *surface =
		wl_container_of(listener, surface, frame_done);
	struct timespec *now = data;

	wlr_surface_send_frame_done(surface->surface, now);
}

static void scene_surface_handle_surface_destroy(
		struct wl_listener *listener, void *data) {
	struct wlr_scene_surface *surface =
		wl_container_of(listener, surface, surface_destroy);

	wlr_scene_node_destroy(&surface->buffer->node);
}

// This is used for wlr_scene where it unconditionally locks buffers preventing
// reuse of the existing texture for shm clients. With the usage pattern of
// wlr_scene surface handling, we can mark its locked buffer as safe
// for mutation.
static void client_buffer_mark_next_can_damage(struct wlr_client_buffer *buffer) {
	buffer->n_ignore_locks++;
}

static void scene_buffer_unmark_client_buffer(struct wlr_scene_buffer *scene_buffer) {
	if (!scene_buffer->buffer) {
		return;
	}

	struct wlr_client_buffer *buffer = wlr_client_buffer_get(scene_buffer->buffer);
	if (!buffer) {
		return;
	}

	assert(buffer->n_ignore_locks > 0);
	buffer->n_ignore_locks--;
}

static int min(int a, int b) {
	return a < b ? a : b;
}

static void surface_reconfigure(struct wlr_scene_surface *scene_surface) {
	struct wlr_scene_buffer *scene_buffer = scene_surface->buffer;
	struct wlr_surface *surface = scene_surface->surface;
	struct wlr_surface_state *state = &surface->current;

	struct wlr_fbox src_box;
	wlr_surface_get_buffer_source_box(surface, &src_box);

	pixman_region32_t opaque;
	pixman_region32_init(&opaque);
	pixman_region32_copy(&opaque, &surface->opaque_region);

	int width = state->width;
	int height = state->height;

	if (!wlr_box_empty(&scene_surface->clip)) {
		struct wlr_box *clip = &scene_surface->clip;

		int buffer_width = state->buffer_width;
		int buffer_height = state->buffer_height;
		width = min(clip->width, width - clip->x);
		height = min(clip->height, height - clip->y);

		wlr_fbox_transform(&src_box, &src_box, state->transform,
			buffer_width, buffer_height);
		wlr_output_transform_coords(state->transform, &buffer_width, &buffer_height);

		src_box.x += (double)(clip->x * buffer_width) / state->width;
		src_box.y += (double)(clip->y * buffer_height) / state->height;
		src_box.width *= (double)width / state->width;
		src_box.height *= (double)height / state->height;

		wlr_fbox_transform(&src_box, &src_box, wlr_output_transform_invert(state->transform),
			buffer_width, buffer_height);

		pixman_region32_translate(&opaque, -clip->x, -clip->y);
		pixman_region32_intersect_rect(&opaque, &opaque, 0, 0, width, height);
	}

	if (width <= 0 || height <= 0) {
		wlr_scene_buffer_set_buffer(scene_buffer, NULL);
		pixman_region32_fini(&opaque);
		return;
	}

	wlr_scene_buffer_set_opaque_region(scene_buffer, &opaque);
	wlr_scene_buffer_set_source_box(scene_buffer, &src_box);
	wlr_scene_buffer_set_dest_size(scene_buffer, width, height);
	wlr_scene_buffer_set_transform(scene_buffer, state->transform);

	scene_buffer_unmark_client_buffer(scene_buffer);

	if (surface->buffer) {
		client_buffer_mark_next_can_damage(surface->buffer);

		wlr_scene_buffer_set_buffer_with_damage(scene_buffer,
			&surface->buffer->base, &surface->buffer_damage);
	} else {
		wlr_scene_buffer_set_buffer(scene_buffer, NULL);
	}

	pixman_region32_fini(&opaque);
}

static void handle_scene_surface_surface_commit(
		struct wl_listener *listener, void *data) {
	struct wlr_scene_surface *surface =
		wl_container_of(listener, surface, surface_commit);
	struct wlr_scene_buffer *scene_buffer = surface->buffer;

	surface_reconfigure(surface);

	// If the surface has requested a frame done event, honour that. The
	// frame_callback_list will be populated in this case. We should only
	// schedule the frame however if the node is enabled and there is an
	// output intersecting, otherwise the frame done events would never reach
	// the surface anyway.
	int lx, ly;
	bool enabled = wlr_scene_node_coords(&scene_buffer->node, &lx, &ly);

	if (!wl_list_empty(&surface->surface->current.frame_callback_list) &&
			surface->buffer->primary_output != NULL && enabled) {
		wlr_output_schedule_frame(surface->buffer->primary_output->output);
	}
}

static bool scene_buffer_point_accepts_input(struct wlr_scene_buffer *scene_buffer,
		double *sx, double *sy) {
	struct wlr_scene_surface *scene_surface =
		wlr_scene_surface_try_from_buffer(scene_buffer);

	*sx += scene_surface->clip.x;
	*sy += scene_surface->clip.y;

	return wlr_surface_point_accepts_input(scene_surface->surface, *sx, *sy);
}

static void surface_addon_destroy(struct wlr_addon *addon) {
	struct wlr_scene_surface *surface = wl_container_of(addon, surface, addon);

	scene_buffer_unmark_client_buffer(surface->buffer);

	wlr_addon_finish(&surface->addon);

	wl_list_remove(&surface->outputs_update.link);
	wl_list_remove(&surface->output_enter.link);
	wl_list_remove(&surface->output_leave.link);
	wl_list_remove(&surface->output_sample.link);
	wl_list_remove(&surface->frame_done.link);
	wl_list_remove(&surface->surface_destroy.link);
	wl_list_remove(&surface->surface_commit.link);

	free(surface);
}

static const struct wlr_addon_interface surface_addon_impl = {
	.name = "wlr_scene_surface",
	.destroy = surface_addon_destroy,
};

struct wlr_scene_surface *wlr_scene_surface_try_from_buffer(
		struct wlr_scene_buffer *scene_buffer) {
	struct wlr_addon *addon = wlr_addon_find(&scene_buffer->node.addons,
		scene_buffer, &surface_addon_impl);
	if (!addon) {
		return NULL;
	}

	struct wlr_scene_surface *surface = wl_container_of(addon, surface, addon);
	return surface;
}

struct wlr_scene_surface *wlr_scene_surface_create(struct wlr_scene_tree *parent,
		struct wlr_surface *wlr_surface) {
	struct wlr_scene_surface *surface = calloc(1, sizeof(*surface));
	if (surface == NULL) {
		return NULL;
	}

	struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_create(parent, NULL);
	if (!scene_buffer) {
		free(surface);
		return NULL;
	}

	surface->buffer = scene_buffer;
	surface->surface = wlr_surface;
	scene_buffer->point_accepts_input = scene_buffer_point_accepts_input;

	surface->outputs_update.notify = handle_scene_buffer_outputs_update;
	wl_signal_add(&scene_buffer->events.outputs_update, &surface->outputs_update);

	surface->output_enter.notify = handle_scene_buffer_output_enter;
	wl_signal_add(&scene_buffer->events.output_enter, &surface->output_enter);

	surface->output_leave.notify = handle_scene_buffer_output_leave;
	wl_signal_add(&scene_buffer->events.output_leave, &surface->output_leave);

	surface->output_sample.notify = handle_scene_buffer_output_sample;
	wl_signal_add(&scene_buffer->events.output_sample, &surface->output_sample);

	surface->frame_done.notify = handle_scene_buffer_frame_done;
	wl_signal_add(&scene_buffer->events.frame_done, &surface->frame_done);

	surface->surface_destroy.notify = scene_surface_handle_surface_destroy;
	wl_signal_add(&wlr_surface->events.destroy, &surface->surface_destroy);

	surface->surface_commit.notify = handle_scene_surface_surface_commit;
	wl_signal_add(&wlr_surface->events.commit, &surface->surface_commit);

	wlr_addon_init(&surface->addon, &scene_buffer->node.addons,
		scene_buffer, &surface_addon_impl);

	surface_reconfigure(surface);

	return surface;
}

void scene_surface_set_clip(struct wlr_scene_surface *surface, struct wlr_box *clip) {
	if (wlr_box_equal(clip, &surface->clip)) {
		return;
	}

	if (clip) {
		surface->clip = *clip;
	} else {
		surface->clip = (struct wlr_box){0};
	}

	surface_reconfigure(surface);
}
