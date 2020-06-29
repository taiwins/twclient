/*
 * egl.h - EGL environment handler header
 *
 * Copyright (c) 2019-2020 Xichen Zhou
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifndef TW_EGL_IFACE_H
#define TW_EGL_IFACE_H

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <wayland-egl.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdbool.h>


#ifdef __cplusplus
extern "C" {
#endif

struct tw_egl_env {
	EGLDisplay egl_display;
	EGLContext egl_context;
	struct wl_display *wl_display;
	EGLConfig config;
};

bool
tw_egl_env_init(struct tw_egl_env *env, const struct wl_display *disp);

bool
tw_egl_env_init_shared(struct tw_egl_env *this,
                       const struct tw_egl_env *another);
void
tw_egl_env_end(struct tw_egl_env *env);

#ifdef __cplusplus
}
#endif





#endif /* EOF */
