#include <fcntl.h>
#include <libliftoff.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wlr/util/log.h>

#include "backend/drm/drm.h"
#include "backend/drm/fb.h"
#include "backend/drm/iface.h"

static bool init(struct wlr_drm_backend *drm) {
	// TODO: lower log level
	liftoff_log_set_priority(LIFTOFF_DEBUG);

	int drm_fd = fcntl(drm->fd, F_DUPFD_CLOEXEC, 0);
	if (drm_fd < 0) {
		wlr_log_errno(WLR_ERROR, "fcntl(F_DUPFD_CLOEXEC) failed");
		return false;
	}

	drm->liftoff = liftoff_device_create(drm_fd);
	if (!drm->liftoff) {
		wlr_log(WLR_ERROR, "Failed to create liftoff device");
		close(drm_fd);
		return false;
	}

	for (size_t i = 0; i < drm->num_planes; i++) {
		struct wlr_drm_plane *plane = &drm->planes[i];
		if (plane->initial_crtc_id != 0) {
			continue;
		}
		plane->liftoff = liftoff_plane_create(drm->liftoff, plane->id);
		if (plane->liftoff == NULL) {
			wlr_log(WLR_ERROR, "Failed to create liftoff plane");
			return false;
		}
	}

	for (size_t i = 0; i < drm->num_crtcs; i++) {
		struct wlr_drm_crtc *crtc = &drm->crtcs[i];

		crtc->liftoff = liftoff_output_create(drm->liftoff, crtc->id);
		if (!crtc->liftoff) {
			wlr_log(WLR_ERROR, "Failed to create liftoff output");
			return false;
		}

		crtc->liftoff_composition_layer = liftoff_layer_create(crtc->liftoff);
		if (!crtc->liftoff_composition_layer) {
			wlr_log(WLR_ERROR, "Failed to create liftoff composition layer");
			return false;
		}
		liftoff_output_set_composition_layer(crtc->liftoff,
			crtc->liftoff_composition_layer);

		if (crtc->primary) {
			crtc->primary->liftoff_layer = liftoff_layer_create(crtc->liftoff);
			if (!crtc->primary->liftoff_layer) {
				wlr_log(WLR_ERROR, "Failed to create liftoff layer for primary plane");
				return false;
			}
		}

		if (crtc->cursor) {
			crtc->cursor->liftoff_layer = liftoff_layer_create(crtc->liftoff);
			if (!crtc->cursor->liftoff_layer) {
				wlr_log(WLR_ERROR, "Failed to create liftoff layer for cursor plane");
				return false;
			}
		}
	}

	return true;
}

static bool register_planes_for_crtc(struct wlr_drm_backend *drm,
		struct wlr_drm_crtc *crtc) {
	// When performing the first modeset on a CRTC, we need to be a bit careful
	// when it comes to planes: we don't want to allow libliftoff to make use
	// of planes currently already in-use on another CRTC. We need to wait for
	// a modeset to happen on the other CRTC before being able to use these.
	for (size_t i = 0; i < drm->num_planes; i++) {
		struct wlr_drm_plane *plane = &drm->planes[i];
		if (plane->liftoff != NULL || plane->initial_crtc_id != crtc->id) {
			continue;
		}
		plane->liftoff = liftoff_plane_create(drm->liftoff, plane->id);
		if (plane->liftoff == NULL) {
			wlr_log(WLR_ERROR, "Failed to create liftoff plane");
			return false;
		}
	}
	return true;
}

static void finish(struct wlr_drm_backend *drm) {
	for (size_t i = 0; i < drm->num_crtcs; i++) {
		struct wlr_drm_crtc *crtc = &drm->crtcs[i];

		if (crtc->primary) {
			liftoff_layer_destroy(crtc->primary->liftoff_layer);
		}
		if (crtc->cursor) {
			liftoff_layer_destroy(crtc->cursor->liftoff_layer);
		}

		liftoff_layer_destroy(crtc->liftoff_composition_layer);
		liftoff_output_destroy(crtc->liftoff);
	}

	for (size_t i = 0; i < drm->num_planes; i++) {
		struct wlr_drm_plane *plane = &drm->planes[i];
		liftoff_plane_destroy(plane->liftoff);
	}

	liftoff_device_destroy(drm->liftoff);
}

static bool add_prop(drmModeAtomicReq *req, uint32_t obj,
		uint32_t prop, uint64_t val) {
	if (drmModeAtomicAddProperty(req, obj, prop, val) < 0) {
		wlr_log_errno(WLR_ERROR, "drmModeAtomicAddProperty failed");
		return false;
	}
	return true;
}

static bool set_plane_props(struct wlr_drm_plane *plane,
		struct liftoff_layer *layer, struct wlr_drm_fb *fb, int32_t x, int32_t y, uint64_t zpos) {
	if (fb == NULL) {
		wlr_log(WLR_ERROR, "Failed to acquire FB for plane %"PRIu32, plane->id);
		return false;
	}

	uint32_t width = fb->wlr_buf->width;
	uint32_t height = fb->wlr_buf->height;

	// The SRC_* properties are in 16.16 fixed point
	return liftoff_layer_set_property(layer, "zpos", zpos) == 0 &&
		liftoff_layer_set_property(layer, "SRC_X", 0) == 0 &&
		liftoff_layer_set_property(layer, "SRC_Y", 0) == 0 &&
		liftoff_layer_set_property(layer, "SRC_W", (uint64_t)width << 16) == 0 &&
		liftoff_layer_set_property(layer, "SRC_H", (uint64_t)height << 16) == 0 &&
		liftoff_layer_set_property(layer, "CRTC_X", (uint64_t)x) == 0 &&
		liftoff_layer_set_property(layer, "CRTC_Y", (uint64_t)y) == 0 &&
		liftoff_layer_set_property(layer, "CRTC_W", width) == 0 &&
		liftoff_layer_set_property(layer, "CRTC_H", height) == 0 &&
		liftoff_layer_set_property(layer, "FB_ID", fb->id) == 0;
}

static bool disable_plane(struct wlr_drm_plane *plane) {
	return liftoff_layer_set_property(plane->liftoff_layer, "FB_ID", 0) == 0;
}

static uint64_t to_fp16(double v) {
	return (uint64_t)round(v * (1 << 16));
}

static bool set_layer_props(struct wlr_drm_backend *drm,
		const struct wlr_output_layer_state *state, uint64_t zpos,
		struct wl_array *fb_damage_clips_arr) {
	struct wlr_drm_layer *layer = get_drm_layer(drm, state->layer);

	uint32_t width = 0, height = 0;
	if (state->buffer != NULL) {
		width = state->buffer->width;
		height = state->buffer->height;
	}

	struct wlr_drm_fb *fb = layer->pending_fb;
	int ret = 0;
	if (state->buffer == NULL) {
		ret = liftoff_layer_set_property(layer->liftoff, "FB_ID", 0);
	} else if (fb == NULL) {
		liftoff_layer_set_fb_composited(layer->liftoff);
	} else {
		ret = liftoff_layer_set_property(layer->liftoff, "FB_ID", fb->id);
	}
	if (ret != 0) {
		return false;
	}

	uint64_t crtc_x = (uint64_t)state->dst_box.x;
	uint64_t crtc_y = (uint64_t)state->dst_box.y;
	uint64_t crtc_w = (uint64_t)state->dst_box.width;
	uint64_t crtc_h = (uint64_t)state->dst_box.height;

	struct wlr_fbox src_box = state->src_box;
	if (wlr_fbox_empty(&src_box)) {
		src_box = (struct wlr_fbox){
			.width = width,
			.height = height,
		};
	}

	uint64_t src_x = to_fp16(src_box.x);
	uint64_t src_y = to_fp16(src_box.y);
	uint64_t src_w = to_fp16(src_box.width);
	uint64_t src_h = to_fp16(src_box.height);

	uint32_t fb_damage_clips = 0;
	if (state->damage != NULL) {
		uint32_t *ptr = wl_array_add(fb_damage_clips_arr, sizeof(fb_damage_clips));
		if (ptr == NULL) {
			return false;
		}
		create_fb_damage_clips_blob(drm, width, height,
			state->damage, &fb_damage_clips);
		*ptr = fb_damage_clips;
	}

	return
		liftoff_layer_set_property(layer->liftoff, "zpos", zpos) == 0 &&
		liftoff_layer_set_property(layer->liftoff, "CRTC_X", crtc_x) == 0 &&
		liftoff_layer_set_property(layer->liftoff, "CRTC_Y", crtc_y) == 0 &&
		liftoff_layer_set_property(layer->liftoff, "CRTC_W", crtc_w) == 0 &&
		liftoff_layer_set_property(layer->liftoff, "CRTC_H", crtc_h) == 0 &&
		liftoff_layer_set_property(layer->liftoff, "SRC_X", src_x) == 0 &&
		liftoff_layer_set_property(layer->liftoff, "SRC_Y", src_y) == 0 &&
		liftoff_layer_set_property(layer->liftoff, "SRC_W", src_w) == 0 &&
		liftoff_layer_set_property(layer->liftoff, "SRC_H", src_h) == 0 &&
		liftoff_layer_set_property(layer->liftoff, "FB_DAMAGE_CLIPS", fb_damage_clips) == 0;
}

static bool devid_from_fd(int fd, dev_t *devid) {
	struct stat stat;
	if (fstat(fd, &stat) != 0) {
		wlr_log_errno(WLR_ERROR, "fstat failed");
		return false;
	}
	*devid = stat.st_rdev;
	return true;
}

static void update_layer_feedback(struct wlr_drm_backend *drm,
		struct wlr_drm_layer *layer) {
	bool changed = false;
	for (size_t i = 0; i < drm->num_planes; i++) {
		struct wlr_drm_plane *plane = &drm->planes[i];
		bool is_candidate = liftoff_layer_is_candidate_plane(layer->liftoff,
			plane->liftoff);
		if (layer->candidate_planes[i] != is_candidate) {
			layer->candidate_planes[i] = is_candidate;
			changed = true;
		}
	}
	if (!changed) {
		return;
	}

	dev_t target_device;
	if (!devid_from_fd(drm->fd, &target_device)) {
		return;
	}

	struct wlr_drm_format_set formats = {0};
	for (size_t i = 0; i < drm->num_planes; i++) {
		struct wlr_drm_plane *plane = &drm->planes[i];
		if (!layer->candidate_planes[i]) {
			continue;
		}

		for (size_t j = 0; j < plane->formats.len; j++) {
			const struct wlr_drm_format *format = &plane->formats.formats[j];
			for (size_t k = 0; k < format->len; k++) {
				wlr_drm_format_set_add(&formats, format->format,
					format->modifiers[k]);
			}
		}
	}

	struct wlr_output_layer_feedback_event event = {
		.target_device = target_device,
		.formats = &formats,
	};
	wl_signal_emit_mutable(&layer->wlr->events.feedback, &event);

	wlr_drm_format_set_finish(&formats);
}

static bool add_connector(drmModeAtomicReq *req,
		const struct wlr_drm_connector_state *state,
		bool modeset, struct wl_array *fb_damage_clips_arr) {
	struct wlr_drm_connector *conn = state->connector;
	struct wlr_drm_crtc *crtc = conn->crtc;
	struct wlr_drm_backend *drm = conn->backend;
	bool active = state->active;
	bool ok = true;

	ok = ok && add_prop(req, conn->id, conn->props.crtc_id,
		active ? crtc->id : 0);
	if (modeset && active && conn->props.link_status != 0) {
		ok = ok && add_prop(req, conn->id, conn->props.link_status,
			DRM_MODE_LINK_STATUS_GOOD);
	}
	if (active && conn->props.content_type != 0) {
		ok = ok && add_prop(req, conn->id, conn->props.content_type,
			DRM_MODE_CONTENT_TYPE_GRAPHICS);
	}
	// TODO: set "max bpc"
	ok = ok &&
		add_prop(req, crtc->id, crtc->props.mode_id, state->mode_id) &&
		add_prop(req, crtc->id, crtc->props.active, active);
	if (active) {
		if (crtc->props.gamma_lut != 0) {
			ok = ok && add_prop(req, crtc->id, crtc->props.gamma_lut, state->gamma_lut);
		}
		if (crtc->props.vrr_enabled != 0) {
			ok = ok && add_prop(req, crtc->id, crtc->props.vrr_enabled, state->vrr_enabled);
		}
		ok = ok &&
			set_plane_props(crtc->primary, crtc->primary->liftoff_layer, state->primary_fb, 0, 0, 0) &&
			set_plane_props(crtc->primary, crtc->liftoff_composition_layer, state->primary_fb, 0, 0, 0);
		liftoff_layer_set_property(crtc->primary->liftoff_layer,
			"FB_DAMAGE_CLIPS", state->fb_damage_clips);
		liftoff_layer_set_property(crtc->liftoff_composition_layer,
			"FB_DAMAGE_CLIPS", state->fb_damage_clips);

		if (state->base->committed & WLR_OUTPUT_STATE_LAYERS) {
			for (size_t i = 0; i < state->base->layers_len; i++) {
				const struct wlr_output_layer_state *layer_state = &state->base->layers[i];
				ok = ok && set_layer_props(drm, layer_state, i + 1,
					fb_damage_clips_arr);
			}
		}

		if (crtc->cursor) {
			if (drm_connector_is_cursor_visible(conn)) {
				ok = ok && set_plane_props(crtc->cursor, crtc->cursor->liftoff_layer,
					state->cursor_fb, conn->cursor_x, conn->cursor_y,
					wl_list_length(&crtc->layers) + 1);
			} else {
				ok = ok && disable_plane(crtc->cursor);
			}
		}
	} else {
		ok = ok && disable_plane(crtc->primary);
		if (crtc->cursor) {
			ok = ok && disable_plane(crtc->cursor);
		}
	}

	return ok;
}

static void connector_update_layers_feedback(const struct wlr_drm_connector_state *state) {
	struct wlr_drm_backend *drm = state->connector->backend;

	if (!(state->base->committed & WLR_OUTPUT_STATE_LAYERS)) {
		return;
	}

	for (size_t i = 0; i < state->base->layers_len; i++) {
		struct wlr_output_layer_state *layer_state = &state->base->layers[i];
		struct wlr_drm_layer *layer = get_drm_layer(drm, layer_state->layer);
		layer_state->accepted =
			!liftoff_layer_needs_composition(layer->liftoff);
		if (!layer_state->accepted) {
			update_layer_feedback(drm, layer);
		}
	}
}

static bool commit(struct wlr_drm_backend *drm,
		const struct wlr_drm_device_state *state,
		struct wlr_drm_page_flip *page_flip, uint32_t flags, bool test_only) {
	bool ok = false;
	struct wl_array fb_damage_clips_arr = {0};
	drmModeAtomicReq *req = NULL;

	if (test_only) {
		flags |= DRM_MODE_ATOMIC_TEST_ONLY;
	}
	if (state->modeset) {
		flags |= DRM_MODE_ATOMIC_ALLOW_MODESET;
	}
	if (!test_only && state->nonblock) {
		flags |= DRM_MODE_ATOMIC_NONBLOCK;
	}

	for (size_t i = 0; i < state->connectors_len; i++) {
		struct wlr_drm_connector_state *conn_state = &state->connectors[i];
		struct wlr_drm_connector *conn = conn_state->connector;
		if (state->modeset && !register_planes_for_crtc(drm, conn->crtc)) {
			goto out;
		}
		if (!drm_atomic_connector_prepare(conn_state, state->modeset)) {
			goto out;
		}
	}

	req = drmModeAtomicAlloc();
	if (req == NULL) {
		wlr_log(WLR_ERROR, "drmModeAtomicAlloc failed");
		goto out;
	}

	for (size_t i = 0; i < state->connectors_len; i++) {
		if (!add_connector(req, &state->connectors[i], state->modeset, &fb_damage_clips_arr)) {
			goto out;
		}
	}

	for (size_t i = 0; i < state->connectors_len; i++) {
		struct wlr_drm_connector *conn = state->connectors[i].connector;
		struct wlr_drm_crtc *crtc = conn->crtc;

		int ret = liftoff_output_apply(crtc->liftoff, req, flags);
		if (ret != 0) {
			wlr_drm_conn_log(conn, test_only ? WLR_DEBUG : WLR_ERROR,
				"liftoff_output_apply failed: %s", strerror(-ret));
			goto out;
		}

		if (crtc->cursor &&
				liftoff_layer_needs_composition(crtc->cursor->liftoff_layer)) {
			wlr_drm_conn_log(conn, WLR_DEBUG, "Failed to scan-out cursor plane");
			goto out;
		}
	}

	ok = drmModeAtomicCommit(drm->fd, req, flags, page_flip) == 0;
	if (!ok) {
		wlr_log_errno(test_only ? WLR_DEBUG : WLR_ERROR,
			"Atomic commit failed");
	}

out:
	drmModeAtomicFree(req);
	for (size_t i = 0; i < state->connectors_len; i++) {
		struct wlr_drm_connector_state *conn_state = &state->connectors[i];
		if (ok && !test_only) {
			drm_atomic_connector_apply_commit(conn_state);
			connector_update_layers_feedback(conn_state);
		} else {
			drm_atomic_connector_rollback_commit(conn_state);
		}
	}

	uint32_t *fb_damage_clips_ptr;
	wl_array_for_each(fb_damage_clips_ptr, &fb_damage_clips_arr) {
		if (drmModeDestroyPropertyBlob(drm->fd, *fb_damage_clips_ptr) != 0) {
			wlr_log_errno(WLR_ERROR, "Failed to destroy FB_DAMAGE_CLIPS property blob");
		}
	}
	wl_array_release(&fb_damage_clips_arr);

	return ok;
}

const struct wlr_drm_interface liftoff_iface = {
	.init = init,
	.finish = finish,
	.commit = commit,
	.reset = drm_atomic_reset,
};
