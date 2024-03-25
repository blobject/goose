/*
 * This is a deprecated interface of wlroots. It will be removed in a future
 * version.
 */

#ifndef WLR_TYPES_WLR_MATRIX_H
#define WLR_TYPES_WLR_MATRIX_H

#include <wayland-server-protocol.h>

struct wlr_box;

/** Writes the identity matrix into mat */
void wlr_matrix_identity(float mat[static 9]);

/** mat ← a × b */
void wlr_matrix_multiply(float mat[static 9], const float a[static 9],
	const float b[static 9]);

void wlr_matrix_transpose(float mat[static 9], const float a[static 9]);

/** Writes a 2D translation matrix to mat of magnitude (x, y) */
void wlr_matrix_translate(float mat[static 9], float x, float y);

/** Writes a 2D scale matrix to mat of magnitude (x, y) */
void wlr_matrix_scale(float mat[static 9], float x, float y);

/** Writes a 2D rotation matrix to mat at an angle of rad radians */
void wlr_matrix_rotate(float mat[static 9], float rad);

/** Writes a transformation matrix which applies the specified
 *  wl_output_transform to mat */
void wlr_matrix_transform(float mat[static 9],
	enum wl_output_transform transform);

/** Shortcut for the various matrix operations involved in projecting the
 *  specified wlr_box onto a given orthographic projection with a given
 *  rotation. The result is written to mat, which can be applied to each
 *  coordinate of the box to get a new coordinate from [-1,1]. */
void wlr_matrix_project_box(float mat[static 9], const struct wlr_box *box,
	enum wl_output_transform transform, float rotation,
	const float projection[static 9]);

#endif
