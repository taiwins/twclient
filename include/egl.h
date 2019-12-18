/*
 * egl.h - EGL environment handler header
 *
 * Copyright (c) 2019 Xichen Zhou
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#ifndef TW_EGL_IFACE_H
#define TW_EGL_IFACE_H

#include <EGL/egl.h>
#include <GL/gl.h>
#include <wayland-egl.h>
#include <stdio.h>
#include <stdbool.h>


#ifdef __cplusplus
extern "C" {
#endif

struct egl_env {
	EGLDisplay egl_display;
	EGLContext egl_context;
	struct wl_display *wl_display;
	EGLConfig config;
};

bool egl_env_init(struct egl_env *env, const struct wl_display *disp);
bool egl_env_init_shared(struct egl_env *this, const struct egl_env *another);
void egl_env_end(struct egl_env *env);

#ifdef __cplusplus
}
#endif





#endif /* EOF */
