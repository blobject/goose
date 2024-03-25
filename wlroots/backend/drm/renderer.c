#include <assert.h>
#include <drm_fourcc.h>
#include <wlr/render/swapchain.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/util/log.h>
#include "backend/drm/drm.h"
#include "backend/drm/fb.h"
#include "backend/drm/renderer.h"
#include "backend/backend.h"
#include "render/drm_format_set.h"
#include "render/allocator/allocator.h"
#include "render/pixel_format.h"
#include "render/wlr_renderer.h"

bool init_drm_renderer(struct wlr_drm_backend *drm,
		struct wlr_drm_renderer *renderer) {
	renderer->wlr_rend = renderer_autocreate_with_drm_fd(drm->fd);
	if (!renderer->wlr_rend) {
		wlr_log(WLR_ERROR, "Failed to create renderer");
		return false;
	}

	uint32_t backend_caps = backend_get_buffer_caps(&drm->backend);
	renderer->allocator = allocator_autocreate_with_drm_fd(backend_caps,
		renderer->wlr_rend, drm->fd);
	if (renderer->allocator == NULL) {
		wlr_log(WLR_ERROR, "Failed to create allocator");
		wlr_renderer_destroy(renderer->wlr_rend);
		return false;
	}

	return true;
}

void finish_drm_renderer(struct wlr_drm_renderer *renderer) {
	if (!renderer) {
		return;
	}

	wlr_allocator_destroy(renderer->allocator);
	wlr_renderer_destroy(renderer->wlr_rend);
}

void finish_drm_surface(struct wlr_drm_surface *surf) {
	if (!surf || !surf->renderer) {
		return;
	}

	wlr_swapchain_destroy(surf->swapchain);

	*surf = (struct wlr_drm_surface){0};
}

bool init_drm_surface(struct wlr_drm_surface *surf,
		struct wlr_drm_renderer *renderer, int width, int height,
		const struct wlr_drm_format *drm_format) {
	if (surf->swapchain != NULL && surf->swapchain->width == width &&
			surf->swapchain->height == height) {
		return true;
	}

	finish_drm_surface(surf);

	surf->swapchain = wlr_swapchain_create(renderer->allocator, width, height,
			drm_format);
	if (surf->swapchain == NULL) {
		wlr_log(WLR_ERROR, "Failed to create swapchain");
		return false;
	}

	surf->renderer = renderer;

	return true;
}

struct wlr_buffer *drm_surface_blit(struct wlr_drm_surface *surf,
		struct wlr_buffer *buffer) {
	struct wlr_renderer *renderer = surf->renderer->wlr_rend;

	if (surf->swapchain->width != buffer->width ||
			surf->swapchain->height != buffer->height) {
		wlr_log(WLR_ERROR, "Surface size doesn't match buffer size");
		return NULL;
	}

	struct wlr_texture *tex = wlr_texture_from_buffer(renderer, buffer);
	if (tex == NULL) {
		wlr_log(WLR_ERROR, "Failed to import source buffer into multi-GPU renderer");
		return NULL;
	}

	struct wlr_buffer *dst = wlr_swapchain_acquire(surf->swapchain, NULL);
	if (!dst) {
		wlr_log(WLR_ERROR, "Failed to acquire multi-GPU swapchain buffer");
		goto error_tex;
	}

	struct wlr_render_pass *pass = wlr_renderer_begin_buffer_pass(renderer, dst, NULL);
	if (pass == NULL) {
		wlr_log(WLR_ERROR, "Failed to begin render pass with multi-GPU destination buffer");
		goto error_dst;
	}

	wlr_render_pass_add_texture(pass, &(struct wlr_render_texture_options){
		.texture = tex,
		.blend_mode = WLR_RENDER_BLEND_MODE_NONE,
	});
	if (!wlr_render_pass_submit(pass)) {
		wlr_log(WLR_ERROR, "Failed to submit multi-GPU render pass");
		goto error_dst;
	}

	wlr_texture_destroy(tex);

	return dst;

error_dst:
	wlr_buffer_unlock(dst);
error_tex:
	wlr_texture_destroy(tex);
	return NULL;
}

bool drm_plane_pick_render_format(struct wlr_drm_plane *plane,
		struct wlr_drm_format *fmt, struct wlr_drm_renderer *renderer) {
	const struct wlr_drm_format_set *render_formats =
		wlr_renderer_get_render_formats(renderer->wlr_rend);
	if (render_formats == NULL) {
		wlr_log(WLR_ERROR, "Failed to get render formats");
		return false;
	}

	const struct wlr_drm_format_set *plane_formats = &plane->formats;

	uint32_t format = DRM_FORMAT_ARGB8888;
	if (!wlr_drm_format_set_get(&plane->formats, format)) {
		const struct wlr_pixel_format_info *format_info =
			drm_get_pixel_format_info(format);
		assert(format_info != NULL &&
			format_info->opaque_substitute != DRM_FORMAT_INVALID);
		format = format_info->opaque_substitute;
	}

	const struct wlr_drm_format *render_format =
		wlr_drm_format_set_get(render_formats, format);
	if (render_format == NULL) {
		wlr_log(WLR_DEBUG, "Renderer doesn't support format 0x%"PRIX32, format);
		return false;
	}

	const struct wlr_drm_format *plane_format =
		wlr_drm_format_set_get(plane_formats, format);
	if (plane_format == NULL) {
		wlr_log(WLR_DEBUG, "Plane %"PRIu32" doesn't support format 0x%"PRIX32,
			plane->id, format);
		return false;
	}

	if (!wlr_drm_format_intersect(fmt, plane_format, render_format)) {
		wlr_log(WLR_DEBUG, "Failed to intersect plane and render "
			"modifiers for format 0x%"PRIX32, format);
		return false;
	}

	if (fmt->len == 0) {
		wlr_drm_format_finish(fmt);
		wlr_log(WLR_DEBUG, "Failed to find matching plane and renderer modifiers");
		return false;
	}

	return true;
}
